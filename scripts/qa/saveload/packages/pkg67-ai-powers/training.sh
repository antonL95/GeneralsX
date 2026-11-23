check() {
    awk '
        /^TEST eagle frame=/ && / victim=(Pkg67[^ ]*|HaradNafaratiMarauderHorde|HaradCaranLambar|CreateAHero) / {
            split($3, frame, "=")
            match($0, / victim=[^ ]+/)
            victim = substr($0, RSTART, RLENGTH)
            if (!seen[victim]++)
                printf "Package 67 intersects the eagle attack at frame %s:%s\n", frame[2], victim
            failed = 1
        }
        END { exit failed }
    ' "$log" || return 1
    if covers 111; then
        expect 'PKG67 binding 111 ModuleTag_Pkg67UnresolvedAI ai=1 name=Pkg67UndefinedButton catalog=0 power=- match=0' || return 1
    fi
    if covers 114; then
        expect 'PKG67 binding 114 BlackenedBloodAI ai=1 name=Command_NafaratiMarauderBlackenedBlood catalog=0 power=- match=0' || return 1
        expect 'PKG67 binding 114 EnsorcelledRageAI ai=1 name=SpecialAbilityCaranLambarPull catalog=0 power=- match=0' || return 1
        expect 'PKG67 binding 114 CrippleStrikeMeleeHotWL3AI ai=1 name=Command_CreateAHero_HotW_CrippleStrikeMelee_Level3 catalog=0 power=- match=0' || return 1
        expect 'PKG67 binding 114 CrippleStrikeMeleeHotWL4AI ai=1 name=Command_CreateAHero_HotW_CrippleStrikeMelee_Level4 catalog=0 power=- match=0' || return 1
        expect 'PKG67 binding 114 CrippleStrikeMeleeSoSL3AI ai=1 name=Command_CreateAHero_SoS_CrippleStrikeMelee_Level3 catalog=0 power=- match=0' || return 1
        expect 'PKG67 binding 114 CrippleStrikeMeleeSoSL4AI ai=1 name=Command_CreateAHero_SoS_CrippleStrikeMelee_Level4 catalog=0 power=- match=0' || return 1
    fi
    if covers 185; then
        expect 'PKG67 power 185 Pkg67HotWCaster Pkg67HotW3Power rank=2 ready=1' || return 1
        expect 'PKG67 power 185 Pkg67HotWCaster Pkg67HotW4Power rank=2 ready=1' || return 1
        expect 'PKG67 power 185 Pkg67SoSCaster Pkg67SoS3Power rank=2 ready=1' || return 1
        expect 'PKG67 power 185 Pkg67SoSCaster Pkg67SoS4Power rank=2 ready=1' || return 1
    fi
    if covers 243; then
        expect 'PKG67 power 243 Pkg67BindingCaster Pkg67BindingPower rank=1 ready=1' || return 1
        expect 'PKG67 power 243 Pkg67HotWCaster Pkg67HotW3Power rank=3 ready=1' || return 1
        expect 'PKG67 power 243 Pkg67HotWCaster Pkg67HotW4Power rank=3 ready=1' || return 1
        expect 'PKG67 power 243 Pkg67SoSCaster Pkg67SoS3Power rank=3 ready=1' || return 1
        expect 'PKG67 power 243 Pkg67SoSCaster Pkg67SoS4Power rank=3 ready=1' || return 1
        expect 'PKG67 power 243 Pkg67PoisonCaster Pkg67PoisonPower rank=1 ready=1' || return 1
        expect 'PKG67 power 243 Pkg67SelfBuffCaster Pkg67SelfBuffPower rank=1 ready=1' || return 1
    fi
    if covers 312; then
        expect 'PKG67 power 312 Pkg67BindingCaster Pkg67BindingPower rank=1 ready=0' || return 1
        expect 'PKG67 power 312 Pkg67UnresolvedCaster Pkg67BindingPower rank=1 ready=1' || return 1
    fi
    if covers 320; then
        expect 'PKG67 power 320 Pkg67HotWCaster Pkg67HotW3Power rank=3 ready=0' || return 1
        expect 'PKG67 power 320 Pkg67HotWCaster Pkg67HotW4Power rank=3 ready=1' || return 1
        expect 'PKG67 power 320 Pkg67SoSCaster Pkg67SoS3Power rank=3 ready=0' || return 1
        expect 'PKG67 power 320 Pkg67SoSCaster Pkg67SoS4Power rank=3 ready=1' || return 1
        expect 'PKG67 power 320 Pkg67PoisonCaster Pkg67PoisonPower rank=1 ready=0' || return 1
        expect 'PKG67 power 320 Pkg67SelfBuffCaster Pkg67SelfBuffPower rank=1 ready=0' || return 1
    fi
    if covers 391; then
        expect 'PKG67 power 391 Pkg67HotWCaster Pkg67HotW4Power rank=4 ready=1' || return 1
        expect 'PKG67 power 391 Pkg67SoSCaster Pkg67SoS4Power rank=4 ready=0' || return 1
    fi
    if covers 430; then
        expect 'PKG67 power 430 Pkg67HotWCaster Pkg67HotW4Power rank=4 ready=0' || return 1
    fi
}

prepare() {
    printf "%s pkg67-load-levels - - 0 %s/levels.ini\n" 100 "$package_dir" "$save_frame" "$package_dir"
}

expect_target() {
    power=$1
    creation=$2
    x=$3
    first_cast=$4
    candidate=$(sed -nE "s/^TEST frame ${creation}: create Pkg67[[:alnum:]]+ for Player_1 id=([0-9]+) at \\(${x}\\.0, .*/\\1/p" "$out/continue.log")
    if [ -z "$candidate" ]; then
        printf 'Missing pkg67 target at frame %s and x %s\n' "$creation" "$x" >&2
        return 1
    fi
    for phase_log in "$out/continue.log" "$out/resume.log"; do
        awk -F '\t' -v name="$power" -v target="$candidate" -v frame="$first_cast" '
            $1 == "AI_TARGET" && $4 == name { matches++; valid = ($2 == frame && $6 == target) }
            END { exit !(matches == 1 && valid) }
        ' "$phase_log" || {
            printf 'Unexpected %s target or frame in %s\n' "$power" "$phase_log" >&2
            return 1
        }
    done
}

compare() {
    if [ "$save_frame" -eq 244 ] && covers 430; then
        expect_target Pkg67HotW3Power 245 1545 299 || return 1
        expect_target Pkg67SoS3Power 245 1675 299 || return 1
        expect_target Pkg67HotW4Power 245 1545 419 || return 1
        expect_target Pkg67SoS4Power 245 1675 359 || return 1
        expect_target Pkg67PoisonPower 245 1980 299 || return 1
        expect_target Pkg67SelfBuffPower 121 1940 299 || return 1
    fi
}
