import csv
import os
from pathlib import Path
import runpy
import signal
import sys
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parent
PLAYTEST = runpy.run_path(str(ROOT.parent / "playtest" / "run.py"))

OWNER = "Player_1"
DUMMIES = "AbilitySweepInfantry,AbilitySweepInfantry,AbilitySweepStructure,PlyrCreeps"
LANES = ((3090, 535), (3150, 690), (2260, 1180), (2520, 1010))
FIRST_FRAME = 44
WATCH_FRAMES = 600
PREPARE_AT = 3
CLICK_AT = 90
SETTLE_FRAMES = 30
SESSION_SECONDS = 600
FACTION_SECONDS = 1800
SESSION_CASES = 30
STALL_SECONDS = 120
RESULT_FIELDS = "animation objects caster ally enemy structure flags recharge ended remaining no_window".split()
TABLE_FIELDS = "faction unit hero command_set button command power verdict problems result".split() + RESULT_FIELDS


def schedule(cases):
    commands = []
    frame = FIRST_FRAME
    for case in cases:
        x, y = LANES[case["id"] % len(LANES)]
        name = case["unit"]

        def line(at, action, argument="-", location=""):
            commands.append(f"{at} {action} {OWNER} {name} {case['id']} {argument}{location}")

        line(frame, "ability-setup", DUMMIES, f" {x} {y}")
        line(frame + PREPARE_AT, "ability-prepare", case["button"])
        line(frame + CLICK_AT, "ability-click", case["button"])
        for at in range(frame + CLICK_AT + 1, frame + CLICK_AT + WATCH_FRAMES):
            line(at, "ability-watch")
        line(frame + CLICK_AT + WATCH_FRAMES, "ability-end")
        frame += CLICK_AT + WATCH_FRAMES + SETTLE_FRAMES
    commands.append(f"{frame} quit - - 0")
    return commands, frame


def parse(log):
    results = {}
    for line in log.splitlines():
        if line.startswith("TEST ability-sweep case="):
            values = dict(field.split("=", 1) for field in line.removeprefix("TEST ability-sweep ").split("\t"))
            results[int(values["case"])] = values
    return results


def judge(values):
    result = values["result"]
    if result == "passive":
        return "passive", []
    if result == "not_shown":
        return "not shown", []
    if result != "accepted":
        return "rejected", ["rejected"]
    reactions = [values["caster"].replace("moved;", ""), values["ally"], values["enemy"], values["structure"]]
    effect = (values["objects"] != "-" or any(text not in ("-", "") for text in reactions)
              or "weaponset" in values["flags"] or "commandset" in values["flags"])
    problems = []
    if not effect:
        problems.append("no effect")
    if values["animation"] == "-":
        problems.append("no animation state")
    if values["recharge"] == "no":
        problems.append("no recharge")
    if values["ended"] == "never":
        problems.append("never ends")
    return problems[0] if problems else "ok", problems


def kill_engine():
    for entry in Path("/proc").iterdir():
        try:
            if entry.name.isdigit() and b"GeneralsMD/GeneralsXZH" in (entry / "cmdline").read_bytes() and int(entry.name) != os.getpid():
                os.kill(int(entry.name), signal.SIGKILL)
        except OSError:
            continue


def watch_for_stall(log, finished, stalled):
    size, changed = -1, time.monotonic()
    while not finished.wait(5):
        current = log.stat().st_size if log.exists() else -1
        if current != size:
            size, changed = current, time.monotonic()
        elif time.monotonic() - changed > STALL_SECONDS:
            stalled.set()
            kill_engine()
            return


def session(build, cases, seconds):
    map_path = PLAYTEST["INSTALL"] / (ROOT / "map.txt").read_text().strip()
    commands, end_frame = schedule(cases)
    checks = {"engine_timeout": seconds, "render_env": {"PLAYTEST_RENDER_WINDOWS": "28:31"}}
    with tempfile.TemporaryDirectory(prefix="ability-sweep-", dir=build) as temp:
        state = Path(temp)
        output = state / "results"
        output.mkdir()
        failure = None
        log = output / "session.log"
        finished, stalled = threading.Event(), threading.Event()
        watcher = threading.Thread(target=watch_for_stall, args=(log, finished, stalled))
        watcher.start()
        try:
            PLAYTEST["render"](build, output, state, map_path, [30], [], end_frame, commands, 28,
                               ROOT / "map.ini", checks, (1280, 720))
        except RuntimeError as error:
            failure = str(error)
        finally:
            finished.set()
            watcher.join()
        hung = stalled.is_set() or bool(failure and ("exit 124" in failure or "exit 137" in failure))
        return (log.read_text(errors="replace") if log.exists() else ""), failure, hung


def result_row(case, values):
    verdict, problems = judge(values)
    return [case[key] for key in TABLE_FIELDS[:6]] + [
        values["power"], verdict, ",".join(problems) or "-", values["result"],
        *[values[key] for key in RESULT_FIELDS]]


def unfinished_row(case, verdict, reason):
    return [case[key] for key in TABLE_FIELDS[:6]] + ["-", verdict, verdict, reason, *["-"] * len(RESULT_FIELDS)]


def run_faction(build, faction, pending, writer):
    deadline = time.monotonic() + FACTION_SECONDS
    while pending and time.monotonic() < deadline:
        batch = pending[:SESSION_CASES]
        log, failure, hung = session(build, batch, min(SESSION_SECONDS, int(deadline - time.monotonic()) + 1))
        results = parse(log)
        finished = 0
        for case in batch:
            if case["id"] not in results:
                break
            writer.writerow(result_row(case, results[case["id"]]))
            finished += 1
        print(f"{faction}: {finished}/{len(batch)} cases, {failure or 'completed'}", file=sys.stderr)
        if finished < len(batch):
            last = next((line for line in reversed(log.splitlines()) if line.startswith("TEST ")), "no log line")
            writer.writerow(unfinished_row(batch[finished], "hang" if hung else "session failed", last[:200]))
            print("\n".join(log.splitlines()[-30:]), file=sys.stderr)
            finished += 1
        pending = pending[finished:]
        sys.stdout.flush()
    for case in pending:
        writer.writerow(unfinished_row(case, "not run", "faction time limit"))


def main():
    cases = list(csv.DictReader(sys.stdin, delimiter="\t"))
    for index, case in enumerate(cases, 1):
        case["id"] = index
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    if sys.argv[1] == "--from-log":
        results = parse(Path(sys.argv[2]).read_text(errors="replace"))
        writer.writerows(result_row(case, results[case["id"]]) for case in cases if case["id"] in results)
        return
    build = Path(sys.argv[1])
    writer.writerow(TABLE_FIELDS)
    for faction in dict.fromkeys(case["faction"] for case in cases):
        run_faction(build, faction, [case for case in cases if case["faction"] == faction], writer)

if __name__ == "__main__":
    main()
