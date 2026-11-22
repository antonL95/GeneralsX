function reject(message) {
    print message > "/dev/stderr"
    failed = 1
}

FNR == 1 {
    parts = split(FILENAME, path, "/")
    package = path[parts - 1]
}

/^[[:space:]]*$/ { next }

FILENAME ~ /\.ini$/ && /^[[:alpha:]]/ && NF > 1 {
    kind = tolower($1)
    if (kind == "childobject" || kind == "objectreskin")
        kind = "object"
    key = kind SUBSEP tolower($2)
    if (key in owner && owner[key] != package)
        reject(package " and " owner[key] " both define " $1 " " $2)
    owner[key] = package
}

FILENAME ~ /\.cash$/ {
    if (NF != 3 || $2 !~ /^[0-9]+$/ || $3 !~ /^[0-9]+$/ || $2 > $3) {
        reject(FILENAME ":" FNR ": expected PLAYER FIRST_FRAME LAST_FRAME")
        next
    }
    player = tolower($1)
    for (i = 1; i <= windows; i++)
        if (cashPlayer[i] == player && cashOwner[i] != package && $2 <= cashEnd[i] && $3 >= cashBegin[i])
            reject(package " and " cashOwner[i] " reserve overlapping cash windows for " $1)
    cashPlayer[++windows] = player
    cashBegin[windows] = $2
    cashEnd[windows] = $3
    cashOwner[windows] = package
}

FILENAME ~ /\.txt$/ && $2 == "pkg23-cash" {
    if ($6 in sampleOwner)
        reject(package " duplicates cash sample " $6 " from " sampleOwner[$6])
    sampleOwner[$6] = package
    sampleFrame[$6] = $1
    samplePlayer[$6] = tolower($3)
}

FILENAME ~ /\.txt$/ && $2 ~ /^(fund|cash|pkg23-set-cash|pkg23-pay|pkg23-sell|pkg23-kill)$/ {
    changePlayer[++changes] = tolower($3)
    changeFrame[changes] = $1
    changeOwner[changes] = package
}

END {
    for (label in sampleOwner) {
        covered = 0
        for (i = 1; i <= windows; i++)
            if (cashOwner[i] == sampleOwner[label] && cashPlayer[i] == samplePlayer[label] &&
                sampleFrame[label] >= cashBegin[i] && sampleFrame[label] <= cashEnd[i])
                covered = 1
        if (!covered)
            reject(sampleOwner[label] " has no reserved cash window for sample " label)
    }
    for (i = 1; i <= changes; i++)
        for (j = 1; j <= windows; j++)
            if (changeOwner[i] != cashOwner[j] && changePlayer[i] == cashPlayer[j] &&
                changeFrame[i] >= cashBegin[j] && changeFrame[i] <= cashEnd[j])
                reject(changeOwner[i] " changes " changePlayer[i] " cash at frame " changeFrame[i] +                    " in a window reserved by " cashOwner[j])
    exit failed
}
