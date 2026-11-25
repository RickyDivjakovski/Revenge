# Configurable native multipass service

This is an independent native desktop API, not a D3D12.X or Xbox API implementation. Trusted native callers may invoke operation `RV_PASS_RENDER` on a running session. The ABI stays at version 1, and the five existing DLL exports are unchanged. Older builds return unsupported for the new operation.

## Packet contract

Use the definitions in `include/revenge/native_abi.h`. Set `rv_request.size`, `version`, `op`, `count` (input size), and `arg0` (0 CPU reference, 1 desktop GPU). All other fields must be zero. `RV_GFX_MULTIPASS` in `RV_GFX_CAPS` indicates a compiled GPU path, not a guarantee of a suitable adapter.

The caller-owned byte buffer begins with `rv_pass_header` (16 bytes), followed by `draw_count` records of `rv_pass_draw` (24 bytes each). Width and height are 1–128; draw count is 1–64. The exact capacity is the maximum of input length and output length (`width * height * 12`). No aliasing between request, result and buffer. Rectangles are half-open, nonempty and inside the framebuffer. Depth is an integer 0–256, converted to depth/256; depth comparison is LESS, initial depth 1. Stencil begins at zero.

- Mode 0: writes red to target 0 and green to target 1, replaces stencil with 1 after depth pass.
- Mode 1: stencil equal to 1, writes yellow to target 0 and red to target 1, depth test/write.
- Mode 2: stencil equal to 1, blends half-alpha blue into target 0, leaves target 1 unchanged, depth test/write.

All three surfaces start cleared; after the first pass the two color targets are transitioned to shader resources and the full third surface receives `(target0.r, target1.g, target0.b, 1)`. Synchronous GPU submission uses a fence and copies each surface into readback memory before returning. Each invocation creates its own resources and four PSOs; there is no persistent resource or command handle. Output overwrites the packet only on success, three row-major RGBA8 surfaces in order. `result` reports output bytes; failure returns zero and leaves the caller buffer unchanged. A stopped session rejects the operation. GPU operations serialize using the existing GPU mutex.

## Validation

The independent ABI test executable links against the shared library only. It compares against explicit expected colors and checks invalid packets, capacity, 64 draws, 128x128 and 1x1 extents, two sessions, shutdown and CPU-only unsupported GPU behavior. The Windows `--gpu` option runs six workloads and compares all color bytes with tolerance 1. These are restricted desktop fixtures; arbitrary shader code, resource descriptors, command lists, asynchronous execution and actual console compatibility are not implemented.
