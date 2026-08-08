#include "widgets/popover.h"
#include "widgets/select.h"
#include "widgets/tooltip.h"
#include "widgets/detail/overlay_popup_controller.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QEventLoop>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QToolTip>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <cstdlib>
#include <iostream>

#if defined(Q_OS_WIN) || defined(_WIN32)
#include <qt_windows.h>
#endif

namespace {
#if defined(Q_OS_WIN) || defined(_WIN32)
HWND toNativeHwnd(WId windowId) {
    // Qt transports the native HWND through its integer-valued WId type.
    return reinterpret_cast<HWND>(windowId); // NOLINT(performance-no-int-to-ptr)
}
#endif

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void flushEvents() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::PolishRequest);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
}

void waitFor(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
    flushEvents();
}

class HoverControllerTestDelegate final
    : public adqt::widgets::detail::OverlayPopupControllerDelegate {
  public:
    HoverControllerTestDelegate(QWidget* scope, QWidget* trigger, QWidget* popup)
        : scope_(scope), trigger_(trigger), popup_(popup) {}

    QObject* popupOwnerObject() const override {
        return scope_;
    }

    QWidget* popupTriggerWidget() const override {
        return trigger_;
    }
    QWidget* popupAnchorWidget() const override {
        return trigger_;
    }
    QWidget* popupScopeWindow() const override {
        return scope_;
    }
    QWidget* popupSurfaceWidget() const override {
        return popup_;
    }
    QWidget* popupEnsureSurface() override {
        return popup_;
    }

    void popupPrepareToShow() override {
        popup_->resize(120, 56);
    }

    bool popupHasContent() const override {
        return true;
    }

    adqt::widgets::detail::OverlayPopupPlacement popupPlacement() const override {
        return adqt::widgets::detail::OverlayPopupPlacement::Bottom;
    }

    bool popupAutoAdjustOverflow() const override {
        return true;
    }
    bool popupArrowVisible() const override {
        return false;
    }
    bool popupArrowPointAtCenter() const override {
        return false;
    }
    int popupOffset() const override {
        return 0;
    }
    int popupArrowOffsetHorizontal() const override {
        return 0;
    }
    int popupArrowOffsetVertical() const override {
        return 0;
    }

    void popupApplyResolvedPlacement(adqt::widgets::detail::OverlayPopupPlacement, qreal) override {
    }

  private:
    QWidget* scope_ = nullptr;
    QWidget* trigger_ = nullptr;
    QWidget* popup_ = nullptr;
};

int tooltipWakeUpDelay(const QWidget& widget) {
    return std::max(0, widget.style()->styleHint(QStyle::SH_ToolTip_WakeUpDelay, nullptr, &widget));
}

void waitForTooltipWakeUp(const QWidget& widget) {
    waitFor(tooltipWakeUpDelay(widget) + 100);
}

QWidget* visibleTopLevel(const QString& objectName) {
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (widget != nullptr && widget->objectName() == objectName && widget->isVisible()) {
            return widget;
        }
    }
    return nullptr;
}

int visibleTopLevelCount(const QString& objectName) {
    int count = 0;
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (widget != nullptr && widget->objectName() == objectName && widget->isVisible()) {
            ++count;
        }
    }
    return count;
}

bool sendTooltipEvent(QWidget& widget, const QPoint& localPosition) {
    QHelpEvent event(QEvent::ToolTip, localPosition, widget.mapToGlobal(localPosition));
    QCoreApplication::sendEvent(&widget, &event);
    flushEvents();
    return event.isAccepted();
}

void sendMouseMove(QWidget& widget, const QPoint& localPosition) {
    QMouseEvent event(QEvent::MouseMove, QPointF(localPosition),
                      QPointF(widget.mapToGlobal(localPosition)), Qt::NoButton, Qt::NoButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &event);
    flushEvents();
}

QString visibleTooltipText(QWidget* surface) {
    if (surface == nullptr) {
        return QString();
    }
    const QList<QLabel*> labels = surface->findChildren<QLabel*>();
    for (QLabel* label : labels) {
        if (label != nullptr && label->isVisible() && !label->text().isEmpty()) {
            return label->text();
        }
    }
    return QString();
}

