# Changelog

Historical release notes, newest first. Historical test counts and planned features describe their respective releases; see [README.md](README.md) for current capabilities.

## v0.47.0 — offscreen multipass graphics foundation

- Added independent 16×16 CPU reference for two simultaneous render targets, depth testing, stencil replacement/equality, scissor coverage, per-target colour masks, source-alpha blending and two-texture composition. Known-answer test checks all 768 pixels across three surfaces (80 covered pixels per surface).
- Added desktop D3D12 fixture with four graphics PSOs, four draws over two passes, root constants, two MRT descriptors, D24S8 depth/stencil, two SRVs, explicit render-target-to-SRV and copy-source barriers, fenced readback and one-byte tolerance. This is a fixed fixture, not an application-configurable command translator. GPU validation requires Windows.
- Focused mode now runs four CPU tests and both desktop graphics comparisons. Full suite is still available. Portable Release: 65/65 CTest and 2/2 Python tests passed; expected full Windows suite: 85. Existing GCC u8path deprecation warning remains.
- Restored current subsystem status table, progress diagram, achievements and architecture diagram in README without moving version history out of CHANGELOG.
- Depth sampling, general constant/structured buffer integration, persistent graphics PSO caching, general executable loading and Xbox API translation remain outstanding.

## v0.46.0 — desktop GPU texture sampling and focused iteration

- Added a shared CPU/GPU workload for RGBA8 and BC1/3/4/5 UNORM, two mip levels, two array layers and four static sampler configurations. Generates 320 expected RGBA samples and matching HLSL.
- Added a desktop D3D12 compute fixture: five simultaneous SRVs, hardware format checks, 20 subresource uploads using device footprints, explicit copy-to-shader transitions, point/bilinear/trilinear sampling, wrap/mirror/clamp/border addressing and fenced float readback. Windows execution is pending; tolerance is 2/255 per component. Border cases with missing BC4/5 channels are kept in bounds to avoid unverified format-swizzle semantics. This is a fixture, not an Xbox shader or application ABI integration.
- Added `-Focused` validation (builds Revenge and three relevant test targets only; runs three texture tests plus GPU sampling) and a source-only incremental update ZIP against the exact v0.45.0 package. The full validation remains available and is the default.
- Portable Release (IPO off): 64/64 CTest and 2/2 Python tests passed; focused selection 3/3 passed. Expected full Windows CTest count: 84. The pre-existing GCC filesystem `u8path` deprecation warning remains.
- Multiple render targets, render-to-texture, general guest loading and console platform/graphics APIs remain unimplemented. No protected or commercial executable is used.

### Subsequent Windows validation

- Focused v0.46.0: 3/3 tests passed (0.17 s); build 67.06 s; 320 GPU texture samples across five SRVs, 20 subresources and four samplers passed with maximum absolute error 7.62939e-06. Full Windows suite not run.

## v0.45.0 — texture reference and faster iteration

- Added CPU BC1 four-colour and transparent three-colour sampling, BC3 colour/alpha decoding, BC4 UNORM single-channel and BC5 UNORM dual-channel sampling.
- Extended upload footprints and independent linear/Morton8 block conversion to BC4 and BC5.
- Added mirror and transparent-black border addressing, independently configurable U/V addressing, point and linear mip filtering, clamped explicit LOD and preserved the original explicit-mip API.
- Added 79 known-answer colour comparisons (including partial blocks, inter-block filtering and layer isolation), non-finite LOD rejection and extended randomized layout tests.
- Windows GPU/multimedia backend now uses a shared object-library build to avoid compiling both source files twice. Windows linkage and actual build-time improvement remain unverified.
- Added `-FastBuild` to disable interprocedural optimization while retaining Release optimization, optional parallel build jobs and a build elapsed-time log. The default remains the normal optimized one-job build.
- Preserved the concise README and all historical release notes. Portable Release without IPO: 63/63 CTest and 2/2 Python tests passed. Expected Windows regression count: 83. See [validation notes](docs/COMPATIBILITY_045.md).
- No new console API, console texture layout or Xbox executable support. CPU block interpolation uses a deterministic reference and is not a claim of exact GPU hardware rounding parity.

### Subsequent Windows validation

- 83/83 tests passed (6.81 s); fast build with two jobs took 76.38 s; 79 CPU texture samples passed. GPU stress: 64 jobs, 5,005,312 verified pixels, 343.570 ms, zero retained resources. No Xbox execution.

## v0.44.0 — TLS template model and fixed unwind reference

- `tls-plan` validates readable template ranges, writable aligned index metadata, declared alignment and a 1 MiB instance cap; it builds independent non-executable byte vectors with zero-filled tails. It never writes the TLS index or invokes callbacks. The alignment field is metadata, not a promise of aligned vector storage.
- The fixed native image fixture contains one x64 unwind record for its built-in 40-byte stack frame. On Windows only, a test registers that record, looks it up, checks virtual unwinding against a synthetic stack context, executes the known wrapper and deregisters the table. No arbitrary input image is executed.
- The independent graphics compiler accepts inline integer selectors for 0 through 64 and -1 through -16 as raw 32-bit source bits, not FP numeric conversions. Tests cover the 81 MOV encodings and HLSL text; exceptional floating-point GPU behaviour is not established.
- The MSVC C4456 shadowing warning in `init_metadata.cpp` is addressed. The validation script retains one-command testing and adds TLS template and shader operand results. Portable Release: 62/62 CTest and 2/2 Python tests passed; expected Windows count is 82. See [validation notes](docs/COMPATIBILITY_044.md).

```bat
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1
```

### Subsequent Windows validation

- 82/82 CTest cases passed (6.72 s); fixed unwind registration, lookup, virtual stack recovery and deregistration passed.
- 4,096 isolated TLS instance pairs across eight workers; seven malformed inputs rejected; no OS TLS installation.
- 81 bit-exact CPU shader operand and HLSL emission checks passed.
- 64 GPU jobs, 5,005,312 verified pixels, 333.183 ms, zero retained resources. Automated validation completed. Audio and physical controller checks were not requested. No Xbox executable was run.

