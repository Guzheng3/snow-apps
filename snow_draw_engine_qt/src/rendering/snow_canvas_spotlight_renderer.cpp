#include "snow_canvas_spotlight_renderer.h"

#include "snow_canvas_renderer.h"
#include "snow_canvas_tile_cache.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace snow_canvas_spotlight_renderer {
namespace {

struct ValidCutout {
    SnowSpotlightCutout value{};
    QRectF viewBounds;
};

thread_local RenderDiagnostics g_diagnostics;

std::size_t regionPixels(const QRegion& region) {
    std::size_t pixels = 0;
    for (const QRect& rect : region) {
        pixels += static_cast<std::size_t>(rect.width()) * static_cast<std::size_t>(rect.height());
    }
    return pixels;
}

void hashValue(std::uint64_t& hash, std::uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
}

void hashDouble(std::uint64_t& hash, double value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    hashValue(hash, bits);
}

QTransform canvasToViewTransform(const SceneDisplayInfo& sceneInfo) {
    const qreal zoom = sceneInfo.camera_zoom > 0.0 ? sceneInfo.camera_zoom : 1.0;
    return QTransform(zoom, 0.0, 0.0, zoom,
                      sceneInfo.surface_width / 2.0 - sceneInfo.camera_center_x * zoom,
                      sceneInfo.surface_height / 2.0 - sceneInfo.camera_center_y * zoom);
}

QPainterPath transformedCutoutPath(const SnowSpotlightCutout& cutout,
                                   const QTransform& canvasToView) {
    QPainterPath rectangle;
    rectangle.addRect(
        QRectF(-cutout.width / 2.0, -cutout.height / 2.0, cutout.width, cutout.height));
    QTransform element;
    element.translate(cutout.center_x, cutout.center_y);
    element.rotateRadians(cutout.rotation);
    return canvasToView.map(element.map(rectangle));
}

bool validCutout(const SnowSpotlightCutout& cutout) {
    return std::isfinite(cutout.center_x) && std::isfinite(cutout.center_y) &&
           std::isfinite(cutout.width) && std::isfinite(cutout.height) &&
           std::isfinite(cutout.rotation) && cutout.width > 0.0 && cutout.height > 0.0;
}

std::vector<ValidCutout> visibleCutouts(const SnowSpotlightCutout* cutouts,
                                        std::uint32_t cutoutCount, const QTransform& canvasToView,
                                        const QRectF& paintBounds) {
    std::vector<ValidCutout> visible;
    if (cutouts == nullptr || cutoutCount == 0) {
        return visible;
    }
    visible.reserve(cutoutCount);
    for (std::uint32_t index = 0; index < cutoutCount; ++index) {
        const SnowSpotlightCutout& cutout = cutouts[index];
        if (!validCutout(cutout)) {
            ++g_diagnostics.locallyCulledCutoutCount;
            continue;
        }
        const QRectF localRect(-cutout.width / 2.0, -cutout.height / 2.0, cutout.width,
                               cutout.height);
        QTransform element;
        element.translate(cutout.center_x, cutout.center_y);
        element.rotateRadians(cutout.rotation);
        const QRectF bounds = canvasToView.mapRect(element.mapRect(localRect));
        if (!bounds.isValid() || !bounds.intersects(paintBounds)) {
            ++g_diagnostics.locallyCulledCutoutCount;
            continue;
        }
        visible.push_back(ValidCutout{cutout, bounds});
    }
    return visible;
}

std::uint64_t spotlightContentKey(const SceneDisplayInfo& sceneInfo, const QSize& logicalSize,
                                  qreal dpr, const QRectF& renderArea, const QColor& baseColor) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    hashValue(hash, static_cast<std::uint64_t>(logicalSize.width()));
    hashValue(hash, static_cast<std::uint64_t>(logicalSize.height()));
    hashDouble(hash, dpr);
    hashDouble(hash, sceneInfo.surface_width);
    hashDouble(hash, sceneInfo.surface_height);
    hashDouble(hash, sceneInfo.camera_center_x);
    hashDouble(hash, sceneInfo.camera_center_y);
    hashDouble(hash, sceneInfo.camera_zoom);
    hashDouble(hash, renderArea.left());
    hashDouble(hash, renderArea.top());
    hashDouble(hash, renderArea.width());
    hashDouble(hash, renderArea.height());
    hashValue(hash, static_cast<std::uint64_t>(baseColor.red()));
    hashValue(hash, static_cast<std::uint64_t>(baseColor.green()));
    hashValue(hash, static_cast<std::uint64_t>(baseColor.blue()));
    hashValue(hash, static_cast<std::uint64_t>(baseColor.alpha()));
    return hash;
}

