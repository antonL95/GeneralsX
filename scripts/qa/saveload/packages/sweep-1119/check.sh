#!/bin/sh
set -eu

build=${1:?Build directory required}
export SWEEP1119_TERRAIN_VERSION=${2:-2}
case "$SWEEP1119_TERRAIN_VERSION" in 0|1|2|3) ;; *) exit 2 ;; esac
fixture=$(cd "$(dirname "$0")" && pwd)
export SWEEP1119_SCENARIO=1
export SWEEP1119_RENDERED_GDB="$fixture/legacy.py"
export GENERALSX_RETAIL_SKIRMISH_AI=1
export SDL_VIDEO_DRIVER=x11
export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
export DXVK_LOG_LEVEL=error
export LP_NUM_THREADS=3

exec timeout 900 xvfb-run -a -s '-screen 0 1280x720x24' sh \
    "$fixture/../../../test-save-load-crc.sh" --only sweep-1119 "$build" high-pass 80 "${3:-600}"
