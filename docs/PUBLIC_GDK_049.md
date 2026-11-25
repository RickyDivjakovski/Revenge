# Public Windows GDK reference

This optional self-authored PC program links against the public SDK installed on the local machine. It uses the Windows loader, not Revenge's experimental image preparation. It does not imitate Xbox identity or implement a GDK binary interface. No console extension, Microsoft library or SDK header is distributed.

## Build and test

Keep public GDK and sample repositories outside the Revenge tree. They are useful as documentation/reference source; no files from either are needed for this build. From the project root, with the public SDK installed:

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1 -FastBuild -BuildJobs 2 -Focused
```

The script detects `GameDKCoreLatest` or chooses a versioned installation under `GameDK`. The explicit override `-GdkRoot "C:\Program Files (x86)\Microsoft GDK\261000"` is supported. `-SkipGdk` opts out. With the SDK enabled the target `RevengeGdkReference.exe` is built using the ordinary Windows x64 compiler, public headers and the installed `xgameruntime.lib`. The actual Windows Gaming Runtime must also be available; SDK installation alone does not establish runtime availability. Failure HRESULTs and the CTest result are recorded; no fake success, service installation, or package registration is attempted. The test has a 30-second CTest timeout.

Expected shared queue trace (independent model and GDK reference):

```
REVENGE-QUEUE-V1 work=32 completion=32 sums=528,3696 canceled=0
```

The GDK reference also verifies two pending callbacks are canceled on termination, checks the PC device type, prints the shared independent game trace and uninitializes the runtime. The model is deliberately single-threaded, with a capacity of 128 callbacks per port. Capacity and cancellation stress are local tests, not an assertion about SDK limits. The GDK queue uses manual dispatch only. No sign-in, entitlement, store, network or controller APIs are invoked. The reference is not an official Microsoft sample or Xbox binary.

## Offline reports

```
Revenge.exe gdk-api-registry
Revenge.exe gdk-import-plan RevengeGdkReference.exe
```

The catalog contains 20 documented names, not 20 implemented functions. Exact named direct imports are matched even if the module is unrelated; identity, signatures and ABI remain unverified. Ordinals, delay imports, dynamic lookups and statically linked SDK functions are excluded. Zero matches is possible on a real GDK binary. No modules are loaded or addresses bound by this command.

## Public references

- [GDK PC directory layout](https://learn.microsoft.com/en-us/gaming/gdk/docs/gdk-dev/pc-dev/overviews/gdk-new-layout)
- [XGameRuntimeInitialize](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/xgameruntimeinit/functions/xgameruntimeinitialize)
- [XSystemGetDeviceType](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/xsystem/functions/xsystemgetdevicetype)
- [Manual task queue](https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/async/async-task-queue-design-howto/creating-manual-task-queue)
- [Task queue termination](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/xtaskqueue/functions/xtaskqueueterminate)

Actual Xbox execution requires an authorised console development target and further compatibility work. The desktop GDK reference does not establish console behaviour.
