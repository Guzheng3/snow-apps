#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>

namespace snow_shot::capture_detail {
struct AdaptiveScrollingCaptureCadenceConfig {
    int minimumFps = 1;
    int maximumFps = 24;
    int initialFps = 8;
    double capacityHeadroom = 1.25;
    double ewmaSampleWeight = 0.25;
};

// Chooses the highest sustainable cadence from measured pipeline cost.
class AdaptiveScrollingCaptureCadence final {
  public:
    enum class LimitingStage {
        Warmup,
        Capture,
        Stitch,
    };

    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    using Config = AdaptiveScrollingCaptureCadenceConfig;

    explicit AdaptiveScrollingCaptureCadence(
        AdaptiveScrollingCaptureCadenceConfig config = {})
        : m_config(normalizeConfig(config)) {}

    void reset() {
        m_captureCostMilliseconds = 0.0;
        m_captureLatestMilliseconds = 0.0;
        m_stitchCostMilliseconds = 0.0;
        m_stitchLatestMilliseconds = 0.0;
    }

    void recordCapture(Duration duration) {
        updateCost(duration, m_captureCostMilliseconds, m_captureLatestMilliseconds);
    }

    void recordStitch(Duration duration) {
        updateCost(duration, m_stitchCostMilliseconds, m_stitchLatestMilliseconds);
    }

    void setMaximumFps(int maximumFps) {
        m_config.maximumFps = std::clamp(maximumFps, m_config.minimumFps, kAbsoluteMaximumFps);
        m_config.initialFps = std::clamp(m_config.initialFps, m_config.minimumFps,
                                         m_config.maximumFps);
    }

    [[nodiscard]] int maximumFps() const {
        return m_config.maximumFps;
    }

    [[nodiscard]] LimitingStage limitingStage() const {
        const double captureCost =
            std::max(m_captureCostMilliseconds, m_captureLatestMilliseconds);
        const double stitchCost = std::max(m_stitchCostMilliseconds, m_stitchLatestMilliseconds);
        if (captureCost <= 0.0 && stitchCost <= 0.0) {
            return LimitingStage::Warmup;
        }
        return captureCost >= stitchCost ? LimitingStage::Capture : LimitingStage::Stitch;
    }

    [[nodiscard]] double fps() const {
        if (!hasPerformanceSample()) {
            return static_cast<double>(m_config.initialFps);
        }
        return sustainableFps();
    }

    [[nodiscard]] Duration period() const {
        return std::chrono::duration_cast<Duration>(std::chrono::duration<double>(1.0 / fps()));
    }

    [[nodiscard]] double sustainableFps() const {
        const double stageCostMilliseconds =
            std::max(std::max(m_captureCostMilliseconds, m_captureLatestMilliseconds),
                     std::max(m_stitchCostMilliseconds, m_stitchLatestMilliseconds));
        if (stageCostMilliseconds <= 0.0) {
            return static_cast<double>(m_config.maximumFps);
        }
        return std::clamp(
            std::floor(1000.0 / (stageCostMilliseconds * m_config.capacityHeadroom)),
            static_cast<double>(m_config.minimumFps), static_cast<double>(m_config.maximumFps));
    }

  private:
    static constexpr int kAbsoluteMinimumFps = 1;
    static constexpr int kAbsoluteMaximumFps = 60;
    static constexpr double kDefaultCapacityHeadroom = 1.25;
    static constexpr double kDefaultEwmaSampleWeight = 0.25;

    static AdaptiveScrollingCaptureCadenceConfig normalizeConfig(
        AdaptiveScrollingCaptureCadenceConfig config) {
        config.minimumFps = std::clamp(config.minimumFps, kAbsoluteMinimumFps,
                                       kAbsoluteMaximumFps);
        config.maximumFps = std::clamp(config.maximumFps, config.minimumFps,
                                       kAbsoluteMaximumFps);
        config.initialFps = std::clamp(config.initialFps, config.minimumFps,
                                       config.maximumFps);
        if (!std::isfinite(config.capacityHeadroom) || config.capacityHeadroom < 1.0) {
            config.capacityHeadroom = kDefaultCapacityHeadroom;
        }
        if (!std::isfinite(config.ewmaSampleWeight) || config.ewmaSampleWeight <= 0.0 ||
            config.ewmaSampleWeight > 1.0) {
            config.ewmaSampleWeight = kDefaultEwmaSampleWeight;
        }
        return config;
    }

    [[nodiscard]] bool hasPerformanceSample() const {
        return m_captureLatestMilliseconds > 0.0 || m_stitchLatestMilliseconds > 0.0;
    }

    static double milliseconds(Duration duration) {
        return std::max(0.0, std::chrono::duration<double, std::milli>(duration).count());
    }

    void updateCost(Duration duration, double& ewma, double& latest) {
        latest = milliseconds(duration);
        if (latest <= 0.0) {
            return;
        }
        ewma = ewma <= 0.0
                   ? latest
                   : (ewma * (1.0 - m_config.ewmaSampleWeight)) +
                         (latest * m_config.ewmaSampleWeight);
    }

    AdaptiveScrollingCaptureCadenceConfig m_config;
    double m_captureCostMilliseconds = 0.0;
    double m_captureLatestMilliseconds = 0.0;
    double m_stitchCostMilliseconds = 0.0;
    double m_stitchLatestMilliseconds = 0.0;
};
} // namespace snow_shot::capture_detail
