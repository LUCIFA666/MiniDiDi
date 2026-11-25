"""Run paired, seeded strategy comparisons and save the actual C++ output."""
import argparse
import csv
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--seeds", nargs="+", type=int, default=[1, 7, 42])
parser.add_argument("--steps", type=int, default=1200)
parser.add_argument("--output", type=Path, default=root / "experiments")
args = parser.parse_args()
exe = root / "build" / ("minididi.exe" if os.name == "nt" else "minididi")
if not exe.exists():
    exe = root / "build" / "Release" / "minididi.exe"
args.output.mkdir(parents=True, exist_ok=True)
rows = []
for seed in args.seeds:
    raw = subprocess.check_output([str(exe), "--compare", str(seed), str(args.steps)])
    result = json.loads(raw.decode("utf-8"))
    (args.output / f"seed-{seed}.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    for run in result["results"]:
        rows.append({"seed": seed, "steps": args.steps, "strategy": run["strategy"], **run["metrics"]})
with (args.output / "comparison.csv").open("w", encoding="utf-8-sig", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
print(json.dumps(rows, ensure_ascii=False, indent=2))
