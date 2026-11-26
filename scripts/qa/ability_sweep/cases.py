import csv
from collections import defaultdict
from pathlib import Path
import sys

APP = Path("/app")
sys.path.insert(0, str(APP / "tools"))

from castle_ini import base_path
from sweep_inventory import Graph, buttons, collect, command_sets, fields, sweep_tree

ABILITY_COMMANDS = {
    "SPECIAL_POWER", "SPECIAL_POWER_TOGGLE", "SPECIAL_POWER_CONSTRUCT", "FIRE_WEAPON", "TOGGLE_WEAPONSET",
    "TOGGLE_WEAPON", "SWITCH_WEAPON", "ONE_RING", "BLOODTHIRSTY", "WAKE_AUTO_PICKUP",
}
CASE_FIELDS = "faction unit hero command_set button command options".split()


def main():
    graph = Graph(list(sweep_tree(APP / "game/ini/aotr-loose/data/ini")), {})
    members = defaultdict(list)
    with (APP / "game/analysis/castle-bases/base-castles.tsv").open() as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            members[base_path(row["path"])].append(row["template"])
    rosters = {fields(roster).get("side", "").casefold(): roster for roster in graph.index["PlayerTemplate"].values()
               if fields(roster).get("playableside", "").casefold() == "yes"}
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    writer.writerow(CASE_FIELDS)
    for side in sys.argv[1:]:
        roster = rosters[side.casefold()]
        player = fields(roster)
        initial = set(player.get("initialupgrades", "").lower().split())
        heroes = player.get("buildableheroesmp", "").split() + player.get("buildableringheroesmp", "").split()
        hero_names = {name.casefold() for name in heroes}
        units = [row[5] for row in collect(graph, roster, members) if row[0] == "unit"]
        seen = set()
        kits = set()
        for unit in [*heroes, *units]:
            if unit.casefold() in seen or unit.casefold() not in graph.objects:
                continue
            seen.add(unit.casefold())
            command_set = next(command_sets(graph, unit, initial), None)
            if not command_set:
                continue
            abilities = {}
            for _, button, data in buttons(graph, command_set):
                command = data.get("command", "").upper()
                if command in ABILITY_COMMANDS:
                    abilities.setdefault(button.name.casefold(), (button.name, command, data.get("options", "")))
            hero = unit.casefold() in hero_names
            if not hero and frozenset(abilities) in kits:
                continue
            kits.add(frozenset(abilities))
            for button, command, options in abilities.values():
                writer.writerow((side, graph.objects[unit.casefold()].name, int(hero), command_set,
                                 button, command, options))


if __name__ == "__main__":
    main()
