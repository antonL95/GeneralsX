prepare() {
    if grep -Fq ' create Player_2 MapScriptDozerFactory ' "$out/commands.txt"; then
        printf '%s\n' 'set environment GENERALSX_RETAIL_SKIRMISH_AI brutal' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/start.gdb"
        printf '%s\n' 'set environment GENERALSX_SKIRMISH_AI_TRACE 1' >> "$out/resume.gdb"
    fi
}

check() {
    if ! grep -Fq ' create Player_2 MapScriptDozerFactory ' "$out/commands.txt"; then
        return 0
    fi
    if covers 64; then
        expect "$(printf 'AI_TRAIN\t64\t4\tMapScriptDozerTrainAssigned\tMapScriptDozerReplacement\t1')"
    fi
    for frame in 66 70; do
        if covers "$frame"; then
            expect "TEST map-ai-dozer frame=$frame assigned=MapScriptDozerReplacement base=MordorPorter queued=MapScriptDozerReplacement trained=0"
        fi
    done
    if covers 105; then
        expect 'TEST map-ai-dozer frame=105 assigned=MapScriptDozerReplacement base=MordorPorter queued=none trained=1'
    fi
}
