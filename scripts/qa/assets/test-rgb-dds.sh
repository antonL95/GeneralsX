#!/bin/sh
set -eu

build=${1:?Pass the engine build directory}
fixture=$(cd "$(dirname "$0")" && pwd)
LD_LIBRARY_PATH=$(find "$build" -name '*.so*' -printf '%h\n' | sort -u | tr '\n' ':')
export LD_LIBRARY_PATH
export GENERALSX_ASSET_AUDIT="$build/dds-rgb.tsv"
export GENERALSX_TEST_RGB_DDS="$fixture/uncompressed-rgb-dds.txt"
cd /game-install/rotwk

status=0
timeout 180 "$build/GeneralsMD/GeneralsXZH" -mod /game-install/aotr -bfme2 /game-install/bfme2 \
	> "$build/dds-rgb-engine.log" 2>&1 || status=$?
if [ -f "$GENERALSX_ASSET_AUDIT" ]; then
	tail -17 "$GENERALSX_ASSET_AUDIT"
else
	tail -20 "$build/dds-rgb-engine.log"
fi
exit "$status"
