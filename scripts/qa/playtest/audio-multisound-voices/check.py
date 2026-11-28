import re

TRACE = re.compile(
    r"AUDIO TRACE frame=\d+ kind=(\S+) owner=(\d+) handle=(\d+) event=(AudioVoice\S+)"
)


def check(log):
    events = [(kind, owner, event) for kind, owner, _, event in TRACE.findall(log)]
    starts = [(owner, event) for kind, owner, event in events if kind == "start"]
    suppressed = [(owner, event) for kind, owner, event in events if kind == "voice-suppress"]
    interrupted = [(owner, event) for kind, owner, event in events if kind == "interrupt"]

    child_starts = [(o, e) for o, e in starts if e in ("AudioVoiceChildA", "AudioVoiceChildB")]
    if len(child_starts) != 2 or len({o for o, _ in child_starts}) != 1:
        raise RuntimeError(f"Expected both Multisound voice children on one owner to start: {events}")
    owner = child_starts[0][0]
    if any(e in ("AudioVoiceChildA", "AudioVoiceChildB") for _, e in suppressed):
        raise RuntimeError(f"Multisound voice child was suppressed: {events}")
    if any(e == "AudioVoiceSeparateNoInterrupt" for _, e in starts):
        raise RuntimeError(f"Separate non-interrupt voice should not start while Multisound children play: {events}")
    if (owner, "AudioVoiceSeparateInterrupt") not in starts:
        raise RuntimeError(f"Expected interrupting voice to start: {events}")
    interrupted_children = {e for o, e in interrupted if o == owner}
    if interrupted_children != {"AudioVoiceChildA", "AudioVoiceChildB"}:
        raise RuntimeError(f"Expected both Multisound children to be interrupted: {events}")

    pair_starts = [(o, e) for o, e in starts if e == "AudioVoicePairMove"]
    if len(pair_starts) != 1:
        raise RuntimeError(f"Expected first separate voice on pair owner to start: {events}")
    pair_owner = pair_starts[0][0]
    if (pair_owner, "AudioVoicePairAttack") not in suppressed or any(
        e == "AudioVoicePairAttack" for _, e in starts
    ):
        raise RuntimeError(f"Expected second separate voice on pair owner to hit voice-suppress: {events}")
