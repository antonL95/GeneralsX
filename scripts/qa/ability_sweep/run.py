import csv
import os
from pathlib import Path
import re
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
FACTION_SECONDS = 2400
SESSION_CASES = 20
STALL_SECONDS = 120
FRAME_STALL_SECONDS = 90
REPEAT_LIMIT = 1000
FRAME_RE = re.compile(r"^TEST ability-heartbeat frame=(\d+)")
RESULT_FIELDS = "animation objects caster ally enemy structure flags recharge ended remaining no_window".split()
TABLE_FIELDS = "faction unit hero command_set button command power verdict problems result".split() + RESULT_FIELDS + ["todo"]


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
        return "passive", ["passive command"]
    if result == "not_shown":
        return "not shown", ["command not shown"]
    if result == "hidden_by_design":
        return "hidden by design", ["command is outside visible palantir slots"]
    if result != "accepted":
        return "rejected", [f"command result: {result}"]
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
    reasons = {"no effect": "no observed effect", "no animation state": "no animation state observed",
               "no recharge": "authored reload did not start", "never ends": "ability state exceeded authored duration"}
    if problems:
        return problems[0], [reasons[problem] for problem in problems]
    recharge = {"n/a:zero_reload": "recharge not required (zero reload)",
                "pending": "recharge pending while effect is active", "yes": "recharge observed",
                "n/a": "no power recharge applies"}.get(values["recharge"], "recharge unknown")
    return "ok", [f"effect observed; {recharge}"]


def kill_engine():
    for entry in Path("/proc").iterdir():
        try:
            if not entry.name.isdigit() or not (entry / "cmdline").read_bytes().split(b"\0", 1)[0].endswith(b"/GeneralsMD/GeneralsXZH"):
                continue
            pid = int(entry.name)
            parent = pid
            while parent > 1 and parent != os.getpid():
                stat = Path(f"/proc/{parent}/stat").read_text()
                parent = int(stat[stat.rfind(")") + 2:].split()[1])
            if parent == os.getpid():
                os.kill(pid, signal.SIGKILL)
        except (OSError, ValueError, IndexError):
            continue


def watch_for_stall(log, finished, stalled, reason):
    size, changed = -1, time.monotonic()
    pos = 0
    max_frame = None
    frame_changed = time.monotonic()
    last_line = ""
    partial = b""
    repeat_count = 0
    while not finished.wait(5):
        now = time.monotonic()
        current = log.stat().st_size if log.exists() else -1
        if current != size:
            size, changed = current, now
        if current > pos:
            try:
                with log.open("rb") as handle:
                    handle.seek(pos)
                    chunk = handle.read(min(current - pos, 1024 * 1024))
                pos += len(chunk)
                lines = (partial + chunk).split(b"\n")
                partial = lines.pop()
                for raw in lines:
                    line = raw.decode(errors="replace").strip()
                    if not line:
                        continue
                    match = FRAME_RE.search(line)
                    if match:
                        frame = int(match.group(1))
                        if max_frame is None or frame > max_frame:
                            max_frame = frame
                            frame_changed = now
                    if line == last_line:
                        repeat_count += 1
                        if repeat_count >= REPEAT_LIMIT:
                            reason.append(f"frame={max_frame} repeat={repeat_count} {line[:180]}")
                            stalled.set()
                            kill_engine()
                            return
                    else:
                        last_line = line
                        repeat_count = 1
            except OSError:
                pass
        if max_frame is not None and now - frame_changed > FRAME_STALL_SECONDS:
            reason.append(f"frame_stalled={max_frame} {last_line[:180]}")
            stalled.set()
            kill_engine()
            return
        if now - changed > STALL_SECONDS:
            reason.append(f"log_stalled {last_line[:180]}")
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
        reason = []
        watcher = threading.Thread(target=watch_for_stall, args=(log, finished, stalled, reason))
        watcher.start()
        try:
            PLAYTEST["render"](build, output, state, map_path, [30], [], end_frame, commands, 28,
                               ROOT / "map.ini", checks, (1280, 720))
        except RuntimeError as error:
            failure = str(error)
        finally:
            finished.set()
            watcher.join()
        timed_out = bool(failure and ("exit 124" in failure or "exit 137" in failure))
        return (log.read_text(errors="replace") if log.exists() else ""), failure, timed_out, (reason[0] if reason else "")


def result_row(case, values):
    verdict, problems = judge(values)
    return [case[key] for key in TABLE_FIELDS[:6]] + [
        values["power"], verdict, ",".join(problems) or "-", values["result"],
        *[values[key] for key in RESULT_FIELDS], case.get("todo", "-")]


def unfinished_row(case, verdict, reason):
    return [case[key] for key in TABLE_FIELDS[:6]] + [
        "-", verdict, verdict, reason, *["-"] * len(RESULT_FIELDS), case.get("todo", "-")]


def run_faction(build, faction, pending, writer):
    deadline = time.monotonic() + FACTION_SECONDS
    while pending and time.monotonic() < deadline:
        batch = pending[:SESSION_CASES]
        log, failure, timed_out, stall_reason = session(build, batch, min(SESSION_SECONDS, int(deadline - time.monotonic()) + 1))
        results = parse(log)
        finished = 0
        for case in batch:
            if case["id"] not in results:
                break
            writer.writerow(result_row(case, results[case["id"]]))
            finished += 1
        print(f"{faction}: {finished}/{len(batch)} cases, {failure or 'completed'}", file=sys.stderr)
        out_of_time = timed_out and not stall_reason and finished > 0
        if finished < len(batch) and not out_of_time:
            last = stall_reason or next((line for line in reversed(log.splitlines()) if line.startswith("TEST ")), "no log line")
            verdict = "hang" if stall_reason or timed_out else "session failed"
            writer.writerow(unfinished_row(batch[finished], verdict, last[:200]))
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
