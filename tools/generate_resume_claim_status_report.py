#!/usr/bin/env python3
"""Generate a markdown status report from resume_claim_gatecheck.py output."""

from __future__ import annotations

import json
import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GATECHECK = ROOT / "tools" / "resume_claim_gatecheck.py"
DEFAULT_OUTPUT = ROOT / "docs" / "resume_claim_status_auto_20260630.md"


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
    contradictions = data["contradictions"]
    facts = data["current_facts"]

    lines: list[str] = []
    lines.append("# IPC 项目 Claim 状态自动报告")
    lines.append("")
    lines.append("## 1. 当前代码事实")
    lines.append("")
    for key, value in facts.items():
        lines.append(f"- `{key}`: `{value}`")

    lines.append("")
    lines.append("## 2. Claim 状态")
    lines.append("")
    lines.append("| Claim | 当前状态 | 目标状态 | 证据 |")
    lines.append("|------|------|------|------|")
    for claim in claims:
        evidence = ", ".join(f"`{item}`" for item in claim["evidence"])
        lines.append(
            f"| {claim['claim']} | `{claim['status']}` | `{claim['target_state']}` | {evidence} |"
        )

    lines.append("")
    lines.append("## 3. 与旧文档冲突或需警惕项")
    lines.append("")
    for key, value in contradictions.items():
        lines.append(f"- `{key}`: `{value}`")

    lines.append("")
    lines.append("## 4. 自动结论")
    lines.append("")
    strong = [c["claim"] for c in claims if c["status"] == "strong"]
    coarse = [c["claim"] for c in claims if c["status"] == "coarse"]
    experiment_ready = [c["claim"] for c in claims if c["status"] == "experiment_ready"]
    toolchain_validated = [c["claim"] for c in claims if c["status"] == "toolchain_validated"]

    lines.append(f"- 已有强证据：{', '.join(strong) if strong else '无'}")
    lines.append(f"- 只有粗证据：{', '.join(coarse) if coarse else '无'}")
    lines.append(
        f"- 工具链已验证、但仍缺板端标准化采样："
        f"{', '.join(toolchain_validated) if toolchain_validated else '无'}"
    )
    lines.append(f"- 仅实验准备完成：{', '.join(experiment_ready) if experiment_ready else '无'}")

    report = "\n".join(lines)
    args.output.write_text(report, encoding="utf-8")
    print(f"Wrote markdown report to: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
