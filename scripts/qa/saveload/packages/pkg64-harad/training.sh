match() {
    grep -E "$1" "$log" || {
        printf 'Missing %s in %s\n' "$1" "$log" >&2
        return 1
    }
}

check() {
    if covers 235; then
        match '^PKG03 frame=233 list=Pkg64RequiredEither type=Pkg64Facing '
        match '^PKG03 frame=234 list=Pkg64FaceSecondary .* angle=1\.571 '
        match '^PKG03 frame=235 list=Pkg64FacePrimary .* angle=0\.785 '
    fi

    if covers 237; then
        match '^PKG03 frame=236 list=Pkg64Move .* idle=0 '
        match '^PKG03 frame=237 list=Pkg64Hold .* idle=1 '
    fi

    if covers 258; then
        awk '
            /label=moved / { x = substr($5, 3); moved = x > 145 && x <= 205 }
            /label=held / { held = ($5 == "x=145.000" && $6 == "y=170.000" && $9 == "moving=0") }
            END { if (!(moved && held)) { print "Movement control failed at frame 258"; exit 1 } }
        ' "$log"
    fi

    if ! grep -q 'pkg64-state Player_1 HaradSpellbookOasis' "$out/continue.txt"; then
        return
    fi

    if covers 235; then
        match '^TEST pkg64 frame=201 label=oasis x=1300\.000 y=2700\.000 '
        match '^TEST pkg64 frame=201 label=after-clear x=1700\.000 y=2700\.000 '
        match '^TEST pkg64 frame=203 label=no-clear x=1700\.000 y=2700\.000 '
        match '^TEST pkg64 frame=208 label=dry-spell x=1250\.000 y=3000\.000 '
        match '^TEST pkg64 frame=213 label=barrage seeds=10 spread=10 facing=10$'
        match '^PKG03 frame=214 list=OCL_NafaratiKnockbacker .*pos=1150\.000,2600\.000,'
        match '^PKG03 frame=215 list=Pkg64IgnoreBlocker .*pos=1150\.000,2600\.000,'
        awk '
            /list=Pkg64AvoidBlocker / {
                split(substr($0, index($0, "pos=") + 4), pos, ",")
                dx = pos[1] - 1150; dy = pos[2] - 2600
                found = dx * dx + dy * dy > 16 && dx * dx + dy * dy <= 4900
            }
            END { if (!found) { print "Unexpected obstacle clearance at frame 216"; exit 1 } }
        ' "$log"
        match '^PKG03 frame=222 list=OCL_SpellBookFieryBarrageProjectile type=SpellBookFieryBarrageProjectile '
        match '^PKG03 frame=224 list=OCL_SpellBookFieryBarrageProjectile spawned=none$'
        match '^PKG03 frame=225 list=Pkg64BarrageNoGate type=SpellBookFieryBarrageProjectile '
        match '^TEST pkg64 frame=235 label=risen-early .* built=1$'
        match '^TEST pkg64 frame=231 label=khan-early .* busy=1 '
    fi
    if covers 245; then
        match '^TEST pkg64 frame=245 label=risen-restored .* built=1$'
    fi
    if covers 240; then
        match '^TEST pkg64 frame=240 label=anchor-control x=1750\.000 y=2720\.000 '
    fi
    if covers 313; then
        match '^TEST pkg64 frame=310 label=risen-expired .* built=0$'
        match '^TEST pkg64 frame=313 label=risen-no-flag .* built=0$'
    fi
    if covers 387; then
        match '^TEST pkg64 frame=366 label=khan-still-busy .* busy=1 '
        match '^TEST pkg64 frame=385 label=khan-expired .* busy=0 '
        match '^TEST pkg64 frame=387 label=khan-no-busy .* busy=0 '
    fi
}

compare() {
    for phase in continue resume; do
        awk -v save="$save_frame" '/^TEST pkg64 / { split($3, field, "="); if (field[2] + 0 > save) print }' \
            "$out/$phase.log" > "$out/pkg64-$phase.trace"
    done
    diff -u "$out/pkg64-continue.trace" "$out/pkg64-resume.trace"
}