void fillDirect(QPainter& painter, const QColor& baseColor, float opacity, const QRectF& renderArea,
                const QRegion& paintRegion, const std::vector<ValidCutout>& visible,
                const QTransform& canvasToView) {
    QColor effectiveColor = baseColor;
    effectiveColor.setAlphaF(baseColor.alphaF() * std::clamp(opacity, 0.0F, 1.0F));
    painter.save();
    painter.setClipRegion(paintRegion, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (visible.empty()) {
        painter.fillRect(renderArea, effectiveColor);
        painter.restore();
        return;
    }

    QPainterPath holes;
    holes.setFillRule(Qt::WindingFill);
    for (const ValidCutout& cutout : visible) {
        holes.addPath(transformedCutoutPath(cutout.value, canvasToView));
        ++g_diagnostics.processedCutoutCount;
    }
    QPainterPath coverage;
    coverage.addRect(renderArea);
    painter.fillPath(coverage.subtracted(holes), effectiveColor);
    painter.restore();
}

} // namespace

RenderDiagnostics diagnosticsForCurrentThread() {
    return g_diagnostics;
}

void resetDiagnosticsForCurrentThread() {
    g_diagnostics = RenderDiagnostics{};
}

void resetRenderCacheForCurrentThread() {
    snow_canvas_tile_cache::clear();
}

