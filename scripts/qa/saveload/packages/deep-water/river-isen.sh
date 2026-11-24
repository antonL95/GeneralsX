#!/bin/sh
set -eu

build=$(CDPATH= cd -- "$1" && pwd)
out=$2
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd)
qa=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
source=${PKGS1_AOTR_DATA:-/game-install/aotr}/maps/'map mp river isen'
map="$out/map-source"
mkdir -p "$map"
ln -sfn "$source/map mp river isen.map" "$map/map mp druwaith iaur.map"
ln -sfn "$source/map.ini" "$map/map.ini"
export PKGS1_DRUWAITH_DATA="$map"
export PKGS1_STATE_DIR="$out"
exec sh "$qa/test-save-load-crc.sh" --only deep-water "$build" marsh-stock 250 125
