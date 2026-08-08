#include "snow_canvas_display_item.h"
#include "snow_canvas_ffi_handles.h"
#include "snow_canvas_spotlight_renderer.h"
#include "snow_canvas_tile_cache.h"
#include "snow_canvas_viewport.h"
#include "snow_draw_engine_qt/snow_canvas_widget.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QImage>
#include <QObject>
#include <QPainter>
#include <QRegion>
#include <QThread>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

class PaintObserver final : public QObject {
  public:
    void begin() {
        m_sawPaint = false;
        m_observing = true;
    }

    bool sawPaint() const {
        return m_sawPaint;
    }

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        Q_UNUSED(watched);
        if (m_observing && event != nullptr && event->type() == QEvent::Paint) {
            m_sawPaint = true;
        }
        return false;
    }

  private:
    bool m_observing = false;
    bool m_sawPaint = false;
};

SceneDisplayInfo sceneInfo() {
    SceneDisplayInfo info;
    info.surface_width = 100.0;
    info.surface_height = 100.0;
    info.camera_center_x = 50.0;
    info.camera_center_y = 50.0;
    info.camera_zoom = 1.0;
    return info;
}

SnowSpotlightCutout cutout(double centerX, double centerY, double width, double height,
                           double rotation = 0.0) {
    SnowSpotlightCutout item{};
    item.center_x = centerX;
    item.center_y = centerY;
    item.width = width;
    item.height = height;
    item.rotation = rotation;
    return item;
}

QImage render(const SnowSpotlightCutout* items, std::uint32_t itemCount,
              const QRectF& renderArea = QRectF(0.0, 0.0, 100.0, 100.0),
              const QRegion& exposed = QRegion(QRect(0, 0, 100, 100)), bool active = true) {
    static std::uint64_t geometryGeneration = 0;
    ++geometryGeneration;
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setClipRegion(exposed);
    snow_canvas_spotlight_renderer::render(
        painter, sceneInfo(), SpotlightDisplayInfo{SnowColorRgba8{0, 0, 0, 255}, 0.64, active},
        items, itemCount, geometryGeneration, items, renderArea, exposed);
    painter.end();
    return image;
}

std::vector<SnowSpotlightCutout> comparisonCutouts(std::size_t count) {
    std::vector<SnowSpotlightCutout> items;
    items.reserve(count);
    const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(count))));
    for (std::size_t index = 0; index < count; ++index) {
        const int column = static_cast<int>(index) % columns;
        const int row = static_cast<int>(index) / columns;
        const double spacing = 100.0 / static_cast<double>(columns + 1);
        items.push_back(cutout(spacing * (column + 1) + 0.31, spacing * (row + 1) + 0.17,
                               std::max(10.0, spacing * 0.85) + 0.23,
                               std::max(8.0, spacing * 0.7) + 0.19,
                               (static_cast<int>(index) % 9 - 4) * 0.13));
    }
    return items;
}

