# Application runtime foundation (0.20.0)

`include/revenge/application.hpp` defines a C++20 variant-based request and reply interface. `src/application.cpp` owns state and services. `src/application_fixture.cpp` is a separately authored, linked workload, not a loaded executable.

| Service | Contract |
|---|---|
| Lifecycle | Created -> running -> stopped, no restart. Stop releases tracked memory and temporary storage and wakes event waiters. |
| Memory | Private 8 MiB native arena, page-rounded commitments, logical byte bounds, at most 128 live allocations; monotonically allocated process-local handles. |
| Events | Manual-reset event; create, set, reset, close and wait (0–10,000 ms). Up to 128 live events. Stop or close invalidates a wait. |
| Storage | Named byte blobs in memory only, 1 MiB total, 128 names, ASCII allowlist, overwrite and erase. No host file opens. |
| Clock | Monotonic elapsed nanoseconds since start; no wall-clock synchronization or timer scheduling. |
| Graphics | Synchronous render callback for 320–3840 × 240–2160 extents. CPU mode validates the command list; GPU mode invokes existing integrated offscreen rendering. |

Requests serialize through one mutex; event waits release the mutex while waiting. The synchronous graphics callback runs under that mutex and must not re-enter the dispatcher. Callers must join threads before destroying the runtime. API errors use C++ exceptions; no Xbox error-code mapping, native thread dispatch, sandbox, or hostile-code isolation is provided. The fixture owns its worker with `std::jthread`; the runtime does not forcibly cancel it. Event waits are bounded, but thread joining and renderer execution are not hard timeouts.

The fixture stores a resolution configuration, allocates 16 KiB of native memory, and runs the independent ALU program on a host worker. After the event is signaled, the main application verifies 4,096 values against `3*i+1`, round-trips the output through temporary storage, retrieves the resolution, and invokes rendering. The CPU ALU outputs are not passed to the GPU as texture data; the existing integrated renderer separately runs its own GPU shader fixture. At shutdown it checks zero live memory handles, events, storage entries and committed bytes. An optional BMP path supplied by the CLI is handled by the existing graphics backend outside the storage service.

```bat
.\build\Release\Revenge.exe app-test
.\build\Release\Revenge.exe app-gpu-test 1920 1080 app-1080.bmp
.\build\Release\Revenge.exe app-gpu-test 3840 2160 app-4k.bmp
```

This release does not implement console import binding, system DLL exports, Xbox resource layouts, persistent files, audio, input, a general graphics API, on-screen indexed rendering or game execution. The v0.19 presentation limiter is unchanged.