void requireNativeOwner(QWidget& tooltipSurface, QWidget& owner) {
    require(tooltipSurface.windowHandle() != nullptr, "tooltip should have a QWindow");
    require(owner.windowHandle() != nullptr, "owner should have a QWindow");
    require(tooltipSurface.windowHandle()->transientParent() == owner.windowHandle(),
            "tooltip transient parent should be the popup window");

#if defined(Q_OS_WIN) || defined(_WIN32)
    if (QGuiApplication::platformName() == QStringLiteral("windows")) {
        const HWND tooltipHwnd = toNativeHwnd(tooltipSurface.winId());
        const HWND ownerHwnd = toNativeHwnd(owner.winId());
        require(GetWindow(tooltipHwnd, GW_OWNER) == ownerHwnd,
                "tooltip HWND owner should be the popup HWND");
        require((GetWindowLongPtr(tooltipHwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0,
                "tooltip HWND should stay in the topmost band");
        require(GetForegroundWindow() != tooltipHwnd,
                "tooltip must not become the foreground window");
    }
#endif
}

void applicationTooltipsUseCustomSurface() {
    QWidget host;
    host.setObjectName(QStringLiteral("application-tooltip-owner"));
    host.resize(420, 260);

    auto* button = new QPushButton(QStringLiteral("Application tooltip"), &host);
    button->setGeometry(30, 30, 160, 32);
    button->setToolTip(QStringLiteral("Application button tooltip"));

    auto* list = new QListWidget(&host);
    list->setGeometry(30, 90, 240, 120);
    auto* item = new QListWidgetItem(QStringLiteral("Application item"), list);
    item->setToolTip(QStringLiteral("Application item tooltip"));

    host.show();
    host.activateWindow();
    host.raise();
    flushEvents();

    sendTooltipEvent(*button, button->rect().center());
    QWidget* tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr, "ordinary QWidget tooltip should use custom surface");
    require(visibleTooltipText(tooltipSurface) == QStringLiteral("Application button tooltip"),
            "ordinary QWidget tooltip text should be preserved");
    require(!QToolTip::isVisible(), "ordinary QWidget tooltip must not create QToolTip");
    requireNativeOwner(*tooltipSurface, host);

    QPointer<QWidget> initialTooltipSurface = tooltipSurface;
    const QPoint initialPosition = tooltipSurface->pos();
    const QRect initialGeometry = tooltipSurface->geometry();
    sendTooltipEvent(*button, QPoint(button->width() - 2, button->rect().center().y()));
    require(tooltipSurface->pos() == initialPosition,
            "ordinary QWidget tooltip should remain fixed after repeated tooltip events");

    QEvent buttonLeaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(button, &buttonLeaveEvent);
    waitFor(350);
    const QPoint alternateEventPosition(2, button->rect().center().y());
    sendTooltipEvent(*button, alternateEventPosition);
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(initialTooltipSurface.isNull() && tooltipSurface != nullptr &&
                tooltipSurface->geometry() == initialGeometry,
            "reshown QWidget tooltips should use a fresh surface and ignore the mouse position");

    QCoreApplication::sendEvent(button, &buttonLeaveEvent);
    waitFor(350);
    button->resize(button->width() + 40, button->height());
    sendTooltipEvent(*button, alternateEventPosition);
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr && tooltipSurface->pos() != initialPosition,
            "Wider controls should reposition tooltips around the updated control bounds");

    const QRect itemRect = list->visualItemRect(item);
    sendTooltipEvent(*list->viewport(), itemRect.center());
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr,
            "a changed tooltip request should reuse Qt's visible tooltip surface immediately");
    require(visibleTooltipText(tooltipSurface) == QStringLiteral("Application item tooltip"),
            "ordinary item tooltip should preserve Qt::ToolTipRole");
    requireNativeOwner(*tooltipSurface, host);

    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(list->viewport(), &leaveEvent);
    waitFor(350);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "ordinary custom tooltip should use Qt's delayed leave hiding");

    adqt::widgets::AdTooltip::showText(button, QStringLiteral("Programmatic bridge tooltip"), 5000);
    flushEvents();
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr &&
                visibleTooltipText(tooltipSurface) == QStringLiteral("Programmatic bridge tooltip"),
            "programmatic tooltip text should use the shared QtTooltipBridge surface");
    require(!QToolTip::isVisible(), "programmatic bridge tooltips must not create QToolTip");
    requireNativeOwner(*tooltipSurface, host);
    adqt::widgets::AdTooltip::showText(button, QString());
    waitFor(350);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "empty programmatic tooltip text should complete Qt's delayed hide before reuse");

    auto* managedButton = new QPushButton(QStringLiteral("Managed tooltip"), &host);
    managedButton->setGeometry(220, 30, 160, 32);
    managedButton->setToolTip(QStringLiteral("Native tooltip must remain suppressed"));
    managedButton->show();
    adqt::widgets::AdTooltip managedTooltip;
    managedTooltip.setActivationMode(adqt::widgets::AdTooltip::ActivationMode::Manual);
    managedTooltip.setLayerMode(adqt::widgets::AdTooltip::LayerMode::TopLevelTransient);
    managedTooltip.setTargetWidget(managedButton);
    managedTooltip.setText(QStringLiteral("Explicit AdTooltip"));
    managedTooltip.show();
    flushEvents();

    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr, "explicit AdTooltip should be visible");
    sendTooltipEvent(*managedButton, managedButton->rect().center());
    require(visibleTopLevelCount(QStringLiteral("adtooltip-surface")) == 1,
            "application bridge must not duplicate an explicitly managed AdTooltip");
    require(visibleTooltipText(tooltipSurface) == QStringLiteral("Explicit AdTooltip"),
            "explicit AdTooltip content should remain authoritative");
    require(!QToolTip::isVisible(), "managed targets must not fall back to QToolTip");
    managedTooltip.hide();
    flushEvents();

    host.hide();
    flushEvents();
}

