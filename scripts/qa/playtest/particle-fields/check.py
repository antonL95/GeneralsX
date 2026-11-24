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
