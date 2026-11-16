import json
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
import play_sessions as sessions
from project_paths import inside, write_text


class PersistentPlayTests(unittest.TestCase):
    def test_progress_survives_interrupted_session_and_mode_switch(self):
        state_path = inside(sessions.STATE)
        before = state_path.read_bytes() if state_path.exists() else None
        first = sessions.prepare('Flat', diagnostic=True)
        sessions.checked_copy(inside('Harry Potter TM/System/Core.u'), first/'Save/save0.usa')
        write_text(first/'GameSaveInfo0.dat', 'inventory metadata')
        settings = (first/'HP.ini').read_text(encoding='utf-8')+'\n[LauncherTest]\nFlatOnly=kept\n'
        write_text(first/'HP.ini', settings, exclusive=False)
        expected = sessions.digest_files(first)
        second = sessions.prepare('VR', {'profile':first.relative_to(sessions.ROOT).as_posix(),'mode':'Flat','status':'interrupted'}, diagnostic=True)
        self.assertEqual(sessions.digest_files(second), expected)
        self.assertIn('WindowedViewportX=2048', (second/'HP.ini').read_text(encoding='utf-8'))
        self.assertIn('FlatOnly=kept', (second/'Flat.ini').read_text(encoding='utf-8'))
        third = sessions.prepare('Flat', {'profile':second.relative_to(sessions.ROOT).as_posix(),'mode':'VR'}, diagnostic=True)
        self.assertEqual(sessions.digest_files(third), expected)
        flat = (third/'HP.ini').read_text(encoding='utf-8')
        self.assertIn('WindowedViewportX=1024', flat)
        self.assertIn('FlatOnly=kept', flat)
        self.assertNotIn(str(first), flat)
        self.assertNotIn(str(second), flat)
        self.assertEqual(state_path.read_bytes() if state_path.exists() else None, before, 'diagnostics cannot replace real progress')
        snapshot = sessions.snapshot(third, 'unit_test')
        self.assertEqual(json.loads((snapshot/'manifest.json').read_text(encoding='utf-8'))['files'], expected)
        write_text(third/'Save/Save.tmp', 'partially written next checkpoint')
        self.assertEqual(sessions.digest_files(third), expected, 'temporary save cannot enter a backup')
        (third/'Save/save0.usa').write_bytes(b'truncated')
        with self.assertRaises(ValueError): sessions.snapshot(third, 'must_not_publish')

    def test_paths_are_rebased_without_unknown_writable_searches(self):
        profile = inside('cache/profiles/settings-unit')
        with self.assertRaises(ValueError): sessions.profile_ini('Paths=../Elsewhere/*.xyz\n', profile, 'VR')
        text = sessions.profile_ini('SavePath=bad\nCachePath=bad\nPaths=../save/*.usa\nPaths=../Maps/*.unr\n', profile, 'Flat')
        self.assertIn(str(profile/'Save/*.usa'), text)
        self.assertIn(str(inside('Harry Potter TM/Maps/*.unr')), text)

    def test_duplicate_launcher_cannot_start_another_game(self):
        with sessions.SessionLock():
            with self.assertRaises(ValueError):
                with sessions.SessionLock(): pass


if __name__ == '__main__': unittest.main()
