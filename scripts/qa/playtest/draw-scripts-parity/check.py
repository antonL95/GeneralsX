import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "MordorDrummerTroll"}


def check(log):
    pattern = (
        r"^TEST draw frame (\d+) id (\d+) MordorDrummerTroll module ModuleTag_01"
        r" model \S+ clip (\S+) at ([\d.]+)/([1-9]\d*) .* flags (\S*) subobjects"
    )
    by_id = {}
    for frame, obj_id, clip, pose, length, flags in re.findall(pattern, log, re.MULTILINE):
        flag_set = set(flags.split(",")) if flags else set()
        if "FIRING_OR_PREATTACK_A" in flag_set:
            by_id.setdefault(int(obj_id), set()).add(clip.lower())

    if len(by_id) < 2:
        raise RuntimeError(f"Expected two MordorDrummerTroll attackers in FIRING_OR_PREATTACK_A, got {by_id}")

    clip_sets = list(by_id.values())
    has_only_kick = any(clips == {"mudrmtroll_atkb"} for clips in clip_sets)
    has_only_swipe = any(clips == {"mudrmtroll_atka"} for clips in clip_sets)
    if not has_only_kick or not has_only_swipe:
        raise RuntimeError(
            f"Expected one troll to play only FrontKick (mudrmtroll_atkb) against STRUCTURE "
            f"and the other only StickSwipe (mudrmtroll_atka) against infantry/hero, got {by_id}"
        )
