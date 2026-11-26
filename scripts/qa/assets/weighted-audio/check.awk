$1 == "AUDIO" && $2 == "REQUEST" {
	if (!seen[$3]++) events++
	requests++
	if (!$NF) missing++
}
$1 == "AUDIO" && $2 == "PICK" {
	picks[$3, $4]++
	if ($3 == "Slot7WeightedAudio" && tolower($5) ~ /eushelo_voidiea\.wav$/) highFile++
	if ($3 == "Slot7DefaultAudioWeight" && tolower($5) ~ /eushelo_voidiea\.wav$/) defaultWeight++
	if ($3 == "Slot7WeightedMulti" && ($5 == "SX_BFME2_Ba1Dwarf01" || $5 == "SX_BFME2_Ba1Goblin01")) high[$4]++
	if ($3 == "Slot7WeightedPlaylist") {
		if ($5 == previous[$4] || ($5 != "SX_BFME2_Ba1Dwarf01" && $5 != "SX_BFME2_Ba1Dwarf02")) badPlaylist++
		previous[$4] = $5
	}
}
$0 == "TEST frame 22: quit" { quit = 1 }
END {
	printf "Audio requests: %d events, %d files, %d unresolved\n", events, requests, missing
	printf "Weighted picks: sound %d/64, expand %d/64, track %d/64\n", highFile, high["expand"], high["track"]
	printf "Default weight picks: %d/64\n", defaultWeight
	printf "Playlist: expand %d picks, track %d picks, %d bad selections\n",
		picks["Slot7WeightedPlaylist", "expand"], picks["Slot7WeightedPlaylist", "track"], badPlaylist
	exit !(events == 64 && requests >= 303 && missing == 0 && quit &&
		picks["Slot7WeightedAudio", "file"] == 64 && highFile >= 62 &&
		picks["Slot7DefaultAudioWeight", "file"] == 64 && defaultWeight >= 60 &&
		picks["Slot7WeightedMulti", "expand"] == 64 && high["expand"] >= 62 &&
		picks["Slot7WeightedMulti", "track"] == 64 && high["track"] >= 62 &&
		picks["Slot7WeightedPlaylist", "expand"] == 64 && picks["Slot7WeightedPlaylist", "track"] == 64 && !badPlaylist)
}
