after_save() {
    grep '^TEST save-load removed spellbook id=[0-9][0-9]* next=[0-9][0-9]*$' "$out/save.log"
    {
        printf '%s snapshot-book-repair Player_1 GondorFighter 0\n' "$save_frame"
        printf '%s quit - - 0\n' "$end"
    } > "$out/resume.txt"
}

compare() {
    grep '^TEST save-load repaired spellbook ' "$out/resume.log"
    awk '
        /^TEST save-load removed spellbook / { split($6, counter, "="); saved = counter[2] }
        /^TEST save-load repaired spellbook / {
            split($5, book, "="); split($6, unit, "=")
            ok = saved && book[2] == saved && unit[2] > book[2] && $7 == "lookup=1"
        }
        END { exit !ok }
    ' "$out/save.log" "$out/resume.log"
}
