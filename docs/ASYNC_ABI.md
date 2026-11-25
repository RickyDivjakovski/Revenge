# Asynchronous graphics ABI (v0.30.0)

Trusted, same-process native callers only. ABI version and structure layout are unchanged. `RV_GFX_ASYNC` is available on portable builds for validation jobs; actual GPU jobs require the Windows custom backend. All unused request fields must be zero.

| Op | Fields / buffer | Result |
|---|---|---|
| 25 `RV_RES_UPDATE` | resource handle; count = capacity; replacement vertex/index/texture bytes, same length | bytes copied |
| 26 `RV_JOB_SUBMIT` | command handle; arg0 width 320–512; arg1 height 240–512; offset 0 GPU or 1 validation-only | job handle |
| 27 `RV_JOB_POLL` | job handle | state |
| 28 `RV_JOB_WAIT` | job handle; arg0 milliseconds 0–10000 | current state, including on timeout |
| 29 `RV_JOB_READ` | completed job handle; exact RGBA8 output buffer for GPU, no buffer for validation-only | bytes or draw count |
| 30 `RV_JOB_RELEASE` | terminal job handle | 0 |
| 31 `RV_JOB_COUNT` | no operands | retained job count |

States: 1 queued, 2 running, 3 complete, 4 failed. Read/release before terminal state returns RV_STATE. Read of failed job returns RV_FAILURE; release still works. Unknown/foreign/released handles return RV_INVALID. Four slots include completed jobs until released; a fifth submission returns RV_RANGE. Wait returns the current state; the caller must check completion. No hardware fence values are exposed.

Submission validates and copies the referenced resources before returning. Subsequent resource updates/releases cannot alter accepted snapshots. Mutable index bytes are checked on subsequent command validation/submission. Existing resource limits remain 64 handles, 8 MiB and 1 MiB transfers; job snapshots have a separate bound of four copies of at most three 1 MiB source resources plus bounded commands, shader data and 1 MiB readback per job. The worker processes FIFO requests in batches of up to two and reports errors as failed jobs. STOP drains accepted jobs before clearing all state; close must not overlap active calls.

On Windows the worker holds a device, root signature, translated PSO, direct queue, monotonic fence and two independent reusable frame slots. Both jobs in a batch can be submitted before either is collected; each slot is retained until its fence and readback complete. Same-shape allocations and command objects are reused; input bytes are uploaded again. A failure drops the context after a best-effort drain. See [GPU slots](GPU_SLOTS.md) for details and stress tests. The GPU uses one ordered queue; no guarantee of simultaneous hardware execution, external fence handles, custom presentation, arbitrary shader package execution or Xbox ABI is provided.

`--async` tests validation jobs and resource/handle semantics, not GPU output. `--async-gpu` submits four different texture snapshots and checks 68,096 pixels per job (272,384 total expected). The client releases its original resources and package before collecting results. Windows GPU path is not locally tested.
