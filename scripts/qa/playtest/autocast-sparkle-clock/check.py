import re

render_env = {
    "PLAYTEST_FIXED_UPDATE_FPS": "125",
    "PLAYTEST_SPARKLE_PAUSE_FRAMES": "58:66",
}

CLIP_FRAMES = 199
FRAME_MS = 33


def check(log):
    if not re.search(r"TEST autocast-art frame=50 slot=2 availability=-?\d+ auto=1 ", log):
        raise RuntimeError("Right click did not enable the tested autocast overlay")
    samples = [(int(frame), int(index), float(elapsed), int(paused)) for frame, index, elapsed, paused in re.findall(
        r"TEST sparkle-clock frame=(\d+) clip=(\d+) elapsed=([\d.]+) paused_updates=(\d+)", log)]
    held = [sample for sample in samples if 59 <= sample[0] <= 65]
    if len(held) < 5 or held[-1][3] <= held[0][3]:
        raise RuntimeError(f"No sparkle samples during the paused client updates: {held}")
    if len({sample[1] for sample in held}) != 1:
        raise RuntimeError(f"Sparkles advanced during paused client updates: {held}")
    if not any(sample[0] >= 72 and sample[1] != held[-1][1] for sample in samples):
        raise RuntimeError("Sparkles failed to resume")
    for first, last in zip(samples, samples[1:]):
        expected = (last[2] - first[2]) / FRAME_MS
        advanced = (last[1] - first[1]) % CLIP_FRAMES
        if abs(advanced - expected) >= 1.001:
            raise RuntimeError(f"Sparkles drifted from the authored 33 ms cadence: {first} to {last}")
