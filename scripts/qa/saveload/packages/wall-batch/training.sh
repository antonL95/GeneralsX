check() {
    if ! covers 261; then
        return
    fi
    expect 'TEST wall-batch-delete frame=250 pieces=3'
    expect 'TEST wall-batch-delete frame=260 pieces=3'
    awk '
        /^TEST wall-batch-rebuilds / { counts[++count] = $3 }
        /^TEST wall-batch-state frame=/ {
            split($3, frame, "=")
            split($4, clear, "=")
            cells[frame[2]] = clear[2]
            hashes[frame[2]] = $5
        }
        END {
            if (count != 2 || counts[1] != 3 || counts[2] != 1) {
                print "Expected three serial and one grouped rebuild, observed", counts[1], counts[2]
                exit 1
            }
            if (!cells[251] || cells[249] <= cells[251] || cells[259] <= cells[261] ||
                cells[249] != cells[259] || cells[251] != cells[261] ||
                hashes[249] != hashes[259] || hashes[251] != hashes[261]) {
                print "Serial and grouped wall cells differ or the survivor is missing"
                exit 1
            }
        }
    ' "$log"
}