void render(QPainter& painter, const SceneDisplayInfo& sceneInfo,
            const SpotlightDisplayInfo& spotlightInfo, const SnowSpotlightCutout* cutouts,
            std::uint32_t cutoutCount, std::uint64_t geometryGeneration, const void* geometryOwner,
            const QRectF& renderArea, const QRegion& exposedRegion, RenderPolicy policy,
            const QRegion& dirtyRegion) {
    const QColor baseColor = snow_canvas_renderer::toQColor(spotlightInfo.color);
    const float opacity = static_cast<float>(std::clamp(spotlightInfo.opacity, 0.0, 1.0));
    const QRectF normalizedArea = renderArea.normalized();
    if (!spotlightInfo.active || baseColor.alphaF() <= 0.0 || opacity <= 0.0 ||
        !normalizedArea.isValid() || normalizedArea.isEmpty() || exposedRegion.isEmpty()) {
        ++g_diagnostics.earlyExitCount;
        return;
    }

    const QRegion paintRegion = exposedRegion.intersected(QRegion(normalizedArea.toAlignedRect()));
    if (paintRegion.isEmpty()) {
        ++g_diagnostics.earlyExitCount;
        return;
    }

    const QTransform canvasToView = canvasToViewTransform(sceneInfo);
    const std::vector<ValidCutout> visible = visibleCutouts(
        cutouts, cutoutCount, canvasToView, normalizedArea.intersected(paintRegion.boundingRect()));
    g_diagnostics.renderedPixelCount += regionPixels(paintRegion);
    g_diagnostics.renderedRegionCount += static_cast<std::size_t>(paintRegion.rectCount());

    if (visible.empty()) {
        ++g_diagnostics.zeroCutoutFastPathCount;
        fillDirect(painter, baseColor, opacity, normalizedArea, paintRegion, visible, canvasToView);
        return;
    }

    if (policy == RenderPolicy::Direct) {
        fillDirect(painter, baseColor, opacity, normalizedArea, paintRegion, visible, canvasToView);
        return;
    }

    const qreal dpr = painter.device() != nullptr
                          ? std::max<qreal>(0.01, painter.device()->devicePixelRatioF())
                          : 1.0;
    const QSize logicalSize(std::max(1, qCeil(sceneInfo.surface_width)),
                            std::max(1, qCeil(sceneInfo.surface_height)));
    const void* owner = geometryOwner != nullptr ? geometryOwner
                        : cutouts != nullptr     ? static_cast<const void*>(cutouts)
                                                 : static_cast<const void*>(&sceneInfo);
    const std::uint64_t contentKey =
        spotlightContentKey(sceneInfo, logicalSize, dpr, normalizedArea, baseColor);
    const QColor coverageColor(Qt::white);
    QColor presentationColor = baseColor;
    presentationColor.setAlphaF(baseColor.alphaF() * opacity);
    // A single hole on a large surface is cheaper to present with the direct
    // winding path once the retained layer is warm. Keep the cache lookup and
    // coverage reuse so geometry and ownership semantics remain unchanged.
    const std::size_t physicalPixels = static_cast<std::size_t>(qCeil(logicalSize.width() * dpr)) *
                                       static_cast<std::size_t>(qCeil(logicalSize.height() * dpr));
    const bool directWarmPresentation = visible.size() == 1 && physicalPixels >= 1'000'000u &&
                                        snow_canvas_tile_cache::retainedBytes(
                                            snow_canvas_tile_cache::Layer::SpotlightCoverage) != 0;

    const auto tileDiagnostics = snow_canvas_tile_cache::render(
        painter, snow_canvas_tile_cache::RenderRequest{
                     owner,
                     logicalSize,
                     dpr,
                     contentKey,
                     geometryGeneration,
                     dirtyRegion,
                     paintRegion,
                     [&](QPainter& coveragePainter, const QRegion& missingRegion) {
                         const QRectF stripBounds =
                             normalizedArea.intersected(missingRegion.boundingRect());
                         if (!stripBounds.isValid() || stripBounds.isEmpty()) {
                             return;
                         }
                         QPainterPath holes;
                         holes.setFillRule(Qt::WindingFill);
                         for (const ValidCutout& cutout : visible) {
                             if (!cutout.viewBounds.intersects(stripBounds)) {
                                 ++g_diagnostics.locallyCulledCutoutCount;
                                 continue;
                             }
                             holes.addPath(transformedCutoutPath(cutout.value, canvasToView));
                             ++g_diagnostics.processedCutoutCount;
                         }
                         QPainterPath coverage;
                         coverage.addRect(normalizedArea);
                         coveragePainter.setRenderHint(QPainter::Antialiasing, true);
                         coveragePainter.fillPath(coverage.subtracted(holes), coverageColor);
                         ++g_diagnostics.coverageRasterizations;
                     },
                     snow_canvas_tile_cache::Layer::SpotlightCoverage,
                     snow_canvas_tile_cache::RenderMode::HorizontalStrips,
                     1.0,
                     presentationColor,
                     directWarmPresentation,
                 });
    g_diagnostics.coverageTileHits += tileDiagnostics.hits;
    g_diagnostics.coverageTileMisses += tileDiagnostics.misses;
    g_diagnostics.coverageTileEvictions += tileDiagnostics.evictions;
    // Keep the historical path counters meaningful for callers that compare the
    // direct reference renderer with retained rendering. A raster strip build is
    // the retained equivalent of one path build, and a fully valid tile is a reuse.
    g_diagnostics.maskPathBuilds += tileDiagnostics.coverageStrips;
    g_diagnostics.maskPathReuses += tileDiagnostics.hits;
    g_diagnostics.coverageStrips += tileDiagnostics.coverageStrips;
    g_diagnostics.rasterizedPhysicalPixels += tileDiagnostics.rasterizedPhysicalPixels;
    g_diagnostics.retainedCoverageBytes = tileDiagnostics.retainedLayerBytes;
    g_diagnostics.retainedAggregateBytes = tileDiagnostics.retainedAggregateBytes;
    g_diagnostics.allocationFailures += tileDiagnostics.allocationFailures;

    if (tileDiagnostics.allocationFailures != 0 || directWarmPresentation) {
        if (tileDiagnostics.allocationFailures != 0) {
            ++g_diagnostics.directRenderFallbacks;
        }
        fillDirect(painter, baseColor, opacity, normalizedArea, paintRegion, visible, canvasToView);
    }
}

} // namespace snow_canvas_spotlight_renderer
