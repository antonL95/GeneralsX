compare() {
    sh "$package_dir/../../../test-skirmish-save-load.sh" \
        --opening "$PKGS1_BUILD_DIR" high-pass FactionMordor FactionMen hard 80 140
}
