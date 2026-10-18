#!/usr/bin/env python3
"""Build and compare Beasy's adapted Beads DSP against the local source.

The generated binary fixtures are deliberately kept outside the repository.
This command is an internal, source-backed parity check; it does not grant
permission to redistribute upstream source, artwork, or generated fixtures.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


BLOCK_SIZES = (32, 64, 128)
QUALITY_COUNT = 4
STATE_COUNT = 4
SOURCE_FILES = (
    "beads/dsp/processor.cc",
    "beads/dsp/delay_controller.cc",
    "beads/dsp/delay_clock.cc",
    "beads/dsp/mu_law.cc",
    "beads/dsp/synth.cc",
    "beads/dsp/trigger_generator.cc",
    "beads/resources.cc",
    "stmlib/dsp/units.cc",
    "stmlib/utils/random.cc",
)


def run(command: list[str], *, cwd: Path | None = None) -> str:
    result = subprocess.run(command, cwd=cwd, check=True, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return result.stdout.strip()


def git_head(path: Path) -> str:
    return run(["git", "-C", str(path), "rev-parse", "HEAD"])


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_layout(root: Path, stmlib_override: Path | None = None) -> tuple[Path, Path]:
    if (root / "beads" / "dsp" / "synth.h").exists():
        source_include = root
    elif (root / "dsp" / "synth.h").exists():
        source_include = root.parent
    else:
        raise SystemExit(f"source checkout has no beads/dsp/synth.h: {root}")
    stmlib = stmlib_override.resolve() if stmlib_override else source_include / "stmlib"
    if not (stmlib / "dsp" / "units.cc").exists():
        raise SystemExit(
            "source checkout has no usable stmlib; provide --source-stmlib-root")
    return source_include, stmlib


def compile_probe(cxx: str, probe: Path, include_root: Path, source_files: list[Path],
                  output: Path, *, port_api: bool) -> None:
    # TEST selects stmlib's portable Clip16/Sqrt implementations. The firmware
    # source otherwise emits ARM-only inline assembly and cannot be compiled on
    # the development host used for this deterministic reference probe.
    command = [cxx, "-std=c++20", "-O2", "-DNDEBUG", "-DTEST", "-I", str(include_root),
               "-I", str(include_root / "stmlib"), str(probe)]
    command.extend(str(path) for path in source_files)
    if port_api:
        command.append("-DBEASY_PORT_API")
    command.extend(["-o", str(output)])
    run(command)


def render_probe(binary: Path, prefix: Path) -> None:
    prefix.parent.mkdir(parents=True, exist_ok=True)
    run([str(binary), str(prefix)])


def compare_file(source: Path, port: Path) -> dict[str, object]:
    source_bytes = source.read_bytes()
    port_bytes = port.read_bytes()
    if len(source_bytes) != len(port_bytes) or len(source_bytes) % 4:
        return {
            "source": str(source),
            "port": str(port),
            "source_sha256": sha256(source),
            "port_sha256": sha256(port),
            "byte_identical": False,
            "frames": None,
            "max_abs_error": None,
            "rmse": None,
        }
    source_samples = struct.unpack(f"<{len(source_bytes) // 4}f", source_bytes)
    port_samples = struct.unpack(f"<{len(port_bytes) // 4}f", port_bytes)
    differences = [float(b) - float(a) for a, b in zip(source_samples, port_samples)]
    max_abs = max((abs(value) for value in differences), default=0.0)
    rmse = math.sqrt(sum(value * value for value in differences) /
                     len(differences)) if differences else 0.0
    return {
        "source": str(source),
        "port": str(port),
        "source_sha256": sha256(source),
        "port_sha256": sha256(port),
        "byte_identical": source_bytes == port_bytes,
        "frames": len(source_samples) // 2,
        "max_abs_error": max_abs,
        "rmse": rmse,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True,
                        help="checkout containing the pinned Beads source")
    parser.add_argument("--source-stmlib-root", type=Path,
                        help="stmlib checkout when source-root/stmlib is absent")
    parser.add_argument("--port-root", type=Path,
                        default=Path("third_party/beads_dsp"),
                        help="adapted DSP include root (default: third_party/beads_dsp)")
    parser.add_argument("--output-dir", type=Path,
                        help="directory for private fixtures and build products")
    parser.add_argument("--receipt", type=Path,
                        help="write the JSON receipt to this path")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[1]
    source_root = args.source_root.resolve()
    port_root = (repo / args.port_root).resolve() if not args.port_root.is_absolute() \
        else args.port_root.resolve()
    source_include, source_stmlib = source_layout(
        source_root, args.source_stmlib_root)
    if args.source_stmlib_root:
        source_stmlib = args.source_stmlib_root.resolve()
        if not (source_stmlib / "dsp" / "units.cc").exists():
            raise SystemExit(f"source stmlib is incomplete: {source_stmlib}")
        # Compile source with a temporary include tree whose stmlib entry points
        # at the explicit reference checkout. This leaves both source checkouts
        # untouched, including repositories that use a symlinked submodule.
        source_include = source_include
    if not (port_root / "beads" / "dsp" / "synth.h").exists():
        raise SystemExit(f"port DSP root is incomplete: {port_root}")

    out = args.output_dir.resolve() if args.output_dir else Path(
        tempfile.mkdtemp(prefix="beasy-source-port-", dir=str(repo.parent)))
    out.mkdir(parents=True, exist_ok=True)
    build = out / "build"
    build.mkdir(exist_ok=True)
    # A symlink is only used in the private build directory when the caller
    # supplies stmlib separately; no upstream files are copied or committed.
    source_build_include = source_include
    if args.source_stmlib_root:
        source_build_include = out / "source-include"
        (source_build_include / "beads").parent.mkdir(parents=True, exist_ok=True)
        os.symlink(source_include / "beads", source_build_include / "beads")
        os.symlink(source_stmlib, source_build_include / "stmlib")

    probe = repo / "scripts" / "beasy_dsp_matrix_probe.cpp"
    source_files = [source_build_include / relative for relative in SOURCE_FILES]
    port_files = [port_root / relative for relative in SOURCE_FILES]
    for path in source_files + port_files:
        if not path.exists():
            raise SystemExit(f"required DSP source is missing: {path}")

    source_binary = build / "source-probe"
    port_binary = build / "port-probe"
    compile_probe(args.cxx, probe, source_build_include, source_files,
                  source_binary, port_api=False)
    compile_probe(args.cxx, probe, port_root, port_files, port_binary,
                  port_api=True)
    source_prefix = out / "source-matrix"
    port_prefix = out / "port-matrix"
    render_probe(source_binary, source_prefix)
    render_probe(port_binary, port_prefix)

    pairs = []
    for block_size in BLOCK_SIZES:
        for quality in range(QUALITY_COUNT):
            for state in range(STATE_COUNT):
                suffix = f"-b{block_size}-q{quality}-c{state}.bin"
                pairs.append(compare_file(Path(f"{source_prefix}{suffix}"),
                                          Path(f"{port_prefix}{suffix}")))
    matching = [pair for pair in pairs if pair["byte_identical"]]
    max_abs = max((float(pair["max_abs_error"]) for pair in pairs
                   if pair["max_abs_error"] is not None), default=0.0)
    max_rmse = max((float(pair["rmse"]) for pair in pairs
                    if pair["rmse"] is not None), default=0.0)
    current_head = git_head(repo)
    receipt = {
        "schema": "beasy.source-port-comparison.v3",
        "status": "pass_internal_reference_only" if len(matching) == len(pairs)
                  else "mismatch",
        "verified_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "project": "Beasy",
        "port": {
            "repository": str(repo),
            "head": current_head,
            "worktree_dirty": bool(run(["git", "-C", str(repo), "status", "--porcelain"])),
            "dsp_root": str(port_root),
        },
        "source": {
            "repository": str(source_root),
            "head": git_head(source_root),
            "stmlib_root": str(source_stmlib),
        },
        "contract": {
            "sample_rate_hz": 48000,
            "frames_per_case": 4096,
            "channels": 2,
            "block_sizes": list(BLOCK_SIZES),
            "quality_modes": QUALITY_COUNT,
            "parameter_states": ["neutral", "pitch_time", "density_size_mix",
                                  "shape_reverb_freeze"],
            "seed_event": "SEED_BUTTON_STATE_ON at frame 0; OFF thereafter",
        },
        "comparison": {
            "fixtures_checked": len(pairs),
            "byte_identical": len(matching),
            "max_abs_error": max_abs,
            "max_rmse": max_rmse,
            "pairs": pairs,
        },
        "fixtures": {
            "directory": str(out),
            "distribution": "private generated evidence; fixtures are not committed",
        },
        "limitations": [
            "This compares the adapted DSP direct-render contract only; it does not prove Beasy Processor adapter behavior.",
            "MIDI sample-offset ordering, velocity, pitch bend, seed modes, CV, freeze persistence, and host state are separate gates.",
            "The source fixture is used under the recorded local MIT source notice; no upstream source, artwork, or fixture is redistributed.",
        ],
    }
    serialized = json.dumps(receipt, indent=2) + "\n"
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(serialized)
    print(serialized, end="")
    return 0 if receipt["status"] == "pass_internal_reference_only" else 1


if __name__ == "__main__":
    raise SystemExit(main())