QImage renderForComparison(const SceneDisplayInfo& projection,
                           const SpotlightDisplayInfo& spotlight,
                           const std::vector<SnowSpotlightCutout>& items, qreal dpr,
                           const QRectF& renderArea, const QRegion& exposed,
                           snow_canvas_spotlight_renderer::RenderPolicy policy, const void* owner,
                           std::uint64_t generation, const QRegion& dirtyRegion = QRegion()) {
    QImage image(qRound(projection.surface_width * dpr), qRound(projection.surface_height * dpr),
                 QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setClipRegion(exposed);
    snow_canvas_spotlight_renderer::render(painter, projection, spotlight,
                                           items.empty() ? nullptr : items.data(),
                                           static_cast<std::uint32_t>(items.size()), generation,
                                           owner, renderArea, exposed, policy, dirtyRegion);
    painter.end();
    return image;
}

void requireImagesEqual(const QImage& expected, const QImage& actual, const char* message) {
    require(expected.size() == actual.size(), message);
    std::size_t mismatchCount = 0;
    for (int y = 0; y < expected.height(); ++y) {
        for (int x = 0; x < expected.width(); ++x) {
            if (expected.pixel(x, y) != actual.pixel(x, y)) {
                if (mismatchCount < 5) {
                    std::cerr << message << " at " << x << ',' << y << " expected=0x" << std::hex
                              << expected.pixel(x, y) << " actual=0x" << actual.pixel(x, y)
                              << std::dec << '\n';
                }
                ++mismatchCount;
            }
        }
    }
    require(mismatchCount == 0, message);
}

bool isDefaultMaskPixel(QRgb pixel) {
    return qAlpha(pixel) == 255 && qRed(pixel) == qGreen(pixel) && qGreen(pixel) == qBlue(pixel) &&
           qRed(pixel) >= 91 && qRed(pixel) <= 93;
}

void defaultMaskHasExactOpacityAndTransparentHole() {
    const SnowSpotlightCutout item = cutout(50.0, 50.0, 40.0, 30.0);
    const QImage image = render(&item, 1);
    require(isDefaultMaskPixel(image.pixel(5, 5)),
            "default spotlight mask must composite 64% black");
    require(image.pixelColor(50, 50) == QColor(Qt::white),
            "spotlight rectangle must reveal the unmasked canvas");
}

void overlappingAndRotatedCutoutsUseAPathUnion() {
    const SnowSpotlightCutout overlap[] = {
        cutout(40.0, 50.0, 30.0, 24.0),
        cutout(60.0, 50.0, 30.0, 24.0),
    };
    const QImage unionImage = render(overlap, 2);
    require(unionImage.pixelColor(50, 50) == QColor(Qt::white),
            "overlapping spotlight rectangles must form one transparent union");

    const SnowSpotlightCutout rotated = cutout(50.0, 50.0, 40.0, 20.0, std::acos(-1.0) / 4.0);
    const QImage rotatedImage = render(&rotated, 1);
    require(rotatedImage.pixelColor(50, 50) == QColor(Qt::white),
            "rotated spotlight must keep its center transparent");
    require(isDefaultMaskPixel(rotatedImage.pixel(68, 50)),
            "rotated spotlight must not use its axis-aligned bounding box as the hole");

    bool foundAntialiasedEdge = false;
    for (int y = 30; y <= 70 && !foundAntialiasedEdge; ++y) {
        for (int x = 30; x <= 70; ++x) {
            const int red = qRed(rotatedImage.pixel(x, y));
            if (red > 92 && red < 255) {
                foundAntialiasedEdge = true;
                break;
            }
        }
    }
    require(foundAntialiasedEdge, "rotated spotlight edge must be antialiased");
}

void renderAreaAndExposureLimitMaskWork() {
    const SnowSpotlightCutout item = cutout(50.0, 50.0, 16.0, 16.0);
    const QImage bounded = render(&item, 1, QRectF(20.0, 20.0, 60.0, 60.0));
    require(bounded.pixelColor(5, 5) == QColor(Qt::white),
            "pixels outside an explicit spotlight render area must remain untouched");
    require(isDefaultMaskPixel(bounded.pixel(25, 25)),
            "pixels inside the bounded render area must be masked");
    require(bounded.pixelColor(50, 50) == QColor(Qt::white),
            "cutouts must remain transparent inside a bounded render area");

    const QImage empty = render(&item, 1, QRectF());
    require(empty.pixelColor(25, 25) == QColor(Qt::white),
            "an explicitly empty render area must suppress the mask");

    QRegion exposed(QRect(0, 0, 20, 20));
    exposed += QRect(80, 80, 20, 20);
    const QImage clipped = render(nullptr, 0, QRectF(0, 0, 100, 100), exposed);
    require(isDefaultMaskPixel(clipped.pixel(5, 5)) && isDefaultMaskPixel(clipped.pixel(90, 90)),
            "every exposed region must receive the mask");
    require(clipped.pixelColor(50, 50) == QColor(Qt::white),
            "unexposed pixels must not be repainted by the mask pass");
}

void inactiveMaskLeavesCanvasUnchanged() {
    const SnowSpotlightCutout item = cutout(50.0, 50.0, 20.0, 20.0);
    const QImage image =
        render(&item, 1, QRectF(0.0, 0.0, 100.0, 100.0), QRegion(QRect(0, 0, 100, 100)), false);
    require(image.pixelColor(5, 5) == QColor(Qt::white),
            "removing the final spotlight cutout must remove the global mask");
}

void retainedCoverageMatchesDirectReferenceAcrossInputs() {
    SceneDisplayInfo projection = sceneInfo();
    const SnowColorRgba8 colors[] = {
        SnowColorRgba8{0, 0, 0, 255},
        SnowColorRgba8{24, 96, 192, 220},
        SnowColorRgba8{220, 60, 30, 128},
    };
    const double opacities[] = {0.0, 0.35, 0.64, 1.0};
    const double dprs[] = {1.0, 1.25, 2.0};
    const std::size_t counts[] = {0, 1, 16, 128};
    int ownerToken = 0;

    for (double dpr : dprs) {
        for (std::size_t count : counts) {
            const std::vector<SnowSpotlightCutout> items = comparisonCutouts(count);
            for (const SnowColorRgba8& color : colors) {
                for (double opacity : opacities) {
                    const SpotlightDisplayInfo spotlight{color, opacity, true};
                    const QRegion exposed(QRect(0, 0, 100, 100));
                    const QImage direct = renderForComparison(
                        projection, spotlight, items, dpr, QRectF(0.0, 0.0, 100.0, 100.0), exposed,
                        snow_canvas_spotlight_renderer::RenderPolicy::Direct, &ownerToken, 1);
                    snow_canvas_spotlight_renderer::resetRenderCacheForCurrentThread();
                    const QImage retained = renderForComparison(
                        projection, spotlight, items, dpr, QRectF(0.0, 0.0, 100.0, 100.0), exposed,
                        snow_canvas_spotlight_renderer::RenderPolicy::Retained, &ownerToken, 1);
                    requireImagesEqual(
                        direct, retained,
                        "retained spotlight coverage must match the direct reference");
                }
            }
        }
    }

    const std::vector<SnowSpotlightCutout> boundedItems = comparisonCutouts(16);
    const QRegion fragmented = QRegion(QRect(0, 0, 25, 25)).united(QRegion(QRect(50, 50, 25, 25)));
    const SpotlightDisplayInfo boundedSpotlight{SnowColorRgba8{72, 32, 180, 210}, 0.73, true};
    const QImage direct = renderForComparison(
        projection, boundedSpotlight, boundedItems, 1.25, QRectF(15.0, 15.0, 70.0, 70.0),
        fragmented, snow_canvas_spotlight_renderer::RenderPolicy::Direct, &ownerToken, 5);
    snow_canvas_spotlight_renderer::resetRenderCacheForCurrentThread();
    const QImage retained = renderForComparison(
        projection, boundedSpotlight, boundedItems, 1.25, QRectF(15.0, 15.0, 70.0, 70.0),
        fragmented, snow_canvas_spotlight_renderer::RenderPolicy::Retained, &ownerToken, 5);
    requireImagesEqual(direct, retained,
                       "fragmented bounded retained coverage must match the direct reference");
}

void partialGeometryInvalidationPreservesUnaffectedTilesAndOwners() {
    SceneDisplayInfo projection = sceneInfo();
    projection.surface_width = 600.0;
    projection.surface_height = 600.0;
    projection.camera_center_x = 300.0;
    projection.camera_center_y = 300.0;
    SpotlightDisplayInfo spotlight{SnowColorRgba8{12, 40, 96, 235}, 0.58, true};
    std::vector<SnowSpotlightCutout> items;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            items.push_back(cutout(80.0 + column * 145.0, 80.0 + row * 145.0, 70.0, 54.0,
                                   (row * 4 + column) * 0.07));
        }
    }
    const QRectF renderArea(0.0, 0.0, 600.0, 600.0);
    const QRegion exposed(QRect(0, 0, 600, 600));
    int ownerA = 0;
    int ownerB = 0;

    snow_canvas_spotlight_renderer::resetRenderCacheForCurrentThread();
    snow_canvas_spotlight_renderer::resetDiagnosticsForCurrentThread();
    renderForComparison(projection, spotlight, items, 1.0, renderArea, exposed,
                        snow_canvas_spotlight_renderer::RenderPolicy::Retained, &ownerA, 1);
    const auto initial = snow_canvas_spotlight_renderer::diagnosticsForCurrentThread();
    require(initial.coverageTileMisses > 0,
            "initial retained spotlight render must populate coverage tiles");

    const SnowSpotlightCutout previous = items.front();
    items.front().center_x += 1.25;
    items.front().center_y += 0.75;
    const QRectF oldBounds(previous.center_x - previous.width / 2.0,
                           previous.center_y - previous.height / 2.0, previous.width,
                           previous.height);
    const SnowSpotlightCutout& next = items.front();
    const QRectF newBounds(next.center_x - next.width / 2.0, next.center_y - next.height / 2.0,
                           next.width, next.height);
    const QRegion dirty(oldBounds.united(newBounds).adjusted(-2.0, -2.0, 2.0, 2.0).toAlignedRect());

    snow_canvas_spotlight_renderer::resetDiagnosticsForCurrentThread();
    const QImage retained = renderForComparison(
        projection, spotlight, items, 1.0, renderArea, exposed,
        snow_canvas_spotlight_renderer::RenderPolicy::Retained, &ownerA, 2, dirty);
    const auto moved = snow_canvas_spotlight_renderer::diagnosticsForCurrentThread();
    require(moved.coverageTileHits > 0 && moved.coverageTileMisses > 0,
            "partial spotlight geometry invalidation must reuse and rebuild separate tiles");
    require(moved.coverageTileHits > moved.coverageTileMisses,
            "partial spotlight geometry invalidation must preserve most coverage tiles");

    const QImage direct =
        renderForComparison(projection, spotlight, items, 1.0, renderArea, exposed,
                            snow_canvas_spotlight_renderer::RenderPolicy::Direct, &ownerA, 2);
    requireImagesEqual(direct, retained,
                       "partially invalidated retained coverage must match direct geometry");

    snow_canvas_spotlight_renderer::resetDiagnosticsForCurrentThread();
    renderForComparison(projection, spotlight, items, 1.0, renderArea, exposed,
                        snow_canvas_spotlight_renderer::RenderPolicy::Retained, &ownerB, 2);
    const auto isolated = snow_canvas_spotlight_renderer::diagnosticsForCurrentThread();
    require(isolated.coverageTileMisses > 0,
            "a second canvas owner must not reuse another owner's spotlight tiles");
    snow_canvas_tile_cache::invalidateNamespace(&ownerA);
    snow_canvas_spotlight_renderer::resetRenderCacheForCurrentThread();
}

