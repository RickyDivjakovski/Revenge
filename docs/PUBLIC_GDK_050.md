# Public GDK validation (v0.50.0)

The optional Windows reference uses the installed public PC GDK, not GDKX. No Microsoft SDK files are distributed. Source and APIs are independently authored/documented. Prior v0.49.0 Windows validation demonstrated successful runtime initialization and a matching manual queue workload; no Xbox execution occurred.

## Build

Run `powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1 -FastBuild -BuildJobs 2 -Focused` from the project root. Detection checks the explicit `-GdkRoot` override, `GameDKCoreLatest`, numeric subdirectories under `GameDK`, and standard Program Files roots. Invalid explicit paths fail instead of falling back. If necessary use `-GdkRoot "C:\Program Files (x86)\Microsoft GDK\261000"`. `-SkipGdk` intentionally disables the optional target. Portable discovery tests create only synthetic placeholder files in the build directory.

## Expected Windows reference output

- `XGameRuntimeInitialize HRESULT=0x0` and the PC device type.
- Existing `REVENGE-QUEUE-V1` trace plus `REVENGE-DELAY-V1 work=1 completion=1 sum=18 canceled=1 alias=1` for both independent and GDK workloads.
- Module delta at initialization, workload and shutdown, loaded module basename inventory and selected public export visibility. These snapshots may miss transient modules, and zero visible exports is not a runtime failure. No addresses or full module paths printed; no calls through export pointers. GetProcAddress on a forwarded export can cause normal loader activity; export visibility is not actual runtime call tracing. Snapshot failure is diagnostic, not silently reported as a successful enumeration.

The independent model uses virtual time, supports 64 live handles and 128 items per port, and explicitly rejects closing the last handle while callbacks are pending. Stop synchronously cancels all pending work. The caller must keep callback contexts alive, stop/drain before final close and use nonthrowing callbacks. Unlike the actual GDK this is not a thread-safe implementation, does not hold background callback references after final close, and exposes no GDK-compatible binary ABI. The real reference tests a small real-time delay with bounded waiting and cancellation of a pending long-delay callback; it does not benchmark timing accuracy.

## Public references

- [Microsoft async task queue design](https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/async/async-task-queue-design)
- [Microsoft queue termination](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/xtaskqueue/functions/xtaskqueueterminate)
- [Win32 module snapshot documentation](https://learn.microsoft.com/en-us/windows/win32/api/tlhelp32/nf-tlhelp32-createtoolhelp32snapshot)

No general executable loader, dynamic guest API binding, console platform API, GameInput implementation or GDKX support is provided by this update.
