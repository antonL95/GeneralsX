expect_state() {
    grep -F "$1" "$log" || {
        printf "Missing in %s: %s\n" "$phase" "$1" >&2
        return 1
    }
}

check() {
    if covers 205; then
        expect "TEST pkg31-enter-check frame=205 label=tower-empty-ok player=1 ai=1"
        expect "TEST pkg31-enter-check frame=205 label=tower-cav-reject player=0 ai=0"
        expect "TEST pkg31-enter-check frame=205 label=tower-enemy-reject player=0 ai=0"
        expect "TEST pkg31-enter-check frame=205 label=keep-inf-ok player=1 ai=1"
        expect "TEST pkg31-enter-check frame=205 label=keep-siege-reject player=0 ai=0"
        expect "TEST pkg31-enter-check frame=205 label=keep-enemy-reject player=0 ai=0"
        expect "TEST pkg31-enter-check frame=205 label=neutral-tower-ok player=1 ai=1"
    fi

    if covers 242; then
        expect_state "TEST pkg31-container frame=242 label=tower-loaded-pre count=1 cap=1 "
        expect_state "TEST pkg31-unit frame=242 label=horde-loaded-pre contained=1 unsel=1 unatk=0 canatk=1 encl=1 mem_unsel=1 mem_encl=1 "
        expect "TEST pkg31-enter-check frame=242 label=tower-full-reject player=0 ai=0"
        expect_state "TEST pkg31-container frame=242 label=keep-loaded-pre count=1 cap=10 "
        expect_state "TEST pkg31-unit frame=242 label=fighter-loaded-pre contained=1 unsel=1 unatk=0 canatk=1 encl=0 "
        expect_state "TEST pkg31-container frame=242 label=neutral-captured-pre count=1 cap=1 members=15 owner=Player_1 "
        expect_state "TEST pkg31-container frame=242 label=mumakil-initial-pre count=1 cap=1 "
        expect "TEST pkg31-enter-check frame=242 label=mumakil-filter-reject player=0 ai=0"
        expect "TEST pkg31-enter-check frame=242 label=mumakil-full-reject player=0 ai=0"
        expect_state "TEST pkg31-unit frame=242 label=mumakil-passenger-pre contained=1 unsel=1 unatk=1 canatk=1 encl=0 "
    fi

    if covers 248; then
        expect_state "TEST pkg31-container frame=248 label=tower-loaded-post count=1 cap=1 "
        expect_state "TEST pkg31-unit frame=248 label=horde-loaded-post contained=1 unsel=1 unatk=0 canatk=1 encl=1 mem_unsel=1 mem_encl=1 "
        expect_state "TEST pkg31-container frame=248 label=keep-loaded-post count=1 cap=10 "
        expect_state "TEST pkg31-unit frame=248 label=fighter-loaded-post contained=1 unsel=1 unatk=0 canatk=1 encl=0 "
        expect_state "TEST pkg31-container frame=248 label=mumakil-initial-post count=1 cap=1 "
        expect_state "TEST pkg31-unit frame=248 label=mumakil-passenger-post contained=1 unsel=1 unatk=1 canatk=1 encl=0 "
    fi

    if covers 252; then
        expect "TEST pkg31-impact frame=252 weapon=MordorCatapultRock"
    fi
    if covers 258; then
        expect_state "TEST pkg31-container frame=258 label=tower-after-control-hit count=1 cap=1 "
    fi
    if covers 260; then
        expect "TEST pkg31-impact frame=260 weapon=MordorCatapultHumanHeads"
    fi
    if covers 270; then
        expect_state "TEST pkg31-container frame=270 label=tower-after-evac-hit count=0 cap=1 "
        expect_state "TEST pkg31-unit frame=270 label=horde-after-evac-hit contained=0 unsel=0 unatk=0 canatk=0 encl=0 mem_unsel=1 mem_encl=0 "
    fi

    if covers 275; then
        expect "TEST pkg31-exit-button frame=275 button=Command_ExitGarrison"
    fi
    if covers 285; then
        expect_state "TEST pkg31-container frame=285 label=keep-after-evac count=0 cap=10 "
        expect_state "TEST pkg31-unit frame=285 label=fighter-after-evac contained=0 unsel=0 unatk=0 canatk=0 encl=0 "
        expect_state "TEST pkg31-container frame=285 label=neutral-reverted count=0 cap=1 members=-1 owner=PlyrCivilian "
    fi

    if covers 248; then
        expect_state "TEST pkg31-container frame=248 label=mumakil-initial-post count=1 cap=1 members=6 "
    fi
    if covers 295; then
        expect_state "TEST pkg31-container frame=295 label=mumakil-after-half-dmg count=1 cap=1 members=4 "
    fi

    if covers 306; then
        expect_state "TEST pkg31-unit frame=306 label=rider-after-mumakil-death contained=0 unsel=0 unatk=0 canatk=0 encl=0 mem_unsel=1 mem_encl=0 members=1 dead=0"
    fi

    if covers 400; then
        expect_state "TEST pkg31-container frame=400 label=tower-loaded-before-death count=1 cap=1 "
    fi
    if covers 408; then
        expect_state "TEST pkg31-unit frame=408 label=horde-after-tower-death contained=0 unsel=0 unatk=0 canatk=0 encl=0 mem_unsel=1 mem_encl=0 "
        awk '
            /TEST pkg31-unit frame=408 label=horde-after-tower-death / {
                for (i = 1; i <= NF; i++) {
                    if ($i ~ /^members=/) { sub(/^members=/, "", $i); members = $i + 0 }
                    if ($i ~ /^dead=/) { sub(/^dead=/, "", $i); dead = $i + 0 }
                }
            }
            END {
                if (members <= 0 || dead != 0) {
                    printf("pkg31-transport: expected tower death to eject living horde (members=%d dead=%d)\n", members, dead) > "/dev/stderr"
                    exit 1
                }
            }
        ' "$log" || return 1
    fi
}