void coverageCacheReusesTilesButInvalidatesColorChanges() {
    const SnowSpotlightCutout item = cutout(50.0, 50.0, 30.0, 20.0, 0.2);
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    snow_canvas_spotlight_renderer::resetRenderCacheForCurrentThread();
    snow_canvas_spotlight_renderer::resetDiagnosticsForCurrentThread();

    QPainter painter(&image);
    snow_canvas_spotlight_renderer::render(
        painter, sceneInfo(), SpotlightDisplayInfo{SnowColorRgba8{0, 0, 0, 255}, 0.64, true}, &item,
        1, 42, &item, QRectF(0.0, 0.0, 100.0, 100.0), QRegion(QRect(0, 0, 50, 100)));
    snow_canvas_spotlight_renderer::render(
        painter, sceneInfo(), SpotlightDisplayInfo{SnowColorRgba8{20, 30, 40, 255}, 0.4, true},
        &item, 1, 42, &item, QRectF(0.0, 0.0, 100.0, 100.0), QRegion(QRect(50, 0, 50, 100)));
    painter.end();

    const auto diagnostics = snow_canvas_spotlight_renderer::diagnosticsForCurrentThread();
    require(diagnostics.maskPathBuilds == 2,
            "a spotlight color change must rebuild coverage with the new base color");
    require(diagnostics.maskPathReuses == 0,
            "a spotlight color change must not reuse coverage from another color");
    require(diagnostics.processedCutoutCount == 2,
            "a color-invalidated spotlight paint must process its cutout geometry");
}

