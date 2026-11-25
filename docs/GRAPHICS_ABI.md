# Native graphics ABI extension (0.26.0–0.27.0)

The v1 native C ABI keeps all five exports and both record layouts from 0.25.0. These additive operations use the same `rv_dispatch` call. All unused fields must be zero. No arbitrary guest GPU addresses or raw D3D objects are exposed.

| Operation | Request | Successful result |
|---|---|---|
| RV_GFX_CAPS (14) | no fields | compiled backend flags: GPU=1, presentation=2, continuous=4; Linux=0 |
| RV_GFX_LOAD (15) | count = buffer capacity, caller buffer contains RSP1 bytes | opaque session-owned package handle |
| RV_GFX_RELEASE (16) | handle | 0 |
| RV_GFX_DRAW (17) | handle, arg0 width (320–3840), arg1 height (240–2160), count presentation seconds (0–30) | number of verified interior GPU pixels |
| RV_GFX_COUNT (18) | no fields | live package count, including after STOP |
| RV_GFX_RUN (19) | handle, offset FPS cap (0–240), count duration (1–30), arg0 width, arg1 height; output buffer rv_frame_stats | 56 bytes written; see CONTINUOUS.md |

The trusted client owns the input buffer until dispatch returns. Loading checks package length, CRC and the existing shader decoders, then requires exact known-answer fixture programs; it does not accept arbitrary console shaders. At most 16 package handles are live per session. Handles use a separate high-bit namespace, and are never reused by this library instance. STOP clears the package table. Calls to graphics services and STOP are serialized per session; Windows GPU draw calls are additionally serialized within the DLL. Callers must join all threads before closing a session.

Windows GPU draw uses the existing renderer and its hardware selection, compute-generated texture, indexed vertex/pixel stages, depth and descriptor management, queue fences and readback oracle. A draw is synchronous; the renderer creates and releases native device/queue/pipeline resources per call. The client receives a verified interior-pixel count only after success. Non-Windows draw returns RV_UNSUPPORTED. Backend flags are compile-time capability flags, not a probe of actual GPU availability.

For RV_GFX_DRAW, when count is nonzero, the verified GPU render target is copied directly into a DXGI swap-chain back buffer, transitioned to PRESENT and presented once, then held in a native window for the requested duration or until Escape/close. The render extent is independent of the initial client size. This is a static frame, not a continuous render loop, frame-rate benchmark, or proof of physical display visibility. The caller should use a dedicated thread with no unrelated windows/message loop. No cache persistence, BMP output, generic resource handles, general command lists, Xbox graphics translation, audio/input ABI integration or game execution is added in this milestone. The optional executable-only Agility SDK configuration is not exported by the DLL; it uses the system D3D12 runtime.

## Windows hardware validation

Create or reuse a known scene package and run the independent client:

```bat
.\build\Release\Revenge.exe package-create scene.rsp
.\build\Release\revenge_abi_graphics_fixture.exe scene.rsp
.\build\Release\revenge_abi_graphics_fixture.exe scene.rsp 1920 1080 0
.\build\Release\revenge_abi_graphics_fixture.exe scene.rsp 3840 2160 10
ctest --test-dir build -C Release --output-on-failure
```

The no-dimensions invocation runs CPU-side package validation only; the hardware commands are deliberately excluded from CTest. The 1080p and 4K commands should return 2,046,676 and 8,240,476 interior pixels respectively if the prior scene oracle continues to pass. Confirm the window is visible separately. No Windows execution was performed locally. The user subsequently reported successful Windows v0.26.0 rendering, swap-chain submission and 44/44 CTest. For continuous execution and frame timing see [CONTINUOUS.md](CONTINUOUS.md).
