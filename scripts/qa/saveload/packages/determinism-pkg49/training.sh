expect_sounds() {
    if ! covers "$1"; then
        return 0
    fi
    actual=$(awk -v frame="$1" '
        $1 == "PKG49" && $2 == "FX" { active = $3 == "frame=" frame }
        active && $1 == "[Retail" && $2 == "FXList]" && $3 == "Sound" { print $4 }
    ' "$log")
    if [ "$actual" != "$2" ]; then
        printf 'Package 49 %s at %s expected <%s>, got <%s>\n' "$phase" "$1" "$2" "$actual" >&2
        return 1
    fi
}

find_id() {
    file="$log"
    if [ "$1" -lt "$save_frame" ]; then
        file="$out/save.log"
    fi
    sed -n "s/^TEST frame $1: create $2 for $3 id=\([0-9]*\) .*/\1/p" "$file"
}

expect_request() {
    if covers "$2"; then
        expect "RETAIL AUDIO $1 frame=$2 owner=$3 sound=$4"
    fi
}

reject_line() {
    if grep -F "$1" "$log"; then
        printf 'Unexpected package 49 event in %s: %s\n' "$phase" "$1" >&2
        return 1
    fi
}

check() {
    expect_sounds 218 TrollVoxFindTree
    expect_sounds 219 ''
    expect_sounds 220 'TrollVoxFindTree
TrollVoxFindTastyOrc'
    expect_sounds 221 ''
    expect_sounds 223 HeroWestFemaleVoiceDie
    expect_sounds 224 UrukDieFalling
    expect_sounds 225 ''
    expect_sounds 258 TrollVoxFindTree

    unit=$(find_id 232 AmrothianGuardsmen Player_1)
    loop=EmotionMenGoodCompilationBFME1and2VoxTauntSinglesLoop
    expect_request start 237 "$unit" "$loop"
    expect_request stop 250 "$unit" "$loop"
    expect_request start 254 "$unit" "$loop"
    expect_request stop 261 "$unit" "$loop"
    reject_line "RETAIL AUDIO start frame=238 owner=$unit sound=$loop"
    if [ "$phase" = resume ]; then
        expect_request start "$save_frame" "$unit" "$loop"
    fi

    citadel=$(find_id 229 DolAmrothFortressCitadel Player_1)
    construction=BuildingConstructionLoop
    expect_request start 239 "$citadel" "$construction"
    expect_request stop 388 "$citadel" "$construction"
    reject_line "RETAIL AUDIO start frame=230 owner=$citadel sound=$construction"
    if [ "$phase" = resume ]; then
        expect_request start "$save_frame" "$citadel" "$construction"
    fi
    doomed=$(find_id 390 Pkg49CitadelDeath Player_1)
    expect_request start 399 "$doomed" "$construction"
    expect_request stop 408 "$doomed" "$construction"

    group=$(find_id 264 AmrothianGuardsmen Player_1)
    expect_request start 271 "$group" ManTauntCalloutsLoop1
    expect_request stop 281 "$group" ManTauntCalloutsLoop1
    reject_line "RETAIL AUDIO start frame=268 owner=$group sound=ManTauntCalloutsLoop1"
    reject_line "RETAIL AUDIO stop frame=276 owner=$group sound=ManTauntCalloutsLoop1"

    marines=$(find_id 285 BelfalasMarines Player_1)
    expect_request start 288 "$marines" BelfalasMarineVoiceAttackRanged
    expect_request start 289 "$marines" BelfalasMarineVoiceAttackMelee
    expect_request start 290 "$marines" BelfalasMarineVoiceAttackRanged

    hero=$(find_id 208 CreateAHero Player_1)
    reject_line "RETAIL AUDIO start frame=292 owner=$hero sound=HeroMenArcherVoiceSelectMS"
    reject_line "RETAIL AUDIO start frame=293 owner=$hero sound=HeroMenArcherVoiceSelectMS"
    expect_request start 294 "$hero" HeroMenArcherVoiceSelectMS

    voice_control=$(find_id 298 Pkg49MarineVoiceControl Player_1)
    always=$(find_id 299 Pkg49RestlessAlways Player_1)
    expect_request start 302 "$voice_control" BelfalasMarineVoiceSelectMS
    expect_request start 303 "$always" DeadArnorVoiceUnique
    if covers 305; then
        first_voice=$(sed -n 's/^RETAIL AUDIO start frame=304 \(owner='"$voice_control"' sound=.*\)$/\1/p' "$log" | head -n 1)
        [ -n "$first_voice" ]
        expect "RETAIL AUDIO start frame=305 $first_voice"
    fi

    frodo=$(find_id 310 RingHeroFrodo Player_1)
    ally_frodo=$(find_id 317 RingHeroFrodo PlyrCivilian)
    if covers 327; then
        expect "RETAIL NOTICE birth frame=310 owner=$frodo"
        expect "RETAIL NOTICE death frame=327 owner=$frodo"
        reject_line "RETAIL NOTICE birth frame=317 owner=$ally_frodo"
        [ "$(grep -c 'event=FrodoCreated sound=RingHeroFrodoCreated valid=1' "$log")" -eq 1 ]
    fi
}
