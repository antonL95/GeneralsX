import re


def check(log):
    if "TEST list 28 Player_1 MultiplayerBeacon " not in log:
        raise RuntimeError("The beacon placement command did not create a player beacon")
    if not re.search(r"AUDIO TRACE frame=\d+ kind=sound-queue owner=\d+ handle=\d+ event=Gui_BeaconPlaced\b", log):
        raise RuntimeError("The authored beacon placement sound was not queued")
