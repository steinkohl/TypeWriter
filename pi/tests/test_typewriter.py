import socket
import threading
import unittest

from typewriter import keymap
from typewriter.daemon import Config, JobServer, Typewriter
from typewriter.link import Link, LinkError
from typewriter.sim import FakeMega
from typewriter.text import decode, prepare


class KeymapTest(unittest.TestCase):
    def test_layers(self):
        self.assertEqual(keymap.taps_for_char("e"), [keymap.Tap(((0, 3),))])
        self.assertEqual(keymap.taps_for_char("E"), [keymap.Tap(((8, 7), (0, 3)))])
        self.assertEqual(keymap.taps_for_char("Ä"), [keymap.Tap(((8, 7), (8, 5)))])
        self.assertEqual(keymap.taps_for_char("µ"), [keymap.Tap(((7, 9), (3, 1)))])
        self.assertIsNone(keymap.taps_for_char("€"))

    def test_no_key_on_non_matrix_rows(self):
        for table in (keymap.BASE, keymap.SHIFTED, keymap.CODED, keymap.FUNCTIONS):
            for row, col in table.values():
                self.assertNotIn(row, (4, 5))
                self.assertIn(col, range(10))

    def test_no_duplicate_keys_per_layer(self):
        for table in (keymap.BASE, keymap.SHIFTED):
            self.assertEqual(len(set(table.values())), len(table))

    def test_accents(self):
        acute, grave = keymap.Tap(((8, 0),)), keymap.Tap(((8, 7), (8, 0)))
        self.assertEqual(keymap.taps_for_char("é"), [acute, keymap.Tap(((0, 3),))])
        self.assertEqual(keymap.taps_for_char("À"), [grave, keymap.Tap(((8, 7), (3, 4)))])
        self.assertEqual(len(keymap.taps_for_char("ô")), 5)

    def test_fallbacks(self):
        fake = FakeMega()
        link = Link(fake)
        for tap in keymap.taps_for_text("„Hi“ – ça 5€ ☃"):
            link.tap(tap.keys, 0)
        self.assertEqual(fake.text, '"Hi" - ca 5EUR ?')
        link.close()


class TextTest(unittest.TestCase):
    def test_wrap_and_blank_lines(self):
        text = prepare("a\r\n\r\n" + "word " * 30 + "\n", width=20)
        lines = text.split("\n")
        self.assertEqual(lines[:2], ["a", ""])
        self.assertTrue(all(len(line) <= 20 for line in lines))
        self.assertTrue(text.endswith("word\n"))

    def test_control_characters(self):
        self.assertEqual(prepare("a\x07b\tc\x1b"), "ab      c\n")
        self.assertEqual(prepare("page1\x0c"), "page1\x0c")
        self.assertEqual(prepare("  \n\n"), "")

    def test_decode(self):
        self.assertEqual(decode("Grüße".encode("utf-8")), "Grüße")
        self.assertEqual(decode("Grüße".encode("latin-1")), "Grüße")


class LinkTest(unittest.TestCase):
    def test_commands_and_events(self):
        events = []
        fake = FakeMega()
        link = Link(fake, on_key=lambda down, key: events.append((down, key)))
        self.assertTrue(link.wait_hello(2))
        link.set_passthrough(False)
        self.assertFalse(fake.passthrough)
        link.tap([(8, 7), (0, 3)], 10)
        self.assertEqual(fake.commands[-1], "T 8703 10")
        self.assertEqual(link.query_status()["passthrough"], "0")
        fake.press((7, 9))
        link.command("?")  # make sure the reader got past the event
        self.assertEqual(events, [(True, (7, 9))])
        with self.assertRaises(LinkError):
            link.command("bogus")
        fake.scanning = False
        with self.assertRaises(LinkError):
            link.tap([(0, 0)], 0)
        link.close()


class DaemonTest(unittest.TestCase):
    def setUp(self):
        self.fake = FakeMega()
        self.tw = Typewriter(Config(key_gap_ms=1, return_gap_ms=3, width=30), self.fake)
        self.worker = threading.Thread(target=self.tw.run, daemon=True)
        self.worker.start()

    def tearDown(self):
        self.tw.stop()
        self.worker.join(5)

    def test_job_round_trip(self):
        self.tw.submit("Füße & Öl: 100%\nZeile 2".encode("utf-8"))
        self.tw.wait_idle()
        self.assertEqual(self.fake.text, "\nFüße & Öl: 100%\nZeile 2\n")
        self.assertTrue(self.fake.passthrough)
        self.assertIn("P 0", self.fake.commands)
        self.assertEqual(self.fake.gaps[0], 3)   # return
        self.assertEqual(self.fake.gaps[1], 1)   # ordinary key

    def test_job_over_tcp(self):
        server = JobServer(("127.0.0.1", 0), self.tw)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        try:
            with socket.create_connection(server.server_address) as sock:
                sock.sendall(b"Hallo Welt\n")
            for _ in range(200):
                if self.fake.text.endswith("Welt\n"):
                    break
                threading.Event().wait(0.02)
            self.assertEqual(self.fake.text, "\nHallo Welt\n")
        finally:
            server.shutdown()
            server.server_close()

    def test_cancel_with_code_key(self):
        self.tw.config.key_gap_ms = 0
        original = self.fake._tap

        def tap_and_cancel(arg):
            original(arg)
            if len(self.fake.typed) == 5:
                self.fake.press(keymap.CODE)
                threading.Event().wait(0.2)

        self.fake._tap = tap_and_cancel
        self.tw.submit(b"x" * 25)
        self.tw.wait_idle()
        self.assertLess(len(self.fake.typed), 25)
        self.assertTrue(self.fake.passthrough)

    def test_typewriter_off(self):
        self.fake.scanning = False
        self.tw.submit(b"abc")
        self.tw.wait_idle()
        self.assertEqual(self.fake.text, "")


if __name__ == "__main__":
    unittest.main()