void zeroAlphaAndNonintersectingExposureSkipGeometry() {
    const SnowSpotlightCutout item = cutout(50.0, 50.0, 30.0, 20.0);
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    snow_canvas_spotlight_renderer::resetRenderCacheForCurrentThread();
    snow_canvas_spotlight_renderer::resetDiagnosticsForCurrentThread();
    snow_canvas_spotlight_renderer::render(
        painter, sceneInfo(), SpotlightDisplayInfo{SnowColorRgba8{0, 0, 0, 0}, 1.0, true}, &item, 1,
        7, &item, QRectF(0.0, 0.0, 100.0, 100.0), QRegion(QRect(0, 0, 100, 100)));
    snow_canvas_spotlight_renderer::render(
        painter, sceneInfo(), SpotlightDisplayInfo{SnowColorRgba8{0, 0, 0, 255}, 1.0, true}, &item,
        1, 7, &item, QRectF(0.0, 0.0, 20.0, 20.0), QRegion(QRect(80, 80, 20, 20)));
    painter.end();
    const auto diagnostics = snow_canvas_spotlight_renderer::diagnosticsForCurrentThread();
    require(diagnostics.maskPathBuilds == 0,
            "transparent and nonintersecting spotlight paints must not build geometry");
    require(diagnostics.earlyExitCount == 2,
            "transparent and nonintersecting spotlight paints must be diagnosed as early exits");
}

