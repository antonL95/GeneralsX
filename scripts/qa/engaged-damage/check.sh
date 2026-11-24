#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
here=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d "$build/engaged-damage.XXXXXX")
log="$out/run.log"

export XDG_DATA_HOME="$out/data" XDG_CONFIG_HOME="$out/config"
export XDG_CACHE_HOME="$out/cache" XDG_RUNTIME_DIR="$out/runtime" SDL_AUDIO_DRIVER=dummy
export LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export GENERALSX_TEST_COMMANDS="$here/commands.txt" GENERALSX_RETAIL_EVA_LOG=1
mkdir -m 700 "$XDG_RUNTIME_DIR"

rotwk=${PKGS1_ROTWK_DATA:-/game-install/rotwk}
aotr=${PKGS1_AOTR_DATA:-/game-install/aotr}
bfme2=${PKGS1_BFME2_DATA:-/game-install/bfme2}
name='AotR Training Grounds'
pack="$aotr/UserData/Maps/$name"
mapdir="$XDG_DATA_HOME/GeneralsX/GeneralsZH/Maps/$name"
mkdir -p "$mapdir"
cp "$pack/$name.map" "$pack/map.str" "$mapdir/"
cat "$pack/map.ini" "$here/map.ini" > "$mapdir/map.ini"

cd "$rotwk"
if ! timeout --signal=INT --kill-after=10 180 env -u HOME gdb -q -nx -batch -return-child-result \
    -x "$here/../saveload/training.gdb" -ex run --args "$build/GeneralsMD/GeneralsXZH" \
    -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
    -mod "$aotr" -bfme2 "$bfme2" -file "$mapdir/$name.map" > "$log" 2>&1; then
    tail -40 "$log" >&2
    exit 1
fi

if ! awk '
    /^TEST engaged-damage frame=244 engaged=1 attacking=0 alert=0 undetected=1 damage=1\.0 ping=1$/ { engaged++ }
    /^TEST engaged-damage frame=584 engaged=0 attacking=0 alert=0 undetected=1 damage=1\.0 ping=1$/ { idle++ }
    /^EVA / && $3 ~ /^event=(UnitUnderAttack|UnitUnderAttackFromShroudedUnit|UnitAmbushed)$/ {
        frame = substr($2, 7) + 0
        if (244 <= frame && frame < 584) first = first $3 ":" $5
        if (584 <= frame) second = second $3 ":" $5
    }
    /^TEST frame 730: quit$/ { finished++ }
    END {
        exit !(engaged == 1 && idle == 1 && finished == 1 &&
            first == "event=UnitUnderAttack:valid=1" && second == "event=UnitAmbushed:valid=1")
    }
' "$log"; then
    grep -E '^TEST engaged-damage|^EVA |^TEST frame .*EngagedDamage' "$log" >&2
    printf 'Engaged damage assertions failed. See %s\n' "$log" >&2
    exit 1
fi

printf '%s\n' 'Engaged damage passed: UnitUnderAttack while engaged, UnitAmbushed while idle, both with attack pings.'
