check() (
    if covers 243; then
        expect 'TEST stance-coverage 233 HoldGround Amoeba 2 Battle'
        expect 'TEST stance-coverage 243 HoldGroundMoving - 2 Battle'
    fi
    if covers 392; then
        expect 'TEST stance-coverage 250 HoldGroundMoving - 2 Battle'
        expect 'TEST stance-coverage 392 HoldGround Amoeba 2 Battle'
    fi
)