void projectionAndGeometryKeysInvalidateMaskExactlyOnce() {
    const SnowSpotlightCutout item = cutout(50.0, 50.0, 24.0, 18.0);
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    snow_canvas_spotlight_renderer::resetRenderCacheForCurrentThread();
    snow_canvas_spotlight_renderer::resetDiagnosticsForCurrentThread();
    const SpotlightDisplayInfo info{SnowColorRgba8{0, 0, 0, 255}, 0.64, true};
    const QRegion exposed(QRect(0, 0, 100, 100));
    const auto paint = [&](const SceneDisplayInfo& projection, std::uint64_t generation,
                           const QRectF& area) {
        snow_canvas_spotlight_renderer::render(painter, projection, info, &item, 1, generation,
                                               &item, area, exposed);
    };

    SceneDisplayInfo projection = sceneInfo();
    paint(projection, 1, QRectF(0.0, 0.0, 100.0, 100.0));
    projection.camera_center_x += 2.0;
    paint(projection, 1, QRectF(0.0, 0.0, 100.0, 100.0));
    paint(projection, 1, QRectF(0.0, 0.0, 100.0, 100.0));
    paint(projection, 1, QRectF(10.0, 10.0, 80.0, 80.0));
    paint(projection, 1, QRectF(10.0, 10.0, 80.0, 80.0));
    paint(projection, 2, QRectF(10.0, 10.0, 80.0, 80.0));
    painter.end();

    const auto diagnostics = snow_canvas_spotlight_renderer::diagnosticsForCurrentThread();
    require(diagnostics.maskPathBuilds == 4,
            "initial, camera, render-area, and geometry changes must each build once");
    require(diagnostics.maskPathReuses == 2,
            "unchanged projection and render-area paints must reuse the mask path");
}

