$1 == "VIDEO" && $2 == "PROBE" {
	print
	if ($4 != "decoded" || $6 < 32) next
	if ($3 == "LoadingRing" && $8 < 255 && $9 > 0 && $11 == 140) ring++
	if ($3 == "movieCredits" && $8 < 255 && $9 > 0 && $11 == 2141) credits++
	if ($3 == "movieSkirmishGondor" && $8 == 255 && $9 == 255 && $11 >= $6) skirmish++
}
$0 == "TEST frame 19: quit" { quit = 1 }
END {
	printf "VP6 streams: %d of 3 decoded with expected alpha\n", ring + credits + skirmish
	exit !(quit && ring == 1 && credits == 1 && skirmish == 1)
}
