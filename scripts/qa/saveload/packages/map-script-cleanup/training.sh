check() {
    if grep -aE '^BFME unimplemented script action (CREATE_REINFORCEMENT_TEAM_AT_UNIT_POSITION|GIVE_PLAYER_UPGRADE|NAMED_BASE_UNPACK_FREE|SET_UNIT_REFERENCE)$|^BFME unimplemented script condition (PLAYER_HAS_NUM_UNITS_WITH_UPGRADE|PLAYER_HAS_N_OR_FEWER_BASES)$|^BFME unsupported signature of script action TEAM_FORCE_EMOTION$' "$log"; then
        return 1
    fi
}