void programmaticTooltipMatchesQtShowHideSemantics() {
    QWidget host;
    host.resize(420, 220);
    auto* target = new QPushButton(QStringLiteral("Target"), &host);
    target->setGeometry(20, 20, 130, 36);
    target->setToolTip(QStringLiteral("Hover candidate"));
    auto* unrelated = new QPushButton(QStringLiteral("Unrelated"), &host);
    unrelated->setGeometry(190, 20, 130, 36);
    auto* list = new QListWidget(&host);
    list->setGeometry(20, 80, 300, 110);
    auto* item = new QListWidgetItem(QStringLiteral("Item"), list);
    item->setToolTip(QStringLiteral("Item tooltip"));
    host.show();
    flushEvents();

    adqt::widgets::AdTooltip::showText(target, QStringLiteral("Initial"), 5000);
    QWidget* surface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(surface != nullptr, "programmatic tooltip should be visible");
    QPointer<QWidget> initialSurface = surface;

    adqt::widgets::AdTooltip::showText(target, QStringLiteral("Initial"), 5000);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == initialSurface,
            "an identical programmatic tooltip must retain Qt's visible surface");
    adqt::widgets::AdTooltip::showText(target, QString());
    waitFor(350);

    adqt::widgets::AdTooltip::showText(target, QStringLiteral("Initial"), 5000);
    surface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(surface != nullptr, "replacement tooltip should be visible");
    QPointer<QWidget> replacementSurface = surface;
    adqt::widgets::AdTooltip::showText(target, QStringLiteral("Updated"), 5000);
    surface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(surface == replacementSurface &&
                visibleTooltipText(surface) == QStringLiteral("Updated"),
            "changed programmatic tooltip text must reuse Qt's visible surface");
    adqt::widgets::AdTooltip::showText(target, QString());
    waitFor(350);

    adqt::widgets::AdTooltip::showText(target, QStringLiteral("Delayed hide"), 5000);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) != nullptr,
            "tooltip should be visible before its empty-text hide request");
    adqt::widgets::AdTooltip::showText(target, QString());
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) != nullptr,
            "empty tooltip text must use QToolTip's delayed hide path");
    waitFor(350);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "empty tooltip text should hide after QTipLabel's 300ms delay");

    adqt::widgets::AdTooltip::showText(target, QStringLiteral("Global leave"), 5000);
    surface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(surface != nullptr, "tooltip should be visible before a global leave event");
    QEvent unrelatedLeave(QEvent::Leave);
    QCoreApplication::sendEvent(unrelated, &unrelatedLeave);
    waitFor(150);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == surface,
            "a global leave should retain the tooltip until Qt's hide delay elapses");
    waitFor(200);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "a global leave should schedule QTipLabel's delayed hide");

    adqt::widgets::AdTooltip::showText(target, QStringLiteral("Ignore hide event"), 5000);
    surface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(surface != nullptr, "tooltip should be visible before a target hide event");
    QEvent targetHide(QEvent::Hide);
    QCoreApplication::sendEvent(target, &targetHide);
    waitFor(350);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == surface,
            "QEvent::Hide must not be an additional QToolTip hide condition");
    adqt::widgets::AdTooltip::showText(target, QString());
    waitFor(350);

    const int wakeUpDelay = tooltipWakeUpDelay(*target);
    require(wakeUpDelay > 0, "platform tooltip wake-up delay should be positive");
    sendMouseMove(*target, target->rect().center());
    sendTooltipEvent(*target, target->rect().center());
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "a hover tooltip should remain pending before its Qt wake-up delay");
    QEvent unrelatedPendingLeave(QEvent::Leave);
    QCoreApplication::sendEvent(unrelated, &unrelatedPendingLeave);
    waitFor(wakeUpDelay + 100);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "every leave event must cancel QApplication's pending tooltip wake-up");

    QEvent targetEnter(QEvent::Enter);
    QCoreApplication::sendEvent(target, &targetEnter);
    waitFor(wakeUpDelay + 100);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "enter alone must not start a Qt tooltip wake-up cycle");

    const int halfWakeUpDelay = std::max(1, wakeUpDelay / 2);
    sendMouseMove(*target, target->rect().center());
    waitFor(halfWakeUpDelay);
    sendMouseMove(*target, target->rect().center());
    waitFor(halfWakeUpDelay);
    sendTooltipEvent(*target, target->rect().center());
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "a matching mouse move must restart Qt's tooltip wake-up delay");
    waitFor(wakeUpDelay + 100);
    surface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(surface != nullptr, "a restarted tooltip wake-up cycle should eventually show");

    QEvent targetLeave(QEvent::Leave);
    QCoreApplication::sendEvent(target, &targetLeave);
    QCoreApplication::sendEvent(target, &targetEnter);
    waitFor(350);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "enter must not cancel QTipLabel's delayed hide after leave");

    const QRect itemRect = list->visualItemRect(item);
    sendTooltipEvent(*list->viewport(), itemRect.center());
    surface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(surface != nullptr, "item tooltip should be visible before leaving its active rect");
    sendMouseMove(*list->viewport(), QPoint(list->viewport()->rect().left() + 4,
                                            list->viewport()->rect().bottom() - 4));
    waitFor(150);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == surface,
            "leaving a tooltip active rect must use QTipLabel's delayed hide");
    waitFor(200);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "leaving a tooltip active rect should schedule QTipLabel's delayed hide");

    host.hide();
    flushEvents();
}

