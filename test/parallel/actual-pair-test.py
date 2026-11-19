#!/usr/bin/env python3
"""Exercise the real upstream helper through direct and forking legacy paths.

Pass a helper built from upstream 633c1bf with --old-helper. All PAM, passwd,
/sys and /dev paths are replaced inside a disposable namespace.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--old-helper', type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
old = args.old_helper.resolve()
assert (build / 'ActualPairTest').is_file() and old.is_file()
print('upstream helper sha256:', hashlib.sha256(old.read_bytes()).hexdigest(), flush=True)
with tempfile.TemporaryDirectory(prefix='v6-actual-pair-') as temp:
    fixture = Path(temp)
    (fixture / 'pam').mkdir()
    (fixture / 'tmp').mkdir()
    (fixture / 'passwd').write_text('root:x:0:0:Fixture:/tmp/fixture:/bin/sh\nold:x:0:0:Old:/tmp/fixture:/bin/sh\n')
    (fixture / 'group').write_text('root:x:0:\n')
    policy = 'auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_exec.so /usr/bin/touch /tmp/fixture/session-opened\n'
    for name in ('sddm', 'sddm-password', 'sddm-fingerprint'):
        (fixture / 'pam' / name).write_text(policy)
    (fixture / 'wayland-session').write_text('#!/bin/sh\nexec "$@"\n')
    (fixture / 'wayland-session').chmod(0o755)
    pair = build / 'pair-bin'
    pair.mkdir(exist_ok=True)
    shutil.copy2(old, pair / 'old-helper')
    path = pair / 'sddm-helper'
    def run(test, mode='normal', action='none'):
        command = ['/usr/bin/bwrap', '--unshare-user', '--uid', '0', '--gid', '0',
                   '--unshare-pid', '--unshare-net', '--unshare-ipc', '--unshare-uts', '--die-with-parent',
                   '--ro-bind', '/', '/', '--tmpfs', '/tmp', '--tmpfs', '/sys', '--proc', '/proc', '--dev', '/dev',
                   '--bind', str(build), str(build), '--bind', str(fixture), '/tmp/fixture',
                   '--ro-bind', str(fixture / 'pam'), '/etc/pam.d',
                   '--ro-bind', str(fixture / 'passwd'), '/etc/passwd',
                   '--ro-bind', str(fixture / 'group'), '/etc/group',
                   '--ro-bind', str(fixture / 'wayland-session'), '/usr/share/sddm/scripts/wayland-session',
                   '--setenv', 'PAIR_FIXTURE', '/tmp/fixture', '--setenv', 'PAIR_MODE', mode,
                   '--setenv', 'PAIR_ACTION', action, '--setenv', 'TMPDIR', '/tmp/fixture/tmp',
                   '/bin/bash', '-euo', 'pipefail', '-c',
                   'test ! -e /sys/bus/platform/drivers/apple_sep; test ! -e /dev/sep-bio; exec "$@"',
                   'fixture', str(build / 'ActualPairTest'), test]
        subprocess.run(command, check=True, timeout=35)
    shutil.copy2(old, path)
    print('current embedded helper: PAM denial preserves normal failure and capability', flush=True)
    for name in ('sddm-password', 'sddm-fingerprint'):
        (fixture / 'pam' / name).write_text('auth required pam_deny.so\naccount required pam_permit.so\nsession required pam_exec.so /usr/bin/touch /tmp/fixture/session-opened\n')
    for _ in range(10):
        run('currentHelperPamDenialIsNotACrash')
    for name in ('sddm-password', 'sddm-fingerprint'):
        (fixture / 'pam' / name).write_text(policy)
    print('direct upstream helper: legacy framing', flush=True)
    run('oldLegacyLogin')
    print('direct upstream helper: cancellation during success callback', flush=True)
    run('cancelOnLegacySuccessDoesNotOpenSession')
    print('direct upstream helper: parallel uses embedded current helper and denies approval', flush=True)
    run('oldOrWrapperIsNotUsedForParallel')
    print('direct upstream helper: approved parallel uses embedded current helper', flush=True)
    run('approvedParallelUsesCurrentEmbeddedHelper')
    (fixture / 'pam' / 'sddm').write_text('auth required pam_deny.so\naccount required pam_permit.so\n')
    print('failed legacy authentication permits later parallel capability', flush=True)
    for _ in range(10):
        run('failedLegacyAllowsParallel')
    (fixture / 'pam' / 'sddm').write_text(policy)
    path.unlink()
    path.write_text('#!/bin/bash\nprintf invoked >"$PAIR_FIXTURE/old-invoked"\n"' + str(pair / 'old-helper') + '" "$@" &\nwait $!\n')
    path.chmod(0o755)
    print('forking wrapper around upstream helper: legacy framing', flush=True)
    run('oldLegacyLogin')
    print('forking wrapper: cancellation during success callback', flush=True)
    run('cancelOnLegacySuccessDoesNotOpenSession')
    print('forking wrapper around upstream helper: never executed in parallel mode', flush=True)
    run('oldOrWrapperIsNotUsedForParallel')
    path.unlink()
    path.write_text('#!/bin/bash\nprintf invoked >"$PAIR_FIXTURE/old-invoked"\nexec /usr/bin/bwrap --unshare-user --uid 0 --gid 0 --unshare-net --unshare-ipc --unshare-uts --die-with-parent --ro-bind / / --tmpfs /sys --proc /proc --dev /dev "' + str(pair / 'old-helper') + '" "$@"\n')
    path.chmod(0o755)
    print('original-style bwrap launcher: never executed in parallel mode', flush=True)
    run('oldOrWrapperIsNotUsedForParallel')
    (fixture / 'mark-session').write_text('#!/bin/sh\ntouch "/tmp/fixture/session-$PAM_USER"\n')
    (fixture / 'mark-session').chmod(0o755)
    (fixture / 'pam' / 'sddm').write_text('auth required pam_permit.so\naccount required pam_permit.so\nsession required pam_exec.so /tmp/fixture/mark-session\n')
    path.unlink()
    path.write_text('#!/bin/bash\nif [ ! -e /tmp/fixture/first-started ]; then\n touch /tmp/fixture/first-started\n ( sleep 0.6; touch /tmp/fixture/old-child-started; exec "' + str(pair / 'old-helper') + '" "$@" ) &\n sleep 10\nelse\n exec "' + str(pair / 'old-helper') + '" "$@"\nfi\n')
    path.chmod(0o755)
    print('cancelled old attempt, fresh-ID retry, then late old child', flush=True)
    run('cancelledLegacyCanRetryWithoutAcceptingOldChild')
    assert not (fixture / 'session-old').exists(), 'old child opened a session after retry'
    (fixture / 'pam' / 'sddm').write_text(policy)
    path.unlink()
    path.write_text('#!/bin/bash\n( sleep 0.4; touch /tmp/fixture/descendant-started; exec "' + str(pair / 'old-helper') + '" "$@" ) &\nexit 0\n')
    path.chmod(0o755)
    print('detached wrapper exits before delayed helper connects: parallel remains disabled', flush=True)
    run('detachedLegacyNeverEnablesParallel', 'before')
    assert not (fixture / 'session-opened').exists(), 'rejected descendant opened PAM after namespace exit'
    (fixture / 'descendant-started').unlink(missing_ok=True)
    print('stable peer handle unavailable: rejected socket stays open', flush=True)
    run('detachedLegacyNeverEnablesParallel', 'before-no-pidfd')
    assert not (fixture / 'session-opened').exists(), 'quarantined descendant opened PAM after namespace exit'
    (fixture / 'hold-auth').write_text('#!/bin/sh\ntouch /tmp/fixture/pam-entered\nwhile [ ! -e /tmp/fixture/continue ]; do /usr/bin/sleep 0.05; done\n')
    (fixture / 'hold-auth').chmod(0o755)
    (fixture / 'pam' / 'sddm').write_text('auth required pam_exec.so /tmp/fixture/hold-auth\n' + policy)
    path.unlink()
    path.write_text('#!/bin/bash\n"' + str(pair / 'old-helper') + '" "$@" &\nwhile [ ! -e /tmp/fixture/pam-entered ]; do sleep 0.02; done\nexit 0\n')
    path.chmod(0o755)
    (fixture / 'continue').unlink(missing_ok=True)
    print('detached wrapper exits after helper connects but before PAM result: parallel remains disabled', flush=True)
    run('detachedLegacyNeverEnablesParallel', 'after')
    for action in ('cancel', 'destroy'):
        (fixture / 'descendant-started').unlink(missing_ok=True)
        path.unlink()
        path.write_text('#!/bin/bash\n( sleep 0.4; touch /tmp/fixture/descendant-started; exec "' + str(pair / 'old-helper') + '" "$@" ) &\nsleep 10\n')
        path.chmod(0o755)
        print(f'{action} wrapper before delayed connection: parallel remains disabled', flush=True)
        run('destroyedOrCancelledLegacyNeverEnablesParallel', 'before', action)
        (fixture / 'pam-entered').unlink(missing_ok=True)
        (fixture / 'continue').unlink(missing_ok=True)
        path.unlink()
        path.write_text('#!/bin/bash\n"' + str(pair / 'old-helper') + '" "$@" &\nsleep 10\n')
        path.chmod(0o755)
        print(f'{action} wrapper after connection with PAM pending: parallel remains disabled', flush=True)
        run('destroyedOrCancelledLegacyNeverEnablesParallel', 'after', action)
    shutil.copy2(Path(__file__).with_name('legacy-slow-peer.py'), pair / 'legacy-slow-peer.py')
    for mode in ('direct-peer', 'forked-peer'):
        for kind in ('partial', 'oversized', 'invalid-opcode'):
            (fixture / 'peer-child').unlink(missing_ok=True)
            (fixture / 'peer-saw-eof').unlink(missing_ok=True)
            path.unlink()
            launch = '/usr/bin/python3 -I "' + str(pair / 'legacy-slow-peer.py') + '" "$@"'
            path.write_text('#!/bin/bash\n' + (('exec ' + launch + '\n') if mode == 'direct-peer' else (launch + ' &\nsleep 20\n')))
            path.chmod(0o755)
            print(f'{mode} {kind} hello with inherited socket: no EOF while daemon runs', flush=True)
            run('truncatedLegacyPeerDoesNotSeeEof', kind)
            if (fixture / 'peer-saw-eof').exists():
                print('shutdown boundary observed: inherited socket received EOF after daemon exit', flush=True)
            else:
                print('shutdown boundary not observed in this run; namespace teardown is not a guarantee', flush=True)
