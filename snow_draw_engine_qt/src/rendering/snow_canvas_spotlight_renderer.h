#pragma once

#include "snow_canvas_display_cache.h"

#include <QRectF>
#include <QRegion>

#include <cstddef>
#include <cstdint>

class QPainter;

namespace snow_canvas_spotlight_renderer {

enum class RenderPolicy : std::uint8_t {
    Retained,
    Direct,
};

struct RenderDiagnostics {
    std::size_t maskPathBuilds = 0;
    std::size_t maskPathReuses = 0;
    std::size_t processedCutoutCount = 0;
    std::size_t locallyCulledCutoutCount = 0;
    std::size_t earlyExitCount = 0;
    std::size_t zeroCutoutFastPathCount = 0;
    std::size_t coverageRasterizations = 0;
    std::size_t coverageStrips = 0;
    std::size_t coverageTileHits = 0;
    std::size_t coverageTileMisses = 0;
    std::size_t coverageTileEvictions = 0;
    std::size_t rasterizedPhysicalPixels = 0;
    std::size_t retainedCoverageBytes = 0;
    std::size_t retainedAggregateBytes = 0;
    std::size_t directRenderFallbacks = 0;
    std::size_t allocationFailures = 0;
    std::size_t renderedPixelCount = 0;
    std::size_t renderedRegionCount = 0;
    std::size_t retainedPathElementCount = 0;
    std::size_t retainedPathBytes = 0;
};

RenderDiagnostics diagnosticsForCurrentThread();
void resetDiagnosticsForCurrentThread();
void resetRenderCacheForCurrentThread();

void render(QPainter& painter, const SceneDisplayInfo& sceneInfo,
            const SpotlightDisplayInfo& spotlightInfo, const SnowSpotlightCutout* cutouts,
            std::uint32_t cutoutCount, std::uint64_t geometryGeneration, const void* geometryOwner,
            const QRectF& renderArea, const QRegion& exposedRegion,
            RenderPolicy policy = RenderPolicy::Retained, const QRegion& dirtyRegion = QRegion());

} // namespace snow_canvas_spotlight_renderer
