check() {
    if covers 243; then
        grep -F 'loaded-a attached=1 dead=0' "$log"
        grep -F 'loaded-b attached=1 dead=0' "$log"
    fi
    if covers 251; then
        awk '/TEST passenger-ejection frame=251 launched-/ {
            split($NF, v, "[=,]");
            split($7, a, "=");
            horizontal = sqrt(v[2] * v[2] + v[3] * v[3]);
            cross = v[2] * sin(a[2]) - v[3] * cos(a[2]);
            dot = v[2] * cos(a[2]) + v[3] * sin(a[2]);
            speed = horizontal * 6;
            rounded = int(speed + 0.5);
            if ($5 != "attached=0" || $6 != "dead=1" || rounded < 2 || rounded > 5 || speed - rounded < -0.01 || speed - rounded > 0.01 || cross < -0.01 || cross > 0.01 || dot < 0 || v[4] - horizontal < -0.01 || v[4] - horizontal > 0.01) exit 1;
            count++;
            print;
        } END { if (count != 2) exit 1 }' "$log"
    fi
}
compare() {
    grep 'TEST passenger-ejection frame=251 launched-' "$out/continue.log" > "$out/passenger-ejection-continued.txt"
    grep 'TEST passenger-ejection frame=251 launched-' "$out/resume.log" > "$out/passenger-ejection-resumed.txt"
    cmp "$out/passenger-ejection-continued.txt" "$out/passenger-ejection-resumed.txt"
}
