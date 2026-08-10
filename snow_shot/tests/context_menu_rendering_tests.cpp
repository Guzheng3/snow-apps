#include <QAction>
#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QThread>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

#include "theme/theme_manager.h"
#include "widgets/context_menu.h"

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    return predicate();
}

std::array<QPoint, 4> cornerPixels(const QSize& size) {
    return {QPoint(0, 0), QPoint(size.width() - 1, 0),
            QPoint(0, size.height() - 1), QPoint(size.width() - 1, size.height() - 1)};
}

void populateProductionSizedMenu(adqt::widgets::AdContextMenu& menu) {
    menu.setFixedWidth(300);
    menu.addItem(QStringLiteral("Copy to Clipboard"));
    QAction* disabled = menu.addItem(QStringLiteral("Recognizing text"));
    disabled->setEnabled(false);
    QAction* checkable = menu.addItem(QStringLiteral("Drawing Mode"));
    checkable->setCheckable(true);
    menu.addSeparator();
    adqt::widgets::AdContextMenu* processMenu = menu.addSubMenu(QStringLiteral("Process Image"));
    processMenu->addItem(QStringLiteral("Rotate Clockwise"));
    processMenu->addItem(QStringLiteral("Flip Horizontally"));
    menu.addSubMenu(QStringLiteral("Opacity"))->addItem(QStringLiteral("100%"));
    menu.addSeparator();
    menu.addItem(QStringLiteral("Close"));
}

void dirtyRenderTargetCornersAreCleared() {
    adqt::widgets::AdContextMenu menu;
    require(menu.windowFlags().testFlag(Qt::Popup),
            "context menu must remain a native popup for tray and keyboard behavior");
    require(menu.windowFlags().testFlag(Qt::FramelessWindowHint),
            "translucent context menu must use a frameless top-level window");
    require(menu.windowFlags().testFlag(Qt::NoDropShadowWindowHint),
            "context menu must own its shadow contract instead of using a platform shadow");
    require(menu.testAttribute(Qt::WA_TranslucentBackground),
            "context menu surface must expose an alpha channel");
    populateProductionSizedMenu(menu);
    menu.ensurePolished();
    menu.adjustSize();
    require(menu.width() > 0 && menu.height() > 0, "context menu did not acquire a render size");

    QImage image(menu.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(0, 0, 0, 255));
    {
        QPainter painter(&image);
        menu.render(&painter);
    }

    for (const QPoint& corner : cornerPixels(image.size())) {
        require(image.pixelColor(corner).alpha() == 0,
                "context menu did not clear a dirty rounded-corner pixel to transparent");
    }
}

int colorDistance(const QColor& lhs, const QColor& rhs) {
    return std::max({std::abs(lhs.red() - rhs.red()), std::abs(lhs.green() - rhs.green()),
                     std::abs(lhs.blue() - rhs.blue())});
}

QColor logicalPixel(const QPixmap& pixmap, const QPoint& logicalPosition) {
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    const qreal dpr = pixmap.devicePixelRatio();
    const QPoint physicalPosition(
        std::clamp(qRound((logicalPosition.x() + 0.5) * dpr - 0.5), 0, image.width() - 1),
        std::clamp(qRound((logicalPosition.y() + 0.5) * dpr - 0.5), 0, image.height() - 1));
    return image.pixelColor(physicalPosition);
}

void nativePopupCornersCompositeOverTheirBackdrop() {
    if (QGuiApplication::platformName() != QStringLiteral("windows")) {
        return;
    }

    const QColor backdropColor(QStringLiteral("#c12f76"));
    QWidget backdrop;
    backdrop.setWindowFlag(Qt::FramelessWindowHint, true);
    backdrop.setAutoFillBackground(true);
    QPalette palette = backdrop.palette();
    palette.setColor(QPalette::Window, backdropColor);
    backdrop.setPalette(palette);
    backdrop.resize(480, 320);
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        backdrop.move(screen->availableGeometry().center() - backdrop.rect().center());
    }
    backdrop.show();
    require(waitUntil([&backdrop]() {
                return backdrop.windowHandle() != nullptr && backdrop.windowHandle()->isExposed();
            }),
            "native backdrop window was not exposed");

    adqt::widgets::AdContextMenu menu(&backdrop);
    populateProductionSizedMenu(menu);
    menu.popupAt(backdrop.mapToGlobal(QPoint(120, 90)));
    require(waitUntil([&menu]() {
                return menu.isVisible() && menu.windowHandle() != nullptr &&
                       menu.windowHandle()->isExposed();
            }),
            "native context menu was not exposed");
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    QScreen* screen = menu.screen();
    require(screen != nullptr, "native context menu has no screen");
    const QRect menuGeometry(menu.mapToGlobal(QPoint(0, 0)), menu.size());
    const QPixmap capture = screen->grabWindow(0, menuGeometry.x(), menuGeometry.y(),
                                                menuGeometry.width(), menuGeometry.height());
    require(!capture.isNull(), "native context menu capture failed");

    for (const QPoint& corner : cornerPixels(menu.size())) {
        const int xDirection = corner.x() == 0 ? 1 : -1;
        const int yDirection = corner.y() == 0 ? 1 : -1;
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                const QPoint sample = corner + QPoint(x * xDirection, y * yDirection);
                require(colorDistance(logicalPixel(capture, sample), backdropColor) <= 12,
                        "native context menu corner did not composite over the known backdrop");
            }
        }
    }
    menu.hide();
    backdrop.hide();
}

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    adqt::theme::ThemeManager::instance().applyTo(application);
    try {
        nativePopupCornersCompositeOverTheirBackdrop();
        dirtyRenderTargetCornersAreCleared();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
