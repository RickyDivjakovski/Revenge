# Reusable GPU frame slots (v0.30.0)

ABI version 1 and its 31 operations are unchanged. Four jobs may be retained by the client. The host worker takes up to two FIFO jobs at a time; for GPU requests it may wait up to 2 ms to coalesce the second submission. Validation-only jobs do not create GPU resources. A mixed batch can contain one GPU job. The worker submits GPU command lists before collecting their results.

Each of two Windows slots owns its upload vertex/index buffers, texture upload, default-heap RGBA8 texture, color/depth targets, readback, RTV/DSV/SRV heaps, allocator and command list. Each successful submission signals a unique increasing value on one native D3D12 fence. Collection checks the fence and copies the mapped readback into the job's own packed framebuffer before the slot is made available. Existing snapshot semantics protect pending jobs from updates/releases of application resources. Same render extent, texture extent and buffer lengths reuse the slot; changed shapes rebuild that retired slot. Texture and render-target states transition back from their prior final states on reuse. Vertex/index/texture data is fully rewritten, not incrementally updated. The fixed per-slot size restrictions remain in place; no exact VRAM accounting or explicit residency management is provided.

A single direct queue preserves command ordering. Two submitted-uncollected slots do not prove overlapping GPU execution; a fast GPU may finish the first job before the second submission. Logs include fence numbers, allocation-set/reuse counts, and peak submitted-uncollected count. No external GPU-fence handles, custom swap chain or new shader opcodes in this release. Errors invalidate the shared context; destruction attempts a best-effort drain before releasing resources. The synchronous command path remains separate and unchanged in behavior.

## Tests

- `--async`: four validation jobs, snapshot/handle checks.
- `--async-gpu`: four different texture snapshots and 272,384 expected verified pixels. Look for `GPU batch: 2 submitted before collection.` and `reused allocations and allocator`.
- `--async-stress`: 64 validation jobs in groups of four; rejects fifth, collects out of order and checks cleanup.
- `--async-gpu-stress`: 64 GPU jobs, alternating render extents in groups of 16 (320x240 and 384x256), independently checks every readback (5,005,312 expected reference pixels, excluding boundaries), changes textures and collects out of order. Elapsed time includes client validation and cleanup; not GPU-only timing or gameplay FPS.

Windows-specific implementation has not been compiled or run in the development environment. Independently authored fixtures only; no Xbox loader or ABI implementation.
