import sys
import unittest
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from probe_session import wait_timeout


class SessionLifetimeTest(unittest.TestCase):
    def test_manual_vr_has_no_python_timeout(self):
        self.assertIsNone(wait_timeout('native-vr',0))

    def test_same_policy_can_be_exercised_without_headset(self):
        self.assertIsNone(wait_timeout('native-replay',0))

    def test_explicit_diagnostic_keeps_deadline(self):
        self.assertEqual(wait_timeout('native-vr',60),100)

    def test_other_probes_remain_bounded(self):
        for mode in ('observe','shift-x8','native-control','native-capture','native-input','native-gameplay'):
            with self.subTest(mode=mode):
                with self.assertRaises(ValueError):wait_timeout(mode,0)

    def test_invalid_duration_rejected(self):
        for value in (-1,1,4,181):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):wait_timeout('native-vr',value)


if __name__=='__main__':
    unittest.main()
