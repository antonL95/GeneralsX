$1 == "AUDIO" && $2 == "DURATION" {
	split($3, handle, "=")
	split($4, time, "=")
	duration[handle[2]] = time[2] + 0
}
$1 == "AUDIO" && $2 == "TRACE" && $4 == "kind=start" {
	split($6, handle, "=")
	split($7, event, "=")
	if (event[2] == "AmbientShirePub" || event[2] == "AmbientPelargirDock1") {
		played[event[2]]++
		if (duration[handle[2]] > 0) {
			decoded[event[2]]++
			printf "Ambient stream %s decoded %.3f ms and started\n", event[2], duration[handle[2]]
		}
	}
	if (event[2] == "DefaultAmbientStream") defaultPlayed++
}
$0 == "TEST frame 18: quit" { quit = 1 }
END {
	printf "Ambient streams: %d decoded, %d default starts\n", decoded["AmbientShirePub"] + decoded["AmbientPelargirDock1"], defaultPlayed
	exit !(quit && !defaultPlayed && played["AmbientShirePub"] == 1 && played["AmbientPelargirDock1"] == 1 &&
		decoded["AmbientShirePub"] == 1 && decoded["AmbientPelargirDock1"] == 1)
}
