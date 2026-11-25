# System services ABI and offline PE64 layout (v0.31.0)

The additive operations retain ABI version 1, its 56-byte request, five exports and 48-byte stats. The independent native client is linked by the host OS; these operations define a Revenge research interface, not Xbox or Win32 API equivalence. See [native ABI](NATIVE_ABI.md).

## New operations

All unused fields must be zero. `RV_OK` result is zero unless specified. Waits return 1 on acquisition, 0 on timeout; errors return a zero result and status. No native OS handle or guest address is exported.

| Opcode | Request fields | Result |
|---|---|---|
| 32 MUTEX_CREATE | none | handle |
| 33 MUTEX_LOCK | handle, arg0 timeout ms | acquired |
| 34 MUTEX_UNLOCK | handle | zero |
| 35 MUTEX_CLOSE | handle | zero |
| 36 SEM_CREATE | arg0 initial, arg1 maximum | handle |
| 37 SEM_WAIT | handle, arg0 timeout ms | acquired |
| 38 SEM_RELEASE | handle, arg0 count | previous count |
| 39 SEM_CLOSE | handle | zero |
| 40 STORE_PUT | arg0 key bytes, count value bytes; buffer = key then value | value bytes |
| 41 STORE_GET | arg0 key bytes, buffer starts with key, capacity covers key and value | bytes copied to start of buffer |
| 42 STORE_SIZE | arg0 key bytes, buffer = key | value bytes |
| 43 STORE_ERASE | arg0 key bytes, buffer = key | zero |
| 44 SYNC_COUNT | none | live mutex plus semaphore count |

There are up to 128 mutexes and 128 semaphores per session, and 0–10,000 ms waits. Mutex ownership uses a unique native-thread token; locks are nonrecursive, unlock by another thread and close while held are rejected. No abandonment recovery, priority inheritance or fairness is promised. Semaphore release is atomic and checked against its maximum (1–65,535). Close wakes waiters, which receive invalid-handle status; STOP cancels waits with lifecycle status, releases objects, and cannot be reversed. Join all callers before closing the session. SYNC_COUNT is available while running only.

Storage is a bounded in-memory key/value service (128 entries, aggregate 1 MiB), not persistent disk access. Keys contain 1–64 ASCII letters, digits, underscores, hyphens or dots; `.` and `..` are invalid. No path traversal or host filename opening is supported. PUT replaces atomically under the runtime lock; empty values are legal. Each transfer is capped at 1 MiB including its key. GET copies only on success; the caller must restore its input key before another lookup. SIZE can be used to size a buffer, but concurrent writes may change the value before GET. Missing GET/SIZE return RV_RANGE; missing ERASE returns RV_INVALID. STOP clears storage. Existing host-selected persistent snapshot fixtures remain separate.

## Offline layout plan

`Revenge layout-plan <authorised-pe64.exe> [--rebase]` uses existing bounded PE image preparation and records section characteristics. The 4 KiB diagnostic page model unions read/write/execute characteristics for shared pages, marks headers read-only in the model, coalesces adjacent equal flags and identifies gaps. It reports writable+executable bytes, entry-page flags, IAT slots not wholly inside writable spans, DIR64 relocation count and existing TLS/unwind metadata. These are not an actual Windows mapping, nor a compatibility or safety verdict. Read-only IAT ranges are possible in normal PE files. It never allocates executable pages, binds imports, registers unwind tables or invokes callbacks or target code. The Windows OS loader still loads the trusted independent executable normally.

## Validation

```bat
.\build\Release\revenge_abi_services_fixture.exe
.\build\Release\Revenge.exe layout-plan build\Release\revenge_abi_services_fixture.exe
.\build\Release\Revenge.exe import-plan build\Release\revenge_abi_services_fixture.exe
ctest --test-dir build -C Release --output-on-failure
```

The independently authored client validates 4,096 protected increments across four threads, semaphore consumption and overflow, ownership, timeouts, foreign/wrong-kind/stale handles, binary storage and buffer limits, cancellation and cleanup. Graphics validation in this client is CPU-only. The separate synthetic PE fixture covers shared-page W+X, gaps, zero fill, entry flags and IAT diagnostics. The Windows CLI test uses a self-authored executable, not commercial software. Actual guest memory mapping, TLS and unwind execution, import binding, platform APIs, audio/input ABI and game execution remain unimplemented.
