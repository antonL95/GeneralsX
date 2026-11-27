check() {
	if covers 236; then
		grep -E '^TEST pkg27 frame=236 object=GarrisonDockLadder .* deployed=0 packing=0 unpacking=0 .* approaching=1 .* accepted=1 ' "$log" || return 1
	fi
	if covers 330; then
		grep -E '^TEST pkg27 landing frame=330 bone=1 .* legal=1 ' "$log" || return 1
		grep -E '^TEST siege-portal frame=330 object=GarrisonDockLadder active=1 docked=1 .* waypoints=4 ' "$log" || return 1
	fi
	if covers 730; then
		grep -E '^TEST siege-portal frame=730 object=GarrisonDockHorde ord=0 layer=15 wall=1 .* contained=0 climbing=0 ' "$log" || return 1
	fi
}
