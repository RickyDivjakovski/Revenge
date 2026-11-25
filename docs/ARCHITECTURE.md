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

## 0.7.0 native Windows runtime

The new Windows runner is a controlled baseline for self-authored PE fixtures, not an Xbox ABI implementation. The native OS loader performs import resolution, TLS startup, unwind registration, and section protection. The runner creates a suspended child, assigns it to a kill-on-close job, then resumes it. It does not map executable code into Revenge's process. An optional debugger loop logs second-chance exception addresses and passes exceptions to normal OS handling. Diagnostic mode adds overhead and is not a benchmark mode. A timeout terminates the job; this is process lifetime supervision, not a security sandbox.

Direct import audit uses a narrow allowlist of system-directory DLL candidates and reports unresolved/unchecked symbols; API-set contracts are left to the native OS loader. No title DLLs are loaded by the audit. Known console markers are rejected by the runner. The offline PE preparation path is unchanged. Full Xbox import binding, console TLS semantics, platform API adapters and shader translation are outstanding.

References: [AssignProcessToJobObject](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-assignprocesstojobobject), [Windows job objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects), [Debugger exception handling](https://learn.microsoft.com/en-us/windows/win32/debug/debugger-exception-handling).

## 0.8.0 system primitives and tuning

`MemoryArena` is a bounded CPU-only page model (maximum 256 MiB of physical backing). Physical indices can be mapped into multiple virtual page ranges, including scatter order; mapped pages cannot be freed. Reads/writes validate every touched mapping first. Physical dirty ranges are collected explicitly; allocation and free zero pages. External synchronization is required, and snapshot/reset must occur only when no concurrent writes can be lost. Page indices are not capability handles and cannot be used as guest addresses. This is not an `xmem.dll` replacement. GPU ownership and native virtual mappings remain unimplemented.

The API registry records unimplemented console dependencies without providing fake exports. The path tool uses synthetic G/T/U/D mappings and rejects traversal, device names and unsupported characters. It performs no actual file IO and is not a sandbox. The Windows audit now attempts a fixed list of common API-set contracts using the system-directory loader, preserving unchecked status for unavailable contracts. The execution gate is unchanged.

The transfer tuner compares medians for several ring sizes subject to a conservative DXGI local-budget estimate, keeps fence waits before allocator reuse and recommends the smallest size within 95% of the peak. It is a standalone experiment, not adaptive scheduling integrated into a game. The CPU capability probe reports vendor and OS-enabled AVX2 availability without selecting a different instruction path. Optional Agility SDK integration uses an externally obtained matching runtime and documented executable exports. Windows SDK headers remain the compile-time interfaces.

Public references: [API sets](https://learn.microsoft.com/en-us/windows/win32/apiindex/windows-81-api-sets), [LoadLibraryExW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw), [DXGI memory budgets](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/ns-dxgi1_4-dxgi_query_video_memory_info), [fence resource reuse](https://learn.microsoft.com/en-us/windows/win32/direct3d12/fence-based-resource-management), [Agility SDK setup](https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/).

## 0.9.0 desktop graphics fixture

`graphics.hpp` and `graphics.cpp` contain the portable frame ring, sample timing summary and BMP export. `gpu_win.cpp` implements an independent D3D12 render harness with a shader-visible SRV heap, RTV heap, per-slot render targets and command allocators, an immutable uploaded texture and one graphics PSO. Every reuse is gated by a monotonically increasing queue fence. Timestamp queries are resolved to a dedicated readback buffer, and the last render target is transitioned to copy-source for pixel validation. The fixture deliberately uses a desktop shader model 5.1 shader and no console-specific calls.

Not yet implemented: presentation, general resource lifetime/descriptor allocation, a dedicated asynchronous transfer queue, adaptive rendering-ring selection, persistent PSO disk caching, console texture swizzling and Xbox graphics ABI translation. The current fixture does not add game execution.

## 0.10.0 ALU shader fixture compiler

The earlier shader plan now has a tested limited GFX10.3 integer decoder, CPU evaluator, optimizer and HLSL emitter. This is a standalone self-authored fixture ABI (v0 input/output, independent active lanes), not a complete Xbox shader ABI. Existing DXC precompile tooling can optionally compile the generated fixture. The system D3DCompile compute verification command requires Windows GPU testing. See [SHADER_IR.md](SHADER_IR.md) for scope, public references and limits. Console execution, complete native shader translation, wave semantics, resource bindings, pipeline disk caching and graphics translation remain pending.

## 0.11.0 forward control flow and FP32 fixtures

A separate bounded decoder builds an acyclic CFG, checks literal boundaries and intersects incoming definite-assignment sets in topological order. Scalar SCC comparisons and forward branches are lowered to guarded HLSL statements in instruction order; this avoids an unbounded dispatch loop but is not general structured control-flow reconstruction. The generated fixture assumes all lanes active and scalar group input. The reference CPU wave mask abstraction does not feed into GPU code generation. Normal FP32 arithmetic uses bit-preserving casts and precise HLSL operations; denormals, NaNs, infinities, signed zero, rounding controls and divergent masks are not validated. See `SHADER_FLOW.md`.

## 0.12.0 buffer resource fixture

The separate `memory_shader` decoder validates a small GFX10 MUBUF subset, `s_waitcnt vmcnt(0)` and no-carry integer addition. A fixed research ABI maps s[0:3] to a read-only structured buffer and s[4:7] to a distinct writable structured buffer; v0 is an immutable index. Load destinations stay unavailable until a full wait. The emitted HLSL has explicit bounds checks; the GPU harness binds t0 using an upload heap and u0 using a separate UAV, then reads back after a fence. This is not a generic descriptor decoder, guest memory model, or a console graphics API adapter. See [SHADER_MEMORY.md](SHADER_MEMORY.md).

## 0.13.0 mixed fixture and compute cache

The flow decoder accepts an opt-in bounded MUBUF resource mode in the same CFG as integer and FP32 operations. The compute harness optionally persists driver PSO blobs keyed by exact compiled shader and root signature and adapter identity. See [SHADER_UNIFIED.md](SHADER_UNIFIED.md).

## 0.14.0 texture layout and graphics PSO cache

See [TEXTURE.md](TEXTURE.md) for the typed texture model, independent Morton8 layout, GPU conversion fixture and fixed graphics cache schema. Actual console tiling, texture instructions and descriptor ABI remain pending.

## 0.15.0 native virtual memory

See [VIRTUAL_MEMORY.md](VIRTUAL_MEMORY.md). A separate host arena uses native reserve/commit/protect/decommit operations and generation-checked suballocations, with a logical retirement timeline. The existing page alias model is unchanged. No guest pointer ABI or GPU integration.

## 0.16.0 resource transfer foundation

The independent transfer pool tracks fixed buffer triplets, recording order and fence-gated reuse. The new Windows fixture uses the native VM arena as CPU staging and a dedicated copy queue for upload and readback. Every submitted payload is verified. See [TRANSFERS.md](TRANSFERS.md) for the exact scope and limitations.

## 0.17.0 desktop presentation foundation

`presentation::BackBuffers` is a portable state validator for actual swap-chain indices and completed fence values. The Windows fixture uses a dedicated direct queue, three flip-discard buffers, indexed RTVs and command allocators, root constants for animated geometry and timestamp readback per slot. Resize is drain-before-release; minimized windows skip drawing. See [PRESENTATION.md](PRESENTATION.md) for limitations and measurement definitions. Existing copy/compute/offscreen paths remain independent.

## 0.18.0 scene command foundation

The portable `scene3d` command validator drives a fixed Windows indexed scene: geometry/texture binding, perspective vertex projection, depth testing, texture sampling and fenced color readback. See [SCENE.md](SCENE.md). This is not a unified console command processor; presentation remains a separate fixture.

## 0.19.0 integrated fixture

See [integration](INTEGRATION.md) for the generated compute palette, direct queue fence handoff, descriptor registry and frame pacing. The scene remains an independent offscreen fixture and is not a console API implementation.

## 0.20.0 application dispatcher

The linked C++ application fixture calls a typed request/reply dispatcher for lifecycle, native host memory, event synchronization, monotonic time and bounded temporary storage. A synchronous render callback invokes the existing integrated graphics fixture. See [application design](APPLICATION.md) for the API contract and limitations. No guest ABI or binary loader is provided.

## 0.21.0 multimedia and snapshot services

The application dispatcher adds `Persist`/`Restore` for a host-selected single snapshot. PCM mixing and controller normalization are portable; XAudio2 playback and XInput polling are standalone Windows hardware fixtures. See [MULTIMEDIA.md](MULTIMEDIA.md). No console ABI or unified multimedia guest execution.
