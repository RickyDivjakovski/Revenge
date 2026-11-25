# v0.42.0 reference lifecycle and logging validation

## Scope

The previous v0.41.0 native image fixture was validated on Windows (69/69 regression cases, five fixed entry calls and two fixed import calls). v0.42.0 keeps it unchanged. The new reference DLL is compiled from `tests/native_reference.cpp`, independently linked to a client alongside `revenge_abi`. The OS loads the modules normally. It does not extend the fixed native image loader or implement an Xbox ABI.

The three C exports are `ref_step`, `ref_unwind`, and `ref_retired`. Each thread has a compiler-managed object initialised to 17. The client creates four batches of eight managed callback threads; each checks 256 increments against a separate Revenge TLS slot and catches 256 nested exceptions inside the reference DLL. Each exception destroys two stack guards; each thread destroys its TLS object when it exits. The client joins all handles before verifying destructor counts, checks independent main-thread state, releases its ABI slot and stops the session. C++ exceptions do not cross the reference DLL's C interface.

## Validation

Local Linux x64 Release: 56/56 CTest, 2/2 Python, 20/20 repeated reference tests, deliberate log marker process exit code 7 and both markers checked. Windows is not available locally. The Windows configuration adds four more cases than the portable addition: DLL export inventory, import-chain inventory (six direct symbols across two DLLs), initialisation metadata report, and PowerShell native output capture. Expected total 74 Windows CTest cases. No GPU changes or new shader instructions in this release.

`Build-And-Test-Revenge.ps1` creates one fresh validation directory. Shared `tools/ValidationLog.ps1` captures each native output line explicitly and checks its exit code, preserving the failure path. The build script passes its log path through to the helper. Windows PowerShell 5.1 behavior requires real validation; the regression test uses the fixture's `--log-probe` mode to print one marker on each stream and exit 7. The test only passes if the wrapper detects that exit code and captures both lines. This avoids relying on transcript rendering. An actual error is logged as `VALIDATION FAILED` and the wrapper rethrows it.

## Remaining blockers

The reference uses native Windows/Linux compiler and operating-system facilities, not custom guest TLS or exception registration. A real authorised console-targeted executable and legitimate platform specifications remain required to establish Xbox compatibility. GDKX probe source remains available but has not been compiled. No external image execution, console-specific system/graphics APIs, general-purpose loader, or commercial-game compatibility is delivered here.
