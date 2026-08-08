#include "icon_registry.h"
#include "external_icon_pack.h"
#include "widgets/button.h"
#include "widgets/control_scale.h"
#include "widgets/dpi_stable_window_controller.h"
#include "widgets/floating_surface.h"
#include "widgets/radio.h"
#include "widgets/select.h"
#include "widgets/slider.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QPainter>

#include <cmath>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace adqt::widgets {

class AdDpiStableWindowControllerTestAccess {
  public:
    static void queueScaleCommit(AdDpiStableWindowController& controller, qreal dpr,
                                 const QRect& geometry) {
        controller.queueScaleCommit(dpr, geometry);
    }
};

} // namespace adqt::widgets

namespace {

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

class ParticipantWidget final : public QWidget, public adqt::widgets::AdControlScaleParticipant {
  public:
    using QWidget::QWidget;
    void prepareControlScale(const adqt::widgets::AdControlScaleContext& context) override {
        ++prepareCount;
        lastPreparedRevision = context.revision;
    }
    void commitControlScale(const adqt::widgets::AdControlScaleContext& context) override {
        ++commitCount;
        lastCommittedRevision = context.revision;
    }
    int prepareCount = 0;
    int commitCount = 0;
    quint64 lastPreparedRevision = 0;
    quint64 lastCommittedRevision = 0;
};

void cumulativeEdgesAreStable() {
    const QVector<qreal> dprs = {1.0, 1.25, 1.5, 1.75, 2.0};
    const QVector<int> widths = {7, 32, 1, 13, 32, 9, 32, 11};
    for (qreal reference : dprs) {
        for (qreal current : dprs) {
            const qreal scale = reference / current;
            const int target = qRound(137 * scale);
            const QVector<int> edges = adqt::widgets::scaleCumulativeWidths(widths, scale, target);
            require(edges.size() == widths.size() + 1,
                    "cumulative edges returned the wrong boundary count");
            require(edges.first() == 0 && edges.last() == target,
                    "cumulative edges did not end at the native target extent");
            for (int i = 1; i < edges.size(); ++i) {
                require(edges.at(i) >= edges.at(i - 1), "cumulative edge order regressed");
                const qreal ideal = widths.at(i - 1) * scale;
                const qreal physicalError =
                    std::abs((edges.at(i) - edges.at(i - 1) - ideal) * current);
                require(physicalError <= current + 0.001,
                        "a cumulative segment exceeded one logical rounding unit");
            }
        }
    }

    const QVector<int> compressed = adqt::widgets::scaleCumulativeWidths({80, 80, 80}, 1.0, 100);
    require(compressed.last() == 100,
            "a smaller native target must remain the exact final boundary");
    for (int i = 1; i < compressed.size(); ++i) {
        require(compressed.at(i) >= compressed.at(i - 1),
                "compressing to a native target inverted cumulative edges");
    }
}

void scopeIsBatchedAndNoOpsRepeatRequests() {
    QWidget root;
    auto* layout = new QHBoxLayout(&root);
    auto* participant = new ParticipantWidget(&root);
    layout->addWidget(participant);
    adqt::widgets::AdControlScaleScope scope(&root);
    int signalCount = 0;
    QObject::connect(&scope, &adqt::widgets::AdControlScaleScope::scaleCommitted,
                     [&signalCount]() { ++signalCount; });
    require(scope.publishScale(1.5, 1.0, QSize(320, 48)), "first scale publication should commit");
    require(participant->prepareCount == 1 && participant->commitCount == 1 &&
                participant->lastPreparedRevision == participant->lastCommittedRevision,
            "participant prepare and commit phases were not paired");
    require(!scope.publishScale(1.5, 1.0, QSize(320, 48)),
            "unchanged scale publication should be a no-op");
    require(participant->commitCount == 1 && signalCount == 1,
            "no-op scale publication produced work");
}

void controllerCoalescesToTheLatestPendingScale() {
    QWidget window;
    window.resize(320, 80);
    window.show();
    QApplication::processEvents();

    adqt::widgets::AdControlScaleScope scope(&window);
    adqt::widgets::AdDpiStableWindowController controller(&window);
    controller.setScaleScope(&scope);
    require(controller.captureBaseline(), "controller baseline capture failed");

    int commitCount = 0;
    qreal committedDpr = 0.0;
    QObject::connect(&controller, &adqt::widgets::AdDpiStableWindowController::scaleCommitCompleted,
                     [&commitCount, &committedDpr](
                         const adqt::widgets::AdControlScaleContext& context, const QSize&) {
                         ++commitCount;
                         committedDpr = context.currentDpr;
                     });

    const QRect firstGeometry(10, 20, 320, 80);
    const QRect latestGeometry(30, 40, 320, 80);
    adqt::widgets::AdDpiStableWindowControllerTestAccess::queueScaleCommit(controller, 1.25,
                                                                           firstGeometry);
    adqt::widgets::AdDpiStableWindowControllerTestAccess::queueScaleCommit(controller, 1.75,
                                                                           latestGeometry);
    QApplication::processEvents();

    const auto diagnostics = controller.diagnostics();
    require(commitCount == 1 && qFuzzyCompare(committedDpr, 1.75),
            "pending DPI messages did not produce one latest-state commit");
    require(diagnostics.coalescedCount == 1 && diagnostics.finalPhysicalGeometry == latestGeometry,
            "coalesced DPI diagnostics did not retain the latest geometry");
}

void controllerCanKeepReferenceDpiSeparateFromWindowDpi() {
    QWidget window;
    window.resize(320, 80);
    window.show();
    QApplication::processEvents();

    adqt::widgets::AdDpiStableWindowController controller(&window);
    const qreal windowDpr = window.devicePixelRatioF();
    const qreal referenceDpr = windowDpr + 0.5;
    require(controller.captureBaseline(referenceDpr),
            "controller baseline capture with an explicit reference failed");
    require(qFuzzyCompare(controller.referenceDpr() + 1.0, referenceDpr + 1.0),
            "controller did not retain the explicit reference display DPI");
    require(controller.stablePhysicalFrameSize().isValid() &&
                !controller.stablePhysicalFrameSize().isEmpty(),
            "controller lost the current window's physical baseline");
}

void componentHintsFollowTheScope() {
    QWidget root;
    auto* layout = new QHBoxLayout(&root);
    auto* button = new adqt::widgets::AdButton(QStringLiteral("Run"), &root);
    auto* radio = new adqt::widgets::AdRadio(QStringLiteral("Choice"), &root);
    auto* slider = new adqt::widgets::AdSlider(&root);
    auto* select = new adqt::widgets::AdSelect(&root);
    layout->addWidget(button);
    layout->addWidget(radio);
    layout->addWidget(slider);
    layout->addWidget(select);
    const QSize buttonBefore = button->sizeHint();
    const QSize radioBefore = radio->sizeHint();
    const QSize sliderBefore = slider->sizeHint();
    const QSize selectBefore = select->sizeHint();
    adqt::widgets::AdControlScaleScope scope(&root);
    require(scope.publishScale(1.5, 1.0), "component scale commit failed");
    require(button->sizeHint().width() > buttonBefore.width() &&
                radio->sizeHint().width() > radioBefore.width() &&
                slider->sizeHint().width() > sliderBefore.width() &&
                select->sizeHint().width() > selectBefore.width(),
            "one or more migrated component hints ignored the scale context");
    require(button->isEnabled() && radio->isEnabled() && slider->isEnabled() && select->isEnabled(),
            "scale commit changed component enabled state");
}

const adqt::icons::ExternalIconPack& testIconPack() {
    static const adqt::icons::ExternalIconPack pack([]() {
        adqt::icons::ExternalIconPackDefinition definition;
        definition.pack = QStringLiteral("dpi-test");
        definition.source = QStringLiteral("DPI stability tests");
        definition.entries.append(adqt::icons::ExternalIconPackEntry{
            QStringLiteral("outlined"),
            QStringLiteral("square"),
            adqt::icons::IconColorModel::Monochrome,
            adqt::icons::IconFit::Contain,
            {},
            QByteArrayLiteral(
                "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 16 16\">"
                "<rect x=\"1\" y=\"1\" width=\"14\" height=\"14\" fill=\"currentColor\"/>"
                "</svg>"),
            {},
            false});
        return definition;
    }());
    return pack;
}

adqt::icons::IconRef testIconRef(adqt::icons::IconRegistry& registry) {
    const auto ref =
        testIconPack().icon(registry, QStringLiteral("outlined"), QStringLiteral("square"));
    require(ref.isValid(), "test icon registration failed");
    return ref;
}

adqt::icons::IconRenderRequest iconRequest(const QSize& size, qreal dpr,
                                           QIcon::Mode mode = QIcon::Normal,
                                           QIcon::State state = QIcon::Off) {
    adqt::icons::IconRenderRequest request;
    request.logicalSize = size;
    request.devicePixelRatio = dpr;
    request.mode = mode;
    request.state = state;
    return request;
}

void iconCacheSharesPhysicalRasters() {
    adqt::icons::IconRegistry registry;
    const auto ref = testIconRef(registry);
    const QPixmap first = registry.renderIconPixmap(ref, iconRequest(QSize(16, 16), 1.5));
    const QPixmap second = registry.renderIconPixmap(ref, iconRequest(QSize(24, 24), 1.0));
    require(!first.isNull() && !second.isNull(), "icon rasterization failed");
    require(qFuzzyCompare(first.devicePixelRatio(), 1.5) &&
                qFuzzyCompare(second.devicePixelRatio(), 1.0),
            "caller DPR metadata was not preserved");
    const adqt::icons::IconCacheStatistics stats = registry.cacheStatistics();
    require(stats.entryCount == 1 && stats.rasterizationCount == 1 && stats.hitCount >= 1,
            "equivalent physical icon requests did not share one raster");
    require(stats.costKB == 3, "icon cache cost should equal the actual 24x24x4 raster bytes");
}

void iconCacheSeparatesVisualKeys() {
    adqt::icons::IconRegistry registry;
    const auto ref = testIconRef(registry);

    registry.renderIconPixmap(ref, iconRequest(QSize(16, 16), 1.0));
    registry.renderIconPixmap(ref, iconRequest(QSize(16, 16), 1.0, QIcon::Normal, QIcon::On));
    registry.renderIconPixmap(ref, iconRequest(QSize(16, 16), 1.0, QIcon::Disabled));
    const auto coloredRef = ref.withColors(adqt::icons::IconColors::primary(QColor(Qt::red)));
    registry.renderIconPixmap(coloredRef, iconRequest(QSize(16, 16), 1.0));

    auto stats = registry.cacheStatistics();
    require(stats.entryCount == 4 && stats.rasterizationCount == 4,
            "mode, state, and color requests must have distinct cache entries");

    registry.setPaletteResolver([]() {
        adqt::icons::IconPalette palette;
        palette.text = QColor(Qt::green);
        palette.revision = 42;
        return palette;
    });
    registry.renderIconPixmap(ref, iconRequest(QSize(16, 16), 1.0));
    stats = registry.cacheStatistics();
    require(stats.entryCount == 1 && stats.rasterizationCount == 5,
            "palette changes must invalidate and separate cached rasters");
}

void concurrentIconMissesRasterizeOnce() {
    adqt::icons::IconRegistry registry;
    const auto ref = testIconRef(registry);

    constexpr int threadCount = 8;
    std::atomic_int ready{0};
    std::atomic_bool start{false};
    std::vector<QPixmap> results(threadCount);
    std::vector<std::thread> threads;
    threads.reserve(threadCount);
    for (int index = 0; index < threadCount; ++index) {
        threads.emplace_back([&, index]() {
            ready.fetch_add(1, std::memory_order_release);
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            results[index] = registry.renderIconPixmap(ref, iconRequest(QSize(32, 32), 1.0));
        });
    }
    while (ready.load(std::memory_order_acquire) != threadCount) {
        std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    for (std::thread& thread : threads) {
        thread.join();
    }

    for (const QPixmap& result : results) {
        require(!result.isNull(), "a concurrent icon request returned no raster");
    }
    const auto stats = registry.cacheStatistics();
    require(stats.entryCount == 1 && stats.missCount == 1 && stats.rasterizationCount == 1 &&
                stats.hitCount == threadCount - 1,
            "concurrent equivalent misses did not rendezvous on one raster");
}

void floatingSurfaceRendersTransparentShadowMargins() {
    adqt::widgets::AdFloatingSurface surface;
    surface.setShadow(18.0, QPointF(0.0, 3.0), QColor(0, 0, 0, 90));
    surface.setCornerRadius(8.0);
    surface.resize(160, 80);
    surface.show();
    QApplication::processEvents();
    QImage image(surface.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    surface.render(&painter);
    painter.end();
    require(image.pixelColor(0, 0).alpha() == 0,
            "floating surface corner margin should remain transparent");
    require(image.pixelColor(surface.bodyRect().center()).alpha() == 255,
            "floating surface body did not render opaquely");
    const QRect body = surface.bodyRect();
    const QPoint topSampleX(body.center().x(), body.top() - 1);
    const QPoint bottomSampleX(body.center().x(), body.bottom() + 1);
    const QPoint leftSampleY(body.left() - 1, body.center().y());
    const QPoint rightSampleY(body.right() + 1, body.center().y());
    require(
        image.pixelColor(leftSampleY).alpha() > 0 && image.pixelColor(rightSampleY).alpha() > 0 &&
            image.pixelColor(topSampleX).alpha() > 0 && image.pixelColor(bottomSampleX).alpha() > 0,
        "floating surface shadow should render throughout the reserved margins");
    require(image.pixelColor(leftSampleY).alpha() < 255 &&
                image.pixelColor(rightSampleY).alpha() < 255 &&
                image.pixelColor(topSampleX).alpha() < 255 &&
                image.pixelColor(bottomSampleX).alpha() < 255,
            "floating surface shadow margin should remain translucent");
    require(surface.interactiveRegion().contains(surface.bodyRect().center()) &&
                !surface.interactiveRegion().contains(QPoint(0, 0)),
            "floating surface interactive region includes shadow-only pixels");
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        cumulativeEdgesAreStable();
        scopeIsBatchedAndNoOpsRepeatRequests();
        controllerCoalescesToTheLatestPendingScale();
        controllerCanKeepReferenceDpiSeparateFromWindowDpi();
        componentHintsFollowTheScope();
        iconCacheSharesPhysicalRasters();
        iconCacheSeparatesVisualKeys();
        concurrentIconMissesRasterizeOnce();
        floatingSurfaceRendersTransparentShadowMargins();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
