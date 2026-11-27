expect_deployment() {
	if covers "$1" && ! grep -E "^TEST horde-climb frame=$1 object=$2 $3" "$log"; then
		printf 'Missing deployment state for %s at frame %s in %s\n' "$2" "$1" "$phase" >&2
		return 1
	fi
}

check() {
	expect_deployment 216 SiegeHordeDeployBundle '.* members=5 wallMembers=0 sameHorde=5' || return 1
	if covers 244; then
		expect 'TEST frame 244: no SiegeHordeDeployBundle 0 of Player_1' || return 1
	fi
	expect_deployment 244 SiegeHordeDeployInfantry '.* wall=0 .* members=4 wallMembers=0 sameHorde=4' || return 1
	expect_deployment 350 SiegeHordeDeployLadder 'deployed=1 .* release=1' || return 1
	expect_deployment 430 SiegeHordeDeployInfantry '.* wall=1 .* contained=0 .* members=4 wallMembers=4 sameHorde=4 held=0'
}
