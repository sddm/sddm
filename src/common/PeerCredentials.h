// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Parallel authentication is enabled only where the socket peer can be tied
// to the exact helper process. The test override compiles the fallback path.
#if defined(__linux__)
#include <sys/syscall.h>
#ifdef SDDM_TEST_MISSING_SYSCALLS
#undef SYS_memfd_create
#undef SYS_pidfd_open
#undef SYS_pidfd_send_signal
#endif
#endif
#if defined(__linux__) && defined(SYS_memfd_create) && defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal) && !defined(SDDM_TEST_NO_PEER_PID)
#define SDDM_TRUSTED_HELPER_PID 1
#else
#define SDDM_TRUSTED_HELPER_PID 0
#endif

#include <functional>
class QObject;

namespace SDDM {
#if SDDM_TRUSTED_HELPER_PID && defined(SDDM_EMBEDDED_PARALLEL_HELPER)
bool parallelRuntimeAvailable();
void disableParallelRuntime();
void registerParallelRuntimeListener(QObject *context, std::function<void()> callback);
#ifdef SDDM_PARALLEL_TEST
void testParallelFailure(int reason);
bool testParallelProbeSettled();
bool testEmbeddedImageSealed();
void testQuarantineFailure(bool enabled);
#endif
#else
inline bool parallelRuntimeAvailable() { return false; }
inline void disableParallelRuntime() {}
inline void registerParallelRuntimeListener(QObject *, std::function<void()>) {}
#endif
}
