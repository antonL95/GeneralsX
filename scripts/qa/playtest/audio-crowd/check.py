import re

engine_timeout = 450


def check(log):
    for marker in (
        "AUDIO GROUP frame=20 selected=4",
        "AUDIO GROUP frame=35 selected=5",
        "AUDIO GROUP frame=70 move=1",
        "AUDIO GROUP frame=130 attack=1",
        "AUDIO GROUP frame=140 attack=1",
        "AUDIO GROUP frame=158 selected=5",
        "AUDIO GROUP frame=178 selected=1",
    ):
        if marker not in log:
            raise RuntimeError(f"Missing command trace: {marker}")
    requests = [(int(frame), event) for frame, event in re.findall(
        r"RETAIL AUDIO start frame=(\d+) owner=\d+ sound=(\S+)", log
    )]
    mismatches = []
    for frame, expected in (
        (35, "DwarfGroupSmall_Select"),
        (70, "DwarfMoveYellSingles"),
        (130, "DwarfGroupSmall_AttackCharge"),
        (140, "DwarfMoveYellSingles"),
        (158, "JungleWargMoveLaughSingles"),
        (178, "OrcGroupMedium_SelectMS"),
    ):
        matched = sum(frame <= actual <= frame + 1 and event == expected for actual, event in requests)
        if matched != 1:
            observed = [event for actual, event in requests if frame <= actual <= frame + 1]
            mismatches.append(f"frame {frame} expected one {expected}, saw {observed}")
    if mismatches:
        raise RuntimeError("\n".join(mismatches))
    if any(frame < 35 and event == "DwarfGroupSmall_Select" for frame, event in requests):
        raise RuntimeError("Crowd select played below the authored threshold")
    if sum(event == "DwarfGroupSmall_AttackCharge" for _, event in requests) != 1:
        raise RuntimeError("Crowd attack charge ignored its cooldown")
