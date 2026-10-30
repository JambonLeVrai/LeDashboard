#!/usr/bin/env python3
"""
Convert SVG icons to LVGL C arrays.

Usage:
    python convert_icons.py

Converts:
- Weather icons (cloudy, drizzle, fog, hail, partly_cloudy, rain, snow, sun, thunder, wind)
  to 96px and 64px C arrays
- Metric icons (air_quality, thermometer, droplet, uv)
  to 24px C arrays
"""

import os
import subprocess
import sys
from pathlib import Path

import cairosvg

# Paths
SCRIPT_DIR = Path(__file__).parent.absolute()
RESOURCES_DIR = SCRIPT_DIR.parent
SVG_DIR = RESOURCES_DIR / "svg_icons"
PNG_DIR = RESOURCES_DIR / "png_temp"
OUTPUT_DIR = RESOURCES_DIR.parent / "src" / "icons"
LVGL_IMAGE_SCRIPT = SCRIPT_DIR / "LVGLImage.py"
PYTHON = sys.executable

# Icon configurations: (name, [sizes])
WEATHER_ICONS = ["cloudy", "drizzle", "fog", "hail", "partly_cloudy",
                 "rain", "snow", "sun", "thunder", "wind"]
WEATHER_SIZES = [96, 64]

METRIC_ICONS = ["air_quality", "thermometer", "droplet", "uv"]
METRIC_SIZES = [24, 32]

MEDIA_ICONS = ["play", "pause"]
MEDIA_SIZES = [16]  # Slightly larger than 6px progress bar


def svg_to_png(svg_path: Path, png_path: Path, size: int, fill_color: str = None):
    """Convert SVG to PNG at specified size, optionally overriding fill color."""
    png_path.parent.mkdir(parents=True, exist_ok=True)
    svg_content = svg_path.read_text(encoding='utf-8')
    if fill_color:
        # Replace common dark fill colors with the target color
        import re
        svg_content = re.sub(r'fill="#(?:0F0F0F|000000|000)"', f'fill="{fill_color}"', svg_content)
        svg_content = re.sub(r'fill="black"', f'fill="{fill_color}"', svg_content)
    cairosvg.svg2png(
        bytestring=svg_content.encode('utf-8'),
        write_to=str(png_path),
        output_width=size,
        output_height=size,
    )
    print(f"  SVG -> PNG: {svg_path.name} -> {png_path.name} ({size}px)")


def png_to_c(png_path: Path, output_dir: Path, name: str):
    """Convert PNG to LVGL C array using LVGLImage.py."""
    output_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        PYTHON, str(LVGL_IMAGE_SCRIPT),
        "--ofmt", "C",
        "--cf", "RGB565A8",  # RGB565 with alpha for transparency
        "--premultiply",     # Pre-multiply alpha
        "-o", str(output_dir),
        "--name", name,
        str(png_path),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  ERROR converting {png_path.name}: {result.stderr}")
        return False
    print(f"  PNG -> C: {png_path.name} -> {name}.c")
    return True


def convert_icon(icon_name: str, sizes: list, fill_color: str = None):
    """Convert a single icon at multiple sizes."""
    svg_path = SVG_DIR / f"{icon_name}.svg"
    if not svg_path.exists():
        print(f"WARNING: {svg_path} not found, skipping")
        return

    print(f"\nProcessing: {icon_name}")
    for size in sizes:
        # Step 1: SVG -> PNG
        png_path = PNG_DIR / f"{icon_name}_{size}px.png"
        svg_to_png(svg_path, png_path, size, fill_color)

        # Step 2: PNG -> C array
        c_name = f"{icon_name}_{size}"
        if icon_name in WEATHER_ICONS:
            output_subdir = OUTPUT_DIR / "weather"
        elif icon_name in MEDIA_ICONS:
            output_subdir = OUTPUT_DIR / "media"
        else:
            output_subdir = OUTPUT_DIR / "metrics"
        png_to_c(png_path, output_subdir, c_name)


def main():
    print("=" * 60)
    print("LVGL Icon Converter")
    print("=" * 60)

    # Create output directories
    (OUTPUT_DIR / "weather").mkdir(parents=True, exist_ok=True)
    (OUTPUT_DIR / "metrics").mkdir(parents=True, exist_ok=True)
    (OUTPUT_DIR / "media").mkdir(parents=True, exist_ok=True)

    # Convert weather icons at 96px and 64px
    print("\n--- Weather Icons (96px, 64px) ---")
    for icon in WEATHER_ICONS:
        convert_icon(icon, WEATHER_SIZES)

    # Convert metric icons at 24px and 32px
    print("\n--- Metric Icons (24px, 32px) ---")
    for icon in METRIC_ICONS:
        convert_icon(icon, METRIC_SIZES)

    # Convert media icons at 16px (black fill for light background)
    print("\n--- Media Icons (16px, black) ---")
    for icon in MEDIA_ICONS:
        convert_icon(icon, MEDIA_SIZES, fill_color="#000000")

    # Clean up temp PNGs
    print("\n--- Cleanup ---")
    import shutil
    if PNG_DIR.exists():
        shutil.rmtree(PNG_DIR)
        print(f"Removed temp directory: {PNG_DIR}")

    print("\n" + "=" * 60)
    print("Conversion complete!")
    print(f"Output directory: {OUTPUT_DIR}")
    print("=" * 60)


if __name__ == "__main__":
    main()
