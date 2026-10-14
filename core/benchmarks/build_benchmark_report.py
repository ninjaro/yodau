#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from collections import defaultdict
from datetime import datetime
from pathlib import Path
from typing import Any


BENCHMARK_NAME_RE = re.compile(r"^replay/(?P<algorithm>[^/]+)/(?P<scenario>[^/]+)$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Convert Google Benchmark JSON from yodau_bench into a "
            "versioned comparison report."
        )
    )
    parser.add_argument("--input", required=True, help="Raw benchmark JSON path")
    parser.add_argument(
        "--hardware-profile",
        required=True,
        help="Hardware profile id under assets/dataset/test/benchmarks/hardware_profiles",
    )
    parser.add_argument(
        "--output",
        required=True,
        help="Final annotated JSON report path",
    )
    parser.add_argument(
        "--summary-output",
        required=False,
        help="Optional markdown summary path",
    )
    parser.add_argument(
        "--report-id",
        required=True,
        help="Stable report id written into the output JSON",
    )
    parser.add_argument(
        "--settings-profile",
        default="default",
        help="High-level settings profile label for the run",
    )
    parser.add_argument(
        "--reference-algorithm",
        default="motion_baseline",
        help="Stable baseline algorithm id for comparisons",
    )
    return parser.parse_args()


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2)
        handle.write("\n")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        handle.write(text)


def git_revision(repo_root: Path) -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"],
            cwd=repo_root,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def scenario_metadata_by_id(scenarios_root: Path) -> dict[str, dict[str, Any]]:
    metadata: dict[str, dict[str, Any]] = {}
    for metadata_path in sorted(scenarios_root.glob("*/metadata.json")):
        payload = load_json(metadata_path)
        scenario_id = str(payload.get("scenario_id", "")).strip()
        if scenario_id:
            metadata[scenario_id] = payload
    return metadata


def normalize_float(value: Any) -> float | None:
    if value is None:
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def normalize_int(value: Any) -> int | None:
    numeric = normalize_float(value)
    if numeric is None:
        return None
    return int(round(numeric))


def benchmark_rows(
    raw_payload: dict[str, Any],
    scenario_catalog: dict[str, dict[str, Any]],
    hardware_profile_id: str,
) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for entry in raw_payload.get("benchmarks", []):
        if entry.get("run_type") != "iteration":
            continue

        match = BENCHMARK_NAME_RE.match(str(entry.get("name", "")))
        if match is None:
            continue

        algorithm_id = match.group("algorithm")
        scenario_id = match.group("scenario")
        scenario_meta = scenario_catalog.get(scenario_id, {})
        extra_metrics = {
            key.removeprefix("metric_"): normalize_float(value)
            for key, value in entry.items()
            if key.startswith("metric_")
        }

        row = {
            "algorithm_id": algorithm_id,
            "scenario_id": scenario_id,
            "stream_count": normalize_int(entry.get("streams")),
            "line_count": normalize_int(entry.get("line_count")),
            "grid_cell_count": normalize_float(entry.get("grid_cell_count")),
            "avg_fps": normalize_float(entry.get("avg_fps")),
            "avg_latency_ms": normalize_float(entry.get("avg_latency_ms")),
            "p95_latency_ms": normalize_float(entry.get("p95_latency_ms")),
            "cpu_time_ms": normalize_float(entry.get("cpu_time")),
            "real_time_ms": normalize_float(entry.get("real_time")),
            "rss_mb": normalize_float(entry.get("rss_mb")),
            "expected_tripwire_count": normalize_float(
                entry.get("expected_tripwire_count")
            ),
            "detected_tripwire_count": normalize_float(
                entry.get("detected_tripwire_count")
            ),
            "false_positive_count": normalize_float(
                entry.get("false_positive_count")
            ),
            "missed_event_count": normalize_float(entry.get("missed_event_count")),
            "avg_events_per_frame": normalize_float(
                entry.get("avg_events_per_frame")
            ),
            "avg_overlays_per_frame": normalize_float(
                entry.get("avg_overlays_per_frame")
            ),
            "avg_metrics_per_frame": normalize_float(
                entry.get("avg_metrics_per_frame")
            ),
            "avg_diagnostics_per_frame": normalize_float(
                entry.get("avg_diagnostics_per_frame")
            ),
            "hardware_profile_id": hardware_profile_id,
            "dataset_revision": scenario_meta.get(
                "dataset_revision", "unknown"
            ),
            "source_mode": scenario_meta.get("source_mode", "unknown"),
            "recording_status": scenario_meta.get(
                "recording_status", "unknown"
            ),
        }
        if extra_metrics:
            row["extra_metrics"] = extra_metrics

        rows.append(row)

    return sorted(rows, key=lambda row: (row["scenario_id"], row["algorithm_id"]))


def common_dataset_revision(rows: list[dict[str, Any]]) -> str | None:
    revisions = {
        str(row.get("dataset_revision", "")).strip()
        for row in rows
        if str(row.get("dataset_revision", "")).strip()
    }
    if len(revisions) == 1:
        return next(iter(revisions))
    return None


