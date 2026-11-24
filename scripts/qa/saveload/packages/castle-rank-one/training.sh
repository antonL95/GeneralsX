check() {
    if covers 13; then
        expect 'TEST castle-rank-one 13 CastleRankOneControl construction=1 rank=1 hide=0 banner=0 show=0 wild=0'
    fi
    if covers 20; then
        expect 'TEST castle-rank-one 20 CastleRankOneControl construction=0 rank=1 hide=1 banner=0 show=0 wild=0'
        expect 'TEST castle-rank-one 20 WildFortressCitadel construction=0 rank=1 hide=1 banner=0 show=0 wild=0'
    fi
    if covers 33; then
        expect 'FORTRESS order 33 WildFortressCitadel Command_PurchaseUpgradeWildFortressGoblinTown Upgrade_WildFortressGoblinTownTrigger available=1 frames=900'
    fi
    if covers 244; then
        expect 'TEST castle-rank-one 244 WildFortressCitadel construction=0 rank=1 hide=1 banner=0 show=0 wild=0'
    fi
    if covers 942; then
        expect 'TEST castle-rank-one 942 WildFortressCitadel construction=0 rank=1 hide=1 banner=1 show=1 wild=1'
    fi
    if covers 946; then
        expect 'TEST snapshot-save frame=946 code=0'
    fi
    if covers 947; then
        expect 'TEST castle-rank-one 947 WildFortressCitadel construction=0 rank=1 hide=1 banner=1 show=1 wild=1'
    fi
    if covers 5 && grep -q castle-rank-one-native "$out/$phase.txt"; then
        expect 'TEST castle-rank-one native 5 faction=FactionWild unpacked=1'
    fi
}

compare() {
    if [ "$end" -lt 950 ]; then
        return
    fi
    printf '%s\n' '947 castle-rank-one-state Player_1 WildFortressCitadel 0' '950 quit - - 0' > "$out/castle-rank-one-after.txt"
    GENERALSX_TEST_COMMANDS="$out/castle-rank-one-after.txt" timeout --signal=INT --kill-after=10 360 \
        env -u HOME gdb -q -nx -batch -return-child-result -x "$out/resume.gdb" -ex run \
        --args "$PKGS1_BUILD_DIR/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod "${PKGS1_AOTR_DATA:-/game-install/aotr}" -bfme2 "${PKGS1_BFME2_DATA:-/game-install/bfme2}" \
        -loadsave castle-rank-one-after.sav > "$out/castle-rank-one-after.log" 2>&1
    grep -Fx 'TEST castle-rank-one 947 WildFortressCitadel construction=0 rank=1 hide=1 banner=1 show=1 wild=1' "$out/castle-rank-one-after.log"
    grep -Fx 'TEST frame 950: quit' "$out/castle-rank-one-after.log"
}