void topLevelTransientTooltipOwnsItsWindow() {
    QWidget owner;
    owner.setObjectName(QStringLiteral("tooltip-test-owner"));
    owner.resize(320, 180);
    auto* target = new QPushButton(QStringLiteral("Target"), &owner);
    target->setGeometry(40, 40, 100, 32);
    owner.show();
    flushEvents();

    adqt::widgets::AdTooltip tooltip;
    require(tooltip.layerMode() == adqt::widgets::AdTooltip::LayerMode::InWindow,
            "AdTooltip should remain in-window by default");
    tooltip.setActivationMode(adqt::widgets::AdTooltip::ActivationMode::Manual);
    tooltip.setLayerMode(adqt::widgets::AdTooltip::LayerMode::TopLevelTransient);
    tooltip.setTargetWidget(target);
    tooltip.setText(QStringLiteral("Owned tooltip"));
    tooltip.show();
    flushEvents();

    QWidget* surface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(surface != nullptr, "top-level AdTooltip surface should be visible");
    require(surface->windowType() == Qt::ToolTip, "surface should use Qt::ToolTip window type");
    require(surface->windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus),
            "surface should not accept focus");
    require(surface->windowFlags().testFlag(Qt::WindowTransparentForInput),
            "surface should be transparent for input");
    requireNativeOwner(*surface, owner);

    QEvent ownerDeactivateEvent(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&owner, &ownerDeactivateEvent);
    flushEvents();
    require(tooltip.isVisible() && surface->isVisible(),
            "non-activating tooltip should survive its owner window's deactivate event");

    tooltip.hide();
    flushEvents();
    require(surface->windowHandle()->transientParent() == nullptr,
            "hidden tooltip should release its transient parent");
#if defined(Q_OS_WIN) || defined(_WIN32)
    if (QGuiApplication::platformName() == QStringLiteral("windows")) {
        require(GetWindow(toNativeHwnd(surface->winId()), GW_OWNER) == nullptr,
                "hidden tooltip HWND should release its owner");
    }
#endif
    owner.hide();
    flushEvents();
}

void automaticTooltipUsesQtWakeUpDelay() {
    const QPoint originalCursorPosition = QCursor::pos();
    QWidget host;
    host.resize(320, 180);
    auto* target = new QPushButton(QStringLiteral("Delayed tooltip"), &host);
    target->setGeometry(40, 40, 140, 32);
    host.show();
    flushEvents();

    adqt::widgets::AdTooltip tooltip;
    tooltip.setTargetWidget(target);
    tooltip.setText(QStringLiteral("Delayed like QToolTip"));

    const int wakeUpDelay = tooltipWakeUpDelay(*target);
    require(wakeUpDelay > 0, "platform tooltip wake-up delay should be positive");
    require(tooltip.hoverOpenDelayMs() == wakeUpDelay,
            "AdTooltip default hover delay should come from SH_ToolTip_WakeUpDelay");
    require(tooltip.hoverCloseDelayMs() == 300,
            "AdTooltip default leave delay should match QTipLabel's 300ms hide timer");

    QCursor::setPos(target->mapToGlobal(target->rect().center()));
    flushEvents();
    QEvent enterEvent(QEvent::Enter);
    QCoreApplication::sendEvent(target, &enterEvent);
    const int earlyDelay = std::max(1, wakeUpDelay / 2);
    waitFor(earlyDelay);
    require(!tooltip.isVisible(), "AdTooltip must remain hidden before Qt's wake-up delay");

    waitFor(wakeUpDelay - earlyDelay + 100);
    require(tooltip.isVisible(), "AdTooltip should appear after Qt's wake-up delay");
    tooltip.hide();
    host.hide();
    QCursor::setPos(originalCursorPosition);
    flushEvents();
}

