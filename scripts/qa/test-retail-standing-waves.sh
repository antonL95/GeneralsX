#!/bin/sh
set -eu

build=${1:?Pass a configured Linux build directory}
map='maps/map mp andrast/map mp andrast.map'
audit=$(mktemp "$build/standing-waves-audit.XXXXXX")
trap 'rm -f "$audit"' EXIT
LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | paste -sd :)
export LD_LIBRARY_PATH GENERALSX_MAP_AUDIT="$audit"

cd /game-install/rotwk
timeout 120 "$build/GeneralsMD/GeneralsXZH" -headless -mod /game-install/aotr -bfme2 /game-install/bfme2 -file "$map"

if ! awk -F '\t' -v map="$map" '$1 == "map-standing-waves" && $2 == map && $3 == "StandingWaveAreas" && $5 == 5 { print; found = 1 } END { exit !found }' "$audit"; then
    printf 'Expected five standing waves on %s\n' "$map" >&2
    exit 1
fi
