import re

from PIL import Image


def check(log):
    if not re.search(r"TEST frame 35: select object \d+ \(MenFortressCitadel\)", log):
        raise RuntimeError("Radial bounds test did not select MenFortressCitadel at frame 35")


def check_captures(output, _frames):
    with Image.open(output / "frame-0030.png") as base_img, Image.open(output / "frame-0045.png") as radial_img:
        width, height = radial_img.size
        scale = min(width / 1024.0, height / 768.0)
        base = base_img.convert("RGB").load()
        radial = radial_img.convert("RGB").load()

        center_x = width // 2
        clock_half = round(50 * scale)
        clock_bottom = round(34 * scale)
        x0 = max(0, round(width * 0.15))
        x1 = min(width, round(width * 0.85))
        y1 = min(height, round(420 * scale))

        changed = 0
        top_xs = []
        top_scan_y = min(height, round(70 * scale))
        for y in range(y1):
            for x in range(x0, x1):
                if y < clock_bottom and abs(x - center_x) < clock_half:
                    continue
                if max(abs(radial[x, y][c] - base[x, y][c]) for c in range(3)) >= 20:
                    changed += 1
                    if y < top_scan_y:
                        top_xs.append(x)

        if changed < round(1200 * scale * scale):
            raise RuntimeError(f"{width}x{height}: radial menu did not render visibly near top ({changed} px)")
        if len(top_xs) < round(80 * scale):
            raise RuntimeError(f"{width}x{height}: radial menu did not reach near top edge ({len(top_xs)} top px)")

        top_xs.sort()
        btn_x = top_xs[len(top_xs) // 2]
        col_x0 = max(0, btn_x - round(16 * scale))
        col_x1 = min(width, btn_x + round(16 * scale))
        top_cluster_rows = []
        for y in range(min(height, round(200 * scale))):
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
            raise RuntimeError(f"{width}x{height}: 12-o'clock radial button missing around x={btn_x}")

        button_height = top_cluster_rows[-1] - top_cluster_rows[0] + 1
        expected_min = round(42 * scale)
        if button_height < expected_min:
            raise RuntimeError(
                f"{width}x{height}: top radial button clipped ({button_height}px < {expected_min}px, "
                f"rows {top_cluster_rows[0]}..{top_cluster_rows[-1]} at x={btn_x})"
            )
