def check(log):
    seats = [
        dict(field.split("=", 1) for field in line.split()[2:])
        for line in log.splitlines()
        if line.startswith("TEST tc ") and " action=seat " in line
    ]
    matriarch = [seat for seat in seats if seat["label"] == "matriarch" and seat["bone"].startswith("H4")]
    if len(matriarch) != 5:
        raise RuntimeError(f"Expected five howdah riders: {seats}")
    for seat in matriarch:
        if seat["found"] != "1" or seat["error"] != "0" or not 100 <= int(seat["height"]) < 150:
            raise RuntimeError(f"Rider is not in the howdah: {seat}")
        if seat["hidden"] != "0" or seat["opacity"] != "100":
            raise RuntimeError(f"Rider is not visible: {seat}")
