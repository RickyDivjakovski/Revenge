# v0.33.0 — managed native callbacks and offline forwarder chains

## ABI

Additive operations 51–56 preserve ABI version 1, the 56-byte request layout and the five DLL exports. `rv_thread_start` holds a typed host C calling-convention callback and a 64-bit argument (16 bytes on x64). CREATE copies this descriptor synchronously. It is only for trusted native clients, never a guest address or an executable loaded from disk.

| Operation | Request | Result |
|---|---|---|
| THREAD_CREATE | buffer = rv_thread_start, exact size | opaque handle |
| THREAD_POLL | handle | running, complete or failed |
| THREAD_WAIT | handle, arg0 timeout 0–10000 ms | current state |
| THREAD_EXIT | terminal handle | callback's uint32 exit code; failed callback returns RV_FAILURE |
| THREAD_RELEASE | terminal handle | join and erase |
| THREAD_COUNT | none | retained handles |

Unused fields zero. Sixteen retained handles per session, including completed handles. Wait is bounded and self-wait rejected. Managed callbacks automatically erase their TLS values before publishing terminal state. External native threads still require explicit TLS detach. Callbacks must release owned mutexes; no abandoned mutex recovery. Callback failures caught as C++ exceptions are reported as failed; hardware faults or process termination are not contained. Callbacks should not throw across incompatible C++ runtimes; prefer normal return codes. No force termination. STOP rejects retained thread handles; release all first. Close joins managed threads naturally; callbacks must not call close, must return, and code and arguments must remain valid through join. No external calls may race close. Not a security sandbox or guest thread scheduler.

## Forwarder chains

`dependency-plan` retains its original output. `dependency-chain` follows direct import roots across supplied PE files by case-insensitive filename and case-sensitive symbol name or explicit ordinal. It uses the last dot in the forwarder as the module/symbol separator; appends `.dll` if no `.dll` or `.exe` suffix. It rejects path-like or malformed module strings, malformed ordinals, missing symbols, cycles keyed by module and ordinal, and chains beyond 16 hops. No implicit lookup, API-set mapping, full transitive import graph, code execution or binding. Terminal export presence does not prove ABI compatibility. Forwarder strings with ambiguous dot separators are not fully general.

## Validation

Linux x64 C++20 Release compilation succeeded. CTest 48/48; Python unittest 2/2. Managed fixture: 16 concurrent callbacks, 160 sequential callbacks (beyond TLS value cap), 45,056 isolated set/get pairs, CPU render validation, exit codes, failed callback, timeout, capacity, foreign/stale handles and cleanup. Synthetic chain fixture covers a named-to-ordinal multi-hop terminal, alias cycle, missing modules/symbols, malformed path and ordinal, malformed forwarder and depth cap. Windows build and GPU regressions were not run locally; expected 58 Windows CTest cases. Previous v0.32.1 passed 55 Windows CTest cases per tester.

No changes to shader or GPU execution paths. Guest TLS initialization, native guest exception/unwind handling, console system APIs, audio/controller/persistent-storage ABI expansion and console executable loading remain future work.
