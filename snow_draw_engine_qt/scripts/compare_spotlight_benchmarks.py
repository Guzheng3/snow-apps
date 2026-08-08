#!/usr/bin/env python3
"""Compare the captured spotlight baseline with a layered-cache candidate."""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path


KEY_FIELDS = ("suite", "scenario", "operation")
CANDIDATE_FIELDS = {
    "format_version",
    "suite",
    "scenario",
    "operation",
    "samples",
    "p50_ms",
    "p95_ms",
    "coverage_rasterizations",
    "coverage_tile_hits",
    "coverage_tile_misses",
    "retained_aggregate_bytes",
    "zero_cutout_fast_paths",
}


def read_rows(
    path: Path,
    expected_version: str,
    required_fields: set[str],
) -> dict[tuple[str, str, str], dict[str, str]]:
    try:
        with path.open(newline="", encoding="utf-8") as stream:
            reader = csv.DictReader(stream)
            fields = set(reader.fieldnames or ())
            missing = required_fields - fields
            if missing:
                raise ValueError(
                    f"{path}: missing columns: {', '.join(sorted(missing))}"
                )
            rows = list(reader)
    except OSError as error:
        raise ValueError(f"{path}: {error}") from error

    if not rows:
        raise ValueError(f"{path}: CSV contains no benchmark rows")
    versions = {row["format_version"] for row in rows}
    if versions != {expected_version}:
        raise ValueError(
            f"{path}: expected format_version={expected_version}, found {sorted(versions)}"
        )

    results: dict[tuple[str, str, str], dict[str, str]] = {}
    for row in rows:
        key = tuple(row[field] for field in KEY_FIELDS)
        if key in results:
            raise ValueError(f"{path}: duplicate result row: {'/'.join(key)}")
        results[key] = row
    return results


def number(row: dict[str, str], field: str) -> float:
    try:
        value = float(row[field])
    except (KeyError, ValueError) as error:
        raise ValueError(
            f"{row.get('scenario', '<unknown>')}: invalid {field}={row.get(field)!r}"
        ) from error
    if value < 0.0:
        raise ValueError(
            f"{row.get('scenario', '<unknown>')}: {field} must be non-negative"
        )
    return value


def structural_errors(
    candidate: dict[tuple[str, str, str], dict[str, str]],
    max_retained_bytes: float,
) -> list[str]:
    errors: list[str] = []
    for key, row in sorted(candidate.items()):
        label = "/".join(key)
        retained = number(row, "retained_aggregate_bytes")
        if retained > max_retained_bytes:
            errors.append(
                f"{label}: retained aggregate {retained:.0f} bytes exceeds "
                f"{max_retained_bytes:.0f}"
            )

        samples = number(row, "samples")
        rasterizations = number(row, "coverage_rasterizations")
        hits = number(row, "coverage_tile_hits")
        misses = number(row, "coverage_tile_misses")
        if samples <= 0.0:
            errors.append(f"{label}: samples must be positive")
        if hits + misses <= 0.0 and "legacy_reference" not in row["scenario"]:
            hidden = "zero_visible" in row["scenario"]
            if not hidden:
                errors.append(f"{label}: no retained coverage tile activity was recorded")
        if "renderer_opacity_preview_burst" in row["scenario"] and rasterizations != 0.0:
            errors.append(
                f"{label}: opacity preview rebuilt coverage {rasterizations:.0f} times"
            )
        if "zero_visible_cutouts" in row["scenario"]:
            fast_paths = number(row, "zero_cutout_fast_paths")
            if rasterizations != 0.0 or fast_paths != samples:
                errors.append(
                    f"{label}: zero-visible fast path expected {samples:.0f} samples, "
                    f"got fast_paths={fast_paths:.0f}, rasterizations={rasterizations:.0f}"
                )
    return errors