void applicationTooltipTargetsHaveIndependentWakeCycles() {
    QWidget host;
    host.resize(520, 180);
    auto* first = new QPushButton(QStringLiteral("First"), &host);
    auto* second = new QPushButton(QStringLiteral("Second"), &host);
    auto* third = new QPushButton(QStringLiteral("Third"), &host);
    first->setGeometry(20, 40, 140, 36);
    second->setGeometry(190, 40, 140, 36);
    third->setGeometry(360, 40, 140, 36);
    first->setToolTip(QStringLiteral("First tooltip"));
    second->setToolTip(QStringLiteral("Second tooltip with different geometry"));
    third->setToolTip(QStringLiteral("Third tooltip"));
    host.show();
    host.activateWindow();
    host.raise();
    flushEvents();

    const int wakeUpDelay = tooltipWakeUpDelay(*first);
    const int partialDelay = std::max(1, wakeUpDelay / 5);
    sendMouseMove(*first, first->rect().center());
    waitFor(partialDelay);
    sendMouseMove(*second, second->rect().center());
    waitFor(partialDelay);
    sendMouseMove(*third, third->rect().center());
    waitFor(partialDelay);

    const bool partialTargetTimesWereNotAccumulated =
        visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr;
    sendTooltipEvent(*third, third->rect().center());
    const bool currentTargetStillRequiresItsOwnThreshold =
        visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr;
    if (!currentTargetStillRequiresItsOwnThreshold) {
        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(third, &leaveEvent);
        waitFor(350);
    }

    sendTooltipEvent(*first, first->rect().center());
    const bool staleRequestWasRejected =
        visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr;
    if (!staleRequestWasRejected) {
        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(first, &leaveEvent);
        waitFor(350);
    }

    sendMouseMove(*first, first->rect().center());
    waitForTooltipWakeUp(*first);
    sendTooltipEvent(*first, first->rect().center());
    QWidget* firstSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(firstSurface != nullptr,
            "first tooltip should be visible for surface reuse reproduction");
    const QPoint firstSurfacePosition = firstSurface->pos();
    QObject* const sharedTooltipManager = first->property("adqt.tooltip.manager").value<QObject*>();
    QPointer<QWidget> previousSurface = firstSurface;

    sendMouseMove(*second, second->rect().center());
    sendTooltipEvent(*second, second->rect().center());
    waitForTooltipWakeUp(*second);
    QWidget* secondSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    const bool previousSurfaceWasReused = previousSurface == secondSurface;
    const bool tooltipManagerWasReused =
        sharedTooltipManager != nullptr &&
        second->property("adqt.tooltip.manager").value<QObject*>() == sharedTooltipManager;
    const bool secondContentAndGeometryAreCurrent =
        secondSurface != nullptr &&
        visibleTooltipText(secondSurface) ==
            QStringLiteral("Second tooltip with different geometry") &&
        secondSurface->pos() != firstSurfacePosition;

    if (!partialTargetTimesWereNotAccumulated || !currentTargetStillRequiresItsOwnThreshold ||
        !staleRequestWasRejected || !previousSurfaceWasReused || !tooltipManagerWasReused ||
        !secondContentAndGeometryAreCurrent) {
        std::cerr << "tooltip reproduction: partialTimesIndependent="
                  << partialTargetTimesWereNotAccumulated
                  << ", currentThresholdRespected=" << currentTargetStillRequiresItsOwnThreshold
                  << ", staleRejected=" << staleRequestWasRejected
                  << ", previousSurfaceReused=" << previousSurfaceWasReused
                  << ", tooltipManagerReused=" << tooltipManagerWasReused
                  << ", secondContentAndGeometryCurrent=" << secondContentAndGeometryAreCurrent
                  << '\n';
    }
    require(partialTargetTimesWereNotAccumulated,
            "partial hover times from different tooltip targets must never be accumulated");
    require(currentTargetStillRequiresItsOwnThreshold,
            "the current target must satisfy its own complete tooltip wake-up threshold");
    require(staleRequestWasRejected,
            "a tooltip event from a previous hover target must not inherit elapsed wake-up time");
    require(previousSurfaceWasReused,
            "a changed tooltip request should reuse Qt's visible tooltip surface");
    require(tooltipManagerWasReused,
            "application tooltips should reuse one logical AdTooltip manager");
    require(secondContentAndGeometryAreCurrent,
            "the second tooltip must show only its own content at its own position");

    host.hide();
    flushEvents();
}

