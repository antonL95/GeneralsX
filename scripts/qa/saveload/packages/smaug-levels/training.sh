check() {
    if covers 235; then
        expect "SMAUG-LEVELS frame=235 button=Command_DrogothWingBlast level3=1 level6=1 ready=1 valid=1" || return
        expect "SMAUG-LEVELS frame=235 button=Command_DrogothFireflight level3=1 level6=1 ready=1 valid=1" || return
    fi
    if covers 244; then
        expect "SMAUG-LEVELS frame=244 button=Command_DrogothWingBlast level3=1 level6=1 ready=1 valid=1" || return
        expect "SMAUG-LEVELS frame=244 button=Command_DrogothFireflight level3=1 level6=1 ready=1 valid=1" || return
    fi
    if covers 250; then
        expect "SMAUG-LEVELS frame=250 button=Command_DrogothWingBlast level3=1 level6=1 ready=1 valid=1" || return
    fi
    if covers 400; then
        expect "SMAUG-LEVELS frame=400 button=Command_DrogothFireflight level3=1 level6=1 ready=1 valid=1" || return
    fi
}
