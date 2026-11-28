import sys


def parse_samples(log):
    rows = []
    for line in log.splitlines():
        if not line.startswith("TEST wading-sample "):
            continue
        parts = line.split()
        frame = int(parts[2])
        template = parts[3]
        fields = dict(item.split("=", 1) for item in parts[4:])
        fields["frame"] = frame
        fields["template"] = template
        rows.append(fields)
    return rows


def check(log):
    rows = parse_samples(log)
    infantry = [r for r in rows if r["template"] == "GondorFighterHorde"]
    cavalry = [r for r in rows if r["template"] == "GondorCavalry"]

    if not infantry or not cavalry:
        raise RuntimeError(f"Missing wading samples: infantry={len(infantry)} cavalry={len(cavalry)}")

    inf_land_start = [r for r in infantry if r["frame"] == 20]
    inf_water = [r for r in infantry if 100 <= r["frame"] <= 140]
    inf_land_end = [r for r in infantry if r["frame"] == 285]
    if not inf_land_start or int(inf_land_start[0]["membersWade"]) != 0:
        raise RuntimeError(f"Infantry horde wading on land at start: {inf_land_start}")
    if not any(int(r["membersWade"]) > 0 and int(r["particles"]) > 0 for r in inf_water):
        raise RuntimeError(f"Infantry horde never set WADING with particles in water: {inf_water}")
    if not inf_land_end or int(inf_land_end[0]["membersWade"]) != 0:
        raise RuntimeError(f"Infantry horde did not clear WADING on return to land: {inf_land_end}")

    cav_walker = [r for r in cavalry if float(r["depth"]) <= 5.0]
    cav_deep = [r for r in cavalry if float(r["depth"]) > 5.0]

    cav_start = [r for r in cav_walker if r["frame"] == 20]
    cav_water = [r for r in cav_walker if 100 <= r["frame"] <= 140]
    cav_end = [r for r in cav_walker if r["frame"] == 285]
    if not cav_start or int(cav_start[0]["wade"]) != 0 or int(cav_start[0]["swim"]) != 0:
        raise RuntimeError(f"Cavalry wading on land at start: {cav_start}")
    if not any(int(r["wade"]) == 1 and int(r["swim"]) == 0 and int(r["particles"]) > 0 for r in cav_water):
        raise RuntimeError(f"Cavalry never set WADING with particles in shallow water: {cav_water}")
    if not cav_end or int(cav_end[0]["wade"]) != 0 or int(cav_end[0]["swim"]) != 0:
        raise RuntimeError(f"Cavalry did not clear WADING on return to land: {cav_end}")

    if not any(int(r["wade"]) == 1 and int(r["swim"]) == 1 and int(r["particles"]) > 0 for r in cav_deep):
        raise RuntimeError(f"Deep-water actor never set WADING+SWIMMING with particles: {cav_deep}")

    print(
        f"Wading verified: infantry max membersWade={max(int(r['membersWade']) for r in inf_water)}, "
        f"cavalry shallow max depth={max(float(r['depth']) for r in cav_water):.2f}, "
        f"deep depth={max(float(r['depth']) for r in cav_deep):.2f}",
        file=sys.stderr,
    )