void displayCachePatchesSpotlightGeometryIndependentlyFromStyle() {
    ScopedRuntimeHandle runtime;
    require(snow_runtime_create(runtime.outParam()) == SNOW_OK,
            "spotlight cache test runtime creation must succeed");
    SnowCanvasViewport viewport;
    SnowEngineConfig config = snow_canvas_viewport::defaultEngineConfig();
    require(viewport.create(runtime.get(), config),
            "spotlight cache test viewport creation must succeed");
    require(snow_viewport_set_surface_size(runtime.get(), viewport.get(), 100, 100) == SNOW_OK,
            "spotlight cache test surface setup must succeed");
    require(snow_viewport_set_active_tool(runtime.get(), viewport.get(),
                                          SNOW_ACTIVE_TOOL_SPOTLIGHT) == SNOW_OK,
            "spotlight cache test tool setup must succeed");

    SnowCanvasDisplayCache cache;
    require(cache.sync(runtime.get(), viewport.get()),
            "initial spotlight display cache sync must succeed");
    const std::uint64_t sceneRevision = cache.patchCursor().scene_revision;
    const std::uint64_t initialGeneration = cache.spotlightGeometryGeneration();

    const auto pointer = [&](SnowPointerEventType type, double x, double y, std::uint8_t buttons) {
        SnowInputEvent event{};
        event.kind = SNOW_INPUT_EVENT_POINTER;
        event.pointer.pointer_id = 1;
        event.pointer.event_type = type;
        event.pointer.device = SNOW_POINTER_DEVICE_MOUSE;
        event.pointer.position_x = x;
        event.pointer.position_y = y;
        event.pointer.button = type == SNOW_POINTER_EVENT_MOVE ? SNOW_POINTER_BUTTON_NONE
                                                               : SNOW_POINTER_BUTTON_PRIMARY;
        event.pointer.buttons = buttons;
        SnowInteractionOutput output{};
        require(snow_viewport_process_input(runtime.get(), viewport.get(), &event, &output) ==
                    SNOW_OK,
                "spotlight pointer input must succeed");
    };
    pointer(SNOW_POINTER_EVENT_DOWN, 30.0, 30.0, 1);
    pointer(SNOW_POINTER_EVENT_MOVE, 70.0, 70.0, 1);
    require(cache.sync(runtime.get(), viewport.get()),
            "spotlight creation preview cache sync must succeed");
    require(cache.spotlightCutoutCount() == 1,
            "spotlight creation preview must populate dedicated cutout storage");
    require(cache.patchCursor().scene_revision == sceneRevision,
            "spotlight creation preview must not advance the scene revision");
    require(cache.sceneDirtyRectCount() == 0,
            "spotlight creation preview must not dirty retained scene content");
    require(cache.spotlightGeometryGeneration() > initialGeneration,
            "spotlight creation preview must advance geometry generation");

    pointer(SNOW_POINTER_EVENT_UP, 70.0, 70.0, 0);
    require(cache.sync(runtime.get(), viewport.get()),
            "committed spotlight cache sync must succeed");
    const std::uint64_t committedGeneration = cache.spotlightGeometryGeneration();
    SnowSpotlightConfig spotlight{};
    require(snow_viewport_get_spotlight_config(runtime.get(), viewport.get(), &spotlight) ==
                SNOW_OK,
            "spotlight config query must succeed");
    spotlight.opacity = 0.35;
    require(snow_viewport_set_spotlight_config(runtime.get(), viewport.get(), &spotlight) ==
                SNOW_OK,
            "spotlight opacity update must succeed");
    require(cache.sync(runtime.get(), viewport.get()), "spotlight opacity cache sync must succeed");
    require(cache.spotlightGeometryGeneration() == committedGeneration,
            "spotlight color/opacity-only updates must preserve geometry generation");
    require(cache.patchCursor().scene_revision == sceneRevision,
            "spotlight color/opacity-only updates must preserve scene revision");
}