void sharedHoverControllerUsesCursorTruth() {
    using Controller = adqt::widgets::detail::OverlayPopupController;

    QWidget host;
    host.resize(420, 260);
    auto* trigger = new QPushButton(QStringLiteral("Hover trigger"), &host);
    trigger->setGeometry(40, 40, 120, 36);
    auto* popup = new QWidget(&host);
    popup->setObjectName(QStringLiteral("hover-controller-test-popup"));
    popup->resize(120, 56);
    popup->hide();

    host.show();
    flushEvents();

    QPoint cursorPosition = host.mapToGlobal(QPoint(360, 220));
    HoverControllerTestDelegate delegate(&host, trigger, popup);
    Controller controller(&delegate, &host, [&cursorPosition]() { return cursorPosition; });
    controller.setMouseEnterDelayMs(0);
    controller.setMouseLeaveDelayMs(40);
    controller.anchorWidgetChanged();
    controller.popupSurfaceChanged();

    QEvent enterEvent(QEvent::Enter);
    QCoreApplication::sendEvent(trigger, &enterEvent);
    waitFor(60);
    require(!controller.popupVisible(),
            "a stale Enter event must not open a hover popup while the cursor is outside");

    cursorPosition = trigger->mapToGlobal(trigger->rect().center());
    QCoreApplication::sendEvent(trigger, &enterEvent);
    flushEvents();
    require(controller.popupVisible(), "cursor entry should open the hover popup");

    cursorPosition = host.mapToGlobal(QPoint(360, 220));
    waitFor(120);
    require(!controller.popupVisible(),
            "the hover watchdog must close an open popup when no Leave event is delivered");

    cursorPosition = trigger->mapToGlobal(trigger->rect().center());
    QCoreApplication::sendEvent(trigger, &enterEvent);
    flushEvents();
    require(controller.popupVisible(), "the hover popup should reopen for handoff testing");

    cursorPosition = popup->mapToGlobal(popup->rect().center());
    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(trigger, &leaveEvent);
    waitFor(80);
    require(controller.popupVisible(),
            "moving from the trigger into the popup must cancel delayed closure");

    cursorPosition = host.mapToGlobal(QPoint(360, 220));
    QCoreApplication::sendEvent(popup, &leaveEvent);
    waitFor(15);
    cursorPosition = popup->mapToGlobal(popup->rect().center());
    waitFor(90);
    require(controller.popupVisible(),
            "returning to the popup before the deadline must cancel closure");

    cursorPosition = host.mapToGlobal(QPoint(360, 220));
    waitFor(120);
    require(!controller.popupVisible(),
            "leaving again after cancellation must start a fresh close cycle");

    controller.setTriggerModes(Controller::Trigger::Hover | Controller::Trigger::Click);
    cursorPosition = trigger->mapToGlobal(trigger->rect().center());
    QCoreApplication::sendEvent(trigger, &enterEvent);
    flushEvents();
    require(controller.popupVisible(), "combined-trigger popup should open by hover");

    const QPoint triggerCenter = trigger->rect().center();
    const QPoint triggerCenterGlobal = trigger->mapToGlobal(triggerCenter);
    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPointF(triggerCenter),
                           QPointF(triggerCenterGlobal), Qt::LeftButton, Qt::LeftButton,
                           Qt::NoModifier);
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPointF(triggerCenter),
                             QPointF(triggerCenterGlobal), Qt::LeftButton, Qt::NoButton,
                             Qt::NoModifier);
    QCoreApplication::sendEvent(trigger, &pressEvent);
    QCoreApplication::sendEvent(trigger, &releaseEvent);

    cursorPosition = host.mapToGlobal(QPoint(360, 220));
    waitFor(120);
    require(controller.popupVisible(),
            "clearing the hover reason must preserve a popup held open by click");

    cursorPosition = triggerCenterGlobal;
    QCoreApplication::sendEvent(trigger, &pressEvent);
    QCoreApplication::sendEvent(trigger, &releaseEvent);
    flushEvents();
    require(!controller.popupVisible(), "clearing the final click reason should close the popup");

    controller.setTriggerModes(Controller::Trigger::Hover);
    controller.setMouseLeaveDelayMs(0);
    QCoreApplication::sendEvent(trigger, &enterEvent);
    flushEvents();
    require(controller.popupVisible(), "zero-delay hover popup should open for close testing");
    cursorPosition = host.mapToGlobal(QPoint(360, 220));
    QCoreApplication::sendEvent(trigger, &leaveEvent);
    flushEvents();
    require(!controller.popupVisible(),
            "zero-delay closure must hide immediately after cursor verification");
    controller.setMouseLeaveDelayMs(40);

    int openRequests = 0;
    int closeRequests = 0;
    controller.setVisibilityMode(Controller::VisibilityMode::External);
    QObject::connect(&controller, &Controller::popupVisibilityRequested, &host, [&](bool visible) {
        visible ? ++openRequests : ++closeRequests;
        controller.setPopupVisible(visible);
    });

    cursorPosition = trigger->mapToGlobal(trigger->rect().center());
    QCoreApplication::sendEvent(trigger, &enterEvent);
    flushEvents();
    require(controller.popupVisible() && openRequests == 1,
            "external visibility should receive one hover-open request");

    cursorPosition = host.mapToGlobal(QPoint(360, 220));
    waitFor(120);
    require(!controller.popupVisible() && closeRequests == 1,
            "external visibility should receive exactly one watchdog close request");

    controller.setPopupVisible(true);
    waitFor(80);
    require(controller.popupVisible() && closeRequests == 1,
            "an externally shown popup must not close without a hover session");
    controller.setPopupVisible(false);
    host.hide();
    flushEvents();
}

