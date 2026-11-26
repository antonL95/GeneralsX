import re


def check(log):
    if not re.search(r"AUDIO TRACE frame=\d+ kind=start owner=\d+ handle=\d+ event=RainStereoLoop\b", log):
        raise RuntimeError("The authored RainStereoLoop streamed sound did not start")
