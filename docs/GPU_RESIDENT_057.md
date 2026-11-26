# Resident transfer services (v0.57.0)

Independent Revenge ABI v1 extension. Discover `RV_GFX_RESIDENT_TRANSFER` (compiled capability, not device availability). The Windows arena uses a hardware FL12_0 D3D12 device, one direct queue, allocator, list and fence per session. Portable hosts return `RV_UNSUPPORTED`. No fallback or Xbox ABI binding.

- `RV_GPU_ALLOC`: count = bytes (1 byte to 1 MiB, i.e. up to 1048576 bytes); arg0 and arg1 = width and height (both zero for a buffer, otherwise 1–512 and count=width*height*4). Returns a distinct handle. New contents cannot be read until initialized. The session limit is 64 allocations and 8 MiB logical payload; actual heap and staging memory may be larger.
- `RV_GPU_UPLOAD` / `RV_GPU_READ`: handle, count and exact-size buffer; full resource only. Texture rows are packed RGBA8 on the caller side and padded to the D3D12 copy footprint in staging. No resource-content shadow is retained on CPU.
- `RV_GPU_COPY`: destination handle, arg0=source handle; matching size and extent, distinct handles, initialized source. Copies default-heap resources directly.
- `RV_GPU_RELEASE`: handle. `RV_GPU_USAGE`: no fields, returns logical bytes, including zero after stop. All unused fields and reserved fields zero.

Operations serialize with the native graphics mutex and global GPU lock; each transfer submits and waits for a fence. Resources return to COMMON after each command list. On command submission failure the arena refuses subsequent transfers; reliable device-loss/hang teardown is not verified. Normal stop releases session resources. No partial transfers, mip chains, shader binding, asynchronous transfers, or multi-frame draw integration in this release.

Hardware fixture checks 24 patterned buffer/texture upload-copy-readback cycles, 50016 bytes compared, including a 13-pixel-wide texture requiring padded row pitches, plus initialization, release and stop. CPU-only contract exercises capability and validation; it does not simulate GPU results.
