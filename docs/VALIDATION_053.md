# Current validation and queue extensions (0.53.0)

## Windows validation

Run `powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1 -FastBuild -BuildJobs 2 -Focused` from the project root. The build script enables `REVENGE_ENABLE_GPU_TESTS` for this combined workflow. The normal CTest group runs first, then the four `desktop_gpu` tests run serially. Each test launches its intended executable via a CMake script, checks the exit status and requires a specific stdout success marker. The process is limited to 110 seconds and CTest to 120 seconds. Empty output, nonzero exit, or a missing marker fails the test. There is no CPU fallback counted as GPU success. The focused workflow omits the full suite. Standalone builds leave the GPU test group disabled unless `-GpuTests` is supplied. Optional public GDK absence is recorded as SKIP. The exact cause of the previous empty output is unknown; this revision ensures the ambiguity cannot count as success in these four checks.

## Independent queue extensions

The existing version-1 entry points retain their signatures. Three additive exports are introduced:

- `rq_query(ctx, handle, &snapshot)`: initialise `snapshot.size` to `sizeof(rq_snapshot)`; returns a consistent queue snapshot under the context lock. Aliases share the same state. Invalid handles or size leave the snapshot unchanged.
- `rq_submit_tracked`: takes the normal submission parameters and an output `rq_ticket*`. Returns a nonzero, context-local, monotonically increasing ticket on success; output zero on failure. The pending limits remain unchanged.
- `rq_cancel`: claims an individual pending tracked item from either port, then invokes its callback with `canceled=1` outside the mutex. Cancellation, dispatch and stop cannot claim the same pending callback twice. Returns `RQ_INVALID` for nonpending, zero or foreign-to-queue tickets; tickets must never be used with a different context. No historical operation status is retained. Cancellation does not interrupt an already executing callback. Concurrent destroy is prohibited as before; caller-owned callback data must remain alive until callback return.

The integration test now submits six trusted graphics requests (8x8, 8x8, 13x7, 13x7, 1x1, 8x8), dispatches them on an existing managed native thread, verifies all 1,125 output pixels, then drains six completion callbacks and validates thread/session cleanup. These dimensions create two reuse opportunities and three size changes; the CPU run does not demonstrate actual GPU reuse. The desktop GPU mode is synchronous and is not a general GPU command scheduler or Xbox ABI.

## Local results

Linux C++20 Release, IPO off: 74/74 CTest passed, 2/2 Python tests passed. The three integration/cancellation/validation-contract tests each passed 20 repeated executions. The validation contract probe accepted zero exit with a stdout marker, rejected silent zero exit, rejected a marker on stderr alone, and rejected exit code 7 despite a stdout marker. No Windows compiler, public GDK runtime, PowerShell or D3D12 GPU was available for local execution. The new Windows validation remains pending; no Xbox application or game execution is demonstrated.
