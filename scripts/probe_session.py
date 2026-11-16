"""Lifetime policy and cooperative stop for the project's owned game session."""
import json
from project_paths import inside,write_text

ACTIVE='cache/active-session.json'


def wait_timeout(mode,seconds):
    if seconds==0 and mode in ('native-vr','native-replay'):
        return None
    if not 5<=seconds<=180:
        raise ValueError('Use 0 for manual-exit native-vr/native-replay, or 5..180 seconds for diagnostics')
    return seconds+40


def register(profile,observer_pid):
    path=inside(profile)
    if path.parent!=inside('cache/profiles') or not path.is_dir():
        raise ValueError('Session must use a private HP1 probe profile')
    record=dict(profile=profile,observer_pid=observer_pid)
    write_text(ACTIVE,json.dumps(record)+'\n',exclusive=False)
    return record


def unregister(record):
    path=inside(ACTIVE)
    if path.exists() and json.loads(path.read_text())==record:
        path.unlink()


def request_stop():
    path=inside(ACTIVE)
    if not path.exists():
        return False
    record=json.loads(path.read_text())
    profile=inside(record['profile'])
    if profile.parent!=inside('cache/profiles') or not profile.is_dir():
        raise ValueError('Invalid active HP1 profile')
    # The observer watches only its own profile and closes only its own child.
    write_text(profile/'stop-session.flag','close owned HP1 session\n',exclusive=False)
    return True
