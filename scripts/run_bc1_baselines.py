#!/usr/bin/env python3
"""Run BC1 baseline experiments with bc7enc_rdo and etcpak."""

from __future__ import annotations

import argparse
import csv
import re
import statistics
import struct
import subprocess
from datetime import datetime
from pathlib import Path

import numpy as np
from PIL import Image

from evaluate_mipmaps import collect_mips, evaluate_level, format_number


BC7ENC_ENCODING_TIME_PATTERN = re.compile(
    r"Total encoding time:\s*([0-9]+(?:\.[0-9]+)?)\s*secs", re.IGNORECASE
)
BC7ENC_PROCESSING_TIME_PATTERN = re.compile(
    r"Total processing time:\s*([0-9]+(?:\.[0-9]+)?)\s*secs", re.IGNORECASE
)
ETCPAK_TIME_PATTERN = re.compile(
    r"Encoding Time\s*([0-9]+(?:\.[0-9]+)?)\s*ms", re.IGNORECASE
)
ETCPAK_RUN_COUNT = 10


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(
        description=(
            "Run per-level bc7enc BC1 fast/high-quality baselines and an "
            "etcpak BC1 mip-chain baseline."
        )
    )
    parser.add_argument(
        "--reference-dir",
        type=Path,
        default=root / "target" / "Ceramic_0557_brick_uneven_stones_color",
        help="Directory containing reference mipN.png images.",
    )
    parser.add_argument(
        "--bc7enc",
        type=Path,
        default=root / "base" / "bc7enc_rdo" / "build" / "Release" / "bc7enc.exe",
        help="Path to bc7enc.exe.",
    )
    parser.add_argument(
        "--etcpak",
        type=Path,
        default=root / "base" / "etcpak" / "build" / "Release" / "etcpak.exe",
        help="Path to etcpak.exe.",
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=root / "output" / "baseline",
        help="Root directory for generated experiment artifacts.",
    )
    return parser.parse_args()


def require_file(path: Path, label: str) -> Path:
    path = path.resolve()
    if not path.is_file():
        raise FileNotFoundError(f"{label} does not exist: {path}")
    return path


def run_command(command: list[str], log_path: Path) -> str:
    result = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(result.stdout, encoding="utf-8")
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed with exit code {result.returncode}:\n"
            f"{' '.join(command)}\nSee log: {log_path}"
        )
    return result.stdout


def parse_time_ms(
    output: str,
    pattern: re.Pattern[str],
    label: str,
    unit_scale_ms: float,
) -> float:
    match = pattern.search(output)
    if match is None:
        raise ValueError(f"Could not find {label} in encoder output.")
    return float(match.group(1)) * unit_scale_ms


def expand_5_to_8(value: int) -> int:
    return (value * 527 + 23) >> 6


def expand_6_to_8(value: int) -> int:
    return (value * 259 + 33) >> 6


def decode_rgb565(value: int) -> tuple[int, int, int]:
    return (
        expand_5_to_8((value >> 11) & 0x1F),
        expand_6_to_8((value >> 5) & 0x3F),
        expand_5_to_8(value & 0x1F),
    )


