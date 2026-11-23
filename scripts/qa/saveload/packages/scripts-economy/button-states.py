import sys
from pathlib import Path

from PIL import Image


def find_states(log_file):
    with log_file.open(errors="replace") as log:
        for line in log:
            if line.startswith("TEST followup frame=") and not line.endswith(" missing\n"):
                parts = dict(word.split("=", 1) for word in line.split()[2:])
                yield int(parts["frame"]), parts


def has_lock(image, state):
    x, y, width, height = (int(state[key]) for key in ("x", "y", "width", "height"))
    size = max(3, width // 4)
    left = x + width // 2 - size // 2
    top = y + height // 2 - size // 2 + size // 2
    pixels = image.crop((left + 1, top, left + size - 1, top + size // 2)).get_flattened_data()
    return sum(
        all(abs(actual - expected) <= 2 for actual, expected in zip(pixel, (211, 194, 153)))
        for pixel in pixels
    ) > len(pixels) / 2


out = Path(sys.argv[1])
states = dict(find_states(out / "buttons.log"))
cases = {
    14: ("prerequisite missing", "0", "0"),
    17: ("insufficient funds", "5", "0"),
    36: ("in production", "0", "1"),
    43: ("maximum reached", "0", "0"),
    50: ("cooldown", "4", "0"),
}
failures = []
for frame, (name, availability, count) in cases.items():
    state = states.get(frame)
    if not state or state["availability"] != availability or state["count"] != count:
        failures.append(f"{name} at frame {frame}: unexpected state {state}")
        continue
    if frame == 36 and float(state["progress"]) <= 0:
        failures.append(f"{name} at frame {frame}: production had not advanced")
        continue
    with Image.open(out / f"frame-{frame}.png") as capture:
        if has_lock(capture.convert("RGB"), state):
            failures.append(f"{name} at frame {frame}: lock overlay")
            continue
    print(f"{name} at frame {frame}: expected availability and no lock")
if failures:
    sys.exit("\n".join(failures))
