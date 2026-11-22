#!/usr/bin/env python3
"""Build and execute OTA host regression tests with address/undefined sanitizers."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    output = ROOT / "build" / "ota_host"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "test_xmodem"
    subprocess.run([
        "cc", "-std=c11", "-g", "-O1", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-I" + str(ROOT / "main"),
        str(ROOT / "tests/test_xmodem.c"), str(ROOT / "main/xmodem.c"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
    lifecycle = output / "test_ota"
    subprocess.run([
        "cc", "-std=c11", "-g", "-O1", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", '-DAM_SOFTWARE_VERSION="v2.1"',
        "-I" + str(ROOT / "tests/ota_stubs"), "-I" + str(ROOT / "tests/stubs"),
        "-I" + str(ROOT / "main"), str(ROOT / "tests/test_ota.c"),
        str(ROOT / "main/xmodem.c"), "-o", str(lifecycle),
    ], check=True)
    subprocess.run([str(lifecycle)], check=True)

if __name__ == "__main__":
    main()
