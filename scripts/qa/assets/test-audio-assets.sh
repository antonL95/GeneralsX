#!/bin/sh
set -eu

build=$(cd "${1:?build directory required}" && pwd)
fixtures=$(cd "$(dirname "$0")" && pwd)
case "${2:-}" in
	weighted-audio|ambient-streams) fixture="$fixtures/$2" ;;
	*) exit 2 ;;
esac
out="$build/$2"
title="AotR Training Grounds"
map_source="/game-install/aotr/UserData/Maps/$title"
map_directory="$out/data/GeneralsX/GeneralsZH/Maps/$title"
mkdir -p -m 700 "$out/runtime" "$map_directory"
cp "$map_source"/* "$map_directory/"
if [ -f "$fixture/map.ini" ]; then
	printf '\n' >> "$map_directory/map.ini"
	cat "$fixture/map.ini" >> "$map_directory/map.ini"
fi
export XDG_DATA_HOME="$out/data" XDG_RUNTIME_DIR="$out/runtime" SDL_AUDIO_DRIVER=dummy
export LD_LIBRARY_PATH="$build/GeneralsMD:$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)"
export GENERALSX_TEST_COMMANDS="$fixture/commands.txt"
cd /game-install/rotwk
timeout --signal=INT --kill-after=10 180 "$build/GeneralsMD/GeneralsXZH" -headless -noFPSLimit \
	-noaudio -nomusic -novideo -noshellmap -mod /game-install/aotr -bfme2 /game-install/bfme2 \
	-file "$map_directory/$title.map" > "$out/session.log" 2>&1 || {
	tail -25 "$out/session.log"
	exit 1
}
awk -f "$fixture/check.awk" "$out/session.log"
