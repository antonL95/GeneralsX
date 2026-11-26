import importlib.util
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("sweep", Path(__file__).with_name("run.py"))
sweep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sweep)


class VerdictTest(unittest.TestCase):
    def test_verdict_reasons(self):
        values = dict(result="accepted", caster="-", ally="-", enemy="-", structure="-",
                      objects="summon", flags="-", animation="CAST", recharge="n/a:zero_reload",
                      ended="none")
        self.assertEqual(sweep.judge(values), ("ok", ["effect observed; recharge not required (zero reload)"]))
        values["recharge"] = "no"
        self.assertIn("authored reload did not start", sweep.judge(values)[1])
        values["recharge"] = "pending"
        self.assertEqual(sweep.judge(values)[0], "ok")
        values["ended"] = "never"
        self.assertIn("ability state exceeded authored duration", sweep.judge(values)[1])

    def test_repeated_log_stops_session(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "session.log"
            log.write_text("TEST ability-heartbeat frame=696\n" + "repeated\n" * 4)
            finished = threading.Event()
            stalled = threading.Event()
            reason = []
            with patch.object(finished, "wait", return_value=False), patch.object(sweep, "REPEAT_LIMIT", 3), patch.object(sweep, "kill_engine") as kill:
                sweep.watch_for_stall(log, finished, stalled, reason)
            self.assertTrue(stalled.is_set())
            self.assertIn("frame=696", reason[0])
            kill.assert_called_once_with()


if __name__ == "__main__":
    unittest.main()
