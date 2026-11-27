prepare() {
    printf '%s promotion-tint-load-levels - - 0 %s/levels.ini\n' 50 "$package_dir" "$save_frame" "$package_dir"
}

probe_tint() {
    if covers "$1"; then
        expect "TEST promotion-tint frame=$1 $2 rank=$3 rgb=$4" || return 1
    fi
}

check() {
    probe_tint 90 PromotionTintFixture 1 '0.000,0.000,0.000' &&
    probe_tint 103 PromotionTintFixture 2 '0.250,0.063,0.000' &&
    probe_tint 106 PromotionTintFixture 2 '1.000,0.251,0.000' &&
    probe_tint 106 PromotionTintDisabled 2 '0.000,0.000,0.000' &&
    probe_tint 106 PromotionTintHorde 2 '1.000,0.251,0.000' &&
    probe_tint 109 PromotionTintFixture 2 '0.500,0.125,0.000' &&
    probe_tint 124 PromotionTintFixture 2 '1.000,0.251,0.000' &&
    probe_tint 124 PromotionTintHorde 2 '1.000,0.251,0.000' &&
    probe_tint 139 PromotionTintFixture 2 '0.375,0.094,0.000' &&
    probe_tint 150 PromotionTintFixture 2 '0.000,0.000,0.000'
}
