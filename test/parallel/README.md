# Parallel authentication review tests

These standalone Qt 6 tests exercise the opt-in prototype without a live fingerprint reader, real account credentials or a real graphical session. They are review fixtures, not a claim of multi-seat or distribution qualification. Provide a native SDDM build directory with generated D-Bus interfaces, then run. The helper used by `RealHelper` is compiled directly from this checkout by the standalone project:

```sh
cmake -S test/parallel -B build/parallel -DSDDM_NATIVE_BUILD="$PWD/build"
cmake --build build/parallel -j2
ctest --test-dir build/parallel --output-on-failure
```

Requirements beyond the native build: Qt 6 Test, `dbus-run-session`, Bubblewrap and unprivileged user namespaces. A missing dependency is a test failure, not a silent pass.

- `ParallelAuth` uses production Auth objects with a synthetic helper embedded in the test executable. It covers concurrent results, cancellation, malformed frames, sealed-image immutability, fast hello ordering, listener reentrancy and failed-start cleanup.
- `RealHelper` runs the helper compiled from the current source in isolated user/PID/network/IPC namespaces. Temporary PAM/account files permit or reject synthetic authentication and session admission. It never changes host PAM or account files.
- `SocketPermission` checks the auth socket under a permissive umask; running it in a mapped-root namespace also checks denial to a different UID. `NoPeerAuth` and `NoSyscallAuth` compile the unsupported fallback and exercise ordinary password framing.
- `LoginMessages` writes through production SocketWriter and checks reply framing, including true/false booleans and following messages.
- `GreeterHandshake` uses production GreeterProxy and SessionModel over a local socket to verify automatic reader startup, clearing account/session selections, and independent password submission.
- `DisplayLifecycle` compiles the production Display, Auth, selection and socket implementation. Test-only application, seat and greeter-process boundaries suppress real compositor/VT/account operations. A private D-Bus instance replaces both bus addresses. It tests either winner retaining its session, the winning helper's exit, late reader startup, user changes, cleared/invalid selections cancelling both channels, stop/disconnect cancellation, and retry after failure. The helper is a synthetic protocol peer; PAM/physical matching are covered separately. Existing logind-session reuse is disabled in this fixture and remains a separate qualification requirement.

`DisplayLifecycle` also tests async probe latency, each injected capability failure followed by legacy password success, old/new greeter framing during downgrade, withholding capability during legacy authentication, and the released-session latch. `actual-pair-test.py --build BUILD --old-helper OLD_HELPER` uses an actual helper built from upstream `633c1bf` with fixture PAM/passwd and fresh `/sys` and `/dev`; it tests direct and forking legacy launch plus rejection of those paths for parallel mode. A restricted legacy session may exit after PAM session-open; this is not a graphical-login test.

The synthetic helper is built only in this test directory and has no installation rule. State/configuration paths are generated inside the test build. Do not deploy or package these fixture binaries as authentication helpers.

AI-assisted implementation and review tests prepared with OpenAI Codex. Maintainer review is pending.

Parallel fingerprint is disabled when `Users.ReuseSession=true`; password authentication keeps the normal SDDM path. The portable tests do not qualify real session reuse or multiseat behavior.
