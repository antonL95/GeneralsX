import math
import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import tarfile
import tempfile

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
INSTALL = Path("/game-install")
WIDTH, HEIGHT = 1280, 720
COLUMNS = 4
CLIP_SIZE = (640, 360)


def read_scenario(name):
    if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", name):
        raise ValueError(f"Invalid scenario name: {name}")
    fixture = ROOT / name
    map_path = (INSTALL / (fixture / "map.txt").read_text().strip()).resolve()
    if not map_path.is_relative_to(INSTALL) or map_path.suffix != ".map" or not map_path.is_file():
        raise ValueError(f"Expected an installed map under {INSTALL}: {map_path}")
    frames = [int(field) for field in (fixture / "frames.txt").read_text().split()]
    if not frames or frames[0] < 20 or frames != sorted(set(frames)):
        raise ValueError("Capture frames must be unique, increasing integers starting at 20 or later")
    render_from_file = fixture / "render-from.txt"
    render_from = int(render_from_file.read_text()) if render_from_file.exists() else 28
    if render_from_file.exists() and not 28 <= render_from <= frames[0]:
        raise ValueError("render-from.txt must be between frame 28 and the first capture")
    end_frame = frames[-1] + 2
    commands = [line for line in (fixture / "commands.txt").read_text().splitlines() if line.strip()]
    for line in commands:
        command_frame, action, *_ = line.split()
        if not 0 < int(command_frame) < end_frame or action in ("quit", "camera"):
            raise ValueError(f"Invalid scenario command: {line}")
    camera_lines = [line for line in (fixture / "camera.txt").read_text().splitlines() if line.strip()]
    if not camera_lines:
        raise ValueError("Expected at least one FRAME PLAYER OBJECT X Y ZOOM line in camera.txt")
    camera_frames = []
    for line in camera_lines:
        camera = line.split()
        if len(camera) != 6:
            raise ValueError("Expected FRAME PLAYER OBJECT X Y ZOOM in camera.txt")
        frame, player, actor = camera[:3]
        x, y, zoom = map(float, camera[3:])
        frame_number = int(frame)
        if not all(map(math.isfinite, (x, y, zoom))) or zoom <= 0:
            raise ValueError(f"Camera requires finite X, Y and positive finite ZOOM: {line}")
        if not 0 < frame_number < end_frame:
            raise ValueError(f"Camera frame must be positive and before quit: {line}")
        if camera_frames and frame_number <= camera_frames[-1]:
            raise ValueError("Camera frames must be strictly increasing")
        camera_frames.append(frame_number)
        commands.append(f"{frame_number} camera {player} {actor} 0 - {x} {y} {zoom}")
    if camera_frames[0] >= frames[0]:
        raise ValueError("The first camera command must run before the first capture")
    commands.sort(key=lambda line: int(line.split()[0]))
    commands.append(f"{end_frame} quit - - 0")
    return map_path, frames, camera_frames, end_frame, commands, render_from, fixture / "map.ini"


