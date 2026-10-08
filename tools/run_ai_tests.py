"""Compile and run hardware-independent C regression tests with GCC."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", help="Path to a GCC-compatible C compiler")
    args = parser.parse_args()
    compiler = args.cc or shutil.which("gcc")
    fallback = Path("C:/Program Files (x86)/Dev-Cpp/MinGW64/bin/gcc.exe")
    if not compiler and fallback.is_file():
        compiler = str(fallback)
    if not compiler:
        parser.error("GCC not found; install GCC or pass --cc")
    root = Path(__file__).resolve().parent.parent
    sources = ["tools/edge_ai_tests.c","src/edge_ai/edge_ai_monitor.c","src/vibration/vibration_anomaly.c"]
    with tempfile.TemporaryDirectory(prefix="ra8d1-ai-tests-") as temporary:
        executable = Path(temporary) / "ai_tests.exe"
        subprocess.run([
            compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
            *[str(root / source) for source in sources],
            "-lm", "-o", str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
