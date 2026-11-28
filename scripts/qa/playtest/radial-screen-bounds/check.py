import re

from PIL import Image


def check(log):
    if not re.search(r"TEST frame 35: select object \d+ \(MenFortressCitadel\)", log):
        raise RuntimeError("Radial bounds test did not select MenFortressCitadel at frame 35")
    if not re.search(r"TEST frame 45: screenpos -?\d+ -?\d+", log):
        raise RuntimeError("Radial bounds test did not log MenFortressCitadel screenpos at frame 45")


def check_captures(output, _frames):
    log_text = (output / "session.log").read_text(errors="replace")
    pos_match = re.search(r"TEST frame 45: screenpos (-?\d+) (-?\d+)", log_text)
    if not pos_match:
        raise RuntimeError(f"{output.name}: missing TEST frame 45 screenpos in session.log")
    sx = int(pos_match.group(1))

    with Image.open(output / "frame-0030.png") as base_img, Image.open(output / "frame-0045.png") as radial_img:
        width, height = radial_img.size
        scale = min(width / 1024.0, height / 768.0)
        base = base_img.convert("RGB").load()
        radial = radial_img.convert("RGB").load()

        center_x = width // 2
        clock_half = round(50 * scale)
        clock_bottom = round(34 * scale)
        col_x0 = max(0, sx - round(16 * scale))
        col_x1 = min(width, sx + round(16 * scale))

        top_cluster_rows = []
        for y in range(min(height, round(240 * scale))):
            row_hits = sum(
                1 for x in range(col_x0, col_x1)
                if not (y < clock_bottom and abs(x - center_x) < clock_half)
                and max(abs(radial[x, y][c] - base[x, y][c]) for c in range(3)) >= 22
            )
            if row_hits >= max(4, round(8 * scale)):
                top_cluster_rows.append(y)
            elif top_cluster_rows:
                break

        if not top_cluster_rows:
            raise RuntimeError(f"{width}x{height}: 12-o'clock radial button missing around screenpos x={sx}")

        button_height = top_cluster_rows[-1] - top_cluster_rows[0] + 1
        expected_min = round(42 * scale)
        if button_height < expected_min:
            raise RuntimeError(
                f"{width}x{height}: top radial button clipped ({button_height}px < {expected_min}px, "
                f"rows {top_cluster_rows[0]}..{top_cluster_rows[-1]} at screenpos x={sx})"
            )
        if height in (720, 1080) and top_cluster_rows[0] > round(12 * scale):
            raise RuntimeError(
                f"{width}x{height}: expected top-clamped radial button near y=0, found top at y={top_cluster_rows[0]}"
            )
