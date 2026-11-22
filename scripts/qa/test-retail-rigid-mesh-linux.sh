#!/bin/sh
set -eu

if [ "$#" -ne 5 ]; then
    printf 'Usage: %s BUILD ROTWK AOTR BFME2 OUTPUT\n' "$0" >&2
    exit 2
fi

build=$(realpath "$1")
rotwk=$(realpath "$2")
aotr=$(realpath "$3")
bfme2=$(realpath "$4")
mkdir -p "$5"
output=$(realpath "$5")
debugger=$(realpath "$(dirname "$0")/retail_rigid_mesh_fixture.py")

export HOME=$(mktemp -d "$output/home.XXXXXX")
export XDG_RUNTIME_DIR="$HOME/runtime" DXVK_LOG_PATH="$HOME" DXVK_STATE_CACHE_PATH="$HOME"
export SDL_VIDEO_DRIVER=x11 SDL_AUDIO_DRIVER=dummy LP_NUM_THREADS=3
export LD_LIBRARY_PATH="$build/GeneralsMD:$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)"
export VK_DRIVER_FILES="${VK_DRIVER_FILES:-$(find /usr/share/vulkan/icd.d -name 'lvp*.json' -print -quit)}"
export GENERALSX_TEST_COMMANDS="$output/commands.txt"
mkdir -m 700 "$XDG_RUNTIME_DIR"
mkdir -p "$HOME/.local/share/GeneralsX/GeneralsZH"
printf 'RetailStaticGameLOD = UltraHigh\nResolution = 1280 720\n' > "$HOME/.local/share/GeneralsX/GeneralsZH/Options.ini"
cat > "$GENERALSX_TEST_COMMANDS" <<'COMMANDS'
11 reveal Player_1 MenPorter 0
19 create Player_1 SnowyRock14 0 760,485,0
19 create Player_1 SnowyRock14 0 905,485,0
21 camera Player_1 MenFortressCitadel 0 - 828 545 .70
31 quit - - 0
COMMANDS

cd "$rotwk"
status=0
timeout --signal=INT --kill-after=10 210 xvfb-run -a -s '-screen 0 1280x720x24' \
    gdb -q -nx -batch -return-child-result -ex 'set debuginfod enabled off' -ex 'set print thread-events off' \
    -ex 'set confirm off' -x "$debugger" -ex run --args "$build/GeneralsMD/GeneralsXZH" -win -xres 1280 -yres 720 \
    -noaudio -nomusic -novideo -noshellmap -mod "$aotr" -bfme2 "$bfme2" \
    -file 'maps/map mp high pass/map mp high pass.map' > "$output/session.log" 2>&1 || status=$?
grep -E '^FX rigid mesh|received signal|Python Exception|exited normally' "$output/session.log" || true
test "$status" = 0
test "$(grep -c '^FX rigid mesh PASS' "$output/session.log")" = 2
! grep -q '^FX rigid mesh FAIL' "$output/session.log"