def render(build, output, state, map_path, frames, camera_frames, end_frame, commands, render_from, map_additions, checks):
    configured = build / "CMakeCache.txt"
    source = ROOT.parents[2]
    if f"CMAKE_HOME_DIRECTORY:INTERNAL={source}" not in configured.read_text().splitlines():
        raise ValueError(f"{build} was not configured for {source}")
    runtime = state / "runtime"
    runtime.mkdir(mode=0o700)
    userdata = state / "data/GeneralsX/GeneralsZH"
    userdata.mkdir(parents=True)
    (userdata / "Options.ini").write_text(
        "RetailStaticGameLOD = UltraHigh\nRetailIdealStaticGameLOD = UltraHigh\n"
        "UseShadowVolumes = Yes\nBrightness = 50\nResolution = 1280 720\n"
    )
    map_dir = userdata / "Maps" / map_path.stem.lower()
    map_dir.mkdir(parents=True)
    for asset in map_path.parent.iterdir():
        if asset.is_file():
            shutil.copyfile(asset, map_dir / asset.name.lower())
    if map_additions.is_file():
        with (map_dir / "map.ini").open("ab") as stream:
            stream.write(b"\n" + map_additions.read_bytes())
    map_path = map_dir / map_path.name.lower()
    commands_path = state / "commands.txt"
    commands_path.write_text("\n".join(commands) + "\n")
    env = dict(
        os.environ,
        HOME=str(state),
        XDG_DATA_HOME=str(state / "data"),
        XDG_CONFIG_HOME=str(state / "config"),
        XDG_CACHE_HOME=str(state / "cache"),
        XDG_RUNTIME_DIR=str(runtime),
        DXVK_LOG_PATH=str(state),
        DXVK_STATE_CACHE_PATH=str(state),
        DXVK_LOG_LEVEL="error",
        SDL_VIDEO_DRIVER="x11",
        SDL_AUDIO_DRIVER="dummy",
        LP_NUM_THREADS="3",
        VK_DRIVER_FILES="/usr/share/vulkan/icd.d/lvp_icd.json",
        LD_LIBRARY_PATH=":".join(sorted({str(path.parent) for path in build.rglob("*.so*")})),
        GENERALSX_TEST_COMMANDS=str(commands_path),
        GENERALSX_TRACE_ABILITY_TIMING="1",
        GENERALSX_TRACE_RETAIL_FX_LISTS="1",
        PLAYTEST_FRAMES=" ".join(map(str, frames)),
        PLAYTEST_RENDER_FROM=str(render_from),
    )
    env.update(checks.get("render_env", {}))
    for frame in frames:
        env[f"PLAYTEST_PNG_{frame}"] = str(output / f"frame-{frame:04d}.png")
    argv = [
        "timeout", "--signal=INT", "--kill-after=10", "225", "xvfb-run", "-a",
        "-s", f"-screen 0 {WIDTH}x{HEIGHT}x24",
        "gdb", "-q", "-nx", "-batch", "-return-child-result",
        "-x", str(ROOT / "start.gdb"), "-ex", f"source {ROOT / 'capture.py'}", "-ex", "run",
        "--args", str(build / "GeneralsMD/GeneralsXZH"), "-win", "-xres", str(WIDTH),
        "-yres", str(HEIGHT), "-noFPSLimit", "-noaudio", "-nomusic", "-novideo", "-noshellmap",
        "-mod", str(INSTALL / "aotr"), "-bfme2", str(INSTALL / "bfme2"), "-file", str(map_path),
    ]
    log = output / "session.log"
    with log.open("wb") as stream:
        status = subprocess.run(argv, cwd=INSTALL / "rotwk", env=env, stdout=stream,
                                stderr=subprocess.STDOUT).returncode
    log_text = log.read_text(errors="replace")
    if (status or f"TEST frame {end_frame}: quit" not in log_text
            or "Python Exception" in log_text or "Traceback (most recent call last)" in log_text):
        print("\n".join(log_text.splitlines()[-40:]), file=sys.stderr)
        raise RuntimeError(f"Engine session failed (exit {status}, expected quit frame {end_frame})")
    if env.get("PLAYTEST_PIN_DYNAMIC_LOD") == "1" and "TEST dynamic-lod-pinned VeryHigh skip-mask=0" not in log_text:
        raise RuntimeError("Dynamic LOD was not pinned to VeryHigh with particle skipping disabled")
    for camera_frame in camera_frames:
        if f"TEST frame {camera_frame}: camera at " not in log_text:
            raise RuntimeError(f"Camera command did not run at frame {camera_frame}")
    if "check" in checks:
        checks["check"](log_text)


def make_media(output, frames):
    tile_width, tile_height = CLIP_SIZE
    rows = (len(frames) + COLUMNS - 1) // COLUMNS
    sheet = Image.new("RGB", (tile_width * min(COLUMNS, len(frames)), tile_height * rows))
    clip = []
    for index, frame in enumerate(frames):
        with Image.open(output / f"frame-{frame:04d}.png") as source:
            if source.size != (WIDTH, HEIGHT):
                raise ValueError(f"Unexpected capture size at frame {frame}: {source.size}")
            image = source.convert("RGB").resize(CLIP_SIZE, Image.Resampling.LANCZOS)
        draw = ImageDraw.Draw(image)
        draw.rectangle((8, 8, 92, 31), fill="#111111")
        draw.text((13, 11), f"Frame {frame}", fill="white", font_size=15)
        sheet.paste(image, (index % COLUMNS * tile_width, index // COLUMNS * tile_height))
        clip.append(image)
    sheet.save(output / "contact.png")
    durations = [max(30, round((right - left) * 1000 / 30))
                 for left, right in zip(frames, frames[1:])]
    clip[0].save(output / "animation.gif", save_all=True, append_images=clip[1:],
                 duration=[*durations, 350], loop=0)


def main():
    build = Path(sys.argv[1])
    scenario = read_scenario(sys.argv[2])
    frames, end_frame = scenario[1], scenario[3]
    check_path = ROOT / sys.argv[2] / "check.py"
    checks = runpy.run_path(str(check_path)) if check_path.is_file() else {}
    with tempfile.TemporaryDirectory(prefix="playtest-", dir=build) as temp:
        state = Path(temp)
        output = state / "results"
        output.mkdir()
        render_variant(build, output, state, scenario, checks)
        for name, overrides in checks.get("render_variants", {}).items():
            variant_output = output / name
            variant_output.mkdir()
            variant_state = state / name
            variant_state.mkdir()
            variant_checks = dict(checks, render_env={**checks.get("render_env", {}), **overrides})
            render_variant(build, variant_output, variant_state, scenario, variant_checks)
        (output / "frames.txt").write_text("\n".join(map(str, frames)) + "\n")
        (output / "frame-count.txt").write_text(f"{end_frame}\n")
        with tarfile.open(fileobj=sys.stdout.buffer, mode="w|") as archive:
            for path in sorted(output.iterdir()):
                archive.add(path, arcname=path.name)


def render_variant(build, output, state, scenario, checks):
    render(build, output, state, *scenario, checks)
    frames = scenario[1]
    if "check_captures" in checks:
        checks["check_captures"](output, frames)
    make_media(output, frames)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError) as error:
        sys.exit(f"Playtest: {error}")
