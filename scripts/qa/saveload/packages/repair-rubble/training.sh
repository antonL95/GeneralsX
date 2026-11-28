check() {
	if covers 205; then
		expect 'TEST rr wall frame=205 health=200.0 dead=0 rubble=0 really=0 pristine=1 bookending=0 nowalk=0 nocoll=0 benefactor=0'
		expect 'TEST rr cell frame=205 dx=0 dy=0 owned=1'
		expect 'TEST rr cell frame=205 dx=35 dy=0 owned=1'
	fi
	if covers 212; then
		expect 'TEST rr wall frame=212 health=0.0 dead=1 rubble=1 really=0 pristine=0 bookending=1 nowalk=1 nocoll=1 benefactor=0'
		expect 'TEST rr cell frame=212 dx=0 dy=0 owned=0'
		expect 'TEST rr cell frame=212 dx=35 dy=0 owned=1'
	fi
	if covers 216; then
		expect 'TEST rr count frame=216 template=RrSideMarker count=1'
	fi
	if covers 222; then
		expect 'TEST rr count frame=222 template=RrSideMarker count=1'
	fi
	if covers 226; then
		expect 'TEST rr plot frame=226 name=RrNormalPlot unselectable=1 hidden=1'
		expect 'TEST rr plot frame=226 name=RrWallUpgradePlot unselectable=0 hidden=0'
		expect 'TEST rr count frame=226 template=RrRemovableProp count=0'
		expect 'TEST rr count frame=226 template=RrTaintObject count=1'
	fi
	if covers 228; then
		expect 'TEST rr gate frame=228 sound=258'
	fi
	if covers 246; then
		expect 'TEST rr wall frame=246 health=0.0 dead=1 rubble=1 really=0 pristine=0 bookending=1 nowalk=1 nocoll=1 benefactor=0'
		expect 'TEST rr cell frame=246 dx=0 dy=0 owned=0'
		expect 'TEST rr cell frame=246 dx=35 dy=0 owned=1'
	fi
	if covers 260; then
		expect 'TEST rr worker frame=260 ordinal=0 task=1'
		expect 'TEST rr worker frame=260 ordinal=1 task=1'
		grep -E '^TEST rr wall frame=260 health=[0-9]+\.[1-9] dead=0 rubble=0 really=1 pristine=0 bookending=0 nowalk=1 nocoll=0 benefactor=0$' "$log" ||
			grep -E '^TEST rr wall frame=260 health=[1-9][0-9]*\.[0-9] dead=0 rubble=0 really=1 pristine=0 bookending=0 nowalk=1 nocoll=0 benefactor=0$' "$log" ||
			{ printf 'Expected wall repaired out of rubble by swarm workers at frame 260\n'; return 1; }
	fi
	if covers 360; then
		expect 'TEST rr wall frame=360 health=200.0 dead=0 rubble=0 really=0 pristine=1 bookending=0 nowalk=0 nocoll=0 benefactor=0'
		expect 'TEST rr cell frame=360 dx=0 dy=0 owned=1'
		expect 'TEST rr rise frame=360 height=0.0 postrubble=0 postcollapse=1'
	fi
}
