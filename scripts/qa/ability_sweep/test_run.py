import importlib.util
import io
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch
from contextlib import redirect_stderr

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
        values["ended"] = "unbounded"
        self.assertIn("ability duration has no observable authored bound", sweep.judge(values)[1])

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

    def test_timeout_after_completed_cases_does_not_retry_interrupted_case(self):
        cases = [dict(id=index, faction="Wild", unit="unit", hero="1", command_set="set",
                      button="button", command="SPECIAL_POWER") for index in range(1, 4)]
        rows = []
        writer = type("Writer", (), {"writerow": lambda self, row: rows.append(row)})()
        sessions = [("first", "exit 137", True, ""), ("second", None, False, "")]
        with patch.object(sweep, "session", side_effect=sessions) as session, \
             patch.object(sweep, "parse", side_effect=[{1: {}}, {3: {}}]), \
             patch.object(sweep, "result_row", side_effect=lambda case, values: [case["id"]]), \
             redirect_stderr(io.StringIO()):
            sweep.run_faction(Path("/build"), "Wild", cases, writer)
        self.assertEqual([case["id"] for case in session.call_args_list[1].args[1]], [3])
        self.assertEqual(rows[1][7], "not run")

    def test_single_session_keeps_unfinished_cases(self):
        cases = io.StringIO("faction\tunit\thero\tcommand_set\tbutton\tcommand\ttodo\n"
                            "Wild\tone\t1\tset\tbutton\tSPECIAL_POWER\t-\n"
                            "Wild\ttwo\t1\tset\tbutton\tSPECIAL_POWER\t-\n")
        output = io.StringIO()
        with patch.object(sweep.sys, "stdin", cases), patch.object(sweep.sys, "stdout", output), \
             patch.object(sweep.sys, "argv", ["run.py", "/build", "--single-session"]), \
             patch.object(sweep, "session", return_value=("", "exit 137", True, "frame_stalled=90")) as session:
            sweep.main()
        session.assert_called_once()
        rows = list(sweep.csv.DictReader(io.StringIO(output.getvalue()), delimiter="\t"))
        self.assertEqual([row["verdict"] for row in rows], ["hang", "not run"])


if __name__ == "__main__":
    unittest.main()
