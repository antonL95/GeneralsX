check() {
    if covers 244; then
        grep -E '^TEST pkg19 frame=244 template=Pkg19SaveShooter weapon=Pkg19SaveWindup locked=1 .*draw=272 ' "$log"
    fi
    if covers 250; then
        grep -E '^TEST damage frame=250 weapon=Pkg19SaveSecondary .*victim=[0-9]+\(Pkg19SaveTarget\) ' "$log"
    fi
}
