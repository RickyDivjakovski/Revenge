# Resident asynchronous submission (v0.62.0)

The existing v1 C ABI and synchronous `RV_GPU_DRAW_BATCH` remain unchanged. The new compiled capability bit is `RV_GFX_RESIDENT_ASYNC` (4096); actual device availability still depends on Windows D3D12 hardware initialization.

| Operation | Request | Result |
|---|---|---|
| RV_GPU_BATCH_SUBMIT (87) | Same packet as DRAW_BATCH: count 1..32, exact array of rv_gpu_batch_draw, all other fields zero | Opaque nonzero ticket |
| RV_GPU_BATCH_POLL (88) | handle=ticket | 0 pending; 1 complete |
| RV_GPU_BATCH_WAIT (89) | handle=ticket, arg0=timeout in milliseconds (0..10000) | 0 pending after timeout; 1 complete |
| RV_GPU_BATCH_RELEASE (90) | handle=ticket | 0 after observed completion |

Submit validates the entire array, resolves pipeline handles and records the native command list before executing and signalling a fence. It does not wait for the fence. Pipeline preparation and driver submission can still consume CPU time. The caller may discard the packet after return. GPU data remains in the arena and no CPU shadow is created.

One retained ticket is allowed per session. Until POLL or WAIT observes completion, resident mutation, synchronous drawing, resource and pipeline release, cache clearing and readback reject with RV_STATE. Read-only usage/count operations remain available. Completion makes the resources accessible again; the completed ticket remains queryable until RELEASE. A new asynchronous submit requires releasing the previous ticket. This intentionally conservative first implementation has no command allocator pool or parallel in-flight batches. Polling and timed waits are externally serialized; WAIT holds the session and global GPU mutex. Polling uses a monotonic deadline with short sleeps; scheduling can extend the requested interval slightly. No cancellation is offered after queue submission.

STOP and close drain outstanding signalled work before releasing GPU objects. Teardown can block if the GPU does not complete or report removal. Device removal during completion is reported as RV_FAILURE; stop/close the affected session. No raw console ABI or arbitrary console shader support is implied.

The focused hardware fixture checks eight async draws across two explicit tickets, pixel readback, invalid batches, busy resource rejection, stale tickets and an additional pending batch drained by STOP.
