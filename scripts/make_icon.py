"""Regenerates installer/pixellabs.ico and docs/icon.png from a 16x16 pixel-art grid.

    python scripts/make_icon.py
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent

GRID = [
    "..bbbbbbbbbbbb..",
    ".bBBBBBBBBBBBBb.",
    "bBBBBBBBBBpppBBb",
    "bBBBBBBBBBpBBpBb",
    "bBBBBBBBBBpBBBBb",
    "bBBBBBBBBBpBBBBb",
    "bBBBBBpppppBBBBb",
    "bBBBBppppppBBBBb",
    "bBBBBpppppBBBBBb",
    "bBBBBBpppBBBBBBb",
    "bBBBBBBBBBBBBBBb",
    "bccccccccccccccb",
    "bwwkwkwwwkwkwkwb",
    "bwwkwkwwwkwkwkwb",
    ".bwwwwwwwwwwwwb.",
    "..bbbbbbbbbbbb..",
]

COLORS = {
    ".": (0, 0, 0, 0),
    "b": (70, 32, 127, 255),     # border
    "B": (13, 2, 33, 255),       # night background
    "p": (255, 46, 151, 255),    # hot pink note
    "c": (0, 240, 255, 255),     # cyan bar
    "w": (237, 230, 255, 255),   # white keys
    "k": (23, 7, 50, 255),       # black keys
}


def build(size_px: int) -> Image.Image:
    img = Image.new("RGBA", (16, 16))
    for y, row in enumerate(GRID):
        for x, ch in enumerate(row):
            img.putpixel((x, y), COLORS[ch])
    return img.resize((size_px, size_px), Image.NEAREST)


def main() -> None:
    sizes = [16, 24, 32, 48, 64, 128, 256]
    big = build(256)
    (ROOT / "installer").mkdir(exist_ok=True)
    (ROOT / "docs").mkdir(exist_ok=True)
    big.save(ROOT / "installer" / "pixellabs.ico", sizes=[(s, s) for s in sizes],
             append_images=[build(s) for s in sizes])
    build(128).save(ROOT / "docs" / "icon.png")
    print("wrote installer/pixellabs.ico and docs/icon.png")


if __name__ == "__main__":
    main()
