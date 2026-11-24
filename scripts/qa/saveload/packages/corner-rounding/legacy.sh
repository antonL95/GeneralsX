#!/bin/sh
set -eu

source_dir=$1
out=$2
build=$3
legacy="$out/corner-legacy"
mkdir -p "$legacy"
cp -R "$source_dir/data" "$legacy/data"
cp -R "$source_dir/config" "$legacy/config"
mkdir -p -m 700 "$legacy/runtime"
printf '330 quit - - 0\n' > "$legacy/commands.txt"
(
    export XDG_DATA_HOME="$legacy/data" XDG_CONFIG_HOME="$legacy/config"
    export XDG_CACHE_HOME="$legacy/cache" XDG_RUNTIME_DIR="$legacy/runtime"
    export GENERALSX_TEST_COMMANDS="$legacy/commands.txt" SDL_AUDIO_DRIVER=dummy
    cd /game-install/rotwk
    timeout --signal=INT --kill-after=10 360 env -u HOME gdb -q -nx -batch -return-child-result \
        -x "$source_dir/resume.gdb" -ex run --args "$build/GeneralsMD/GeneralsXZH" \
        -headless -noFPSLimit -noaudio -nomusic -novideo -noshellmap \
        -mod /game-install/aotr -bfme2 /game-install/bfme2 -loadsave parity.sav \
        > "$legacy/legacy.log" 2>&1
)
grep -Fx 'TEST frame 330: quit' "$legacy/legacy.log"
