#!/bin/sh
set -eu

build=${1:?Build directory required}
fixture=$(cd "$(dirname "$0")" && pwd)
export TREE_COVER_LEGACY_GDB="$fixture/legacy-visual.py"
export SDL_VIDEO_DRIVER=x11
export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
export DXVK_LOG_LEVEL=error
export LP_NUM_THREADS=3

exec timeout 900 xvfb-run -a -s '-screen 0 1280x720x24' sh \
    "$fixture/../../../test-save-load-crc.sh" --only tree-cover "$build" training 244 20
