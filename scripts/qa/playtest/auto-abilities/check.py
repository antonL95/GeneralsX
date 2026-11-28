import re
import sys

from PIL import Image


def check(log):
    power = "SpecialPowerShieldBubbleGondor"
    if f"TEST auto state frame=25 power={power} ready=1 next=5 dead=0 disabled=0 bubble=0" not in log:
        raise RuntimeError("The shield was not ready before combat")
    if not re.search(
        rf"^TEST auto state frame=(78|130) power={power} ready=0 next=\d+ dead=0 disabled=0 bubble=1$",
        log,
        re.M,
    ):
        raise RuntimeError("The shield did not activate during combat")
    gondor_hud = [line for line in log.splitlines() if line.startswith("TEST hud 25 ")]
    if not gondor_hud or any("Command_GondorShieldBubble " in line for line in gondor_hud):
        raise RuntimeError("Gandalf's HUD must omit Shield Bubble")
    if not re.search(r"^TEST hud 141 slot=3 Command_BladeofWesternessePassive type=\d+ disabled ", log, re.M):
        raise RuntimeError("Pippin's passive Blade of Westernesse must be visible and disabled for input")
    if not re.search(rf"^TEST auto state frame=285 power={power} ready=0 next=\d+ dead=0 disabled=0 bubble=1$", log, re.M):
        raise RuntimeError("Mounted Gandalf did not raise the shield")
    if not re.search(r"^TEST draw frame 285 id \d+ GondorGandalf module ModuleTag_ShieldBubble model gugandalfcrstl ", log, re.M):
        raise RuntimeError("The raised shield did not draw its authored crystal model")


engine_timeout = 900
render_env = {"GENERALSX_TEST_DRAWTRACE": "GondorGandalf", "PLAYTEST_PIN_DYNAMIC_LOD": "1"}


def bright_fraction(image, area, test):
    pixels = image.crop(area).get_flattened_data()
    return sum(test(rgb) for rgb in pixels) / len(pixels)


def check_captures(output, frames):
    buttons = ((328, 588, 348, 608), (328, 635, 348, 655), (298, 671, 318, 691))
    caster = (595, 295, 685, 365)
    for frame in (256, 262, 270, 285):
        with Image.open(output / f"frame-{frame:04d}.png") as capture:
            image = capture.convert("RGB")
        blank = max(bright_fraction(image, area, lambda rgb: min(rgb) >= 245) for area in buttons)
        flat = bright_fraction(image, caster, lambda rgb: min(rgb) >= 200 and max(rgb) - min(rgb) <= 20)
        print(f"Frame {frame}: blank button={blank:.3f}, flat caster={flat:.3f}", file=sys.stderr)
        if blank >= 0.5 or flat >= 0.3:
            raise RuntimeError(
                f"Shield cast during mounting left flat white render state at frame {frame}: "
                f"blank button={blank:.3f}, flat caster={flat:.3f}"
            )