## v0.43.0 — dependency coverage and graphics instructions

- `dependency-graph` traverses direct imports of explicitly supplied files, counts distinct unprovided modules, and terminates on repeated modules or cycles. No DLL search, loading, binding, delay/forwarder edge discovery or API-set mapping occurs.
- `init-plan` adds TLS raw-template permission diagnostics and version-1 unwind slot grouping, operand-length, prologue ordering and local stack-adjustment checks. This is not an unwind interpreter or guest TLS initialisation.
- The independent vertex/pixel compiler accepts GFX10 VOP2 `v_min_f32` and `v_max_f32` and eight inline FP32 constants (selectors 240–247). Normal finite values are the reference scope; exceptional FP behaviour is not validated.
- `package-create-minmax` produces an independent shader package with a min/max vertex transform; the existing framebuffer oracle is reused for synchronous and asynchronous GPU comparisons. The host HLSL compiler can optimise the arithmetic; the framebuffer test is not proof of individual native GPU opcodes.
- Linux Release: 60/60 CTest, 2/2 Python tests. Expected Windows/MSVC: 79 CTest cases and existing GPU checks plus the min/max package check. Reported Windows run passed 79/79 and the min/max vertex GPU comparison. See [validation notes](docs/COMPATIBILITY_043.md).

```bat
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1
```


## v0.42.0 — compiler reference and validation capture

- A separately compiled reference DLL and linked native client exercise the ordinary OS loader, three C exports, compiler-generated thread-local storage, nested C++ exception unwinding, and TLS destructor completion after Revenge managed-thread joins. The client compares OS/compiler TLS with the independent ABI's scalar TLS values in 32 threads, with 8,192 comparisons and 8,192 unwind checks per run. This is a reference workload, not a guest TLS or exception implementation.
- Windows-only regression checks inspect the actual DLL exports, its initialisation metadata, and direct imports into both reference and Revenge ABI DLLs. No arbitrary PE mapping or additional execution capability is introduced.
- The single-command validation wrapper now explicitly captures native stdout/stderr and build/test output into the same UTF-8 log rather than relying on PowerShell transcripts. A Windows-only regression check tests both streams and preservation of a deliberate nonzero process exit.
- Linux Release: 56/56 CTest and 2/2 Python tests; 20 repeated reference runs passed. Expected Windows/MSVC: 74 CTest cases plus existing GPU tests. Windows logging and reference DLL metadata checks are unverified locally. See [validation notes](docs/REFERENCE_042.md).

```bat
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1
```


## v0.41.0 — fixed native image execution and target inventory

- A separate fixed-fixture executable constructs its own minimal PE32+ image, prepares two distinct native mappings, applies a DIR64 relocation and R/RX/RW/no-access page permissions, verifies zero fill, and invokes only its built-in entries. One entry updates isolated data; the other calls one explicitly declared, signature-matched host function. No arbitrary executable, dynamic module loading, guest callbacks or console platform bindings are supported by this test.
- `target-plan` reports PE64 alignment, stack/heap metadata, direct import classifications, named and ordinal RVA-form delay imports, load configuration/guard flags and CLR presence. The inspection is offline; it does not modify security settings or resolve signatures. Console indicators are heuristics.
- The Windows wrapper retains previous GPU and game validation and includes the fixed fixture and target report. An optional authorised target remains strictly offline. The official GDKX source probe remains unchanged and requires an authorised development environment.
- Portable validation: 55/55 CTest and 2/2 Python. Expected Windows/MSVC: 69 CTest cases, plus the existing GPU checks. See [native image validation](docs/NATIVE_IMAGE_041.md).

```bat
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1
```

## v0.40.0 — source-level platform probe and expanded graphics

- `RevengePlatformProbe` exercises the exact Arena gameplay core (collision and four collectibles), round-trips its checkpoint trace through native ABI memory and TLS, and checks clock and cleanup. It is a Windows/Linux host application, not a console executable.
- `samples/platform/gdk_probe.cpp` is a source-only test for an authorised official GDKX project. It calls the actual device-type API after normal official runtime initialisation, reports its result, and runs the same game workload. It is not compiled or shipped as a binary, and does not pretend to be a port of the console runtime.
- The `--compare` trace mode compares a single supplied gameplay trace against the host results; matching traces establish only agreement of the shared game logic, not provenance, ABI compatibility or Xbox execution.
- Custom RSP1 packages now accept validated vertex and pixel programs with the same fixed compute fixture. A new narrow-geometry package tests a translated vertex multiply using a pixel oracle, on both synchronous and asynchronous GPU paths when the hardware test is run. Known-answer scene commands still require the exact original package.
- One-command validation retains all previous checks and adds host trace validation and the custom vertex GPU comparison. See [platform research and validation](docs/PLATFORM_040.md).
- Portable Release: 53/53 CTest; 2/2 Python. Expected Windows: 65 CTest cases plus hardware checks. GDKX build and execution remain blocked by unavailable authorised development environment.

```bat
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1 -Play
```


## v0.39.0 — consolidated development release

