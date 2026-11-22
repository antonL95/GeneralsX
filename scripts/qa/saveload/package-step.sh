#!/bin/sh
set -eu

hook=$1
step=$2
out=$3
save_frame=$4
end=$5
phase=${6:-continue}
first=0
if [ "$phase" = resume ]; then
    first=$save_frame
fi
log="$out/$phase.log"
package_dir=${hook%/*}

covers() {
    [ "$first" -le "$1" ] && [ "$1" -le "$end" ]
}

expect() {
    grep -Fx "$1" "$log" || {
        printf 'Missing in %s: %s\n' "$phase" "$1" >&2
        return 1
    }
}

. "$hook"
if command -v "$step" > /dev/null; then
    "$step"
fi
