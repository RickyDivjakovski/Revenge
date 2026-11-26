# Resident indexed drawing (v0.58.0)

This is an independent desktop D3D12 interface, not an Xbox ABI. It adds `RV_GPU_DRAW_INDEXED` (76) and `RV_GFX_RESIDENT_DRAW` (256) without changing the existing ABI version. Capability indicates a compiled backend, not successful device creation.

Use `RV_GPU_ALLOC` and `RV_GPU_UPLOAD` to create initialized vertex and index buffers and a source RGBA8 texture. Vertices are packed pairs of IEEE float32 clip-space coordinates; indices are packed uint16 triangle-list indices. Allocate an equally sized RGBA8 target; no initial upload is needed. All handles belong to the same session. Submit a request with:

| Field | Value |
|---|---|
| handle | target texture |
| offset | source texture |
| arg0 | vertex buffer |
| arg1 | index buffer |
| count | nonzero index count, divisible by 3, max 65535 |
| buffer / capacity | null / 0 |

Result is the submitted index count. The fixed pixel shader loads the source texel at the destination pixel coordinate. The render target is cleared to transparent black on each call. The operation is synchronous, uses a cached root signature, PSO and descriptor heaps, and transitions resources back to COMMON after fence completion. A target can become a subsequent source without host upload. No arbitrary shader binding, depth buffer, blend control, sampler control, command batching or console translation is implemented. Validation covers handle roles, extents, buffer sizes and index count; it does not inspect individual GPU index values or floating-point vertex contents. Supply valid indices and finite vertex coordinates. Device failure recovery is not guaranteed.

The hardware fixture checks 24 full-target draws and two single-triangle partial draws, two texture extents, byte-exact output, rebinding, rejection of invalid requests and cleanup. Portable validation only checks capability and unsupported behavior. Windows compilation and GPU validation must be performed using the consolidated PowerShell script.
