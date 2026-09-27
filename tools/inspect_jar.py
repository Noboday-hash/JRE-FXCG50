#!/usr/bin/env python3
"""Run the shared host archive/class inspector without converting a JAR."""

import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description="Static CGJRE JAR inspection")
    parser.add_argument("jar", type=Path)
    args = parser.parse_args()
    binary = Path(__file__).resolve().parents[1] / "build-host" / "cgjre-host"
    if not binary.is_file():
        parser.error("build-host/cgjre-host is missing; run the documented host build")
    print("Static report only; referenced APIs may be unreachable, and execution is unverified.",
          flush=True)
    return subprocess.run([str(binary), "--inspect", str(args.jar)],
                          check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
