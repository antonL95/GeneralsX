#!/bin/sh
set -eu

build=${1:?build directory required}
qa=$(cd "$(dirname "$0")" && pwd)
export PKGS1_TRAINING_EXTRA_INI="$qa/saveload/package11.ini"
export PKGS1_TRAINING_EXTRA_COMMANDS="$qa/saveload/package11.txt"
result=$(mktemp)
trap 'rm -f "$result"' EXIT
if ! sh "$qa/test-save-load-crc.sh" "$build" training 244 300 > "$result" 2>&1; then
    cat "$result"
    exit 1
fi
cat "$result"
session=$(sed -n '1p' "$result")
log="$session/continue.log"

expect() {
    if ! grep -E "$1" "$log"; then
        printf 'Missing package 11 result: %s\n' "$1" >&2
        return 1
    fi
}

expect '^TEST emotion frame=186 .*type=CHEER source=AmrothianGuardsmen_Patrol '
expect '^TEST pkg11 state frame=187 .*cheer=0 '
expect '^TEST pkg11 state frame=191 name=DolAmrothPorter hp=240.0/500.0 state=1 .*recovery=300 '
expect '^TEST pkg11 state frame=191 name=Pkg11PorterControl hp=240.0/500.0 state=1 .*recovery=0 '
expect '^TEST pkg11 state frame=495 name=DolAmrothPorter hp=240.0/500.0 state=1 '
expect '^TEST pkg11 state frame=495 name=Pkg11PorterControl hp=240.0/500.0 state=1 '
expect '^TEST pkg11 state frame=194 name=GondorGwaihir hp=2900.0/3000.0 .*healfx=0 '
expect '^TEST pkg11 state frame=194 name=Pkg11GwaihirControl hp=2900.0/3000.0 .*healfx=1 '
expect '^TEST impact frame=194 weapon=Pkg11Dodgeable .*victim=0 ' >/dev/null
expect '^TEST pkg11 state frame=195 name=DolAmrothMasterAtArms hp=1800.0/2000.0 .*dodge=0.80 '
expect '^TEST pkg11 state frame=209 name=AmrothianGuardsmenHorde .*memberhp=1725.0$'
expect '^TEST pkg11 state frame=209 name=Pkg11HordeControl .*memberhp=1785.0$'
expect '^TEST pkg11 pulse frame=209 name=AmrothianGuardsmenHorde combat=1 delay=30$'
expect '^TEST pkg11 pulse frame=209 name=Pkg11HordeControl combat=1 delay=60$'
expect '^TEST pkg11 state frame=212 name=AmrothianGuardsmenHorde .*items=15 .*memberhp=1785.0$'
expect '^TEST pkg11 state frame=212 name=Pkg11HordeNoContained .*memberhp=1725.0$'
expect '^TEST pkg11 state frame=236 name=AmrothianMilitia hp=0.0/200.0 state=3 .*burning=385 firefx=1 '
expect '^TEST pkg11 state frame=236 name=Pkg11MilitiaControl hp=0.0/200.0 state=3 .*burning=0 firefx=0 '
expect '^TEST pkg11 state frame=236 name=Pkg11MilitiaNormal hp=0.0/200.0 state=3 .*burning=0 firefx=1 '
expect '^TEST pkg11 state frame=239 name=Pkg11Captain hp=550.0/800.0 '
expect '^TEST pkg11 state frame=245 name=Pkg11Captain hp=800.0/800.0 '
expect '^TEST projectile frame=242 weapon=Pkg11SiegeLaunch .*warhead=Pkg11SiegeWarhead '
expect '^TEST pkg11 state frame=240 name=DolAmrothGuardhouse hp=2615.0/3000.0 state=0 front=0 '
expect '^TEST pkg11 state frame=251 name=DolAmrothGuardhouse hp=2230.0/3000.0 state=0 front=1 '
expect '^TEST pkg11 state frame=251 name=Pkg11GuardhouseControl hp=2230.0/3000.0 state=0 front=0 '
expect '^TEST pkg11 debris frame=252 prefix=BuildingGBBarracksN2D2BitA count=1$'
expect '^TEST pkg11 debris frame=280 prefix=BuildingDamageBits count=0$'
expect '^TEST pkg11 debris frame=291 prefix=BuildingDamageBits count=7$'
expect '^TEST pkg11 state frame=326 name=DolAmrothGuardhouse .*state=2 front=0 '
expect '^TEST pkg11 state frame=328 name=DolAmrothGuardhouse .*state=1 front=0 '
expect '^TEST pkg11 state frame=470 name=RohanEntFir .*items=1 first=EntThrownBuildingRock '
expect '^TEST pkg11 state frame=470 name=RohanEntOak .*items=0 '
expect '^TEST pkg11 state frame=470 name=TDHDwarfCampWall hp=1910.0/2400.0 .*grab=490,16,0,1 '
expect '^TEST pkg11 state frame=470 name=Pkg11WallControl hp=2400.0/2400.0 .*grab=0,0,0,0 '