def scenario_summary_lines(
    rows: list[dict[str, Any]], reference_algorithm_id: str
) -> list[str]:
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        grouped[str(row["scenario_id"])].append(row)

    lines: list[str] = []
    for scenario_id in sorted(grouped):
        scenario_rows = grouped[scenario_id]
        scenario_rows.sort(key=lambda row: str(row["algorithm_id"]))
        fastest = max(
            scenario_rows,
            key=lambda row: row.get("avg_fps") or float("-inf"),
        )

        def metric_or(row: dict[str, Any], key: str, fallback: float) -> float:
            value = row.get(key)
            return fallback if value is None else float(value)

        lowest_miss = min(
            scenario_rows,
            key=lambda row: (
                metric_or(row, "missed_event_count", float("inf")),
                metric_or(row, "false_positive_count", float("inf")),
                -metric_or(row, "avg_fps", 0.0),
            ),
        )

        lines.append(f"## {scenario_id}")
        lines.append("")
        lines.append(
            "| Algorithm | FPS | Avg ms | P95 ms | CPU ms | RSS MB | Missed | False+ | Detected / Expected |"
        )
        lines.append(
            "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"
        )

        def key(row: dict[str, Any]) -> tuple[int, str]:
            return (
                0 if row["algorithm_id"] == reference_algorithm_id else 1,
                str(row["algorithm_id"]),
            )

        for row in sorted(scenario_rows, key=key):
            lines.append(
                "| {algorithm} | {fps:.1f} | {avg:.3f} | {p95:.3f} | {cpu:.3f} | {rss:.1f} | {missed:.1f} | {false_pos:.1f} | {detected:.1f} / {expected:.1f} |".format(
                    algorithm=row["algorithm_id"],
                    fps=row.get("avg_fps") or 0.0,
                    avg=row.get("avg_latency_ms") or 0.0,
                    p95=row.get("p95_latency_ms") or 0.0,
                    cpu=row.get("cpu_time_ms") or 0.0,
                    rss=row.get("rss_mb") or 0.0,
                    missed=row.get("missed_event_count") or 0.0,
                    false_pos=row.get("false_positive_count") or 0.0,
                    detected=row.get("detected_tripwire_count") or 0.0,
                    expected=row.get("expected_tripwire_count") or 0.0,
                )
            )

        lines.append("")
        lines.append(
            f"- fastest by avg_fps: `{fastest['algorithm_id']}`"
        )
        lines.append(
            f"- lowest miss/false-positive pair: `{lowest_miss['algorithm_id']}`"
        )
        lines.append("")

    return lines


def build_summary_markdown(
    report_payload: dict[str, Any],
    rows: list[dict[str, Any]],
    hardware_profile: dict[str, Any],
) -> str:
    lines = [
        "# Benchmark Report",
        "",
        f"- report_id: `{report_payload['report_id']}`",
        f"- benchmark_date: `{report_payload['benchmark_date']}`",
        f"- hardware_profile_id: `{report_payload['hardware_profile_id']}`",
        f"- reference_algorithm_id: `{report_payload['reference_algorithm_id']}`",
        f"- dataset_revision: `{report_payload.get('dataset_revision', 'mixed')}`",
        f"- source_mode: `{report_payload.get('source_mode', 'mixed')}`",
        f"- git_revision: `{report_payload['git_revision']}`",
        "",
        "This report comes from the current synthetic placeholder scenario catalog.",
        "It is useful for repeatable algorithm comparison, but it is not yet a",
        "replacement for recorded clip benchmarking.",
        "",
        "## Hardware profile",
        "",
        f"- cpu_model: `{hardware_profile.get('cpu_model', 'unknown')}`",
        f"- logical_cores: `{hardware_profile.get('logical_cores', 'unknown')}`",
        f"- ram_gib: `{hardware_profile.get('ram_gib', 'unknown')}`",
        f"- os_kernel: `{hardware_profile.get('os_kernel', 'unknown')}`",
        f"- compiler: `{hardware_profile.get('compiler', 'unknown')}`",
        f"- build_type: `{hardware_profile.get('build_type', 'unknown')}`",
        "",
        "## Scenario summaries",
        "",
    ]
    lines.extend(
        scenario_summary_lines(rows, report_payload["reference_algorithm_id"])
    )
    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[2]
    benchmarks_root = repo_root / "assets" / "dataset" / "test" / "benchmarks"
    input_path = Path(args.input).resolve()
    output_path = Path(args.output).resolve()
    summary_path = (
        Path(args.summary_output).resolve()
        if args.summary_output
        else None
    )
    hardware_path = (
        benchmarks_root
        / "hardware_profiles"
        / f"{args.hardware_profile}.json"
    )
    scenarios_root = benchmarks_root / "scenarios"

    raw_payload = load_json(input_path)
    hardware_profile = load_json(hardware_path)
    scenario_catalog = scenario_metadata_by_id(scenarios_root)
    rows = benchmark_rows(raw_payload, scenario_catalog, args.hardware_profile)
    if not rows:
        print("No replay benchmark rows were found in the input JSON.", file=sys.stderr)
        return 1

    context = raw_payload.get("context", {})
    report_payload: dict[str, Any] = {
        "report_id": args.report_id,
        "status": "recorded",
        "benchmark_date": context.get("date", datetime.now().astimezone().isoformat()),
        "git_revision": git_revision(repo_root),
        "hardware_profile_id": args.hardware_profile,
        "dataset_revision": common_dataset_revision(rows) or "mixed",
        "settings_profile": args.settings_profile,
        "reference_algorithm_id": args.reference_algorithm,
        "source_mode": hardware_profile.get("source_mode", "unknown"),
        "notes": [
            "Derived from the committed Google Benchmark replay harness output.",
            "This report still uses the synthetic placeholder scenario catalog until recorded clips are checked in.",
            "motion_baseline remains the explicit comparison reference until a later benchmark review changes that rule.",
        ],
        "benchmark_context": {
            "executable": context.get("executable"),
            "num_cpus": context.get("num_cpus"),
            "mhz_per_cpu": context.get("mhz_per_cpu"),
            "library_version": context.get("library_version"),
            "library_build_type": context.get("library_build_type"),
        },
        "results": rows,
    }

    write_json(output_path, report_payload)
    if summary_path is not None:
        write_text(
            summary_path, build_summary_markdown(report_payload, rows, hardware_profile)
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
