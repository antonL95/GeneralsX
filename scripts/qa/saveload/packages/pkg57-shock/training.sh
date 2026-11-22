prepare() {
    samples() {
        awk -v first="$1" -v last="$2" -v command="$3" 'BEGIN {
            for (frame = first; frame <= last; ++frame)
                print frame, command
        }'
    }
    samples 221 429 'pkg57-probe PlyrCreeps AngmarDarkDunedain 0 dunedain'
    samples 221 429 'pkg57-probe PlyrCreeps Pkg57DefaultControl 0 control'
    samples 221 429 'pkg57-probe PlyrCreeps Pkg57HighGravityControl 0 highgrav'
    samples 221 429 'pkg57-probe PlyrCreeps Pkg57FixedHighControl 0 fixedhigh'
    samples 401 609 'pkg57-probe PlyrCreeps MordorDrummerTroll 0 troll'
    samples 401 530 'pkg57-probe PlyrCreeps HaradZigur 0 zigur'
    samples 641 829 'pkg57-horde-probe PlyrCreeps AngmarDarkDunedainHorde 0 horde'
    samples 861 939 'pkg57-probe Player_1 BattleWagonOilBarrelProjectile 0 barrel-3x'
    samples 861 939 'pkg57-probe Player_1 Pkg57BarrelLowGrav 0 barrel-1x'
}

check() {
    if ! covers 429 || ! covers 939; then
        return 0
    fi
    awk -v first="$first" '
        $1 == "TEST" && $2 == "pkg57" {
            for (i = 3; i <= NF; ++i) {
                split($i, field, "=")
                row[field[1]] = field[2]
            }
            name = row["label"]
            frame = row["frame"] + 0
            if (row["airborne"] == 1 && !(name in air))
                air[name] = frame
            if (row["flailing"] == 1 && !(name in flight))
                flight[name] = frame
            if (row["prone"] == 1 && !(name in prone))
                prone[name] = frame
            if (row["standing"] == 1 && !(name in stand))
                stand[name] = frame
            if ((name in stand) && row["stunned"] == 0 && !(name in idle))
                idle[name] = frame
            if ((name in air) && (row["airborne"] == 0 || row["dead"] == 1) && !(name in land))
                land[name] = frame
            if (row["missing"] == 1 && !(name in gone))
                gone[name] = frame
            if (frame == 870)
                vz[name] = row["vz"] + 0
            if ("gravity" in row)
                config[name] = row["gravity"] "/" row["low"] "/" row["high"] "/" row["up"]
            for (key in row)
                delete row[key]
        }
        function fail(message) {
            print "pkg57: " message > "/dev/stderr"
            error = 1
        }
        function recovery(name, low, high, standing,    duration) {
            duration = stand[name] - prone[name]
            if (!flight[name] || !prone[name] || !stand[name] || !idle[name] ||
                duration < low || duration > high || duration % 6 || idle[name] - stand[name] != standing)
                fail("unexpected recovery for " name)
            printf "pkg57 %s flight=%d prone=%d stand=%d idle=%d\n", name, flight[name], prone[name], stand[name], idle[name]
        }
        END {
            if (config["dunedain"] != "1.0/42/72/42" || config["control"] != "1.0/30/60/30" ||
                config["highgrav"] != "3.0/42/42/42" || config["fixedhigh"] != "1.0/72/72/42" ||
                config["troll"] != "1.0/30/60/90" ||
                config["horde"] != "1.0/30/60/30" || config["horde-m0"] != "1.0/42/72/42" ||
                config["zigur"] != "1.0/30/60/30" || config["barrel-3x"] !~ /^3\.0\// ||
                config["barrel-1x"] !~ /^1\.0\//)
                fail("unexpected effective physics module fields")
            recovery("dunedain", 42, 72, 42)
            recovery("control", 30, 60, 30)
            recovery("fixedhigh", 72, 72, 42)
            recovery("troll", 30, 60, 90)
            recovery("horde-m0", 42, 72, 42)
            if (first == 0) {
                recovery("highgrav", 42, 42, 42)
                if (flight["dunedain"] <= 221 || flight["dunedain"] != flight["control"] ||
                    land["dunedain"] != land["control"] || land["highgrav"] >= land["dunedain"])
                    fail("expected equal default trajectories and earlier landing at triple gravity")
            }
            if (flight["zigur"] || flight["horde"] || !air["zigur"] || land["zigur"] <= air["zigur"])
                fail("expected Zigur to resist shock, then fall, and impact to launch members only")
            barrelEnd = land["barrel-3x"] ? land["barrel-3x"] : gone["barrel-3x"]
            lowEnd = land["barrel-1x"] ? land["barrel-1x"] : gone["barrel-1x"]
            if (!air["barrel-3x"] || !air["barrel-1x"] || !barrelEnd || !lowEnd ||
                barrelEnd >= lowEnd || vz["barrel-3x"] >= vz["barrel-1x"] || vz["barrel-1x"] >= 0)
                fail("expected the authored barrel to fall faster and land first")
            printf "pkg57 land zigur=%d barrel-3x=%d barrel-1x=%d\n", land["zigur"], barrelEnd, lowEnd
            exit error
        }
    ' "$log"
}
