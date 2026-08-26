#!/usr/bin/env python3
"""Evaluate generated mip levels against reference images with PSNR and FLIP."""

from __future__ import annotations

import argparse
import csv
import math
import re
from pathlib import Path

import numpy as np
from flip_evaluator import evaluate as evaluate_flip
from PIL import Image


MIP_PATTERN = re.compile(r"^mip(\d+)\.png$", re.IGNORECASE)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compute per-mip PSNR and mean LDR-FLIP error."
    )
    parser.add_argument(
        "--reference-dir",
        type=Path,
        help="Directory containing reference mipN.png images.",
    )
    parser.add_argument(
        "--test-dir",
        type=Path,
        help="Directory containing generated mipN.png images.",
    )
    parser.add_argument(
        "--result-dir",
        type=Path,
        help="Output directory for metrics.csv and metrics.md (default: <test-dir>/metrics).",
    )
    return parser.parse_args()


def resolve_image_directory(explicit: Path | None, root: Path, label: str) -> Path:
    if explicit is not None:
        directory = explicit
    else:
        candidates = sorted(
            path for path in root.iterdir() if path.is_dir() and collect_mips(path)
        )
        if len(candidates) != 1:
            raise ValueError(
                f"{label}: expected exactly one mip directory under '{root}', "
                f"found {len(candidates)}. Pass its path explicitly."
            )
        directory = candidates[0]

    if not directory.is_dir():
        raise ValueError(f"{label} directory does not exist: {directory}")
    return directory


def collect_mips(directory: Path) -> dict[int, Path]:
    mip_paths: dict[int, Path] = {}
    for path in directory.iterdir():
        if not path.is_file():
            continue
        match = MIP_PATTERN.match(path.name)
        if match:
            mip_paths[int(match.group(1))] = path
    return mip_paths


def load_rgb(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGB"), dtype=np.uint8)


def compute_psnr(reference: np.ndarray, test: np.ndarray) -> float:
    difference = reference.astype(np.float64) - test.astype(np.float64)
    mse = float(np.mean(difference * difference))
    if mse == 0.0:
        return math.inf
    return 10.0 * math.log10((255.0 * 255.0) / mse)


def evaluate_level(level: int, reference_path: Path, test_path: Path) -> dict[str, object]:
    reference = load_rgb(reference_path)
    test = load_rgb(test_path)
    if reference.shape != test.shape:
        raise ValueError(
            f"mip{level}: image sizes differ: "
            f"reference={reference.shape[1]}x{reference.shape[0]}, "
            f"test={test.shape[1]}x{test.shape[0]}"
        )

    _, flip_mean, _ = evaluate_flip(
        str(reference_path),
        str(test_path),
        "LDR",
        inputsRGB=True,
        applyMagma=False,
        computeMeanError=True,
    )
    return {
        "mip_level": level,
        "width": reference.shape[1],
        "height": reference.shape[0],
        "psnr_db": compute_psnr(reference, test),
        "flip_mean": float(flip_mean),
    }


def format_number(value: float) -> str:
    return "inf" if math.isinf(value) else f"{value:.6f}"


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(
            output,
            fieldnames=["mip_level", "width", "height", "psnr_db", "flip_mean"],
        )
        writer.writeheader()
        for row in rows:
            writer.writerow(
                {
                    **row,
                    "psnr_db": format_number(float(row["psnr_db"])),
                    "flip_mean": format_number(float(row["flip_mean"])),
                }
            )


def write_markdown(
    path: Path,
    reference_dir: Path,
    test_dir: Path,
    rows: list[dict[str, object]],
) -> None:
    lines = [
        "# Mipmap 품질 평가",
        "",
        f"- Reference: `{reference_dir}`",
        f"- Test: `{test_dir}`",
        "- PSNR: RGB 8-bit 전체 채널 기준 (높을수록 좋음)",
        "- FLIP: NVIDIA LDR-FLIP mean error, 기본 67 PPD (낮을수록 좋음)",
        "",
        "| Mip | 해상도 | PSNR (dB) | Mean FLIP |",
        "|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append(
            f"| {row['mip_level']} | {row['width']}×{row['height']} | "
            f"{format_number(float(row['psnr_db']))} | "
            f"{format_number(float(row['flip_mean']))} |"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    args = parse_args()
    reference_dir = resolve_image_directory(args.reference_dir, Path("target"), "Reference")
    test_dir = resolve_image_directory(args.test_dir, Path("output"), "Test")

    reference_mips = collect_mips(reference_dir)
    test_mips = collect_mips(test_dir)
    if not reference_mips or not test_mips:
        raise ValueError("No mipN.png images were found.")
    if reference_mips.keys() != test_mips.keys():
        missing_test = sorted(reference_mips.keys() - test_mips.keys())
        missing_reference = sorted(test_mips.keys() - reference_mips.keys())
        raise ValueError(
            f"Mip levels do not match. Missing test={missing_test}, "
            f"missing reference={missing_reference}"
        )

    rows = []
    for level in sorted(reference_mips):
        row = evaluate_level(level, reference_mips[level], test_mips[level])
        rows.append(row)
        print(
            f"mip{level}: {row['width']}x{row['height']}, "
            f"PSNR={format_number(float(row['psnr_db']))} dB, "
            f"FLIP={format_number(float(row['flip_mean']))}"
        )

    result_dir = args.result_dir or test_dir / "metrics"
    result_dir.mkdir(parents=True, exist_ok=True)
    write_csv(result_dir / "metrics.csv", rows)
    write_markdown(result_dir / "metrics.md", reference_dir, test_dir, rows)
    print(f"Results saved to: {result_dir}")


if __name__ == "__main__":
    main()
