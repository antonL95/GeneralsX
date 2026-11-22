import re


def check(log):
    for frame, name in ((34, "HSLightningStrike3"), (52, "HSLightningStrike2"),
                        (70, "HSLightningStrike")):
        pattern = rf"^TEST particle-systems frame={frame} name={name} systems=[1-9]\d* particles=[1-9]\d* "
        if not re.search(pattern, log, re.MULTILINE):
            raise RuntimeError(f"Expected emitted lightning points for {name} at frame {frame}")
