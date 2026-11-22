from pathlib import Path
import sys

from PIL import Image


def fraction(path, box, match):
    with Image.open(path) as frame:
        pixels = frame.convert("RGB").crop(box)
        return sum(match(pixel) for pixel in pixels.get_flattened_data()) / (pixels.width * pixels.height)


def main(result):
    black = fraction(result / "frame-0260.png", (565, 255, 710, 370), lambda rgb: max(rgb) < 12)
    white = fraction(result / "frame-0292.png", (80, 40, 1200, 480), lambda rgb: min(rgb) >= 220)
    print(f"Word of Power frame 260 near-black: {black:.1%}; frame 292 near-white: {white:.1%}")
    if black > 0.10 or white > 0.25:
        raise SystemExit("Word of Power charge or blast obscures the scene")


if __name__ == "__main__":
    main(Path(sys.argv[1]))
