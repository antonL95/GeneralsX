#!/bin/sh
set -eu

if [ "$#" -lt 2 ]; then
    printf 'Usage: %s WORKTREE BUILD_DIR [FACTION_SIDE ...]\n' "$0" >&2
    exit 2
fi

worktree=$1
build=${2#/build/}
shift 2
[ "$#" -gt 0 ] || set -- Mordor Guldur Wild Reported

cd "${BFME_ROOT:-$HOME/Developer/bfme}"
cases=game/analysis/ability-sweep-cases.tsv
table=game/analysis/ability-sweep.tsv
COMPOSE_PROGRESS=quiet devbox run --build=false tools \
    python -I "/app/engine/$worktree/scripts/qa/ability_sweep/cases.py" "$@" </dev/null > "$cases"
if [ -n "${ABILITY_SWEEP_FILTER:-}" ]; then
    awk -v pattern="$ABILITY_SWEEP_FILTER" 'NR == 1 || $0 ~ pattern' "$cases" > "$cases.filtered"
    cases="$cases.filtered"
    table=game/analysis/ability-sweep-filtered.tsv
fi
COMPOSE_PROGRESS=quiet devbox run --build=false engine \
    /opt/playtest-media/bin/python -I "/app/engine/$worktree/scripts/qa/ability_sweep/run.py" "/build/$build" \
    < "$cases" > "$table"
awk -F '\t' 'NR > 1 { count[$1 "\t" $8]++ } END { for (key in count) print key "\t" count[key] }' "$table" | sort
