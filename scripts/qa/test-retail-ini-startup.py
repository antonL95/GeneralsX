#!/usr/bin/env python3
import difflib
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def canonical(path):
    return path.replace("/", "\\").lower()


def check(log, reference):
    expected = [
        kind + " " + (canonical(name) if kind == "file" else name)
        for kind, name in (line.split(" ", 1) for line in reference.splitlines() if line.strip())
    ]
    expected_files = {item[5:] for item in expected if item.startswith("file ")}
    actual = []
    define_events = {}
    file_stages = {}
    loaded_files = []
    stage = None
    for line in log:
        if line.startswith("[INI subsystem] "):
            stage = line.removeprefix("[INI subsystem] ").strip()
            actual.append("subsystem " + stage)
        elif line.startswith("[INI define] "):
            before, after, filename = line.removeprefix("[INI define] ").strip().split(" ", 2)
            filename = canonical(filename)
            define_events.setdefault(filename, []).append((before, after))
            file_stages.setdefault(filename, []).append(stage)
            loaded_files.append(filename)
            if filename in expected_files:
                actual.append("file " + filename)

    if expected != actual:
        sys.stderr.writelines(difflib.unified_diff(
            [line + "\n" for line in expected], [line + "\n" for line in actual],
            fromfile="retail order", tofile="engine order",
        ))
        return False

    expected_visibility = {
        r"data\ini\default\subsystemlegendexpansion1.ini": [("0", "0")],
        r"data\ini\default\gamedata.ini": [("0", "0")],
        r"data\ini\gamedata.ini": [("0", "0")],
        r"data\ini\default\water.ini": [("0", "1")],
        r"data\ini\playertemplate.ini": [("1", "1")],
    }
    for filename, visibility in expected_visibility.items():
        if define_events.get(filename) != visibility:
            print(f"{filename} expected define visibility {visibility}, got {define_events.get(filename)}", file=sys.stderr)
            return False

    game_data = loaded_files.index(r"data\ini\gamedata.ini")
    default_game_data = loaded_files.index(r"data\ini\default\gamedata.ini")
    sage_patch = [index for index, filename in enumerate(loaded_files) if filename.endswith(r"\sagepatch.ini")]
    if len(sage_patch) != 1 or not default_game_data < sage_patch[0] < game_data:
        print("Expected SagePatch between default GameData and the loose override", file=sys.stderr)
        return False

    expected_file_stages = {
        r"data\ini\upgrade.ini": "TheUpgradeCenter",
        r"data\ini\weather.ini": "TheGlobalWeatherSystem",
        r"data\ini\playeraitypes.ini": "ThePlayerAITypeSet",
        r"data\ini\fxparticlesystem.ini": "TheFXParticleSystemManager",
        r"data\ini\object\cinematic\cinematicweapon.ini": "TheWeaponStore",
        r"data\ini\object\cinematic\cinematiclocomotor.ini": "TheLocomotorStore",
        r"data\ini\experiencelevels.ini": "TheExperienceLevelSystem",
        r"data\ini\attributemodifier.ini": "TheAttributeModifierStore",
        r"data\ini\commandmapdebug.ini": "TheMetaMap",
        r"data\ini\housecolor.ini": "TheHouseColorSystem",
        r"data\ini\firelogicsystem.ini": "TheFireLogicSystem",
    }
    for filename, owner in expected_file_stages.items():
        if file_stages.get(filename) != [owner]:
            print(f"{filename} expected one load under {owner}, got {file_stages.get(filename)}", file=sys.stderr)
            return False

    for line in log:
        if line.startswith("[INI skipped] ") and canonical(line).startswith(
            r"[ini skipped] data\ini\commandmapdebug.ini:"
        ):
            print(line, file=sys.stderr, end="")
            return False

    print(f"RotWK INI order matches all {len(expected)} stages. The Water define becomes visible before PlayerTemplate.")
    return True


def main():
    if len(sys.argv) != 3:
        print(f'usage: {sys.argv[0]} BUILD "$(cat INIT_ORDER_FILE)"', file=sys.stderr)
        return 2
    build = Path(sys.argv[1]).resolve(strict=True)
    reference = sys.argv[2]
    env = os.environ.copy()
    env.update(
        GENERALSX_INI_TRACE_DEFINE="AOTR_PLAYER_MAX_PURCHASE_POINTS_DEFAULT",
        LD_LIBRARY_PATH=":".join(sorted({str(p.parent) for p in build.rglob("*.so*")})),
        SDL_AUDIO_DRIVER="dummy",
    )

    with tempfile.TemporaryDirectory(prefix="ini-startup-", dir=build) as output:
        for variable, folder in (("XDG_DATA_HOME", "data"), ("XDG_CONFIG_HOME", "config"),
                                 ("XDG_CACHE_HOME", "cache"), ("XDG_RUNTIME_DIR", "runtime")):
            directory = Path(output) / folder
            directory.mkdir(mode=0o700)
            env[variable] = str(directory)

        with tempfile.TemporaryFile(mode="w+t", encoding="latin-1") as log:
            command = [
                "timeout", "300", str(build / "GeneralsMD/GeneralsXZH"),
                "-headless", "-noaudio", "-nomusic", "-novideo", "-noshellmap",
                "-mod", "/game-install/aotr", "-bfme2", "/game-install/bfme2",
            ]
            status = subprocess.run(command, cwd="/game-install/rotwk", env=env,
                                    stdout=log, stderr=subprocess.STDOUT, check=False).returncode
            log.seek(0)
            lines = log.readlines()
            if status or not any("Retail headless startup requires a map, save, or replay." in line for line in lines):
                sys.stderr.writelines(lines[-40:])
                return status or 1
            return 0 if check(lines, reference) else 1


if __name__ == "__main__":
    sys.exit(main())
