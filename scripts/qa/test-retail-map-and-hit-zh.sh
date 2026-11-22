#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
rotwk=${PKGS1_ROTWK_DATA:-/game-install/rotwk}
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
out=$(mktemp -d "$build/retail-map-and-hit.XXXXXX")
printf '%s\n' "$out"

export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config"
export XDG_CACHE_HOME="$out/cache" XDG_RUNTIME_DIR="$out/runtime" SDL_AUDIO_DRIVER=dummy
export LD_LIBRARY_PATH="$build/GeneralsMD:$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)"
export GENERALSX_TEST_COMMANDS="$out/commands.txt"
mkdir -m 700 "$XDG_RUNTIME_DIR"
user_maps="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps"
install="$out/install"
name='AotR Training Grounds'
pack="$aotr/UserData/Maps/$name"

stage_map() {
    mkdir -p "$1"
    cp "$pack/$name.map" "$1/$name.map"
    cp "$pack/map.ini" "$pack/map.str" "$1/"
}

stage_map "$user_maps/$name"
stage_map "$user_maps-other/$name"
stage_map "$install/Maps/$name"
for asset in "$rotwk"/*; do
    case "${asset##*/}" in Maps|maps) continue ;; esac
    ln -s "$asset" "$install/"
done
ln -s "$user_maps-other/$name" "$user_maps/escape"
mkdir -p "$user_maps/link"
ln -s "$user_maps-other/$name/$name.map" "$user_maps/link/link.map"

run() {
    log=$1
    (
        cd "$install"
        timeout --signal=INT --kill-after=10 90 "$build/GeneralsMD/GeneralsXZH" \
            -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
            -mod "$aotr" -bfme2 "$bfme2" -file "$2"
    ) > "$out/$log.log" 2>&1
}

result=0
expect() {
    if ! grep -q "$2" "$out/$1.log"; then
        printf '%s failed in %s/%s.log\n' "$2" "$out" "$1" >&2
        result=1
    fi
}

printf '%s\n' '34 quit - - 0' > "$GENERALSX_TEST_COMMANDS"
for escape in outside parent directory-link file-link missing; do
    case "$escape" in
        outside) path="$user_maps-other/$name/$name.map" ;;
        parent) path="$user_maps/../Maps-other/$name/$name.map" ;;
        directory-link) path="$user_maps/escape/$name.map" ;;
        file-link) path="$user_maps/link/link.map" ;;
        missing) path="$out/missing/$name.map" ;;
    esac
    run "$escape" "$path" || result=1
    expect "$escape" '^Rejected retail map path'
    if grep -q '^TEST frame 34: quit' "$out/$escape.log"; then
        printf 'Rejected map started a game in %s/%s.log\n' "$out" "$escape" >&2
        result=1
    fi
done

cat > "$GENERALSX_TEST_COMMANDS" <<'COMMANDS'
6 create Player_1 MordorGreatBeast 0 2958,470,0
10 damage Player_1 MordorGreatBeast 0 120
12 pkg50-state Player_1 MordorGreatBeast 0
16 damage Player_1 MordorGreatBeast 0 250
18 pkg50-state Player_1 MordorGreatBeast 0
50 pkg50-state Player_1 MordorGreatBeast 0
51 quit - - 0
COMMANDS
for form in user install relative filename; do
    case "$form" in
        user) path="$user_maps/$name/$name.map" ;;
        install) path="$install/Maps/$name/$name.map" ;;
        relative) path="maps/$name/$name.map" ;;
        filename) path="$name.map" ;;
    esac
    run "$form" "$path" || result=1
    expect "$form" '^TEST pkg50 frame=12 name=MordorGreatBeast present=1 .*hit=0000 '
    expect "$form" '^TEST pkg50 frame=18 name=MordorGreatBeast present=1 .*hit=1100 '
    expect "$form" '^TEST pkg50 frame=50 name=MordorGreatBeast present=1 .*hit=0000 '
    expect "$form" '^TEST frame 51: quit'
done

printf '%s\n' '5 xpcrc - - 0' '6 quit - - 0' > "$GENERALSX_TEST_COMMANDS"
run layered 'Maps/map mp high pass/map mp high pass.map' || result=1
expect layered '^TEST xp-crc frame=5 crc='
exit "$result"
