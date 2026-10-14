"""Exercise the authored report helper independently of the caller's directory."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[3]
REPORT_HELPER = PROJECT_ROOT / "core/benchmarks/build_benchmark_report.py"


class BenchmarkReportTests(unittest.TestCase):
    def run_report(self, directory, entries):
        raw = directory / "input.json"
        raw.write_text(json.dumps({
            "context": {"date": "2026-10-13T00:00:00Z"},
            "benchmarks": entries,
        }), encoding="utf-8")
        return subprocess.run([
            sys.executable, str(REPORT_HELPER),
            "--input", str(raw),
            "--hardware-profile", "dev_laptop_cpu",
            "--output", str(directory / "report.json"),
            "--summary-output", str(directory / "report.md"),
            "--report-id", "report-helper-regression",
        ], cwd=directory, capture_output=True, text=True, check=False)

    def test_report_uses_project_metadata_from_an_unrelated_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            result = self.run_report(directory, [{
                "name": "replay/motion_baseline/single_day_sparse",
                "run_type": "iteration",
                "avg_fps": 12.5,
                "expected_tripwire_count": 3,
                "detected_tripwire_count": 3,
            }])
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((directory / "report.json").read_text())
            scenario = json.loads((PROJECT_ROOT / "assets/dataset/test/benchmarks/"
                                   "scenarios/single_day_sparse/metadata.json").read_text())
            self.assertEqual(report["report_id"], "report-helper-regression")
            self.assertEqual(report["benchmark_date"], "2026-10-13T00:00:00Z")
            self.assertEqual(report["results"][0]["dataset_revision"],
                             scenario["dataset_revision"])
            self.assertEqual(report["results"][0]["avg_fps"], 12.5)
            self.assertIn("motion_baseline", (directory / "report.md").read_text())

    def test_empty_input_fails_without_creating_reports(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            result = self.run_report(directory, [])
            self.assertEqual(result.returncode, 1)
            self.assertIn("No replay benchmark rows", result.stderr)
            self.assertFalse((directory / "report.json").exists())
            self.assertFalse((directory / "report.md").exists())


if __name__ == "__main__":
    unittest.main()
