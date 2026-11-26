import argparse
import re
import sys
from pathlib import Path


render_env = {
    "PLAYTEST_FACTION": "FactionWild",
    "GENERALSX_TEST_DRAWTRACE": "MordorBalrog",
}


def check_arrival(log, expect_opaque=False):
    draws = re.findall(
        r"^TEST draw frame (\d+) id (\d+) MordorBalrog module ModuleTag_BodyDraw model \S+"
        r" clip (\S+) at ([\d.]+)/(\d+)",
        log, re.MULTILINE | re.IGNORECASE,
    )
    if not draws:
        raise RuntimeError("The spellbook cast produced no visible Balrog body")

    first, drawable, *_ = draws[0]
    opacity = re.findall(
        rf"^TEST draw-opacity frame {first} id {drawable} mesh \S+ effective ([\d.]+) ocl ([\d.]+)",
        log, re.MULTILINE,
    )
    order = re.search(r"^TEST default-order (\d+) selectable=1$", log, re.MULTILINE)
    if not order or not opacity:
        raise RuntimeError("Expected a move order for the summoned Balrog and an opacity sample on its first drawn frame")

    order_frame = int(order.group(1))
    roars = [frame for frame, _, clip, pose, length in draws
             if clip.lower().endswith("mubalrog_rora") and 0 < float(pose) < int(length) - 1]
    if str(order_frame) not in roars:
        raise RuntimeError("Expected the move order to arrive during the unfinished roar")

    movement_clips = ("mubalrog_wlka", "mubalrog_trna", "mubalrog_trnb")
    moving = [int(frame) for frame, _, clip, *_ in draws if clip.lower().endswith(movement_clips)]
    walking = [int(frame) for frame, _, clip, *_ in draws if clip.lower().endswith("mubalrog_wlka")]
    movement = moving[0] if moving else None
    walk = walking[0] if walking else None
    print(f"Balrog first draw={first} opacity={sorted(set(opacity))} order={order_frame} "
          f"first movement={movement} first walk={walk}", file=sys.stderr)

    if movement is None or not 0 <= movement - order_frame <= 5 or walk is None:
        raise RuntimeError("Expected locomotion within five frames of the order and the authored walk clip afterwards")
    if expect_opaque and any(float(effective) != 1 or float(ocl) != 1 for effective, ocl in opacity):
        raise RuntimeError("Expected full opacity on the first drawn frame")


def check(log):
    check_arrival(log)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--expect-opaque", action="store_true")
    args = parser.parse_args()
    check_arrival(args.log.read_text(errors="replace"), args.expect_opaque)
