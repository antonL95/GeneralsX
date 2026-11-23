import argparse
import collections
import pathlib
import re


def ini_lines(path):
    for number, raw in enumerate(path.read_text(encoding="latin-1").splitlines(), 1):
        line = re.split(r";|//", raw, maxsplit=1)[0].strip()
        if line:
            yield number, line


def blocks(path, owner_pattern, block_pattern):
    owner = None
    fields = None
    for number, line in ini_lines(path):
        if fields is not None:
            if line.casefold() == "end":
                yield owner, start, fields
                fields = None
            else:
                key, value = line.split("=", 1)
                fields.append((key.strip(), value.strip()))
            continue
        match = re.fullmatch(owner_pattern, line, re.IGNORECASE)
        if match:
            owner = match.group(1)
        if re.fullmatch(block_pattern, line, re.IGNORECASE):
            if owner is None:
                raise ValueError(f"{path}:{number}: block has no owner")
            start = number
            fields = []
    if fields is not None:
        raise ValueError(f"{path}:{start}: unterminated block")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("ini", type=pathlib.Path)
    args = parser.parse_args()
    actions = collections.Counter()
    weapons = set()
    for file in sorted(args.ini.rglob("*.ini")):
        for name, number, fields in blocks(file, r"Weapon\s+([^\s=]+)", r"FireLogicNugget"):
            tags = dict(fields)
            weapons.add(name)
            actions[tags["LogicType"]] += 1
            print(file.relative_to(args.ini), number, name, *[f"{k}={v}" for k, v in fields], sep="\t")

    particles = args.ini / "fxparticlesystem.ini"
    emitters = {
        name: dict(fields)
        for name, _, fields in blocks(
            particles, r"FXParticleSystem\s+(\S+)", r"EmissionVolume\s*=\s*TerrainFireEmission"
        )
    }
    for name, fields in emitters.items():
        print(name, *[f"{key}={value}" for key, value in fields.items()], sep="\t")
    assert set(emitters) == {"GpuTerrainFire", "GpuTerrainSmoke"}, emitters
    assert set(actions) == {
        "INCREASE_BURN_RATE",
        "DECREASE_BURN_RATE",
        "INCREASE_FUEL",
        "INCREASE_BURN_RATE_ON_EXISTING_FIRE",
        "INCREASE_FUEL_ON_EXISTING_FIRE",
    }, actions
    print(f"{len(weapons)} weapons, {actions.total()} nuggets, {len(emitters)} emitters")
    for action, count in sorted(actions.items()):
        print(action, count)


if __name__ == "__main__":
    main()