- `RevengeArena` is a small independently written 3D collect-and-avoid game. It links only the public native ABI; movement, walls, depth-tested boxes, textured geometry, controller polling, sound and checksummed save data exercise the runtime together. The native window displays the D3D12 framebuffer via GDI; it is not a console title or zero-copy presentation.
- Seven additive ABI operations provide media capabilities, PCM16 playback, XInput polling, local snapshot save/load, custom pixel-program package loading and aligned partial resource updates (63 total operations, five DLL exports, ABI v1 unchanged). Managed callback threads handle audio in the interactive game.
- Custom graphics accepts the existing fixed compute/vertex programs and validated pixel-stage programs, including up to eight ordered samples with explicit waits. The game uses an RGBA export package, and compares it against the legacy BGR package on GPU in the hardware test. The async worker drains before changing program pipelines.
- `init-plan` checks TLS index/callback page permissions and bounded unwind header/slot/tail metadata in a non-executable data model. `-AuthorisedTarget` optionally runs offline reports on an explicitly supplied authorised PE64 file; it does not execute or bind that file. Real Xbox platform services and executable initialisation are not implemented.
- Linux Release: 50/50 CTest and 2/2 Python. Expected Windows CTest count: 61, plus integrated GPU and stress tests. See [consolidated design and validation](docs/CONSOLIDATED_039.md).