void qtToolRoutesTooltipsAndSuppressesItsTrigger() {
    QWidget host;
    host.resize(480, 320);
    auto* trigger = new QPushButton(QStringLiteral("Open"), &host);
    trigger->setGeometry(80, 80, 100, 32);
    trigger->setToolTip(QStringLiteral("Trigger tooltip"));

    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    auto* button = new QPushButton(QStringLiteral("Preset"), content);
    button->setToolTip(QStringLiteral("Popup button tooltip"));
    button->setToolTipDuration(5000);
    layout->addWidget(button);

    auto* shortButton = new QPushButton(QStringLiteral("Short"), content);
    shortButton->setToolTip(QStringLiteral("Short-lived tooltip"));
    shortButton->setToolTipDuration(800);
    layout->addWidget(shortButton);

    auto* list = new QListWidget(content);
    auto* item = new QListWidgetItem(QStringLiteral("Color"), list);
    item->setToolTip(QStringLiteral("Popup item tooltip"));
    auto* secondItem = new QListWidgetItem(QStringLiteral("Same tooltip"), list);
    secondItem->setToolTip(QStringLiteral("Popup item tooltip"));
    layout->addWidget(list);

    host.show();
    flushEvents();

    adqt::widgets::AdPopover popover(&host);
    popover.setSourceWidget(trigger);
    popover.setContentWidget(content);
    popover.setVisibilityPolicy(adqt::widgets::AdPopover::VisibilityPolicy::Manual);
    popover.setPopupLayerMode(adqt::widgets::AdPopover::PopupLayerMode::QtTool);
    popover.show();
    flushEvents();

    QWidget* popup = visibleTopLevel(QStringLiteral("adpopover-surface"));
    require(popup != nullptr, "QtTool popover should be visible");
    require(popup->windowType() == Qt::Tool, "popover should use Qt::Tool window type");

    const QPoint initialEventPosition = button->rect().center();
    const int popupWakeUpDelay = tooltipWakeUpDelay(*button);
    const int popupPartialDelay = std::max(1, popupWakeUpDelay / 3);
    sendMouseMove(*button, initialEventPosition);
    waitFor(popupPartialDelay);
    sendMouseMove(*shortButton, shortButton->rect().center());
    waitFor(popupPartialDelay);
    sendTooltipEvent(*button, initialEventPosition);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "QtTool must reject a tooltip event from the previous hover target");

    sendMouseMove(*button, initialEventPosition);
    sendTooltipEvent(*button, initialEventPosition);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "QtTool tooltip targets must satisfy independent wake-up thresholds");
    waitForTooltipWakeUp(*button);
    QWidget* tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr, "popup button should show custom tooltip surface");
    require(visibleTooltipText(tooltipSurface) == QStringLiteral("Popup button tooltip"),
            "custom tooltip should preserve QWidget::toolTip text");
    require(!QToolTip::isVisible(), "Qt QToolTip should not be used inside QtTool");
    requireNativeOwner(*tooltipSurface, *popup);

    const QPoint initialTooltipPosition = tooltipSurface->pos();
    sendTooltipEvent(*button, QPoint(button->width() - 2, button->rect().center().y()));
    require(tooltipSurface->pos() == initialTooltipPosition,
            "same QWidget tooltip should remain at its first position");

    const QPoint originalButtonPosition = button->pos();
    button->move(originalButtonPosition + QPoint(20, 0));
    flushEvents();
    require(tooltipSurface->pos() == initialTooltipPosition,
            "visible QWidget tooltip should not follow target geometry changes");
    button->move(originalButtonPosition);
    flushEvents();

    const QPoint originalPopupPosition = popup->pos();
    popup->move(originalPopupPosition + QPoint(20, 20));
    flushEvents();
    require(tooltipSurface->pos() == initialTooltipPosition,
            "visible tooltip should keep its global position when its owner popup moves");

    button->setToolTip(QStringLiteral("Updated popup button tooltip"));
    sendTooltipEvent(*button, initialEventPosition);
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr,
            "changed tooltip content should reuse Qt's visible tooltip immediately");
    require(tooltipSurface->pos() != initialTooltipPosition,
            "changed tooltip text should recapture control geometry after its owner moves");
    require(visibleTooltipText(tooltipSurface) == QStringLiteral("Updated popup button tooltip"),
            "changed tooltip text should update the reused surface");
    const QPoint updatedTooltipPosition = tooltipSurface->pos();
    popup->move(originalPopupPosition);
    flushEvents();
    require(
        tooltipSurface->pos() == updatedTooltipPosition,
        "a repositioned tooltip should retain its new global position when the owner moves again");

    QKeyEvent keyPress(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    QCoreApplication::sendEvent(button, &keyPress);
    QKeyEvent keyRelease(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
    QCoreApplication::sendEvent(button, &keyRelease);
    flushEvents();
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == tooltipSurface,
            "ordinary key events should not hide a Qt-style tooltip");

    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(button, &leaveEvent);
    QEvent enterEvent(QEvent::Enter);
    QCoreApplication::sendEvent(button, &enterEvent);
    waitFor(350);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "re-entering alone should not cancel Qt's delayed tooltip hiding");

    sendTooltipEvent(*shortButton, QPoint(2, shortButton->rect().center().y()));
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr, "short-lived tooltip should be visible initially");
    const QPoint shortTooltipPosition = tooltipSurface->pos();
    waitFor(300);
    sendTooltipEvent(*shortButton,
                     QPoint(shortButton->width() - 2, shortButton->rect().center().y()));
    require(tooltipSurface->pos() == shortTooltipPosition,
            "same tooltip request should not move a visible tooltip");
    waitFor(550);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "same tooltip request should not restart Qt's expiry timer");

    popup->raise();
    flushEvents();

    auto* nestedTrigger = new QPushButton(QStringLiteral("Nested"), content);
    nestedTrigger->setToolTip(QStringLiteral("Nested trigger tooltip"));
    layout->insertWidget(1, nestedTrigger);
    auto* nestedContent = new QLabel(QStringLiteral("Nested popup content"));
    adqt::widgets::AdPopover nestedPopover(&host);
    nestedPopover.setSourceWidget(nestedTrigger);
    nestedPopover.setContentWidget(nestedContent);
    nestedPopover.setVisibilityPolicy(adqt::widgets::AdPopover::VisibilityPolicy::Manual);
    nestedPopover.setPopupLayerMode(adqt::widgets::AdPopover::PopupLayerMode::QtTool);
    nestedPopover.show();
    waitFor(50);
    require(nestedPopover.isVisible() &&
                visibleTopLevel(QStringLiteral("adpopover-surface")) != nullptr,
            "nested QtTool popover should be visible");

    sendTooltipEvent(*nestedTrigger, nestedTrigger->rect().center());
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "nested popup trigger tooltip should be suppressed before outer popup routing");
    require(!QToolTip::isVisible(), "nested popup trigger must not create QToolTip");

    nestedPopover.hide();
    flushEvents();
    sendTooltipEvent(*nestedTrigger, nestedTrigger->rect().center());
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr &&
                visibleTooltipText(tooltipSurface) == QStringLiteral("Nested trigger tooltip"),
            "nested trigger should return to outer popup tooltip routing after close");
    requireNativeOwner(*tooltipSurface, *popup);

    const QRect itemRect = list->visualItemRect(item);
    list->viewport()->setToolTipDuration(100);
    sendTooltipEvent(*list->viewport(), itemRect.center());
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr,
            "moving from a widget tooltip to an item tooltip should reuse Qt's visible surface");
    require(visibleTooltipText(tooltipSurface) == QStringLiteral("Popup item tooltip"),
            "custom tooltip should support Qt::ToolTipRole");
    requireNativeOwner(*tooltipSurface, *popup);
    waitFor(150);
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == tooltipSurface,
            "item-view tooltips should use QAbstractItemDelegate's default duration");

    const QPoint firstItemTooltipPosition = tooltipSurface->pos();
    sendTooltipEvent(*list->viewport(), itemRect.bottomRight() - QPoint(1, 1));
    require(tooltipSurface->pos() == firstItemTooltipPosition,
            "tooltip should remain fixed while the cursor stays in the same item rect");

    const QRect secondItemRect = list->visualItemRect(secondItem);
    sendTooltipEvent(*list->viewport(), secondItemRect.center());
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr,
            "moving between tooltip item rects should reuse Qt's visible surface");
    require(tooltipSurface->pos() != firstItemTooltipPosition,
            "moving outside the active item rect should reposition the tooltip");

    const bool triggerTooltipAccepted = sendTooltipEvent(*trigger, trigger->rect().center());
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "trigger tooltip should be suppressed while QtTool is open");
    require(!triggerTooltipAccepted,
            "suppressed trigger should not activate Qt's tooltip fall-asleep mode");
    require(!QToolTip::isVisible(), "suppressed trigger must not create QToolTip");

    popover.hide();
    flushEvents();
    sendTooltipEvent(*trigger, trigger->rect().center());
    tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr &&
                visibleTooltipText(tooltipSurface) == QStringLiteral("Trigger tooltip"),
            "trigger should return to application custom tooltip routing after popup closes");
    requireNativeOwner(*tooltipSurface, host);
    require(!QToolTip::isVisible(),
            "trigger must not return to native QToolTip after popup closes");
    QEvent triggerLeaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(trigger, &triggerLeaveEvent);
    waitFor(350);
    host.hide();
    flushEvents();
}

