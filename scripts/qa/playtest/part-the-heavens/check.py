import re

render_env = {
    "GENERALSX_TRACE_RETAIL_PARTICLE_SPAWNS": "1",
    "PLAYTEST_PIN_DYNAMIC_LOD": "1",
}


def check(log):
    spawns = {int(emitter): (int(frame), name) for frame, emitter, name in re.findall(
        r"^\[Retail Particle\] frame=(\d+) id=(\d+) name=(\S+)", log, re.MULTILINE)}
    samples = {}
    for frame, emitter, count, age in re.findall(
            r"^TEST particle-live frame=(\d+) id=(\d+) .* count=(\d+) age=(\d+)", log, re.MULTILINE):
        frame, emitter, count, age = map(int, (frame, emitter, count, age))
        samples.setdefault(emitter, {})[frame] = (count, frame - age)

    roots = [emitter for emitter, (_, name) in spawns.items() if name == "GandalfHeavenRaysSourceSmall"]
    if len(roots) != 6:
        raise RuntimeError(f"Part the Heavens should create six small Heaven Rays, got {len(roots)}")
    for root in roots:
        spawn_frame = spawns[root][0]
        chain = (root, root + 1, root + 2)
        names = tuple(spawns.get(emitter, (None, None))[1] for emitter in chain)
        if names != ("GandalfHeavenRaysSourceSmall", "GandalfHeavenRaysCloudsSmall", "GandalfHeavenRaysMotesSmall"):
            raise RuntimeError(f"Unexpected Heaven Rays chain for emitter {root}: {names}")
        first = [min((birth for _, birth in samples.get(emitter, {}).values()), default=None)
                 for emitter in chain]
        if first[0] is None or not 130 <= first[0] - spawn_frame <= 191 or first != [first[0]] * 3:
            raise RuntimeError(f"Heaven Rays {root} created at {spawn_frame} first emitted on frames {first}")

    fireworks = [emitter + 1 for emitter, (_, name) in spawns.items() if name == "firework2"]
    if len(fireworks) != 1 or spawns.get(fireworks[0], (None, None))[1] != "firework3":
        raise RuntimeError("Fireworks should create a linked firework3 emitter")
    bursts = samples.get(fireworks[0], {})
    first_burst = bursts.get(42)
    if first_burst is None or first_burst[0] != 30 or bursts.get(46) != first_burst:
        raise RuntimeError(f"Linked firework3 should emit 30 particles in a single burst: {bursts}")
