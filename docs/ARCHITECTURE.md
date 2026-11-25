# Architecture and research decisions

## Scope and separation

Current Revenge 0.4.0 sources were inspected: `revenge-loader/src/lib.rs` describes native code mapping plus import resolution/TLS, `revenge-shader/src/ir.rs` models scalar/vector registers, SCC and EXEC in SSA, `revenge-gpu/src/shader_cache.rs` keys the cache by translator source and translation settings, and `apps/revenge/src/precompile.rs` provides the earlier AOT scanner. The new implementation does not copy those implementations. It deliberately does not claim feature parity.

Public references:
- Microsoft GDK (console graphics/compiler differences): https://github.com/microsoft/GDK
- Microsoft PE format (imports, relocations, TLS, exception metadata): https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- AMD RDNA2 ISA reference: https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture
- AMD ISA specifications: https://gpuopen.com/machine-readable-isa/
- Microsoft DXC: https://github.com/microsoft/DirectXShaderCompiler
- D3D12 multi-engine synchronisation: https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization
- Fence-based memory reuse: https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management
- Pipeline library hardware/driver mismatch: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device1-createpipelinelibrary
- DirectX specs: https://github.com/microsoft/DirectX-Specs
- Xenia pipeline cache reference (different GPU): https://github.com/xenia-project/xenia/blob/master/src/xenia/gpu/d3d12/pipeline_cache.cc
- shadPS4 reference (different console and ISA): https://github.com/shadps4-emu/shadPS4
- vkd3d-proton reference (D3D12-to-Vulkan, not Xbox D3D12.X): https://github.com/HansKristian-Work/vkd3d-proton

## Implementation milestones and acceptance

1. PE inspection: complete first version. Next: validate delay imports, recursive dependency inventory, debug and unwind metadata. Use licensed development binaries. Do not modify security checks or protected packages.
2. Native loader: NOT implemented. Must establish exact console/Win32 ABI, static TLS, CRT startup, exception registration, thread lifecycle, relocations and authorised package context. Missing APIs must fail explicitly rather than return fake success. Exercise a self-authored GDK test program first.
3. Graphics: implemented a PC D3D12 capability probe, buffer transfer bench and known-answer compute harness (Windows testing pending). NOT a D3D12.X interface translator. Next implement a documented ABI adapter with resource/descriptor lifetimes, root signature binding, queue ordering, swap chain and actual draw validation. GPU virtual address aliasing and Xbox tiling require separate research and authorised fixtures.
4. Shader: implemented structural container inventory and PC HLSL AOT compiler tool. NOT a native Xbox shader compiler. Next build a decoder from RDNA2 ISA docs; implement scalar/vector execution, wave32/wave64, EXEC/VCC/SCC, memory/texture/export semantics and control flow; compare against reference outputs. Only then emit and validate DXIL and integrate the runtime. No unsupported op silently becomes a no-op. AOT needs exact pipeline state, shader stages and resource binding information, not only shader bytes.
5. Memory: implemented per-resource dirty bitmap and contiguous range coalescing. No OS page trapping, sparse mappings, Xbox physical-memory emulation, or automatic GPU upload integration yet. Next compare explicit writes, dirty tracking and GPU ownership epochs with cross-queue fences. Avoid copying unchanged ranges. Treat UMA and discrete GPU allocation strategies separately based on actual capabilities, not vendor ID.
6. Synchronisation: implemented a standalone submission ring, waiting on the fence before allocator or resource reuse. Do not skip dependencies to improve FPS. Add queue timestamp queries, resource hazard tracking and barriers before adding async compute. Measure one-slot versus N-slot throughput. Do not disable Windows GPU timeout or driver protections.
7. Pipeline cache: design only. Separate portable translated DXIL key (source/ISA/translator version/bindings/features) from adapter/driver/runtime-dependent PSO cache. Use canonical PSO descriptors and root signature hashes; reject incompatible blobs via the driver and rebuild. Persist atomically and bound disk usage.
8. Input/audio/storage/lifecycle: not implemented. Use documented GameInput, WASAPI/XAudio2 and approved GDK services where available. Test offline local development builds and standard storage first. No authentication or entitlement bypass.
9. Compatibility and performance: validate each title and build independently. First triangle, texture, compute, resource lifetime, save and input fixtures, then complete authorised test titles. No specific commercial title is supported yet.

## Performance acceptance

10 FPS corresponds to 100 ms/frame. Proposed engineering budget: 30 ms CPU translation/submission, 15 ms memory/transfer management, 45 ms GPU execution, 10 ms other overhead. These are planning allocations only; CPU/GPU overlap means measured end-to-end times and critical path matter more than summing subsystems. Record median, p95 and p99 frame time, actual frames presented, GPU/CPU busy, bytes uploaded, fence stalls, shader cache hit/miss, PSO misses, memory budgets and visual correctness. Use fixed repeatable 120-second scenes, cold and warm caches separately, at the same resolution and quality. A minimum across every game cannot be guaranteed; distinguish sustained average from 1% lows and stalls. Compare against 0.4.0 only on titles it already supports. Never count repeated or synthetic frames as rendered gameplay.

## What is needed next

An authorised small Xbox GDKX development test binary and its source plus expected output, build/SDK version, shader source and console-compiled shader pairs, and corresponding PC-compiled binaries are most useful. Use licensed SDK access; do not redistribute confidential SDK files. Start with compute and triangle samples. On the user's PC gather the gpu-info output, both GPU bench runs and the compute known-answer test. Then profile with PIX or GPU vendor tools. These experiments do not establish that a retail game can run.

## 0.6.0 loader preparation update

The new offline `prepare_image` implementation maps file sections into a bounded zero-initialized data buffer, inventories direct IAT slot RVAs, parses AMD64 relocation blocks and applies DIR64 fixups to the buffer. It validates basic TLS template/callback address ranges and runtime-function entry bounds. It is not an executable memory mapper. It neither calls nor binds title code. It rejects unsupported relocation types and overlapping mapped sections. Full loader semantics, including thread startup/TLS, exception handling, host API ABI verification, section protections, package requirements and delay/dynamic imports remain pending. See Microsoft's PE format documentation linked above. Windows testing and game-specific validation require authorised development fixtures.
