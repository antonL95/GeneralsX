check() {
    covers 252 || return 0
    awk '
        $1 == "TEST" && $2 == "drag-preview-decals" && $3 == 252 {
            seen = 1
            for (i = 4; i <= NF; i++) {
                split($i, kv, "=")
                counts[kv[1]] = kv[2] + 0
            }
        }
        END {
            if (!seen || counts["total"] != 35 || counts["FPInfSingle"] != 15 || counts["FPArchSingle"] != 15 ||
                counts["FPHeroDecal"] != 1 || counts["FPinfanBig"] != 0 || counts["FParchBig"] != 0) {
                printf "drag-preview-decals failed: seen=%d total=%d inf=%d arch=%d hero=%d bigInf=%d bigArch=%d\n",
                    seen, counts["total"], counts["FPInfSingle"], counts["FPArchSingle"],
                    counts["FPHeroDecal"], counts["FPinfanBig"], counts["FParchBig"]
                exit 1
            }
        }
    ' "$log"
}
