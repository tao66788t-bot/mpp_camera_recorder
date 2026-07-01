#!/usr/bin/env python3
"""Generate a consolidated readiness report for resume claim closure."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GATECHECK = ROOT / "tools" / "resume_claim_gatecheck.py"
DEFAULT_OUTPUT = ROOT / "docs" / "resume_claim_readiness_auto_20260630.md"


def run_gatecheck() -> dict[str, object]:
    proc = subprocess.run(
        [sys.executable, str(GATECHECK)],
        check=True,
        capture_output=True,
    )
    stdout = proc.stdout.decode("utf-8", errors="replace")
    return json.loads(stdout)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    data = run_gatecheck()
    claims = data["claims"]

    strong = [c for c in claims if c["status"] == "strong"]
    coarse = [c for c in claims if c["status"] == "coarse"]
    toolchain_validated = [c for c in claims if c["status"] == "toolchain_validated"]
    experiment_ready = [c for c in claims if c["status"] == "experiment_ready"]
    missing = [c for c in claims if c["status"] == "missing"]

    lines: list[str] = []
    lines.append("# IPC 项目简历 Claim Ready 状态自动报告")
    lines.append("")
    lines.append("## 1. 可直接写进简历")
    lines.append("")
    for claim in strong:
        lines.append(f"- `{claim['claim']}`")
    if not strong:
        lines.append("- 无")

    lines.append("")
    lines.append("## 2. 工具链已验证，但还缺板端标准化采样")
    lines.append("")
    for claim in toolchain_validated:
        lines.append(f"- `{claim['claim']}`")
        lines.append(f"  证据：{', '.join(claim['evidence'])}")
    if not toolchain_validated:
        lines.append("- 无")

    lines.append("")
    lines.append("## 3. 只有粗证据，不能写死")
    lines.append("")
    for claim in coarse:
        lines.append(f"- `{claim['claim']}`")
        lines.append(f"  证据：{', '.join(claim['evidence'])}")
    if not coarse:
        lines.append("- 无")

    lines.append("")
    lines.append("## 4. 仅实验准备完成")
    lines.append("")
    for claim in experiment_ready:
        lines.append(f"- `{claim['claim']}`")
        lines.append(f"  证据：{', '.join(claim['evidence'])}")
    if not experiment_ready:
        lines.append("- 无")

    lines.append("")
    lines.append("## 5. 仍缺失")
    lines.append("")
    for claim in missing:
        lines.append(f"- `{claim['claim']}`")
    if not missing:
        lines.append("- 无")

    lines.append("")
    lines.append("## 6. 自动结论")
    lines.append("")
    lines.append(
        f"- 已做实、可直接写：{len(strong)} 项"
    )
    lines.append(
        f"- 工具链已验证、只差板端标准化采样：{len(toolchain_validated)} 项"
    )
    lines.append(
        f"- 只有粗证据：{len(coarse)} 项"
    )
    lines.append(
        f"- 仅实验准备完成：{len(experiment_ready)} 项"
    )
    lines.append(
        f"- 完全缺失：{len(missing)} 项"
    )

    report = "\n".join(lines)
    args.output.write_text(report, encoding="utf-8")
    print(f"Wrote readiness report to: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
