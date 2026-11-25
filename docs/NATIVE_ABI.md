# Native application ABI v1 (Revenge 0.25.0)

An independently compiled native application can link `revenge_abi.dll` through its generated import library (Linux: `librevenge_abi.so`). The DLL owns a private instance of the existing application runtime per session. Clients include only `include/revenge/native_abi.h`; no C++ STL objects or exceptions cross the exported interface. The two supplied clients link only the ABI library (and their platform thread/runtime libraries), not `revenge_core`.

This is a trusted, same-process native library interface using the host platform C calling convention (`__cdecl` on Windows x64, normal platform C ABI on Linux). It is not IPC, an Xbox ABI, an IAT patcher, an arbitrary executable loader or a security boundary. The Windows build uses the ordinary Windows loader to resolve the new library's imports. Clients compiled against the v0.25.0 header remain supported by the additive v0.26.0 operations. Recompile to access new graphics operations. It must supply valid non-overlapping request/result/buffer pointers, keep them live for the duration of the call, and not close a session until every caller has finished. Logical handle and offset validation does not make arbitrary native pointers safe. Use only self-authored or explicitly authorised programs.

## Interface

`rv_abi_version` reports 1. `rv_open` accepts only version 1 and returns an opaque session pointer; opening automatically starts the application. `rv_dispatch` takes five arguments: session, request pointer, optional caller buffer, buffer capacity in bytes, and a mandatory writable 64-bit result. `rv_close` destroys the session and any remaining owned resources. `rv_status_name` gives a static description of the returned numeric status.

Request layout: 56 bytes on the supported 64-bit host ABI. Four uint32 fields (size, version, operation, reserved) followed by five uint64 fields (handle, offset, count, arg0, arg1). The first handle begins at byte 16. All unused fields must be zero. Stats layout: six uint64 values, 48 bytes. No raw guest address or executable memory operation is exposed.

| Opcode | Inputs | Result |
|---|---|---|
| ALLOC | count (1–8 MiB max, subject to session budget) | memory handle |
| FREE | memory handle | zero |
| READ / WRITE | memory handle, offset, count, caller byte buffer and capacity | bytes copied |
| EVENT_CREATE | none | event handle |
| EVENT_SET / RESET / CLOSE | event handle | zero |
| EVENT_WAIT | event handle, arg0 = timeout ms (0–10000) | 0 timeout, 1 signalled |
| CLOCK | none | elapsed monotonic nanoseconds |
| RENDER_VALIDATE | arg0 width, arg1 height | zero; CPU scene command validation only |
| STATS | writable buffer of at least 48 bytes | 48 bytes |
| STOP | none | zero; releases tracked memory/events and ends lifecycle |

Transfers are capped at 1 MiB each; all memory accesses are validated against the logical allocation bounds by the runtime. Events are manual-reset. Sessions do not share handle ownership. After STOP, STATS and close remain available; other implemented service requests are rejected by the runtime. Error results are zero and numeric statuses differentiate malformed requests, version mismatch, unknown operation, range error, lifecycle error and other service failure. A valid request and result pointer must be supplied; no arbitrary pointer probing is performed. C++ exceptions are caught before returning through the C boundary.

## Tests

On Windows, build with `Build-Revenge.ps1` and execute:

```bat
.\build\Release\revenge_abi_fixture.exe
.\build\Release\revenge_abi_c_fixture.exe
.\build\Release\Revenge.exe import-plan build\Release\revenge_abi_fixture.exe
ctest --test-dir build -C Release --output-on-failure
```

The existing offline import classifier may label Revenge's own DLL as unsupported/unverified: it is not a Windows system module. This is expected; the classifier is not a binding mechanism. The normal OS loader resolves the fixture's actual imports when it starts. Keep the ABI DLL beside the clients.

The C++ client verifies 4,096 native worker results through memory read/write and event synchronization, two independent sessions, invalid version/size/reserved fields, unknown operation, short buffer, logical overflow, invalid render/wait sizes, foreign/wrong-kind/stale handles, stop cleanup and post-stop rejection. The separate plain-C client checks structure sizes and offsets, all five arguments of dispatch, nonzero-offset buffer transfer and shutdown. These are native host arithmetic tests, not translated GPU shaders. Linux Release validation passed; the v0.25.0 Windows tester subsequently reported both clients passed and 43/43 CTest.

Not implemented through this ABI yet: cross-process messaging, thread creation services, persistent disk storage, audio, input, Xbox guest-address mapping, console imports, console system ABI or gameplay. Mutexes, semaphores and session RAM storage are added in v0.31.0; graphics operations are described in the subsequent release documents. Existing separate runtime/GPU fixtures remain unchanged.

## v0.26.0 additive graphics operations

See [GRAPHICS_ABI.md](GRAPHICS_ABI.md) for operations 14–18. The original ABI version, structures and exports are unchanged. The new Windows GPU path is pending hardware validation.

## v0.31.0 additive system operations

See [SYSTEM_ABI.md](SYSTEM_ABI.md) for operations 32–44, limits and the independent client. ABI v1 layout and exports remain unchanged.

## v0.32.0 additive native TLS operations

See [DEPENDENCY_TLS.md](DEPENDENCY_TLS.md) for operations 45–50. These are session-owned native thread scalar slots, not guest TLS. The five exports and request layout remain unchanged.

## v0.33.0 managed threads

See [managed threads and forwarder chains](THREADS_AND_CHAINS.md). `STOP` requires all managed threads to have been released. Managed callbacks must return before session destruction can finish.
