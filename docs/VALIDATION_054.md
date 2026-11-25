# GPU lifecycle validation (0.54.0)

The v0.53.0 Windows focused suite passed 15/15; two desktop GPU tests passed. The native ABI and queue GPU fixtures exited zero with no stdout or stderr, so strict validation correctly failed. The precise cause is unconfirmed.

The restricted multipass renderer now owns its device, queue, fence, pipeline objects and single size-keyed resource slot per session. The desktop CLI uses a local cache. `RV_STOP` and `rv_close` release the cache explicitly; managed workers must have finished. Close waits for managed threads and the internal job queue before releasing resources. The session cache is lazy and no CPU request creates a GPU device. A failed submission poisons its context; teardown attempts a bounded drain if GPU work might remain outstanding. A timeout or removed device is not a successful render; reliable recovery after GPU hangs is not demonstrated.

GPU fixtures emit flushed stderr checkpoints; adapter reporting no longer mixes wide and narrow stdout. The CLI benchmark does not emit the per-pass diagnostic trace. The ABI fixture verifies a fresh session after closing the prior two sessions. The queue fixture requires actual cache teardown counters (six calls, four size allocations, two reuses), not merely reuse opportunities. The wrapper still requires the original verified-pixel stdout marker and a zero exit code, plus a lifecycle stderr marker for the two integrated GPU tests.

Run the existing combined PowerShell build and validation command with `-FastBuild -BuildJobs 2 -Focused`. Expected Windows results with GDK: 15 focused tests and four desktop GPU tests. The full suite is not run by this command. Windows compilation and GPU execution of this version require local testing.

Portable Release: 74/74 CTest, 2/2 Python, and 20 repeated runs each of native ABI multipass, queue integration and validation contract. The Windows D3D12 implementation was not compiled or run in this environment.