def timing_errors(
    baseline: dict[tuple[str, str, str], dict[str, str]],
    candidate: dict[tuple[str, str, str], dict[str, str]],
) -> list[str]:
    errors: list[str] = []
    warm_128 = [
        key
        for key in candidate
        if key[1].startswith("renderer_warm_") and "_cutouts128" in key[1]
    ]
    if not warm_128:
        errors.append("no warm 128-cutout candidate rows were found")

    for key in sorted(warm_128):
        if key not in baseline:
            print(f"Candidate-only warm 128 case: {'/'.join(key)}")
            continue
        before_p50 = number(baseline[key], "p50_ms")
        after_p50 = number(candidate[key], "p50_ms")
        before_p95 = number(baseline[key], "p95_ms")
        after_p95 = number(candidate[key], "p95_ms")
        if before_p50 <= 0.0 or after_p50 > before_p50 / 3.0:
            errors.append(
                f"{'/'.join(key)}: p50 {after_p50:.4f} ms does not meet the 3x gate "
                f"against {before_p50:.4f} ms"
            )
        if after_p95 > before_p95:
            errors.append(
                f"{'/'.join(key)}: p95 {after_p95:.4f} ms regressed from {before_p95:.4f} ms"
            )

    shared_warm = sorted(
        key for key in candidate
        if key in baseline and key[1].startswith("renderer_warm_")
    )
    for key in shared_warm:
        before_p95 = number(baseline[key], "p95_ms")
        after_p95 = number(candidate[key], "p95_ms")
        if after_p95 > before_p95:
            errors.append(
                f"{'/'.join(key)}: warm p95 {after_p95:.4f} ms regressed "
                f"from {before_p95:.4f} ms"
            )

    return errors


def print_comparison(
    baseline: dict[tuple[str, str, str], dict[str, str]],
    candidate: dict[tuple[str, str, str], dict[str, str]],
) -> None:
    shared = sorted(set(baseline) & set(candidate))
    candidate_only = sorted(set(candidate) - set(baseline))
    baseline_only = sorted(set(baseline) - set(candidate))
    if candidate_only:
        print(f"Candidate-only rows: {len(candidate_only)}")
    if baseline_only:
        print(f"Baseline-only rows: {len(baseline_only)}")
    print("p50/p95 comparison for shared rows:")
    for key in shared:
        before = number(baseline[key], "p50_ms")
        after = number(candidate[key], "p50_ms")
        before_p95 = number(baseline[key], "p95_ms")
        after_p95 = number(candidate[key], "p95_ms")
        ratio = after / before if before else float("nan")
        print(
            f"  {'/'.join(key)}: p50 {before:.4f} -> {after:.4f} ms "
            f"({ratio:.3f}x), p95 {before_p95:.4f} -> {after_p95:.4f} ms"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path, help="captured format-v1 CSV")
    parser.add_argument("candidate", type=Path, help="candidate format-v2 CSV")
    parser.add_argument(
        "--max-retained-mib",
        type=float,
        default=128.0,
        help="maximum aggregate retained scene/spotlight bytes (default: 128 MiB)",
    )
    parser.add_argument(
        "--allow-structural-errors",
        action="store_true",
        help="print structural errors without failing",
    )
    parser.add_argument(
        "--enforce-timing-gates",
        action="store_true",
        help="enforce warm 128 p50/p95 and shared warm p95 timing gates",
    )
    args = parser.parse_args()

    try:
        baseline = read_rows(
            args.baseline,
            "1",
            {"format_version", "suite", "scenario", "operation", "p50_ms", "p95_ms"},
        )
        candidate = read_rows(
            args.candidate,
            "2",
            CANDIDATE_FIELDS,
        )
        max_retained_bytes = args.max_retained_mib * 1024.0 * 1024.0
        errors = structural_errors(candidate, max_retained_bytes)
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    if errors:
        print("Structural diagnostics:", file=sys.stderr)
        for error in errors:
            print(f"  {error}", file=sys.stderr)
        if not args.allow_structural_errors:
            return 1

    if args.enforce_timing_gates:
        errors = timing_errors(baseline, candidate)
        if errors:
            print("Timing gates:", file=sys.stderr)
            for error in errors:
                print(f"  {error}", file=sys.stderr)
            return 1

    print_comparison(baseline, candidate)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
