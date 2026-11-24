check() {
    if ! grep -q ' create Player_1 DeepWaterBombardProbe ' "$out/commands.txt"; then
        return 0
    fi
    if covers 230; then
        expect 'TEST deep-water-route 230 DeepWaterBombardProbe exists=1 src=7 dst=7 goal=1925.0,1835.0' || return 1
        expect 'TEST deep-water-route 230 DeepWaterBombardProbe exists=0 src=7 dst=1 goal=1905.0,1845.0' || return 1
        expect 'TEST deep-water-route 230 DeepWaterLargeProbe exists=1 src=7 dst=7 goal=1915.0,1845.0' || return 1
        expect 'TEST deep-water-route 230 DeepWaterTransportProbe exists=1 src=7 dst=1 goal=1895.0,1825.0' || return 1
        expect 'TEST deep-water-route 230 DeepWaterTransportProbe exists=0 src=7 dst=0 goal=1895.0,1835.0' || return 1
        expect 'TEST deep-water-route 230 DeepWaterBeachProbe exists=1 src=7 dst=1 goal=465.0,1145.0' || return 1
        expect 'TEST deep-water-route 230 DeepWaterBeachProbe exists=1 src=7 dst=0 goal=475.0,1145.0' || return 1
        expect 'TEST deep-water-hierarchy 230 DeepWaterTransportProbe exists=1 goal=1895.0,1825.0' || return 1

        awk '
            /^TEST deep-water-sample 230 DeepWaterBombardProbe / {
                print
                bombard = ($0 ~ / cell=7 uw=1 river=0 .* loco=1 cap=1\.0000 /)
            }
            /^TEST deep-water-sample 230 DeepWaterLargeProbe / {
                print
                large = ($0 ~ / cell=7 uw=1 river=1 .* loco=1 cap=0\.5000 /)
            }
            /^TEST deep-water-sample 230 DeepWaterTransportProbe / {
                print
                transport = ($0 ~ / cell=7 uw=1 river=0 .* loco=1 cap=1\.0000 /)
            }
            END { exit !(bombard && large && transport) }
        ' "$log" || return 1
    fi
    if covers 255; then
        expect 'TEST deep-water-route 255 DeepWaterTransportProbe exists=1 src=7 dst=1 goal=1895.0,1825.0' || return 1
        expect 'TEST deep-water-route 255 DeepWaterBeachProbe exists=1 src=7 dst=0 goal=475.0,1145.0' || return 1
        expect 'TEST deep-water-hierarchy 255 DeepWaterTransportProbe exists=1 goal=1895.0,1825.0' || return 1
        awk '
            /^TEST deep-water-sample 255 DeepWaterBombardProbe / {
                print
                split($12, speed, "=")
                moved = (speed[2] + 0 > 0 && $0 ~ / waterOffset=0\.00$/)
            }
            END { exit !moved }
        ' "$log" || return 1
    fi
    if covers 365; then
        awk '
            /^TEST deep-water-sample 365 DeepWaterBombardProbe / {
                print
                split(substr($5, 5), p, ",")
                bombard = ($0 ~ / cell=7 uw=1 river=1 .* loco=1 cap=0\.5000 .* waterOffset=0\.00$/ && p[1] + 0 >= 1920)
            }
            /^TEST deep-water-sample 365 DeepWaterLargeProbe / {
                print
                split(substr($5, 5), p, ",")
                large = ($0 ~ / cell=7 uw=1 river=0 .* loco=1 cap=1\.0000 / && p[1] + 0 < 1920)
            }
            /^TEST deep-water-sample 365 DeepWaterTransportProbe / {
                print
                split(substr($5, 5), p, ",")
                transport = ($0 ~ / cell=1 uw=1 river=0 .* loco=1 / && p[1] + 0 < 1900)
            }
            /^TEST deep-water-sample 365 DeepWaterBeachProbe / {
                print
                split(substr($5, 5), p, ",")
                beach = ($0 ~ / cell=0 .* loco=1 / && p[1] + 0 >= 470)
            }
            END { exit !(bombard && large && transport && beach) }
        ' "$log" || return 1
    fi
}
