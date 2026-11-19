# Parallel fingerprint authentication prototype

This opt-in proposal keeps password and fingerprint authentication available independently for the greeter-selected account and session. A successful PAM authentication and account check must receive an explicit daemon approval before its helper may open a session. One helper wins; the other is cancelled without blocking the daemon. A reader failure leaves the password path available. Submitting a password does not cancel the reader.

Enable this only with a matching greeter and distribution-reviewed PAM services:

```ini
[General]
ParallelFingerprint=true

[Users]
ReuseSession=false
```

Both settings are required. `ReuseSession` defaults to true; when it is true, parallel fingerprint authentication remains disabled even if the General switch is enabled. Setting it false changes normal SDDM behavior: logging in starts a new session instead of reusing an existing one. The parallel feature defaults to disabled. In this mode normal password requests use `sddm-password`, and reader requests use `sddm-fingerprint`. Each must retain the distribution's account restrictions and session rules. Keep fingerprint verification out of the password service. Reader/device errors must not be counted as incorrect passwords. Duration, retry and power policy require device/distribution review.

The greeter receives the `parallelFingerprint` capability and can call `startFingerprint(user, sessionIndex)` for its actual selection. `fingerprintActiveChanged(bool)` describes reader activity independently of password submission. The greeter should submit its ordinary password request without clearing or disabling the reader. Disconnecting cancels pending authentication; account/session changes cancel stale work. The existing logind-session lookup also recognizes the new password and fingerprint service names. Existing-session reuse, including a target session ending during authentication, still needs dedicated qualification; the portable controller fixture disables reuse.

The theme must report every selection change, including an empty username or an invalid session index. Either clears the previous selection and cancels its pending authentication. A later valid selection starts a new attempt. Do not keep the last valid username listening while the visible selection is empty.

Reader readiness is a versioned, opt-in PAM conversation contract. A supporting module sends `PAM_TEXT_INFO` containing byte `0x1e` followed by the ASCII string `FPRINTD_VERIFY_READY_V1` only after the device accepts `VerifyStart`. SDDM consumes it only from the current, active fingerprint helper; it is never an authentication result or permission to open a session. Ordinary translated placement prompts do not mark the reader active. Without this notification fingerprint authentication can still succeed, but the greeter does not advertise reader readiness. The companion fprintd change adds the `notify-ready` module option; deployments wanting the readiness indicator must ship that change and enable the option only in the fingerprint PAM service. Password services and other applications retain the default PAM behavior.

Distributions may select another service at build time with `-DPAM_FINGERPRINT_SERVICE=NAME`; the default is `sddm-fingerprint`. Supply and review that service separately, including account restrictions, failure accounting, and session policy. The qualification package uses an isolated service name and module path to preserve the installed PAM files, but builds the same submitted sources. A submission manifest must record this build setting, the exact PAM policy and companion module source, rather than treating them as untracked test fixtures.

Ordinary logins use the installed helper, including administrator-provided wrappers, and retain legacy framing. Parallel mode never runs that path: the daemon carries the helper built with it and starts a sealed anonymous copy, requiring a versioned handshake and explicit approval before session opening. It validates the peer with a pidfd and executable identity. A changed, stripped, old or wrapped installed helper cannot replace the parallel executable. The async capability probe requires executable memfds, `/proc`, pidfds and `SO_PEERPIDFD`; if any are unavailable, the ordinary password path remains available. A probe that finishes during a legacy login waits until that login fails or its session is released before advertising parallel; the originating display cannot start another login after release. Requests made while capability is unavailable are ignored. An old greeter receives only legacy messages; a new greeter queries the capability with a payload-free request and receives later changes.

Parallel mode bypasses administrator or distribution wrappers at the installed helper path; those wrappers may enforce environment, confinement or audit policy. Before enabling this opt-in mode, deployments must review and provide equivalent policy for the embedded helper. The built-in executable also needs review against distribution hardening and executable-memory policy. A policy that disallows the sealed executable turns off parallel rather than weakening authentication. A legacy launcher that exits without a definite result keeps parallel unavailable until daemon restart. While the daemon remains alive, rejected legacy sockets are held open until peer disconnect; where available, the peer is also signalled through a pidfd. If an orphan or another socket holder survives daemon shutdown, closing the daemon's socket can still deliver EOF to an old helper. Builds without the required peer-pidfd syscall definitions disable parallel but retain upstream legacy limitations; this proposal does not claim universal legacy-wrapper containment. Real PAM, multiseat and session behavior remain qualification work.

The portable review suites are documented in [test/parallel](../test/parallel/README.md). They use production protocol and display-controller code with isolated process/PAM fixtures. They do not establish multi-seat support, actual suspend/resume, every PAM distribution or physical fingerprint accuracy.

Fingerprint success does not provide the password used to decrypt an existing login keyring. A distribution may retain its ordinary password-keyring authentication hook on the password path. This proposal does not store a password, remove keyring encryption or equate biometric authentication with possession of that decryption secret.

This work overlaps [SDDM PR #2176](https://github.com/sddm/sddm/pull/2176). Coordinate the API and session lifecycle with that work before a merge request. Its currently described password-submission behavior stops fingerprint polling; this proposal specifically keeps both methods available.

AI-assisted prototype prepared with OpenAI Codex; maintainer and distribution policy review are pending.
