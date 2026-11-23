render_env = {
    "PLAYTEST_FACTION": "FactionArnor",
    "PLAYTEST_STATIC_CAPTURES": "1",
}


def check(log):
    dogs = [
        line.split()
        for line in log.splitlines()
        if line.startswith("TEST maggot-dog ")
    ]
    if len({dog[4] for dog in dogs if dog[2] == "575" and dog[13] == "0"}) != 3:
        raise RuntimeError("Expected three untargeted hounds after recruiting Farmer Maggot")
    if not any(dog[13] != "0" and dog[14] == "1" for dog in dogs):
        raise RuntimeError("Expected the hounds to reach the attack target")