void selectQtToolUsesTheSharedTooltipRoute() {
    QWidget host;
    host.resize(480, 320);
    auto* select = new adqt::widgets::AdSelect(&host);
    select->setGeometry(80, 80, 180, 36);
    select->setOptions({
        {QStringLiteral("red"), QStringLiteral("Red"), false, QString(), {}},
        {QStringLiteral("blue"), QStringLiteral("Blue"), false, QString(), {}},
    });
    QPushButton* footerButton = nullptr;
    select->setPopupExtraContentFactory([&footerButton](QWidget* parent) {
        footerButton = new QPushButton(QStringLiteral("Footer"), parent);
        footerButton->setToolTip(QStringLiteral("Select footer tooltip"));
        return footerButton;
    });
    select->setPopupLayerMode(adqt::widgets::AdSelect::PopupLayerMode::QtTool);

    host.show();
    flushEvents();
    select->showPopup();
    waitFor(50);

    QWidget* popup = visibleTopLevel(QStringLiteral("adselect-popup"));
    require(popup != nullptr, "AdSelect QtTool should be visible");
    require(footerButton != nullptr, "AdSelect popup footer should be created");

    sendTooltipEvent(*footerButton, footerButton->rect().center());
    QWidget* tooltipSurface = visibleTopLevel(QStringLiteral("adtooltip-surface"));
    require(tooltipSurface != nullptr, "AdSelect popup should route tooltip events");
    require(visibleTooltipText(tooltipSurface) == QStringLiteral("Select footer tooltip"),
            "AdSelect popup tooltip text should be preserved");
    requireNativeOwner(*tooltipSurface, *popup);
    require(select->popupVisible(), "non-activating tooltip must not close AdSelect popup");

    select->hidePopup();
    flushEvents();
    require(visibleTopLevel(QStringLiteral("adtooltip-surface")) == nullptr,
            "AdSelect should hide routed tooltip when popup closes");
    host.hide();
    flushEvents();
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    adqt::widgets::AdTooltip::installApplicationTooltips();
    applicationTooltipsUseCustomSurface();
    programmaticTooltipMatchesQtShowHideSemantics();
    topLevelTransientTooltipOwnsItsWindow();
    automaticTooltipUsesQtWakeUpDelay();
    applicationTooltipTargetsHaveIndependentWakeCycles();
    sharedHoverControllerUsesCursorTruth();
    qtToolRoutesTooltipsAndSuppressesItsTrigger();
    selectQtToolUsesTheSharedTooltipRoute();
    return 0;
}
