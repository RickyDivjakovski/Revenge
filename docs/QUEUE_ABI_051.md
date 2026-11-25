# Independent native task queue ABI

`include/revenge/queue_abi.h` defines the original eight exported C functions (plus the additive extensions in [0.53.0](VALIDATION_053.md)) in `revenge_queue.dll` (Windows) or `librevenge_queue.so` (Linux). This is an independent, trusted host ABI, not the GDK ABI and not a drop-in replacement for Microsoft libraries. The existing five-export `revenge_abi` is unchanged.

1. Open a context with `rq_open(RQ_VERSION, &ctx)`; create queues, and duplicate handles if needed.
2. Submit trusted callback pointers and caller-owned contexts to work or completion ports. Delay is 0–60000 ms. Each port has 128 pending entries; each context has 64 handles. Handle IDs are scoped to a context.
3. Dispatch explicitly on caller threads with a timeout of 0–10000 ms. Earliest due work is selected first. Concurrent completion ordering is not guaranteed. Callback execution runs outside the mutex, not in an automatic thread pool. Callbacks must return and not throw. They may submit more work.
4. Stop rejects new work and synchronously invokes pending callbacks with canceled=1. In-flight callbacks finish independently; concurrent/reentrant stop during cancellation returns RQ_STATE. Last handle close rejects pending or active callbacks. Join all caller threads before destroying the context. Do not destroy concurrently or from a callback. Do not retain caller context past its lifetime.

Tests: `revenge_queue_c_fixture` and `revenge_queue_threads_fixture`; the second accepts `--gpu` on Windows. The C client compares its delayed trace with the previously validated GDK reference. The C++ client launches a managed native thread, dispatches the render callback, verifies completion and releases the thread and session. It uses the existing native graphics ABI, not D3D12.X or a console shader. No automatic guest import binding, GDK binary ABI implementation or Xbox execution is claimed.
