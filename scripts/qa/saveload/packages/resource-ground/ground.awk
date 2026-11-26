function value(name,    i, f) {
    for (i = 1; i <= NF; ++i) {
        split($i, f, "=")
        if (f[1] == name) return f[2]
    }
    return ""
}
function assert(ok, text) {
    if (!ok) { print "resource ground failed: " text; failed = 1 }
}
function in_range(frame) { return frame >= first && frame <= last }
$1 == "TEST" && $2 == "resground" {
    key = $3 " " value("frame") " " value("x")
    seen[key] = 1
    efficiency[key] = value("efficiency") + 0
    progress[key] = value("progress") + 0
    complete[key] = value("complete") + 0
}
END {
    if (in_range(100)) {
        assert(efficiency["preview 100 3300"] > 0.9, "lone preview of the first site")
        assert(efficiency["preview 100 3500"] > 0.5, "lone preview of the second site")
    }
    if (in_range(260)) {
        assert(seen["site 260 3300"] && !complete["site 260 3300"] && progress["site 260 3300"] == 0, "first site not started at the preview")
        assert(seen["preview 260 3500"] && efficiency["preview 260 3500"] > 0 && efficiency["preview 260 3500"] < 0.5, "preview beside an unstarted foundation")
    }
    if (in_range(280)) {
        assert(seen["site 280 3300"] && progress["site 280 3300"] == 0, "first site not started after the second placement")
        assert(efficiency["site 280 3300"] == efficiency["site 260 3300"], "first foundation keeps its ground")
        assert(seen["site 280 3500"] && efficiency["site 280 3500"] < 0.5, "second foundation is reduced")
    }
    if (in_range(260) && in_range(280))
        assert(efficiency["site 280 3500"] == efficiency["preview 260 3500"], "second foundation matches its preview")
    if (in_range(1900)) {
        assert(complete["site 1900 3300"] && complete["site 1900 3500"], "both sites completed")
        assert(efficiency["site 1900 3300"] > 0.9, "first final efficiency")
        assert(efficiency["site 1900 3500"] > 0 && efficiency["site 1900 3500"] < 0.5, "second final efficiency")
    }
    if (in_range(280) && in_range(1900))
        assert(efficiency["site 1900 3500"] == efficiency["site 280 3500"], "final efficiency equals the foundation value")
    exit failed
}
