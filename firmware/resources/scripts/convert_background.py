#!/usr/bin/env python3
"""
Convert an image to LVGL C array for use as a background.

Usage:
    python convert_background.py [input_image] [output_name] [--saturation 0.5] [--brightness 1.2] [--size 480]

Parameters:
    --saturation   0.0-2.0  (1.0 = original, <1 = desaturated, >1 = more vivid)
    --brightness   0.0-2.0  (1.0 = original, <1 = darker, >1 = brighter)
    --size         output resolution (default 480 for 480x480 display)
    --quality      JPEG-like quality not applicable, but we can use PNG optimization
"""

import argparse
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageEnhance

SCRIPT_DIR = Path(__file__).parent.absolute()
RESOURCES_DIR = SCRIPT_DIR.parent
DEFAULT_INPUT = RESOURCES_DIR / "backgrounds"
OUTPUT_DIR = RESOURCES_DIR.parent / "src" / "backgrounds"
LVGL_IMAGE_SCRIPT = SCRIPT_DIR / "LVGLImage.py"
PYTHON = sys.executable


def process_image(input_path: Path, output_png: Path, size: int, saturation: float, brightness: float):
    """Resize and adjust image, save as PNG."""
    img = Image.open(input_path).convert("RGB")
    
    # Crop to aspect ratio, then resize to exact size
    img = resize_to_fit(img, size, size)
    
    # Adjust saturation
    if saturation != 1.0:
        img = ImageEnhance.Color(img).enhance(saturation)
    
    # Adjust brightness
    if brightness != 1.0:
        img = ImageEnhance.Brightness(img).enhance(brightness)
    
    output_png.parent.mkdir(parents=True, exist_ok=True)
    img.save(output_png, "PNG", optimize=True)
    print(f"  Processed: {input_path.name} -> {output_png.name} ({size}x{size}, sat={saturation}, bright={brightness})")


def resize_to_fit(img: Image.Image, target_w: int, target_h: int) -> Image.Image:
    """Crop to target aspect ratio first, then resize to exact dimensions."""
    src_w, src_h = img.size
    target_ratio = target_w / target_h
    src_ratio = src_w / src_h
    
    # Crop to match target aspect ratio (center crop)
    if src_ratio > target_ratio:
        # Image is wider than target - crop sides
        new_w = int(src_h * target_ratio)
        left = (src_w - new_w) // 2
        img = img.crop((left, 0, left + new_w, src_h))
    else:
        # Image is taller than target - crop top/bottom
        new_h = int(src_w / target_ratio)
        top = (src_h - new_h) // 2
        img = img.crop((0, top, src_w, top + new_h))
    
    # Now resize to exact target dimensions
    return img.resize((target_w, target_h), Image.LANCZOS)


def png_to_c(png_path: Path, output_dir: Path, name: str):
    """Convert PNG to LVGL C array (RGB565, no alpha for opaque background)."""
    output_dir.mkdir(parents=True, exist_ok=True)
    cmd = [
        PYTHON, str(LVGL_IMAGE_SCRIPT),
        "--ofmt", "C",
        "--cf", "RGB565",  # No alpha needed for opaque background
        "-o", str(output_dir),
        "--name", name,
        str(png_path),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  ERROR: {result.stderr}")
        return False
    print(f"  C array: {name}.c generated in {output_dir}")
    return True


def main():
    parser = argparse.ArgumentParser(description="Convert image to LVGL background")
    parser.add_argument("input", nargs="?", default=None, help="Input image path (default: first file in resources/backgrounds/)")
    parser.add_argument("name", nargs="?", default="background", help="Output variable name (default: background)")
    parser.add_argument("--saturation", type=float, default=0.5, help="Saturation 0.0-2.0 (default 0.5 for muted bg)")
    parser.add_argument("--brightness", type=float, default=1.25, help="Brightness 0.0-2.0 (default 1.25, +25%% brighter)")
    parser.add_argument("--size", type=int, default=480, help="Output size (default 480)")
    args = parser.parse_args()
    
    # Find input
    if args.input:
        input_path = Path(args.input)
    else:
        files = list(DEFAULT_INPUT.glob("*.png")) + list(DEFAULT_INPUT.glob("*.jpg")) + list(DEFAULT_INPUT.glob("*.jpeg"))
        if not files:
            print(f"ERROR: No images found in {DEFAULT_INPUT}")
            sys.exit(1)
        input_path = files[0]
    
    if not input_path.exists():
        print(f"ERROR: {input_path} not found")
        sys.exit(1)
    
    print(f"Background converter")
    print(f"  Input:  {input_path}")
    print(f"  Output: {args.name} ({args.size}x{args.size})")
    print(f"  Saturation: {args.saturation}, Brightness: {args.brightness}")
    
    # Step 1: Process image (resize, adjust)
    temp_png = OUTPUT_DIR.parent / "resources" / "png_temp" / f"{args.name}_processed.png"
    process_image(input_path, temp_png, args.size, args.saturation, args.brightness)
    
    # Step 2: Convert to C array
    png_to_c(temp_png, OUTPUT_DIR, args.name)
    
    # Cleanup
    import shutil
    if temp_png.parent.exists():
        shutil.rmtree(temp_png.parent)
    
    # Report size
    c_file = OUTPUT_DIR / f"{args.name}.c"
    if c_file.exists():
        size_kb = c_file.stat().st_size / 1024
        print(f"\nDone! {c_file} ({size_kb:.0f} KB source)")
        print(f"Binary size will be ~{size_kb * 0.5 / 1024:.1f} MB (rough estimate)")


if __name__ == "__main__":
    main()
