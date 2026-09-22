"""Regenerates every CyGameCapture branding asset from the one master logo.

    python Tools/GenerateBranding.py [path/to/master.png]

The master (a square RGBA PNG, transparent outside the disk) is copied to
Resources/Branding/CyGameCapture_Logo.png, and everything else is derived from it:

  CyGameCapture.ico            Windows icon embedded in every binary (16 to 256 px)
  CyGameCapture_Logo_256.png   what the ReShade overlay draws (embedded in the add-on)
  CyGameCapture_Logo_512.png   documentation, README, test package
  CyGameCaptureUE/Resources/Icon128.png   the Unreal plugin's icon in the editor's Plugins window

Nothing here runs during a normal build: the outputs are committed, so building needs no Python.
Run it again only when the logo itself changes.

Requires Pillow (pip install pillow).
"""

import io
import shutil
import struct
import sys
from pathlib import Path

from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
BRANDING = ROOT / "Resources" / "Branding"
MASTER = BRANDING / "CyGameCapture_Logo.png"
UE_ICON = ROOT / "CyGameCaptureUE" / "Resources" / "Icon128.png"

# Every size Windows asks for somewhere: 16/20/24/32 for lists and title bars at 100-200 % scaling,
# 40/48/64 for the "medium icons" views, 96/128/256 for large and extra large icons.
ICON_SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]


def resized(master, size):
    """Downscaled in one step with a good filter; the smallest sizes get a light sharpening so the
    white monogram, which is what has to stay legible at 16 px, keeps a clean edge."""
    image = master.resize((size, size), Image.LANCZOS)
    if size <= 32:
        image = image.filter(ImageFilter.UnsharpMask(radius=0.6, percent=60, threshold=0))
    return image


def bmp_entry(image):
    """A classic 32-bit DIB icon entry: BITMAPINFOHEADER, bottom-up BGRA rows, then the 1-bit AND mask.

    PNG entries are only guaranteed for 256 px; the smaller sizes are stored as bitmaps so that every
    consumer, including the resource compiler and old shell code, reads them."""
    width, height = image.size
    rgba = image.tobytes()
    header = struct.pack("<IiiHHIIiiII", 40, width, height * 2, 1, 32, 0, 0, 0, 0, 0, 0)

    pixels = bytearray()
    for y in range(height - 1, -1, -1):
        row = rgba[y * width * 4:(y + 1) * width * 4]
        for x in range(width):
            r, g, b, a = row[x * 4:x * 4 + 4]
            pixels += bytes((b, g, r, a))

    mask_stride = ((width + 31) // 32) * 4
    mask = bytearray()
    for y in range(height - 1, -1, -1):
        row_bits = bytearray(mask_stride)
        for x in range(width):
            if rgba[(y * width + x) * 4 + 3] == 0:
                row_bits[x // 8] |= 0x80 >> (x % 8)
        mask += row_bits
    return header + bytes(pixels) + bytes(mask)


def png_entry(image):
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", optimize=True)
    return buffer.getvalue()


def write_ico(master, path):
    entries = []
    for size in ICON_SIZES:
        image = resized(master, size)
        data = png_entry(image) if size >= 256 else bmp_entry(image)
        entries.append((size, data))

    header = struct.pack("<HHH", 0, 1, len(entries))
    offset = 6 + 16 * len(entries)
    directory = b""
    blobs = b""
    for size, data in entries:
        dim = 0 if size >= 256 else size   # 0 means 256 in an ICONDIRENTRY
        directory += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
        blobs += data
    path.write_bytes(header + directory + blobs)


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else MASTER
    BRANDING.mkdir(parents=True, exist_ok=True)
    if source.resolve() != MASTER.resolve():
        shutil.copyfile(source, MASTER)

    master = Image.open(MASTER).convert("RGBA")
    if master.width != master.height:
        raise SystemExit(f"The master logo must be square, got {master.width}x{master.height}")

    write_ico(master, BRANDING / "CyGameCapture.ico")
    for size in (256, 512):
        resized(master, size).save(BRANDING / f"CyGameCapture_Logo_{size}.png", optimize=True)
    resized(master, 128).save(UE_ICON, optimize=True)

    for name in sorted(p.name for p in BRANDING.iterdir()):
        print(f"  {name:32} {(BRANDING / name).stat().st_size:>9} bytes")


if __name__ == "__main__":
    main()
