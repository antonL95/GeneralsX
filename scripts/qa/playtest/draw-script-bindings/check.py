import re

render_env = {"GENERALSX_TEST_DRAWTRACE": "DrawBindingProbe"}


def check(log):
    failures = []
    if not re.search(r"^AUDIO TRACE frame=\d+ kind=sound-queue .* event=DrawBindingProbeSound$", log, re.MULTILINE):
        failures.append("draw script sound was not queued")
    marker = re.findall(
        r"^TEST draw-visibility frame (\d+) id \d+ DrawBindingProbe module ModuleTag_DrawBindingMarker hidden ([01])$",
        log, re.MULTILINE | re.IGNORECASE,
    )
    if not any(42 <= int(frame) < 60 and hidden == "1" for frame, hidden in marker):
        failures.append("marker did not hide")
    if not any(42 <= int(frame) < 60 and hidden == "1" for frame, hidden in marker) or not any(
        62 <= int(frame) < 78 and hidden == "0" for frame, hidden in marker
    ):
        failures.append("marker did not show again")
    clips = {
        clip.lower() for clip, flags in re.findall(
            r"^TEST draw frame \d+ id \d+ DrawBindingProbe module ModuleTag_DrawBindings"
            r" model \S+ clip (\S+) at .* flags (\S*) subobjects",
            log, re.MULTILINE | re.IGNORECASE,
        ) if "FIRING_OR_PREATTACK_A" in flags.split(",") and clip.lower().startswith("gunsoldr_atk")
    }
    if clips != {"gunsoldr_atkb"}:
        failures.append(f"bearing selected unexpected clips: {sorted(clips)}")
    if failures:
        raise RuntimeError("; ".join(failures))
