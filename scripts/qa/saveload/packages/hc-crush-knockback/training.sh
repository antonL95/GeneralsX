check() (
    if covers 243; then
        grep -Eq '^TEST crush-knockback scan 240 (18|24)$' "$log"
        expect 'TEST crush-knockback brace 240 ahead 1'
        expect 'TEST crush-knockback brace 240 sideways 0'
        expect 'TEST crush-knockback brace 240 beyond 0'
        expect 'TEST crush-knockback marker 243 CrushCoverageSmallMarker 1'
        expect 'TEST crush-knockback marker 243 CrushCoverageMediumMarker 0'
        expect 'TEST crush-knockback marker 243 CrushCoverageLargeMarker 0'
    fi
    if covers 251; then
        expect 'TEST crush-knockback marker 247 CrushCoverageSmallMarker 1'
        expect 'TEST crush-knockback marker 249 CrushCoverageMediumMarker 1'
        expect 'TEST crush-knockback marker 249 CrushCoverageLargeMarker 0'
        expect 'TEST crush-knockback marker 251 CrushCoverageLargeMarker 1'
    fi
)
