# v0.39.0 — Consolidated integration release

## Scope and status

This version combines work on the proposed v0.34–v0.39 roadmap in one archive and one validation run. It is **not** a completed Xbox compatibility layer. No authorised Xbox-targeted executable, GDK environment, or console API contract was supplied for this build. The supplied application is a separately compiled native host client. No console title, firmware, key, or SDK assets are included.

| Milestone | Implemented here | Remaining |
|---|---|---|
| Multimedia / graphics ABI | XAudio2 PCM16, XInput poll, graphics jobs, snapshot save/load | Windows hardware validation |
| Independent playable 3D game | Arena geometry, depth, textures, player motion, collision, four pickups, win condition, input, audio, save and FPS | Interactive Windows validation |
| Authorised target compatibility | Explicit PE64 offline inventory, layout, init and dependency-chain reports; source-level native ABI example | Real authorised Xbox-targeted test build and compatibility contract |
| Platform system translation | Existing independent system ABI plus native multimedia/storage bridge | Xbox platform-specific calling contracts and service translation |
| Executable initialisation/memory | Non-executable PE layout and relocation data model; TLS permissions and unwind metadata report | Guest memory mappings, import binding, TLS initialization, unwind registration, entry execution |
| Graphics/shaders | Partial aligned updates, custom pixel programs, multiple ordered samples, PSO switching with previous slot collection | Broader RDNA2 ISA, resource bindings, console graphics API and game workloads |

## Single build/test run

From the extracted project on Windows, run:

```
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1 -Play
```

The wrapper invokes the normal build and CTest suite, generates independent shader packages in a fresh validation directory, tests six integrated graphics jobs including both RGB and BGR program variants, runs the prior 64-job GPU stress regression, and prints initialisation/dependency reports for the independent executable. It saves a transcript to `validation-*/validation.log`. `-Hardware` selects the integrated audio/controller probe; a disconnected controller is reported, not treated as a successful physical input test. Audio hardware failure will fail that requested probe. `-Play` starts the game after the tests. The game uses `arena-save.rvs` in the project directory, and existing unrelated files are not overwritten by snapshot storage. The wrapper itself has not been executed on Windows here.

Optional `-AuthorisedTarget <path>` performs offline analysis only on a file the caller has authorisation to inspect. `-DependencyDlls` accepts a PowerShell array of explicit local DLL paths. No discovery, loading, execution, import address binding or API-set mapping is performed by these reports. The regular Windows fixture runner remains unchanged. A report is not a compatibility verdict.

## Native ABI additions

The five DLL exports and `rv_request` (56 bytes) are unchanged. `rv_pad_state` is 40 bytes; all values are fixed-width integers or native IEEE binary32. The new operation numbers are 57–63. See `include/revenge/native_abi.h` for the exact contract. Audio is synchronous per request and runs on a managed callback thread in the interactive game. There is one short effect at a time. Snapshot paths are explicit trusted UTF-8 paths and use the existing checksummed local snapshot implementation; the total ABI transfer is still capped at 1 MiB including the path. Do not race snapshot writers or stop/close with active media operations. `RV_RES_PATCH` permits aligned byte-range updates to mutable vertex, index or RGBA8 resources; it validates changed vertices, preserves job snapshots, and does not reduce the GPU upload volume. Job mode 2 suppresses noisy per-frame logs but otherwise uses the same GPU path.

## Graphics limits

The custom renderer keeps fixed known compute and vertex programs, one texture/sampler, RGBA8 readback, indexed triangle draws, and at most eight sequential explicit-LOD-zero samples of that texture. Pixel shaders still use a restricted GFX10.3 instruction subset. `RV_GFX_LOAD_CUSTOM` allows only validated programs with fixed compute/vertex stages; known-answer `RV_GFX_DRAW` and `RV_GFX_RUN` still require the original exact program. The device is recreated on a shader-program change after previous submissions are collected; this is not a multi-PSO cache. The repeated sample in the RGBA example can be optimized away by the downstream shader compiler, so the GPU comparison does not prove two distinct sampling instructions execute in hardware. The game runs at 320×240, uses a fixed perspective camera and CPU-generated world-space geometry, with depth testing and texture sampling on D3D12. Display uses a Win32 window with GDI blitting of GPU readback (not DXGI zero-copy or commercial game FPS). Movement is a simple 2D ground-plane collision model; no complex physics or streaming assets.

## Validation

Local Linux Release build and CTest: 50/50 passed. Python shader tests: 2/2 passed. The integrated CPU path verified collision, scripted completion of all four pickups, six validation-only jobs, custom package acceptance, rejected invalid patch, runtime clock, cross-session saved-state restoration and resource cleanup. The portable shader tests verify repeated sample decode/evaluation/HLSL emission, and reject missing waits. Synthetic metadata tests check TLS page permissions and unwind header/handler diagnostics. The optional Windows integrated GPU test checks that 76,800 initial framebuffer pixels obey the RGB/BGR channel permutation, that the scene has more than 5,000 nonblack pixels and that subsequent game frames change. These are programmed checks, not measured Windows results. Windows build, 61 expected CTest cases, actual GPU readback, controller and audio, and interactive gameplay remain unverified.