def decode_bc1_level(data: bytes, width: int, height: int) -> np.ndarray:
    block_width = max(1, (width + 3) // 4)
    block_height = max(1, (height + 3) // 4)
    expected_size = block_width * block_height * 8
    if len(data) != expected_size:
        raise ValueError(
            f"Invalid BC1 level size: expected {expected_size}, got {len(data)}"
        )

    image = np.zeros((height, width, 3), dtype=np.uint8)
    offset = 0
    for block_y in range(block_height):
        for block_x in range(block_width):
            color0, color1, selectors = struct.unpack_from("<HHI", data, offset)
            offset += 8
            c0 = decode_rgb565(color0)
            c1 = decode_rgb565(color1)

            if color0 > color1:
                c2 = tuple((2 * c0[i] + c1[i] + 1) // 3 for i in range(3))
                c3 = tuple((c0[i] + 2 * c1[i] + 1) // 3 for i in range(3))
            else:
                c2 = tuple((c0[i] + c1[i] + 1) // 2 for i in range(3))
                c3 = (0, 0, 0)
            palette = (c0, c1, c2, c3)

            for texel_y in range(4):
                y = block_y * 4 + texel_y
                if y >= height:
                    continue
                for texel_x in range(4):
                    x = block_x * 4 + texel_x
                    if x >= width:
                        continue
                    texel_index = texel_y * 4 + texel_x
                    selector = (selectors >> (texel_index * 2)) & 0x3
                    image[y, x] = palette[selector]
    return image


def extract_bc1_dds_mips(dds_path: Path, output_dir: Path) -> list[Path]:
    data = dds_path.read_bytes()
    if len(data) < 128 or data[:4] != b"DDS ":
        raise ValueError(f"Not a legacy DDS file: {dds_path}")

    height, width = struct.unpack_from("<II", data, 12)
    mip_count = max(1, struct.unpack_from("<I", data, 28)[0])
    if data[84:88] != b"DXT1":
        raise ValueError(f"Expected DXT1/BC1 DDS, found FourCC {data[84:88]!r}")

    output_dir.mkdir(parents=True, exist_ok=True)
    offset = 128
    paths: list[Path] = []
    for level in range(mip_count):
        level_width = max(1, width >> level)
        level_height = max(1, height >> level)
        level_size = max(1, (level_width + 3) // 4) * max(
            1, (level_height + 3) // 4
        ) * 8
        level_data = data[offset : offset + level_size]
        if len(level_data) != level_size:
            raise ValueError(f"DDS ended while reading mip{level}")
        image = decode_bc1_level(level_data, level_width, level_height)
        path = output_dir / f"mip{level}.png"
        Image.fromarray(image, mode="RGB").save(path)
        paths.append(path)
        offset += level_size
    return paths


def run_bc7enc_mode(
    executable: Path,
    references: dict[int, Path],
    output_dir: Path,
    quality_level: int,
) -> dict[int, dict[str, float]]:
    output_dir.mkdir(parents=True, exist_ok=True)
    logs_dir = output_dir / "logs"
    times_ms: dict[int, dict[str, float]] = {}
    for level, reference in sorted(references.items()):
        dds_path = output_dir / f"mip{level}.dds"
        png_path = output_dir / f"mip{level}.png"
        output = run_command(
            [
                str(executable),
                "-1",
                f"-L{quality_level}",
                "-c",
                str(reference),
                str(dds_path),
                str(png_path),
            ],
            logs_dir / f"mip{level}.log",
        )
        encoding_time_ms = parse_time_ms(
            output,
            BC7ENC_ENCODING_TIME_PATTERN,
            "bc7enc total encoding time",
            1000.0,
        )
        processing_time_ms = parse_time_ms(
            output,
            BC7ENC_PROCESSING_TIME_PATTERN,
            "bc7enc total processing time",
            1000.0,
        )
        times_ms[level] = {
            "encoding_time_ms": encoding_time_ms,
            "total_processing_time_ms": processing_time_ms,
        }
        print(
            f"bc7enc L{quality_level} mip{level}: "
            f"encoding={encoding_time_ms:.6f} ms, "
            f"total_processing={processing_time_ms:.6f} ms"
        )
    return times_ms


def run_etcpak(
    executable: Path,
    base_image: Path,
    output_dir: Path,
) -> list[float]:
    output_dir.mkdir(parents=True, exist_ok=True)
    dds_path = output_dir / "mipmap.dds"
    times_ms = []
    for run_index in range(1, ETCPAK_RUN_COUNT + 1):
        output = run_command(
            [
                str(executable),
                "-M",
                "-m",
                "-c",
                "bc1",
                "-h",
                "dds",
                str(base_image),
                str(dds_path),
            ],
            output_dir / "logs" / f"run_{run_index:02d}.log",
        )
        time_ms = parse_time_ms(
            output, ETCPAK_TIME_PATTERN, "etcpak encoding time", 1.0
        )
        times_ms.append(time_ms)
        print(
            f"etcpak mip chain run {run_index}/{ETCPAK_RUN_COUNT}: "
            f"encoding={time_ms:.6f} ms"
        )

    extract_bc1_dds_mips(dds_path, output_dir)
    print(
        f"etcpak mip chain median: "
        f"encoding={statistics.median(times_ms):.6f} ms"
    )
    return times_ms


def evaluate_encoder(
    name: str,
    references: dict[int, Path],
    test_dir: Path,
    times_ms: dict[int, dict[str, float]] | None,
) -> list[dict[str, object]]:
    test_mips = collect_mips(test_dir)
    if references.keys() != test_mips.keys():
        raise ValueError(
            f"{name}: mip levels differ: reference={sorted(references)}, "
            f"test={sorted(test_mips)}"
        )

    rows: list[dict[str, object]] = []
    for level in sorted(references):
        row = evaluate_level(level, references[level], test_mips[level])
        row["encoder"] = name
        row["encoding_time_ms"] = (
            "" if times_ms is None else times_ms[level]["encoding_time_ms"]
        )
        row["total_processing_time_ms"] = (
            ""
            if times_ms is None
            else times_ms[level]["total_processing_time_ms"]
        )
        rows.append(row)
        print(
            f"{name} mip{level}: PSNR={format_number(float(row['psnr_db']))} dB, "
            f"FLIP={format_number(float(row['flip_mean']))}"
        )
    return rows


def write_level_results(path: Path, rows: list[dict[str, object]]) -> None:
    fieldnames = [
        "encoder",
        "mip_level",
        "width",
        "height",
        "psnr_db",
        "flip_mean",
        "encoding_time_ms",
        "total_processing_time_ms",
    ]
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(
                {
                    **row,
                    "psnr_db": format_number(float(row["psnr_db"])),
                    "flip_mean": format_number(float(row["flip_mean"])),
                    "encoding_time_ms": (
                        ""
                        if row["encoding_time_ms"] == ""
                        else f"{float(row['encoding_time_ms']):.6f}"
                    ),
                    "total_processing_time_ms": (
                        ""
                        if row["total_processing_time_ms"] == ""
                        else f"{float(row['total_processing_time_ms']):.6f}"
                    ),
                }
            )


def build_performance_rows(
    references: dict[int, Path],
    fast_times: dict[int, dict[str, float]],
    quality_times: dict[int, dict[str, float]],
    etcpak_times_ms: list[float],
) -> list[dict[str, object]]:
    total_pixels = 0
    for path in references.values():
        with Image.open(path) as image:
            total_pixels += image.width * image.height

    measurements = [
        {
            "encoder": "bc7enc_bc1_fast_L0",
            "encoding_time_ms": sum(
                value["encoding_time_ms"] for value in fast_times.values()
            ),
            "total_processing_time_ms": sum(
                value["total_processing_time_ms"] for value in fast_times.values()
            ),
        },
        {
            "encoder": "bc7enc_bc1_quality_L18",
            "encoding_time_ms": sum(
                value["encoding_time_ms"] for value in quality_times.values()
            ),
            "total_processing_time_ms": sum(
                value["total_processing_time_ms"]
                for value in quality_times.values()
            ),
        },
        {
            "encoder": "etcpak_bc1_mipmap_mt_median_10",
            "encoding_time_ms": statistics.median(etcpak_times_ms),
            "total_processing_time_ms": None,
        },
    ]
    rows = []
    for measurement in measurements:
        encoding_time_ms = float(measurement["encoding_time_ms"])
        processing_time_ms = measurement["total_processing_time_ms"]
        rows.append(
            {
                **measurement,
                "total_pixels": total_pixels,
                "encoding_throughput_mpix_s": total_pixels
                / (encoding_time_ms * 1000.0),
                "total_processing_throughput_mpix_s": (
                    None
                    if processing_time_ms is None
                    else total_pixels / (float(processing_time_ms) * 1000.0)
                ),
            }
        )
    return rows


def write_performance_results(
    path: Path, rows: list[dict[str, object]]
) -> None:
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(
            output,
            fieldnames=[
                "encoder",
                "total_pixels",
                "encoding_time_ms",
                "total_processing_time_ms",
                "encoding_throughput_mpix_s",
                "total_processing_throughput_mpix_s",
            ],
        )
        writer.writeheader()
        for row in rows:
            writer.writerow(
                {
                    **row,
                    "encoding_time_ms": f"{float(row['encoding_time_ms']):.6f}",
                    "total_processing_time_ms": (
                        ""
                        if row["total_processing_time_ms"] is None
                        else f"{float(row['total_processing_time_ms']):.6f}"
                    ),
                    "encoding_throughput_mpix_s": (
                        f"{float(row['encoding_throughput_mpix_s']):.6f}"
                    ),
                    "total_processing_throughput_mpix_s": (
                        ""
                        if row["total_processing_throughput_mpix_s"] is None
                        else f"{float(row['total_processing_throughput_mpix_s']):.6f}"
                    ),
                }
            )


def write_etcpak_samples(path: Path, times_ms: list[float]) -> None:
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.writer(output)
        writer.writerow(["run", "encoding_time_ms"])
        for run_index, time_ms in enumerate(times_ms, start=1):
            writer.writerow([run_index, f"{time_ms:.6f}"])
        writer.writerow(["median", f"{statistics.median(times_ms):.6f}"])


def main() -> None:
    args = parse_args()
    reference_dir = args.reference_dir.resolve()
    if not reference_dir.is_dir():
        raise FileNotFoundError(f"Reference directory does not exist: {reference_dir}")
    references = collect_mips(reference_dir)
    if not references or 0 not in references:
        raise ValueError(f"No mip0.png-based mip chain found in {reference_dir}")

    bc7enc = require_file(args.bc7enc, "bc7enc executable")
    etcpak = require_file(args.etcpak, "etcpak executable")

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    run_dir = args.output_root.resolve() / reference_dir.name / timestamp
    fast_dir = run_dir / "bc7enc_fast"
    quality_dir = run_dir / "bc7enc_quality"
    etcpak_dir = run_dir / "etcpak_mipmap"
    run_dir.mkdir(parents=True, exist_ok=False)

    print(f"Experiment output: {run_dir}")
    fast_times = run_bc7enc_mode(bc7enc, references, fast_dir, quality_level=0)
    quality_times = run_bc7enc_mode(
        bc7enc, references, quality_dir, quality_level=18
    )
    etcpak_times_ms = run_etcpak(etcpak, references[0], etcpak_dir)

    level_rows = []
    level_rows.extend(
        evaluate_encoder(
            "bc7enc_bc1_fast_L0", references, fast_dir, fast_times
        )
    )
    level_rows.extend(
        evaluate_encoder(
            "bc7enc_bc1_quality_L18", references, quality_dir, quality_times
        )
    )
    level_rows.extend(
        evaluate_encoder("etcpak_bc1_mipmap_mt", references, etcpak_dir, None)
    )
    performance_rows = build_performance_rows(
        references, fast_times, quality_times, etcpak_times_ms
    )

    write_level_results(run_dir / "level_metrics.csv", level_rows)
    write_performance_results(run_dir / "performance.csv", performance_rows)
    write_etcpak_samples(run_dir / "etcpak_timing_samples.csv", etcpak_times_ms)

    print(f"Level metrics: {run_dir / 'level_metrics.csv'}")
    print(f"Performance: {run_dir / 'performance.csv'}")
    print(f"etcpak timing samples: {run_dir / 'etcpak_timing_samples.csv'}")


if __name__ == "__main__":
    main()