One command builds, runs the full suite and GPU checks, writes a validation transcript and opens the independent game:

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1 -Play
```

Omit `-Play` for unattended validation. Add `-Hardware` for an XAudio2 stream completion and XInput slot-zero probe. Controls: WASD or arrows / left stick; collect the four gold blocks; F5 save, F9 load, R reset, Esc quit. No commercial game compatibility is claimed. Version numbers v0.34–v0.39 are consolidated for testing convenience, not a claim that all original platform milestones are complete.

## v0.33.0 — managed native threads and dependency chains

- Six additive ABI v1 operations create, poll, wait, query exit codes, release and count trusted native callback threads. Up to 16 retained handles; managed threads automatically detach TLS values before completion. Existing five DLL exports and request layout remain unchanged.
- Offline `dependency-chain` follows named and ordinal forwarders through explicitly supplied file inventories with cycle detection and a 16-hop cap. It never searches for or loads modules, binds imports, or maps API-set contracts.
- Portable tests: 48/48 CTest and 2/2 Python. Expected Windows suite: 58 tests. See [design and validation](docs/THREADS_AND_CHAINS.md).

```bat
.\build\Release\revenge_abi_threads_fixture.exe
.\build\Release\revenge_chain_tests.exe
.\build\Release\Revenge.exe dependency-chain build\Release\revenge_abi_tls_fixture.exe build\Release\revenge_abi.dll
```

## v0.32.1 — Windows compilation fix

- Add the explicit `<string>` include to the native TLS fixture for `std::to_string`. The reported MSVC build compiled the runtime and DLL successfully but failed on the TLS fixture. No ABI or runtime behaviour changes.
- See [patch validation](docs/VALIDATION_0.32.1.md). Windows rebuild subsequently passed all 55 CTest cases, as reported by the tester.

## v0.32.0 — dependency inventory and native thread-local values

- Offline `exports` parses file-backed PE64 export tables, names, aliases, ordinals, holes and forwarder strings. `dependency-plan` compares direct imports with explicitly supplied files; it does not follow forwarders, search for modules, map API sets or bind addresses. Export presence is not ABI compatibility.
- Six additive ABI v1 operations expose bounded per-session, per-native-thread scalar slots. No guest PE TLS initialization, thread creation, or exception registration is implemented. Clients explicitly detach before worker exit.
- See [dependency and TLS design](docs/DEPENDENCY_TLS.md) and [validation](docs/VALIDATION_0.32.md).

```bat
.\build\Release\revenge_export_tests.exe
.\build\Release\revenge_abi_tls_fixture.exe
.\build\Release\Revenge.exe exports build\Release\revenge_abi.dll
.\build\Release\Revenge.exe dependency-plan build\Release\revenge_abi_tls_fixture.exe build\Release\revenge_abi.dll
ctest --test-dir build -C Release --output-on-failure
```

## v0.31.0 — system services and PE64 layout diagnostics

- ABI v1 retains its five exports and request layout; 13 additive operations expose owner-checked nonrecursive mutexes, bounded semaphores, session RAM storage and synchronization counts.
- Independent client verifies 4,096 mutex-protected increments, four worker threads, semaphore overflow and timeout, storage roundtrip and isolation, stop cancellation and cleanup.
- Offline `layout-plan` reports section-derived 4 KiB page permission spans, gaps, W+X, entry-page flags and IAT diagnostics. It does not map executable memory or bind imports.
- Linux Release: 44/44 CTest, 2/2 Python; Windows 51/51 subsequently reported by the tester. See [system ABI](docs/SYSTEM_ABI.md) and [validation](docs/VALIDATION_0.31.md).

```bat
.\build\Release\revenge_abi_services_fixture.exe
.\build\Release\Revenge.exe layout-plan build\Release\revenge_abi_services_fixture.exe
ctest --test-dir build -C Release --output-on-failure
```

## v0.30.0 — reusable GPU frame slots

Prior Windows validation: 48/48 CTest; 64 GPU stress jobs, 5,005,312 verified pixels, 56 allocation reuses and 8 allocation sets. Peak two submissions before collection.

- ABI v1 is unchanged. Four retained host jobs, two independently backed GPU slots, one direct queue and a monotonic internal hardware fence.
- A worker can submit two jobs before collecting either readback. Each slot retains upload vertex/index buffers, texture and texture upload, color/depth targets, readback, descriptors and its allocator/list. Same-shape jobs reuse them; shape changes rebuild a retired slot. Full resource data is uploaded on each job.
- Checked fence completion and readback precede slot reuse. Device, root signature and translated PSO remain persistent. Allocation/reuse and peak submitted-uncollected counts are printed; they are not measured GPU concurrency or VRAM residency.
- New 64-job stress fixture checks two framebuffer extents, changing textures and out-of-order job collection. The portable mode validates command snapshots; GPU mode independently checks each readback. See [GPU slots](docs/GPU_SLOTS.md) and [validation](docs/VALIDATION_0.30.md).

```bat
.\build\Release\Revenge.exe package-create scene.rsp
.\build\Release\revenge_abi_commands_fixture.exe scene.rsp --async-gpu
.\build\Release\revenge_abi_commands_fixture.exe scene.rsp --async-stress
.\build\Release\revenge_abi_commands_fixture.exe scene.rsp --async-gpu-stress
ctest --test-dir build -C Release --output-on-failure
```

## v0.29.0 — asynchronous graphics jobs

- ABI v1 adds seven operations for same-size resource updates, asynchronous submission, polling, waiting, reading, releasing and counting jobs.
- Four retained job slots per session; each accepted job owns immutable copies of geometry, texture, commands and shader package. Resources may be updated or released after submission.
- A single worker processes jobs in FIFO order. On Windows it retains a D3D12 device, root signature, translated PSO and direct queue between successful jobs. Each job still allocates its own buffers, texture, framebuffer, command allocator and fence and completes before the next job executes. No simultaneous GPU frames or external hardware fence handles.
- Portable validation-only jobs check commands, not rendered pixels. GPU readback is checked by the independent client. See [asynchronous ABI](docs/ASYNC_ABI.md).

```bat
.\build\Release\Revenge.exe package-create scene.rsp
.\build\Release\revenge_abi_commands_fixture.exe scene.rsp --async
.\build\Release\revenge_abi_commands_fixture.exe scene.rsp --async-gpu
ctest --test-dir build -C Release --output-on-failure
```

## v0.28.0 — application-defined graphics resources and commands

- ABI v1 remains binary compatible; five additional operations create immutable vertex/index/texture/command resources, release them, validate command lists, submit a synchronous indexed render pass and query counts.
- Application supplies geometry, RGBA8 texture, shader package handle and up to 1,024 indexed draw ranges; the backend translates the existing restricted vertex/pixel instructions and returns a packed RGBA8 framebuffer to the independent client. The custom path uploads the supplied texture rather than running the fixed compute texture generator.
- Resource handles are session-owned; 64 resources and 8 MiB aggregate resource data, at most 1 MiB per transfer. Stale dependencies, malformed commands, incorrect handle types, invalid indices and buffer limits are rejected. STOP clears packages and resources.
- Submission is synchronous offscreen, with device and GPU allocations created per submission; custom readback is bounded to 320–512 × 240–512. General device handles, asynchronous queues and custom on-screen rendering are pending.

```bat
.\build\Release\Revenge.exe package-create scene.rsp
.\build\Release\revenge_abi_commands_fixture.exe scene.rsp
.\build\Release\revenge_abi_commands_fixture.exe scene.rsp --gpu
ctest --test-dir build -C Release --output-on-failure
```

Reuse an existing known `scene.rsp`. The optional GPU test independently checks two submissions with different texture data, colour export and depth ordering. See [command ABI](docs/COMMAND_ABI.md).

## v0.27.0 — continuous translated graphics

- New additive ABI v1 operation `RV_GFX_RUN` (19) returns a fixed-width frame statistics record; `RV_GFX_CONTINUOUS` advertises the compiled Windows backend.
- A single bounded run retains the device, translated VS/PS, compute-generated texture, mesh, pipeline and command allocator/list. Each frame draws both indexed meshes again, shifts the viewport, copies to one of two DXGI buffers and presents. GPU fences serialize frame reuse; this is not an asynchronous multi-frame scheduler.
- Automatic horizontal motion, focused-window arrow keys and first connected XInput controller left stick. Escape closes the window. Timing excludes startup and final readback, reports successful Present calls and mean GPU draw/copy timestamps, not scanout or gameplay FPS.
- First and final frames are checked against the CPU oracle (final shifted by an integer pixel offset). The fixed render extent is stretched to the window; compute texture generation happens once. General command lists and externally owned device handles are still pending.

```bat
.\build\Release\Revenge.exe package-create scene.rsp
.\build\Release\revenge_abi_graphics_fixture.exe scene.rsp 1920 1080 10 60
.\build\Release\revenge_abi_graphics_fixture.exe scene.rsp 3840 2160 10 0
ctest --test-dir build -C Release --output-on-failure
```

Reuse an existing `scene.rsp`. Last argument is the cap (0 uncapped, 1–240 capped); duration 1–30 seconds. See [continuous graphics](docs/CONTINUOUS.md). No Xbox ABI or gameplay.

## v0.26.0 — native ABI graphics integration

- Five additive ABI v1 operations: compiled backend flags, load validated RSP1 bytes, release a package, submit a synchronous verified draw, and query live package count. Existing structure layout and ABI version remain unchanged.
- Independent client links only `revenge_abi`; the DLL invokes the existing translated compute/vertex/pixel pipeline, waits for the GPU and returns the checked interior-pixel count. Optional native DXGI presentation copies the verified GPU render target directly to a swap-chain buffer and holds a static image for up to 30 seconds.
- Package handles are session-owned, bounded to 16, and cleared on STOP. Invalid and stale handles, malformed packages and invalid dimensions are rejected. Non-Windows drawing returns unsupported rather than a synthetic GPU result.
- The import planner labels the five known Revenge ABI declarations separately, without export verification or binding; the Microsoft atomic-wait runtime is classified as a host audit candidate.

```bat
.\build\Release\Revenge.exe package-create scene.rsp
.\build\Release\revenge_abi_graphics_fixture.exe scene.rsp
.\build\Release\revenge_abi_graphics_fixture.exe scene.rsp 1920 1080 0
.\build\Release\revenge_abi_graphics_fixture.exe scene.rsp 3840 2160 10
ctest --test-dir build -C Release --output-on-failure
```

Use a fresh package filename if `scene.rsp` already exists, or reuse the previous known fixture. See [graphics ABI design](docs/GRAPHICS_ABI.md). Linux CTest 38/38 and Python 2/2 passed. Windows suite registers 44 tests; hardware GPU/presentation tests are opt-in. Static frame only, no general graphics command API, Xbox ABI or gameplay.

## v0.25.0 — native application ABI

- `revenge_abi` shared library exposes a versioned C calling interface; fixed-width request and stats records, numeric status codes, opaque per-session runtime contexts.
- Native memory allocation, bounded offset reads/writes, event services, monotonic clock, CPU scene command validation and lifecycle cleanup.
- Independent C++ executable exercises 4,096 worker results and invalid request cases; a separate C-language executable verifies layout, five-argument calls and nonzero-offset buffers. Neither links the core directly.
- Trusted same-process ABI only: no guest pointer sandbox, no IPC, no console import binding. Existing GPU tests remain available independently.

```bat
.\build\Release\revenge_abi_fixture.exe
.\build\Release\revenge_abi_c_fixture.exe
ctest --test-dir build -C Release --output-on-failure
```

See [native ABI design](docs/NATIVE_ABI.md). Linux Release CTest 37/37 and Python 2/2 passed. Windows tester reported 43/43, both ABI clients passed. v0.24.0 Windows tester reported 41/41, 10,287,152 checked pixels, accepted graphics cache and 70 offline import slots classified (none bound).

## v0.24.0 — shader package and application integration

- Versioned little-endian RSP1 package: compute, vertex and pixel instruction streams, bounded lengths and CRC32 over metadata and payload. Decoding validates each stage with the existing restricted ISA compiler. Not an Xbox shader format.
- The application fixture loads and validates the package, processes its compute words, round-trips the package through application storage and dispatches the existing translated scene using the supplied streams. GPU execution accepts only the known-answer fixture. No arbitrary guest shader execution.
- Optional local scene PSO caching uses adapter/driver identity, both compiled shader blobs, root signature and a fixed-state schema tag. Driver rejection falls back to uncached creation; cache saved after pixel verification. The underlying Windows path remains to be tested.
- Offline `import-plan` reports each direct IAT slot, classification and whether it is eligible for a host export audit. It never binds imports or launches the executable.

```bat
.\build\Release\Revenge.exe package-create scene.rsp
.\build\Release\Revenge.exe package-inspect scene.rsp
.\build\Release\Revenge.exe package-app-test scene.rsp
.\build\Release\Revenge.exe package-gpu-test scene.rsp 1920 1080 package-1080.bmp scene.pso
.\build\Release\Revenge.exe package-gpu-test scene.rsp 3840 2160 package-4k.bmp scene.pso
.\build\Release\Revenge.exe import-plan build\Release\revenge_runtime_fixture.exe
```

See [shader package design](docs/SHADER_PACKAGE.md). Linux Release CTest 35/35 and Python 2/2 passed. The Windows suite registers 41 tests. v0.23.0 tester reported 38/38 Windows tests and 10,287,152 verified GPU interior pixels across 1080p and 4K.


## v0.23.0 — translated graphics stages

- Decode VOP1 moves, VOP2 FP32 add/subtract/multiply, full-mask uncompressed EXP POS0/PARAM0/MRT0, restricted MIMG 2D `image_sample_l` and `s_waitcnt vmcnt(0)`. Reject unknown flags, descriptors, pending/uninitialized registers, nonzero or unknown LOD, missing exports and malformed streams.
- Generate separate HLSL vertex and pixel stages with a fixed independently defined register ABI and TEXCOORD0 linkage. The host supplies interpolated UVs; no RDNA2 interpolation opcodes or Xbox descriptors are implemented.
- The GPU fixture reuses the integrated compute-generated texture, depth-tested geometry and readback path, replacing both graphics shaders. The pixel export deliberately swaps red and blue, and the framebuffer oracle checks that change. The fixture uses SM5.1 DXBC via D3DCompile, not DXIL.

```bat
.\build\Release\Revenge.exe shader-stage-decode vertex samples\rdna2-vertex.hex
.\build\Release\Revenge.exe shader-stage-decode pixel samples\rdna2-pixel.hex
.\build\Release\Revenge.exe shader-stage-emit vertex samples\rdna2-vertex.hex vertex.hlsl
.\build\Release\Revenge.exe shader-stage-emit pixel samples\rdna2-pixel.hex pixel.hlsl
.\build\Release\Revenge.exe gpu-stage-test 1920 1080 stage-1080.bmp
.\build\Release\Revenge.exe gpu-stage-test 3840 2160 stage-4k.bmp
```

See [graphics stage design](docs/SHADER_STAGES.md). Linux CTest 33/33, Python 2/2; 8 vertex outputs, 4,096 CPU pixel results and 71,836 CPU framebuffer pixels checked. Windows tester subsequently reported 38/38 tests and both GPU fixtures passed. Native wave opcodes, general descriptor translation and interpolation instructions remain pending.

## v0.22.0 — bounded shader loops and logical wave masks

- Opt-in loop mode extends the existing flow decoder with signed branch displacements and SOP2 `s_add_u32` (including SCC carry). Backedges must preserve definite assignments; buffer loops are rejected. CPU and generated HLSL execution are limited to 1,024 instructions per invocation.
- The independent loop fixture runs 1–64 iterations according to group ID; all 4,096 CPU results matched a closed-form oracle. The old forward-only modes remain intact.
- Logical Wave32 and Wave64 fixtures model EXEC-filtered comparisons and masked writes with group-shared HLSL words. 8,192 CPU results matched independent mask calculations; no native RDNA2 EXEC/VCC opcodes are decoded by this path.
- Windows GPU tests compile generated compute HLSL with the existing known-answer harness. Vertex/pixel recompilation, texture instruction decoding and console ABI remain pending.

```bat
.\build\Release\Revenge.exe shader-loop-decode samples\rdna2-loop.hex
.\build\Release\Revenge.exe shader-loop-emit samples\rdna2-loop.hex samples\rdna2-loop-generated.hlsl
.\build\Release\Revenge.exe shader-loop-gpu-test
.\build\Release\Revenge.exe shader-wave-gpu-test 32
.\build\Release\Revenge.exe shader-wave-gpu-test 64
```

See [bounded loop and wave design](docs/SHADER_LOOPS.md). Linux Release CTest 30/30 and Python 2/2 passed; Windows GPU path untested locally.

## v0.21.0 — audio, controller input and persistent snapshots

- Portable stereo PCM16 mixer, saturating gain, one-second faded tone fixture at 48 kHz. Windows XAudio2 source/mastering voices and bounded playback completion test.
- XInput polling of the first connected controller, radial stick dead zones and normalized triggers; optional brief vibration, stopped on exit when the controller remains connected. No keyboard/mouse hook.
- `Persist` and `Restore` service requests use one trusted host-selected file, version/length/CRC32 validation, 1 MiB payload limit and same-filesystem staged replacement. No arbitrary application filesystem access or power-loss durability guarantee.

```bat
.\build\Release\Revenge.exe storage-test revenge-save.bin write
.\build\Release\Revenge.exe storage-test revenge-save.bin read
.\build\Release\Revenge.exe audio-test
.\build\Release\Revenge.exe input-test 10 0
```

See [multimedia and storage design](docs/MULTIMEDIA.md). The audio and input tests are standalone host fixtures, not guest services or a unified interactive application. Controller absence returns status 2.

## v0.20.0 — application runtime service foundation

- Typed request/reply dispatch, explicit created/running/stopped lifecycle, checked process-local memory and event handles.
- Native host memory arena, logical bounds checking, manual-reset events with bounded waits, monotonic elapsed clock and a private in-memory key/value store.
- An independent C++ fixture uses a host worker to verify 4,096 translated CPU ALU outputs, round-trips 16 KiB through temporary storage, then dispatches the existing integrated graphics fixture using its stored resolution configuration.
- CPU-only execution validates graphics commands; GPU mode invokes the existing offscreen compute-to-graphics pixel oracle. No guest import resolution, external application loader or persistent filesystem is added.

```bat
.\build\Release\Revenge.exe app-test
.\build\Release\Revenge.exe app-gpu-test 1920 1080 app-1080.bmp
.\build\Release\Revenge.exe app-gpu-test 3840 2160 app-4k.bmp
```

See [application service design](docs/APPLICATION.md). The new API is a trusted, linked C++ test interface, not a security boundary or an Xbox ABI.

## v0.19.0 — integrated compute-to-graphics fixture

- An independently decoded GFX10.3 ALU program selects four texture colours in generated HLSL. A compute queue produces a padded texture upload buffer; the direct queue waits on its fence, copies into a texture, and executes the indexed depth fixture. No CPU upload of the generated colours in this path. The existing pixel oracle verifies the result.
- A typed, generation-checked descriptor registry rejects foreign, stale and wrong-kind handles. A shader-visible heap uses an allocated nonzero descriptor slot; retired slots cannot be reused until their last graphics fence completes. It is a single-timeline, thread-confined registry, not a full resource manager.
- Presentation caps now use a high-resolution waitable timer when available, with standard timer/sleep fallback and a short yield tail. The cap is a maximum target, not a guaranteed rate.

```bat
.\build\Release\Revenge.exe gpu-integrated-test 1920 1080 integrated-1080.bmp
.\build\Release\Revenge.exe gpu-integrated-test 3840 2160 integrated-4k.bmp
.\build\Release\Revenge.exe gpu-present 20 3840 2160 0 1 120
```

See [integration architecture](docs/INTEGRATION.md). The integrated scene is offscreen; the on-screen fixture is still the triangle. Actual Xbox shader stages, descriptor formats, API dispatch, application ABI and game execution remain unimplemented.

## v0.18.0 — indexed perspective and depth fixture

- Portable command validation: begin, bind mesh, bind texture, indexed draw, end. Rejects missing bindings, invalid indices and malformed command order.
- Two overlapping perspective-projected textured quads at different depths, submitted near-first to check depth rejection. Exact offscreen dimensions up to 3840×2160; CPU pixel oracle excludes rasterization and sampling boundaries.
- Existing triangle presentation adds optional fixed-size swap-chain rendering, frame-rate cap and exit-reason reporting. The client extent and rendering extent are reported separately.

```bat
.\build\Release\Revenge.exe gpu-scene-test 1920 1080 scene-1080.bmp
.\build\Release\Revenge.exe gpu-scene-test 3840 2160 scene-4k.bmp
.\build\Release\Revenge.exe gpu-present 20 3840 2160 0 1 120
```

See [scene architecture](docs/SCENE.md). This is a restricted desktop command fixture, not a general console command dispatcher. Native scene execution and fixed render extent passed Windows tests reported by the tester; the frame cap underperformed. No recompiled graphics shader, borderless fullscreen, shared descriptor allocator or cross-queue scene integration yet.

## v0.17.0 — native desktop presentation

- Resizable Win32 window with an animated coloured triangle, triple-buffered flip-discard DXGI swap chain and a bounded number of GPU submissions in flight.
- VSync on by default; optional interval zero with tearing only when supported. Esc or Close stops the fixture. Minimized windows pause rendering.
- Live successful Present calls/s in the title bar; CPU frame-loop and GPU timestamp summaries at exit. Present calls are not physical scanout FPS.
- Back-buffer allocator reuse and resize require completed fences. The viewport tracks client dimensions.

```bat
.\build\Release\Revenge.exe gpu-present 20 1280 720 1
.\build\Release\Revenge.exe gpu-present 20 1920 1080 0
```

See [presentation architecture](docs/PRESENTATION.md). This standalone desktop fixture is not the console graphics dispatcher, and does not use recompiled Xbox shaders.

## v0.16.0 — resource transfers

- Portable transfer pool validates recording order, fence completion, slot reuse and retirement.
- Dedicated D3D12 copy queue with per-slot upload, default and readback buffers; host arena staging.
- Every submission uses a sequence-dependent payload and verifies all returned bytes before reuse or destruction.
- Logical payload budget and conservative DXGI local-budget check; not a residency manager.

```bat
.\build\Release\Revenge.exe gpu-transfer-test 64 3 4
.\build\Release\Revenge.exe gpu-transfer-test 64 1 4
.\build\Release\Revenge.exe gpu-transfer-test 64 6 4
```

See [resource transfer architecture](docs/TRANSFERS.md). This fixture is not zero-copy, shared CPU/GPU virtual addressing, or a console graphics API dispatcher.

## v0.15.0 — Native virtual memory foundation

Private host page reservation, commitment, protection, aligned allocation, process-local generation handles, budget and fragmentation tracking, serialized access and logical fence retirement. Run `Revenge.exe vm-bench 8 4` and the regression suite. Linux passed 20/20 CTest cases and 2 Python tests; the tester subsequently reported 25/25 Windows tests and 800 MiB total verified across three benchmarks. See [virtual memory design](docs/VIRTUAL_MEMORY.md). No Xbox memory ABI or GPU coherence implementation is claimed.

## v0.14.0 — Texture layout and graphics cache foundations

```powershell
.\build\Release\Revenge.exe texture-gpu-test cache\texture.pso
.\build\Release\Revenge.exe gpu-render 240 3 revenge-frame.bmp cache\graphics.pso
.\build\Release\Revenge.exe gpu-render 240 3 revenge-frame.bmp cache\graphics.pso
ctest --test-dir build -C Release --output-on-failure
```

The GPU conversion fixture verifies 4,096 independently generated RGBA8 bit patterns. The CPU model handles mip levels, arrays, aligned upload footprints and block payloads. It also includes point and bilinear CPU sampling. Morton8 is a synthetic layout, **not** a verified Xbox swizzle. BC1/BC3 data is copied as opaque blocks, not decoded. The graphics cache is restricted to the fixed desktop triangle PSO schema. Use only locally generated caches; driver rejection falls back to normal creation. See [texture design](docs/TEXTURE.md).

## v0.13.0 — Mixed shader compiler and compute PSO cache

The forward-flow compiler now supports a restricted buffer resource mode with definite-assignment checks, mandatory waits before control-flow boundaries and exactly one output store on every path. The earlier standalone ALU and memory compilers remain for regression testing. See [SHADER_UNIFIED.md](docs/SHADER_UNIFIED.md).

```bat
.\build\Release\Revenge.exe shader-unified-decode samples\rdna2-unified.hex
.\build\Release\Revenge.exe shader-unified-emit samples\rdna2-unified.hex samples\rdna2-unified-generated.hlsl
.\build\Release\Revenge.exe shader-unified-gpu-test cache\unified.pso
.\build\Release\Revenge.exe shader-unified-gpu-test cache\unified.pso
```

Use a private, locally generated cache, not downloaded driver blobs. Cache miss or driver rejection falls back to normal pipeline creation. The compute cache does not avoid HLSL compilation. The graphics fixture now has a separate cache. Actual console texture instructions, loops, divergent GPU wave operations, console descriptors and title execution remain pending.

## v0.12.0 — Buffer instructions and resource binding research

Run from the repository root, after building:

```powershell
.\build\Release\Revenge.exe shader-memory-decode samples\rdna2-buffer.hex
.\build\Release\Revenge.exe shader-memory-emit samples\rdna2-buffer.hex samples\rdna2-buffer-generated.hlsl
.\build\Release\Revenge.exe shader-memory-gpu-test
ctest --test-dir build -C Release --output-on-failure
```

The separate compiler accepts selected public GFX10 MUBUF dword load/store encodings, a full vector-load wait, and integer add. It binds a fixed 4,096-element read-only buffer to t0 and a separate output to u0. It rejects unsupported addressing, flags, descriptors, register hazards, unaligned offsets and missing termination. The fixture adds adjacent input elements modulo 2^32; the last lane reads zero past the logical input end. Windows GPU execution uses a padded upload buffer, generated cs_5_1 HLSL and the existing fenced readback harness. All 4,096 outputs must match the independent CPU oracle. No commercial shaders or proprietary descriptor bytes are used.

Optional offline HLSL-to-DXIL compilation (requires separately installed DXC):

```powershell
py -3 tools\shaders.py compile samples\rdna2-buffer-shaders.json --dxc 'C:\Tools\dxc\bin\x64\dxc.exe' --cache cache\rdna2-buffer --jobs 2
```

This manifest is for offline compilation only. Use `shader-memory-gpu-test` for the fixed-buffer GPU fixture. See [shader buffer design](docs/SHADER_MEMORY.md). Dynamic resource descriptors, real console ABI, texture sampling, divergent wave instructions, loops and driver PSO caching remain outstanding.

## v0.11.0 — Control flow, FP32 and wave mask research

Run these commands separately in the repository root after building:

```powershell
.\build\Release\Revenge.exe shader-flow-decode samples\rdna2-flow.hex
.\build\Release\Revenge.exe shader-flow-emit samples\rdna2-flow.hex samples\rdna2-flow-generated.hlsl
.\build\Release\Revenge.exe shader-flow-gpu-test
ctest --test-dir build -C Release --output-on-failure
```

The new compiler is separate from the existing straight-line integer optimizer. It decodes forward SOPP branches and unsigned SOPC conditions, moves, selected integer operations and FP32 add/subtract/reverse-subtract/multiply. It rejects backwards jumps, literal targets and registers or SCC not initialized on all incoming paths. The GPU fixture uses only finite normal FP32 operands and results, round-to-nearest-even, and a self-authored fixture ABI. It tests addition and multiplication on different threadgroups, with 4,096 bit-exact expected outputs. The CPU tests also exercise subtract and reverse-subtract. No loop, divergent EXEC control flow, memory, texture or console shader resource translation is implemented.

Optional DXC compilation of the generated HLSL, using the existing parallel source cache:

```powershell
py -3 tools\shaders.py compile samples\rdna2-flow-shaders.json --dxc 'C:\Tools\dxc\bin\x64\dxc.exe' --cache cache\rdna2-flow --jobs 2
```

The flow manifest is for offline compilation only; `gpu-compute` still expects the original integer fixture. Use `shader-flow-gpu-test` for the new built-in fixture. The wave mask model is CPU-only; native wave operations and persistent driver PSO caching remain planned. See [flow shader design](docs/SHADER_FLOW.md).

## v0.10.0 — RDNA2 integer ALU research

```powershell
.\build\Release\Revenge.exe shader-decode samples\rdna2-alu.hex
.\build\Release\Revenge.exe shader-emit samples\rdna2-alu.hex samples\rdna2-generated.hlsl
.\build\Release\Revenge.exe shader-gpu-test
```

The decoder accepts a bounded text sequence of little-endian GFX10.3 instruction dwords (numeric hexadecimal words, not raw binary files). The fixture ABI seeds v0 with a dispatch index, assumes all lanes active and observes only v0 at termination; generated HLSL writes to u0. The optimizer propagates constants, folds integer arithmetic and removes dead register writes before emitting uniquely named uint temporaries. The built-in GPU test compiles with system D3DCompile (cs_5_1) and checks all 4,096 outputs, without needing DXC. It does not run an Xbox shader binary.

Optional offline DXIL compilation of the generated fixture (DXC must be installed separately):

```powershell
py -3 tools\shaders.py compile samples\rdna2-shaders.json --dxc 'C:\Tools\dxc\bin\x64\dxc.exe' --cache cache\rdna2-fixture --jobs 2
```

The manifest expects the HLSL file generated by `shader-emit`. The existing AOT cache includes the generated source and compiler identity. For the included fixture only, its output obeys the known-answer layout used by `gpu-compute`. Driver PSO caching for selected fixtures was added in later releases. See [shader design and limits](docs/SHADER_IR.md).

## v0.9.0 — desktop graphics fixture

Run from the repository root after building:

```powershell
.\build\Release\Revenge.exe gpu-render 240 2 revenge-frame.bmp
.\build\Release\Revenge.exe gpu-render 240 1
.\build\Release\Revenge.exe gpu-render 240 3
```

The positional arguments are frame count (1–2000), submission slots (1–8), and an optional BMP destination (overwrites an existing file at that path). An optional fourth positional argument is a private local graphics PSO cache path (v0.14.0). The defaults are 240 frames and two slots. The fixture uses an independently authored HLSL vertex and pixel shader compiled by the Windows system `D3DCompile` interface (Shader Model 5.1), a 2×2 RGBA texture, persistent descriptor heaps and a reusable graphics PSO. It draws offscreen at 256×256. There is no window presentation or Xbox shader translation in this command.

The texture upload uses `GetCopyableFootprints`, and command allocator reuse is gated by completion fences. GPU timestamp intervals cover clear/draw and related graphics commands, not the whole workload. CPU wall time includes submission, initial texture upload and final readback completion. Five known pixels in the final frame must match the expected background and texture colors before an image is saved. The BMP is an optional visual aid, not proof of correctness for all pixels. None of these metrics is game FPS. The explicit slot count is not automatically applied to gameplay; the existing `gpu-tune` command still only recommends a synthetic transfer configuration.

For the current graphics work, see [Microsoft D3D12 timing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing), [texture upload and readback](https://learn.microsoft.com/en-us/windows/win32/direct3d12/upload-and-readback-of-texture-data), and [command list recording](https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles). The fixture is opt-in and not part of unattended CI because it requires a compatible graphics adapter.

## v0.8.0 system and performance tools

```powershell
.\build\Release\Revenge.exe cpu-info
.\build\Release\Revenge.exe api-registry
.\build\Release\Revenge.exe gpu-tune 240 4
.\build\Release\Revenge.exe map-path 'C:\RevengeData' 'G:\samples\texture.bin'
```

`gpu-tune` uses 1, 2, 3, 4, 6 and 8 transfer slots subject to a budget cap (one sixteenth of remaining reported local budget, capped at 256 MiB, including estimated upload/readback sizes). Each candidate is measured three times with final readback verification. It recommends the smallest ring within 95% of peak median throughput. It does not apply any game settings or measure game FPS. GPU memory budgets may change between queries, and resource allocation overhead is not fully captured by the estimate. The previous fixed-slot benchmark remains available.

The CPU page model is a bounded, CPU-only test abstraction. It models explicit physical page allocation and virtual aliasing, but does not expose a console ABI, OS physical pages, GPU addresses, GPU ownership epochs, or automatic transfer scheduling. It requires external locking. Path mapping is a synthetic lexical test only, not verified Xbox drive semantics, filesystem interception or a sandbox. No file is opened. The audit checks an explicit allowlist of Windows API-set names with the OS loader; no game DLLs are loaded and export presence is not ABI equivalence.

### Optional DirectX Agility SDK

Default builds use the system D3D12 runtime. To experiment with an authorised app-local SDK, obtain the x64 `D3D12Core.dll` from the official [Microsoft Agility SDK package](https://devblogs.microsoft.com/directx/directx12agility/) and supply the matching numeric `D3D12SDKVersion` from Microsoft's release table, e.g. 620 for package 1.620.2. The following directory must contain the matching DLL:

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-Revenge.ps1 -AgilityDirectory 'C:\SDKs\Agility\build\native\bin\x64' -AgilityVersion 620
.\build\Release\Revenge.exe gpu-info
```

The build exports the SDK version and app-local path and copies the supplied core DLL into the build directory only. No SDK binaries are included in this repository. A mismatched DLL may prevent device creation. Rebuild without the options to use the inbox runtime again. The feature probe still caps the reported shader model at 6.6; higher shader models are not required or used by the current sample. This option has not been tested on Windows and does not guarantee a higher shader model or FPS.