void unchangedRenderAreaDoesNotScheduleRepaint() {
    SnowCanvasWidget canvas;
    canvas.resize(1920, 1080);
    canvas.show();
    QApplication::processEvents();

    const QRectF renderArea(-800.0, -450.0, 1600.0, 900.0);
    canvas.setSpotlightRenderArea(renderArea);
    QApplication::processEvents();

    PaintObserver observer;
    canvas.installEventFilter(&observer);
    observer.begin();
    canvas.setSpotlightRenderArea(QRectF(renderArea.bottomRight(), renderArea.topLeft()));
    QApplication::processEvents();
    canvas.removeEventFilter(&observer);

    require(!observer.sawPaint(), "an unchanged spotlight render area must not schedule a repaint");
}

void spotlightPreviewCoalescesAndCommitCancelsQueuedValue() {
    SnowCanvasWidget canvas;
    int appliedCount = 0;
    QObject::connect(&canvas, &SnowCanvasWidget::spotlightPreviewApplied,
                     [&appliedCount]() { ++appliedCount; });

    SnowCanvasSpotlightConfig first;
    first.color = QColor(32, 96, 180, 255);
    first.opacity = 0.35;
    SnowCanvasSpotlightConfig latest = first;
    latest.color = QColor(210, 64, 48, 220);
    latest.opacity = 0.72;

    canvas.previewCanvasSpotlightConfig(first);
    require(appliedCount == 1, "the first spotlight preview should apply synchronously");
    canvas.previewCanvasSpotlightConfig(first);
    require(appliedCount == 1,
            "an unchanged spotlight preview should not emit another application");
    canvas.previewCanvasSpotlightConfig(latest);
    require(appliedCount == 1,
            "later spotlight preview writes should coalesce until the refresh callback");

    QElapsedTimer previewWait;
    previewWait.start();
    while (appliedCount < 2 && previewWait.elapsed() < 100) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(1);
    }
    require(appliedCount == 2,
            "the refresh-paced spotlight callback should deliver the latest value once");

    SnowCanvasWidget cancellationCanvas;
    int cancellationCount = 0;
    QObject::connect(&cancellationCanvas, &SnowCanvasWidget::spotlightPreviewApplied,
                     [&cancellationCount]() { ++cancellationCount; });
    cancellationCanvas.previewCanvasSpotlightConfig(first);
    SnowCanvasSpotlightConfig stale = first;
    stale.opacity = 0.91;
    SnowCanvasSpotlightConfig committed = latest;
    committed.color = QColor(24, 144, 88, 255);
    committed.opacity = 0.48;
    cancellationCanvas.previewCanvasSpotlightConfig(stale);
    require(cancellationCanvas.setCanvasSpotlightConfig(committed),
            "a spotlight commit should succeed while a preview is queued");
    QCoreApplication::processEvents();
    require(cancellationCount == 1,
            "a persistent spotlight commit should cancel its queued transient preview");
    require(cancellationCanvas.canvasSpotlightConfig() == committed,
            "the persistent spotlight commit should remain authoritative");
}

} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    defaultMaskHasExactOpacityAndTransparentHole();
    overlappingAndRotatedCutoutsUseAPathUnion();
    renderAreaAndExposureLimitMaskWork();
    inactiveMaskLeavesCanvasUnchanged();
    retainedCoverageMatchesDirectReferenceAcrossInputs();
    partialGeometryInvalidationPreservesUnaffectedTilesAndOwners();
    coverageCacheReusesTilesButInvalidatesColorChanges();
    zeroAlphaAndNonintersectingExposureSkipGeometry();
    projectionAndGeometryKeysInvalidateMaskExactlyOnce();
    displayCachePatchesSpotlightGeometryIndependentlyFromStyle();
    unchangedRenderAreaDoesNotScheduleRepaint();
    spotlightPreviewCoalescesAndCommitCancelsQueuedValue();
    return 0;
}
