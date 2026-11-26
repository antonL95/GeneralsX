check() {
    awk -v first="$first" -v last="$end" -f "$package_dir/ground.awk" "$log"
}
