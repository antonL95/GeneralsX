from pathlib import Path
import re


render_env = {
    "PLAYTEST_STATIC_CAPTURES": "exact",
    "PLAYTEST_TRACE_CAPTURE_RESOURCES": "1",
}


def check(log):
    expected = [int(value) for value in Path(__file__).with_name("frames.txt").read_text().split()]
    samples = [
        tuple(map(int, sample))
        for sample in re.findall(r"^CAPTURE frame=(\d+) present_delta=(\d+) textures=(\d+) vertex_buffers=(\d+)$", log, re.M)
    ]
    if [frame for frame, *_ in samples] != expected:
        raise RuntimeError("High Pass capture resource trace is incomplete")
    if "TEST frame 30: create WildMountainOrcSwordHorde" not in log:
        raise RuntimeError("High Pass capture resource fixture did not spawn the horde")
    for frame, presents, *_ in samples:
        if presents != 1:
            raise RuntimeError(f"Capture at frame {frame} presented {presents} times")
    warmed, remaining = samples[len(samples) // 3:2 * len(samples) // 3], samples[2 * len(samples) // 3:]
    for index, name in ((2, "cached textures"), (3, "live vertex buffers")):
        limit = max(sample[index] for sample in warmed)
        for sample in remaining:
            if sample[index] > limit:
                raise RuntimeError(f"Frame {sample[0]} has {sample[index]} {name}, above the warm peak of {limit}")
