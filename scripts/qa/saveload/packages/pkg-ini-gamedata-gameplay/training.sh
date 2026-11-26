prepare() {
    if grep -q '^290 powerstate Player_1' "$out/commands.txt"; then
        printf '%s pkg67-load-levels - - 0 %s/levels.ini\n' 279 "$package_dir"
    fi
}

check() {
    if covers 217; then
        grep -E '^TEST pkg61 range frame=217 weapon=GameDataFixtureSmallSplash .*range=97\.50 ' "$log" || return 1
    fi
    if covers 223; then
        grep -E '^TEST hobbit-stealth 223 GameDataFixtureCamouflage .*stealth=1 detected=0 ' "$log" || return 1
    fi
    if covers 229; then
        grep -E '^TEST hobbit-stealth 229 GameDataFixtureCamouflage .*stealth=1 detected=1 ' "$log" || return 1
    fi
    if covers 243; then
        grep -E '^TEST health 243 PlyrCreeps GameDataFixtureRecloak 0 .*health=980\.00 ' "$log" || return 1
        grep -E '^TEST hobbit-stealth 243 GameDataFixtureRecloak .*stealth=0 ' "$log" || return 1
    fi
    if covers 247; then
        grep -E '^TEST hobbit-stealth 247 GameDataFixtureCamouflage .*stealth=1 detected=1 ' "$log" || return 1
    fi
    if covers 248; then
        grep -E '^TEST hobbit-stealth 248 GameDataFixtureRecloak .*stealth=0 ' "$log" || return 1
    fi
    if covers 250; then
        grep -E '^TEST health 250 PlyrCreeps GameDataFixtureVictim 0 .*health=980\.00 ' "$log" || return 1
        grep -E '^TEST health 250 PlyrCreeps GameDataFixtureVictim 1 .*health=1000\.00 ' "$log" || return 1
    fi
    if covers 267; then
        grep -E '^TEST pkg08 fire frame=267 contained=1 ' "$log" || return 1
        grep -E '^TEST pkg61 range frame=267 weapon=GameDataFixtureSmallSplash .*range=122\.50 ' "$log" || return 1
    fi
    if covers 268; then
        grep -E '^TEST pkg08 fire frame=268 contained=1 ' "$log" || return 1
        grep -E '^TEST pkg61 range frame=268 weapon=GameDataFixtureSmallSplash .*range=122\.50 ' "$log" || return 1
    fi
    if covers 274; then
        grep -E '^TEST health 274 PlyrCreeps GameDataFixtureVictim 2 .*health=980\.00 ' "$log" || return 1
        grep -E '^TEST health 274 PlyrCreeps GameDataFixtureVictim 3 .*health=980\.00 ' "$log" || return 1
    fi
    if covers 277; then
        grep -E '^TEST frame 277: check GameDataFixtureBuildProbe .*legal 3 order 3 ai 3 ' "$log" || return 1
    fi
    if covers 280; then
        grep -E '^TEST frame 280: check GameDataFixtureBuildProbe .*legal 0 order 0 ai 0 ' "$log" || return 1
    fi
    if covers 298; then
        grep -E '^TEST hobbit-stealth 298 GameDataFixtureCamouflage .*stealth=1 detected=0 ' "$log" || return 1
    fi
    if covers 306; then
        grep -E '^TEST hobbit-stealth 306 GameDataFixtureRecloak .*stealth=1 ' "$log" || return 1
    fi
    if grep -q '^290 powerstate Player_1' "$out/commands.txt"; then
        if covers 290; then
            grep -E '^TEST powerstate 290 Player_1 rank=3 ' "$log" || return 1
        fi
        if covers 253; then
            awk '$2 == "hobbit-stealth-draw" && $4 == "GameDataFixtureCamouflage" && $3 >= 247 && $3 <= 253 {
                split($8, field, "=")
                opacity = field[2] + 0
                if (opacity < 0.199 || opacity > 0.801) exit 1
                if (count == 0) initial = opacity
                if (opacity != initial) varied = 1
                count++
                print
            } END { if (count != 3 || !varied) exit 1 }' "$log" || return 1
        fi
    fi
}
