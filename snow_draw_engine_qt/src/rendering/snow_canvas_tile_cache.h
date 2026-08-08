#pragma once

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPoint>
#include <QRegion>
#include <QSize>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "snow_canvas_filter_render.h"

namespace snow_canvas_tile_cache {

constexpr int kTilePhysicalSize = 256;
constexpr std::size_t kSceneByteLimit = 64u * 1024u * 1024u;
constexpr std::size_t kSpotlightByteLimit = 48u * 1024u * 1024u;
constexpr std::size_t kWatermarkByteLimit = 16u * 1024u * 1024u;
constexpr std::size_t kGlobalByteLimit =
    kSceneByteLimit + kSpotlightByteLimit + kWatermarkByteLimit;
constexpr std::size_t kLayeredRasterByteLimit = kSceneByteLimit + kSpotlightByteLimit;

enum class Layer : std::uint8_t {
    Scene,
    SpotlightCoverage,
};

enum class RenderMode : std::uint8_t {
    PerTile,
    HorizontalStrips,
};

struct Diagnostics {
    std::size_t hits = 0;
    std::size_t misses = 0;
    std::size_t evictions = 0;
    std::size_t candidateTileCount = 0;
    std::size_t visitedTileCount = 0;
    std::size_t retainedBytes = 0;
    std::size_t retainedLayerBytes = 0;
    std::size_t retainedAggregateBytes = 0;
    std::size_t coverageStrips = 0;
    std::size_t rasterizedPhysicalPixels = 0;
    std::size_t allocationFailures = 0;
};

struct RenderRequest {
    const void* canvasNamespace = nullptr;
    QSize logicalSize;
    qreal devicePixelRatio = 1.0;
    std::uint64_t contentKey = 0;
    std::uint64_t sceneRevision = 0;
    QRegion dirtyRegion;
    QRegion exposedRegion;
    std::function<void(QPainter&, const QRegion&)> renderMissing;
    Layer layer = Layer::Scene;
    RenderMode mode = RenderMode::PerTile;
    qreal destinationOpacity = 1.0;
    QColor presentationColor;
    bool skipPresentation = false;
};

struct MaskTile {
    QImage image;
    QPoint origin;
    std::vector<snow_canvas_filter_render::MaskSpan> spans;
    std::vector<QRect> occupiedBlocks;
    std::size_t coveredPixels = 0;
};

Diagnostics render(QPainter& destination, const RenderRequest& request);
std::shared_ptr<const MaskTile> findMask(const void* canvasNamespace, std::uint64_t key);
void storeMask(const void* canvasNamespace, std::uint64_t key, MaskTile tile);
std::size_t consumeMaskEvictions();
std::size_t maskRetainedBytes();
void invalidate(const void* canvasNamespace, const QRegion& logicalRegion, qreal dpr);
void invalidateForRevision(const void* canvasNamespace, const QRegion& logicalRegion, qreal dpr,
                           std::uint64_t sceneRevision);
void invalidate(const void* canvasNamespace, const QRegion& logicalRegion, qreal dpr, Layer layer);
void invalidateForRevision(const void* canvasNamespace, const QRegion& logicalRegion, qreal dpr,
                           std::uint64_t layerRevision, Layer layer);
void invalidateNamespace(const void* canvasNamespace);
void clear();
std::size_t retainedBytes();
std::size_t retainedBytes(Layer layer);
std::size_t retainedAggregateBytes();

} // namespace snow_canvas_tile_cache
