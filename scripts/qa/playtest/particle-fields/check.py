import re

render_env = {"PLAYTEST_PIN_DYNAMIC_LOD": "1", "GENERALSX_TEST_PARTICLE_ORIENTATION": "1"}


def check(log):
    samples = {}
    for frame, name, x, y, z in re.findall(
        r"^TEST particle-live frame=(40|50) id=\d+ name=(TestParticleControl|TestParticleTurbulence) .* xyz=([-\d.]+),([-\d.]+),([-\d.]+)",
        log, re.MULTILINE
    ):
        samples[(int(frame), name)] = (float(x), float(y), float(z))
    for frame in (40, 50):
        if (frame, "TestParticleControl") not in samples or (frame, "TestParticleTurbulence") not in samples:
            raise RuntimeError(f"Missing particle motion sample at frame {frame}: {samples}")
    control = samples[(50, "TestParticleControl")][0] - samples[(40, "TestParticleControl")][0]
    turbulent = samples[(50, "TestParticleTurbulence")][0] - samples[(40, "TestParticleTurbulence")][0]
    if abs(control) > 0.1 or turbulent < 1.0:
        raise RuntimeError(f"Unexpected particle displacement: control={control}, turbulence={turbulent}")
    frequency = re.search(
        r"^TEST particle-live frame=40 id=\d+ name=TestParticleFrequency .* xyz=([-\d.]+),[-\d.]+,[-\d.]+",
        log, re.MULTILINE
    )
    if not frequency or float(frequency.group(1)) - samples[(40, "TestParticleTurbulence")][0] < 10:
        raise RuntimeError("Turbulence frequency did not change particle motion")
    radius = {}
    for frame, name, x, y in re.findall(
        r"^TEST particle-live frame=(40|50) id=\d+ name=(TestParticleRadiusControl|TestParticleRadiusGrowth) .* xyz=([-\d.]+),([-\d.]+),[-\d.]+",
        log, re.MULTILINE
    ):
        radius[(int(frame), name)] = ((float(x) - 2520) ** 2 + (float(y) - 1010) ** 2) ** 0.5
    for frame in (40, 50):
        if (frame, "TestParticleRadiusControl") not in radius or (frame, "TestParticleRadiusGrowth") not in radius:
            raise RuntimeError(f"Missing cylinder radius sample at frame {frame}: {radius}")
        if abs(radius[(frame, "TestParticleRadiusControl")] - 10) > 0.2:
            raise RuntimeError(f"Control cylinder radius changed: {radius}")
    if radius[(50, "TestParticleRadiusGrowth")] - radius[(40, "TestParticleRadiusGrowth")] < 8:
        raise RuntimeError(f"Cylinder radius did not grow: {radius}")
    minimum = re.search(
        r"^TEST particle-live frame=40 id=\d+ name=TestParticleRadiusMinimum .* xyz=([-\d.]+),([-\d.]+),[-\d.]+",
        log, re.MULTILINE
    )
    if not minimum or not 0.08 <= ((float(minimum.group(1)) - 2520) ** 2 + (float(minimum.group(2)) - 1010) ** 2) ** 0.5 <= 0.12:
        raise RuntimeError("Zero radius did not reach the retail minimum")
    swirl = {}
    for name, y in re.findall(
        r"^TEST particle-live frame=40 id=\d+ name=(TestParticleSwirlControl|TestParticleSwirl) .* xyz=[-\d.]+,([-\d.]+),[-\d.]+",
        log, re.MULTILINE
    ):
        swirl[name] = float(y)
    if len(swirl) != 2 or abs(swirl["TestParticleSwirl"] - swirl["TestParticleSwirlControl"]) < 1:
        raise RuntimeError(f"Swirly particle did not move sideways relative to its control: {swirl}")
    rotations = {}
    for frame, name, xy, z in re.findall(
        r"^TEST particle-rotation frame=(40|50) name=(TestParticleRotation[XYV]) xy=([-\d.]+) z=([-\d.]+)$",
        log, re.MULTILINE
    ):
        rotations[(int(frame), name)] = (float(xy), float(z))
    for axis in "XYV":
        first = rotations.get((40, f"TestParticleRotation{axis}"))
        last = rotations.get((50, f"TestParticleRotation{axis}"))
        if not first or not last or last[0] - first[0] < 0.4 or last[1] - first[1] < 0.9:
            raise RuntimeError(f"Particle angles did not advance for {axis}: {rotations}")
    axes = {}
    for name, x, y, z in re.findall(
        r"^TEST particle-orientation frame=40 name=(TestParticleRotation[XYV]) x-axis=([-\d.]+),([-\d.]+),([-\d.]+)$",
        log, re.MULTILINE
    ):
        axes[name[-1]] = (float(x), float(y), float(z))
    if len(axes) != 3 or abs(axes["X"][0] - 1) > 0.01 or abs(axes["Y"][2]) < 0.1 or abs(axes["V"][1]) < 0.1:
        raise RuntimeError(f"Drawable rotation axes did not render as expected: {axes}")
