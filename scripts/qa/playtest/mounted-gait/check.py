from pathlib import Path
from runpy import run_path

render_env = {"GENERALSX_TEST_DRAWTRACE": "GondorGandalf", "GENERALSX_TEST_GAITTRACE": "1"}

common = run_path(str(Path(__file__).resolve().parent.parent / "horse-gait" / "check.py"))


def check(log):
    tracks = common["tracks_for"](log, "GondorGandalf")
    common["check_turns"](tracks, "GondorGandalf", "gugdfhrs_skl.gugdfhrs_", 5, 1.0)
    if not any(draw["clip"] == "gugdfhrs_skl.gugdfhrs_runa" and draw["distance"] == "0.000" and draw["rate"] == "1.00000"
               for frames in tracks.values() for draw in frames.values()):
        raise RuntimeError("Expected mounted Gandalf to use the authored fixed-rate running clip")
