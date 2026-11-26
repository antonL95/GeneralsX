isolated() {
    grep -q ' map-ini-probe ' "$out/commands.txt"
}

prepare() {
    isolated || return 0
    cat "$package_dir/particles.gdb" >> "$out/start.gdb"
    cat "$package_dir/particles.gdb" >> "$out/resume.gdb"
}

check() {
    isolated || return 0
    expect 'TEST map-ini retail particle definitions initialized=1'
    if covers 200; then
        expect 'TEST map-ini first player=777 particle=42.0 fire=777'
        expect 'TEST map-ini extras first player=1 particle=1'
    fi
    if covers 202; then
        expect 'TEST map-ini existing first played=0'
    fi
    if covers 242; then
        expect 'TEST map-ini eva first played=1'
    fi
    if covers 245; then
        expect 'TEST map-ini saved player=777 particle=42.0 fire=777'
        expect 'TEST map-ini extras saved player=1 particle=1'
    fi
    if covers 247; then
        expect 'TEST map-ini existing saved played=0'
    fi
    if covers 253; then
        expect 'TEST map-ini eva saved played=1'
    fi
}

compare() {
    isolated || return 0
    aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
    bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
    maps="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps"
    first_map="$maps/aotr training grounds/aotr training grounds.map"
    second_map="$maps/map-ini-base/map-ini-base.map"
    mkdir -p "${second_map%/*}"
    cp "$aotr/maps/map mp high pass/map mp high pass.map" "$second_map"
    cp "$aotr/maps/map mp high pass/map.ini" "${second_map%/*}/map.ini"
    {
        printf '%s\n' '200 map-ini-probe Player_1 - 0 transition' \
            '200 map-ini-existing-queue Player_1 - 0 transition' \
            '202 map-ini-existing-probe Player_1 - 0 transition' \
            '240 map-ini-eva-queue Player_1 - 0 transition' \
            '242 map-ini-eva-probe Player_1 - 0 transition'
        printf '250 map-ini-next Player_1 - 0 %s\n' "$second_map"
    } > "$out/transition.txt"
    phase=transition
    log="$out/$phase.log"
    GENERALSX_TEST_COMMANDS="$out/transition.txt" timeout --signal=INT --kill-after=10 240 \
        env -u HOME gdb -q -nx -batch -return-child-result -x "$out/start.gdb" -ex run \
        --args "$PKGS1_BUILD_DIR/GeneralsMD/GeneralsXZH" -headless -noFPSLimit -noaudio -nomusic \
        -novideo -noshellmap -mod "$aotr" -bfme2 "$bfme2" -file "$first_map" > "$log" 2>&1 || {
        tail -40 "$log" >&2
        return 1
    }
    expect 'TEST map-ini transition player=777 particle=42.0 fire=777'
    expect 'TEST map-ini extras transition player=1 particle=1'
    expect 'TEST map-ini existing transition played=0'
    expect 'TEST map-ini eva transition played=1'
    expect "TEST map-ini leaving $first_map"
    expect 'TEST map-ini transition player=0 particle=2.0 fire=100'
    expect 'TEST map-ini extras transition player=0 particle=0'
    expect 'TEST map-ini existing transition played=1'
    expect 'TEST map-ini eva transition played=0'
    expect "TEST map-ini finished $second_map"
}
