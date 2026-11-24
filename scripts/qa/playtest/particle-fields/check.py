import re

render_env = {"PLAYTEST_PIN_DYNAMIC_LOD": "1"}


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
