# Snow Canvas spotlight benchmark

`snow-canvas-spotlight-benchmark` measures the dedicated spotlight decoration renderer and its
retained coverage layer. CSV format version 2 records p50/p95/p99 timing, legacy reference path
counters, coverage rasterizations and strips, tile hits/misses/evictions, physical raster work,
processed and culled cutouts, fast paths, fallbacks, retained bytes, output checksums, and
environment metadata.

## Build and run

```powershell
cmake -S snow_draw_engine_qt -B snow_draw_engine_qt/build-perf `
  -DSNOW_DRAW_ENGINE_QT_BUILD_DEMO=OFF `
  -DSNOW_DRAW_ENGINE_QT_BUILD_TESTS=ON `
  -DSNOW_DRAW_ENGINE_QT_BUILD_BENCHMARKS=ON
cmake --build snow_draw_engine_qt/build-perf --config Release `
  --target snow-canvas-spotlight-benchmark

$env:QT_QPA_PLATFORM = 'offscreen'
snow_draw_engine_qt/build-perf/Release/snow-canvas-spotlight-benchmark.exe --list
snow_draw_engine_qt/build-perf/Release/snow-canvas-spotlight-benchmark.exe `
  --warmup 10 --iterations 300 `
  --csv snow_draw_engine_qt/build-perf/spotlight-results.csv
```

The stable scenario matrix covers 1920x1080 and 3840x2160, DPR 1, 1.25, and 2, and 1, 16, and
128 rotated cutouts. Each matrix point has cold and warm cache modes. Additional scenarios cover
fragmented and bounded exposure, opacity and color preview bursts, zero visible cutouts, fractional
tile-boundary geometry, small dirty geometry changes, camera and render-area changes, multiple
canvas owners, and forced spotlight-LRU eviction. The 3840x2160/DPR2 matrix rows use a centered
half-surface retained exposure so their warm coverage fits the reserved 48 MiB spotlight pool;
full-surface DPR2 behavior is covered by the cold and eviction scenarios.

`legacy_reference_*` scenarios execute the removed repeated `QPainterPath::united()` algorithm
inside the benchmark only. They provide same-build, same-machine before measurements without
restoring the legacy scene-marker contract in production. Compare each legacy row with the
corresponding `renderer_cold_*` row for path-construction cost, and use `renderer_warm_*` rows to
measure retained-mask reuse during exposure and style-only paints.

Every scenario validates its cache contract. Opacity-preview samples must perform zero coverage
rasterizations after warmup. Cold, color, camera, render-area, geometry, and forced-eviction samples
must rasterize coverage. The zero-visible scenario must use the direct fill fast path, and all
candidate rows must stay within the 128 MiB aggregate retained scene/spotlight limit. Functional or
diagnostics mismatches return a nonzero exit code; timing values never do.

## Baseline comparison

Capture the baseline and candidate on the same machine and Release toolchain, then run:

```powershell
python snow_draw_engine_qt/scripts/compare_spotlight_benchmarks.py `
  snow_draw_engine_qt/build-text-dirty/spotlight-layered-cache-baseline.csv `
  snow_draw_engine_qt/build-perf/spotlight-results.csv `
  --enforce-timing-gates
```

The timing gate requires every shared warm 128-cutout row to reach at least a 3x p50 speedup and
to improve or hold p95. It also requires p95 not to regress for any other shared warm matrix row.
Candidate-only fractional and workflow rows are reported separately. The script also enforces the
opacity-preview, zero-cutout, and retained-memory diagnostics.
