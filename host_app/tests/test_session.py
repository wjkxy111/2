from __future__ import annotations

import csv
import tempfile
import threading
import time
import unittest
from pathlib import Path

from ra8d1_can_ids.models import MessageKind
from ra8d1_can_ids.protocol import parse_line
from ra8d1_can_ids.session import SessionRecorder, SessionReplay, load_jsonl


class SessionTests(unittest.TestCase):
    def test_bounded_recording_preserves_order_after_many_evictions(self) -> None:
        recorder = SessionRecorder(max_records=3)
        recorder.start()
        for index in range(100):
            recorder.ingest(parse_line(f"TELEM,T_MS,{index}", timestamp=float(index)))
        self.assertEqual([97, 98, 99], [m.get_int("T_MS") for m in recorder.snapshot()])
        self.assertEqual(97, recorder.dropped_records)
        recorder.stop()
        recorder.start(clear=False)
        recorder.ingest(parse_line("TELEM,T_MS,100", timestamp=100.0))
        self.assertEqual([98, 99, 100], [m.get_int("T_MS") for m in recorder.snapshot()])
        self.assertEqual(98, recorder.dropped_records)
        recorder.clear()
        self.assertEqual(0, recorder.count)
        self.assertEqual(0, recorder.dropped_records)
        recorder.stop()

    def test_replay_rejects_nonfinite_speed_and_skips_bad_timestamps(self) -> None:
        for speed in (float("nan"), float("inf"), -1.0):
            with self.assertRaises(ValueError):
                SessionReplay([]).play(lambda _: None, speed=speed)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "timestamps.jsonl"
            path.write_text(
                '{"kind":"TELEM","timestamp":"NaN"}\n'
                '{"kind":"TELEM","timestamp":"Infinity"}\n'
                '{"kind":"TELEM","timestamp":3}\n', encoding="utf-8",
            )
            self.assertEqual([3.0], [m.timestamp for m in load_jsonl(path)])

    def test_record_jsonl_load_and_replay(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "capture.jsonl"
            recorder = SessionRecorder(max_records=2)
            recorder.start(path)
            for index in range(3):
                recorder.ingest(parse_line(f"TELEM,T_MS,{index},MODE,LEARN", timestamp=10.0 + index))
            recorder.stop()

            self.assertEqual(2, recorder.count)
            self.assertEqual(1, recorder.dropped_records)
            loaded = load_jsonl(path)
            self.assertEqual(3, len(loaded))
            self.assertEqual(0, loaded[0].get_int("T_MS"))

            delivered = []
            sleeps = []
            count = SessionReplay(loaded).play(delivered.append, speed=2.0, sleep=sleeps.append)
            self.assertEqual(3, count)
            self.assertEqual([0.5, 0.5], sleeps)
            self.assertEqual(loaded, delivered)

    def test_csv_is_rectangular_utf8_and_bad_json_is_skipped(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            recorder = SessionRecorder()
            recorder.start()
            recorder.ingest(parse_line("EVENT,T+12,ID,0x100,DLC,8,SCORE,650,REASON,0x01", timestamp=1.0))
            recorder.ingest(parse_line("TELEM,T_MS,13,MODE,MONITOR,FPS,100", timestamp=2.0))
            csv_path = recorder.export_csv(Path(temporary) / "export.csv")
            recorder.stop()

            with csv_path.open("r", encoding="utf-8-sig", newline="") as source:
                rows = list(csv.DictReader(source))
            self.assertEqual(2, len(rows))
            self.assertIn("REASON", rows[0])
            self.assertEqual("EVENT", rows[0]["kind"])

            broken = Path(temporary) / "broken.jsonl"
            broken.write_text('{"kind":"TELEM","timestamp":1,"fields":{}}\n{broken', encoding="utf-8")
            loaded = load_jsonl(broken)
            self.assertEqual(1, len(loaded))
            self.assertEqual(MessageKind.TELEM, loaded[0].kind)

    def test_replay_stop_interrupts_a_long_timing_gap(self) -> None:
        records = [
            parse_line("TELEM,T_MS,0,MODE,LEARN", timestamp=1.0),
            parse_line("TELEM,T_MS,100000,MODE,MONITOR", timestamp=101.0),
        ]
        stop = threading.Event()
        delivered = []
        result = []
        thread = threading.Thread(
            target=lambda: result.append(
                SessionReplay(records).play(delivered.append, stop_event=stop)
            )
        )
        thread.start()
        deadline = time.monotonic() + 1.0
        while not delivered and time.monotonic() < deadline:
            time.sleep(0.005)
        stop.set()
        thread.join(0.5)
        self.assertFalse(thread.is_alive())
        self.assertEqual(1, result[0])
        self.assertEqual(1, len(delivered))


if __name__ == "__main__":
    unittest.main()
