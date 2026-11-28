check() {
	covers 670 || return 0
	awk '
		function field(name,    i, parts) {
			for (i = 4; i <= NF; ++i) {
				split($i, parts, "=")
				if (parts[1] == name) return parts[2]
			}
			return ""
		}
		$1 == "TEST" && $2 == "ship-unload-probe" && field("label") == "ship0-after-board" {
			ship0_boarded = field("contain") + 0
			ship0_bpos = field("pos")
		}
		$1 == "TEST" && $2 == "ship-unload-probe" && field("label") == "ship0-before-evac" {
			ship0_pre_uw = field("uw") + 0
			ship0_pre_cell = field("cell") + 0
			ship0_pre_pos = field("pos")
		}
		$1 == "TEST" && $2 == "ship-unload-probe" && field("label") == "ship0-after-evac" {
			ship0_after = 1
			ship0_post_contain = field("contain") + 0
			ship0_post_uw = field("uw") + 0
		}
		$1 == "TEST" && $2 == "ship-unload-probe" && field("label") == "horde0-after-evac" {
			horde0_after = 1
			horde0_contained_by = field("contained_by") + 0
			horde0_cell = field("cell") + 0
			horde0_uw = field("uw") + 0
			horde0_members = field("members") + 0
			horde0_land = field("land") + 0
			horde0_water = field("water") + 0
		}
		$1 == "TEST" && $2 == "ship-unload-probe" && field("label") == "ship1-after-board" {
			ship1_boarded = field("contain") + 0
			ship1_bpos = field("pos")
		}
		$1 == "TEST" && $2 == "ship-unload-probe" && field("label") == "ship1-before-exit" {
			ship1_pre_uw = field("uw") + 0
			ship1_pre_cell = field("cell") + 0
			ship1_pre_pos = field("pos")
		}
		$1 == "TEST" && $2 == "ship-unload-probe" && field("label") == "ship1-after-exit" {
			ship1_after = 1
			ship1_post_contain = field("contain") + 0
			ship1_post_uw = field("uw") + 0
		}
		$1 == "TEST" && $2 == "ship-unload-probe" && field("label") == "horde1-after-exit" {
			horde1_after = 1
			horde1_contained_by = field("contained_by") + 0
			horde1_cell = field("cell") + 0
			horde1_uw = field("uw") + 0
			horde1_members = field("members") + 0
			horde1_land = field("land") + 0
			horde1_water = field("water") + 0
		}
		END {
			if (ship0_boarded != 1) { printf "ship0 did not board horde0 (contain=%d)\n", ship0_boarded + 0; exit 1 }
			if (ship0_pre_uw != 1 || ship0_pre_cell != 7 || ship0_pre_pos == ship0_bpos) {
				printf "ship0 did not move to deep water before evac (uw=%d cell=%d bpos=%s pos=%s)\n",
					ship0_pre_uw + 0, ship0_pre_cell + 0, ship0_bpos, ship0_pre_pos
				exit 1
			}
			if (!ship0_after || ship0_post_contain != 0 || ship0_post_uw != 1) {
				printf "ship0 did not unload via evacuate and stay in water (contain=%d uw=%d)\n", ship0_post_contain + 0, ship0_post_uw + 0
				exit 1
			}
			if (!horde0_after || horde0_contained_by != 0 || horde0_cell != 0 || horde0_uw != 0 || horde0_land < horde0_members || horde0_water != 0) {
				printf "horde0 did not land cleanly on dry shore (contained_by=%d cell=%d uw=%d land=%d/%d water=%d)\n",
					horde0_contained_by + 0, horde0_cell + 0, horde0_uw + 0, horde0_land + 0, horde0_members + 0, horde0_water + 0
				exit 1
			}
			if (ship1_boarded != 1) { printf "ship1 did not board horde1 (contain=%d)\n", ship1_boarded + 0; exit 1 }
			if (ship1_pre_uw != 1 || ship1_pre_cell != 7 || ship1_pre_pos == ship1_bpos) {
				printf "ship1 did not move to deep water before exit (uw=%d cell=%d bpos=%s pos=%s)\n",
					ship1_pre_uw + 0, ship1_pre_cell + 0, ship1_bpos, ship1_pre_pos
				exit 1
			}
			if (!ship1_after || ship1_post_contain != 0 || ship1_post_uw != 1) {
				printf "ship1 did not unload via exit button and stay in water (contain=%d uw=%d)\n", ship1_post_contain + 0, ship1_post_uw + 0
				exit 1
			}
			if (!horde1_after || horde1_contained_by != 0 || horde1_cell != 0 || horde1_uw != 0 || horde1_land < horde1_members || horde1_water != 0) {
				printf "horde1 did not land cleanly on dry shore (contained_by=%d cell=%d uw=%d land=%d/%d water=%d)\n",
					horde1_contained_by + 0, horde1_cell + 0, horde1_uw + 0, horde1_land + 0, horde1_members + 0, horde1_water + 0
				exit 1
			}
		}
	' "$log"
}
