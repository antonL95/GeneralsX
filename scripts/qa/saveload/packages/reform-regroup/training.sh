check() {
    covers 1080 && covers 1190 || return 0
    awk '
        $1 == "TEST" && $2 == "regroup" {
            split($4, a, "="); split($5, b, "=")
            if (seen && (a[2] - angle > 0.785398 || angle - a[2] > 0.785398)) {
                snap = $3
                speed = b[2]
            }
            if (snap && $3 > snap && $3 <= snap + 15 && b[2] == 0) stopped = 1
            angle = a[2]; seen = 1
        }
        $1 == "TEST" && $2 == "regroup-member" && $3 == snap {
            split($5, e, "=")
            if (e[2] > 1) displaced = 1
        }
        END {
            print "reform snap frame=" snap " speed=" speed " displaced=" displaced " stopped=" stopped
            exit !(snap && speed > 0 && displaced && !stopped)
        }
    ' "$log"
}
