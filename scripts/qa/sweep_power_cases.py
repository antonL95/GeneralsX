"""Replay and classify the power cases attributed by the second High Pass sweep."""

import argparse
from collections import defaultdict
import csv
from pathlib import Path
import sys


CAUSES = {
    "Passive display button counted as cast failure",
    "Weapon mode update has no cast owner",
    "Capture button has no authored power binding",
    "Other active power buttons lack matching authored bindings",
    "Audit skips CloudBreakSpecialPower",
    "Audit skips PlayerUpgradeSpecialPower",
    "Audit skips TaintSpecialPower",
    "Audit skips UntamedAllegianceSpecialPower",
    "Weather FX command is bound outside the spellbook",
}

DISPOSITIONS = {
    "passive_display": ("passive", "nonpressable_display"),
    "passive_upgrade_granted": ("ok", "science_granted_player_upgrade"),
    "passive_upgrade_missing": ("broken", "player_upgrade_missing_after_science"),
    "capture_unbound": ("blocked", "capture_requires_authored_module"),
    "not_a_spellbook_cast": ("blocked", "command_is_outside_spellbook_dispatch"),
    "power_not_ready": ("blocked", "bound_power_was_not_ready"),
}


def read_tsv(path):
    with path.open(encoding="utf-8", newline="") as source:
        return list(csv.DictReader(source, delimiter="\t"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "report"))
    parser.add_argument("baseline", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--side")
    args = parser.parse_args()

    sys.path.insert(0, str(Path("tools").resolve()))
    from sweep_inventory import FIELDS, Graph, commands, sweep_tree
    from sweep_results import collect_log, outcome, tsv

    cases = defaultdict(dict)
    for row in read_tsv(args.baseline / "cause-cases.tsv"):
        if row["cause"] in CAUSES and (args.side is None or row["faction"] == args.side):
            cases[row["faction"]][row["id"]] = row["cause"]
    args.output.mkdir(parents=True, exist_ok=True)
    if args.action == "prepare":
        graph = Graph(list(sweep_tree(Path("game/ini/aotr-loose/data/ini"))), {})
        for side, causes in sorted(cases.items()):
            inventory = read_tsv(args.baseline / f"{side}.tsv")
            rows = [row for row in inventory if row["id"] in causes]
            if len(rows) != len(causes):
                raise ValueError(f"{side} inventory is missing scoped case IDs")
            indexed_rows = [(int(row["id"]), tuple(row[key] for key in FIELDS[1:])) for row in rows]
            (args.output / f"{side}.commands").write_text(commands(graph, indexed_rows, True, False), encoding="utf-8")
            tsv(args.output / f"{side}.inventory.tsv", FIELDS, (tuple(row[key] for key in FIELDS) for row in rows))
        return

    results = []
    for side, causes in sorted(cases.items()):
        traces, _, _, quit_frame, fatal, _ = collect_log(args.output / f"{side}.log")
        for row in read_tsv(args.output / f"{side}.inventory.tsv"):
            trace = traces.get(row["id"], {})
            starts = trace.get("start", [])
            disposition = DISPOSITIONS.get(starts[-1][2]["reason"]) if starts else None
            if disposition:
                result, reason = disposition
                evidence = starts[-1:]
            else:
                result, reason, evidence = outcome(row, trace, fatal)
            results.append((side, row["id"], row["source"], row["button"], row["item"], causes[row["id"]],
                            result, reason, quit_frame or "", " | ".join(record[1] for record in evidence)))

    tsv(args.output / "power-cases.tsv",
        ("faction", "id", "source", "button", "item", "cause", "result", "reason", "quit_frame", "evidence"), results)


if __name__ == "__main__":
    main()
