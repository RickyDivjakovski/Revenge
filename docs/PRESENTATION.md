# Desktop presentation fixture (v0.17.0)

`gpu-present [seconds=20] [width=1280] [height=720] [vsync=1] [fixed=0] [fps-cap=0]`

Seconds: 1–120; initial client width 320–3840, height 240–2160. VSync: 0 or 1. The loop ends on timeout, Esc, Close or a maximum of 100,000 submissions. The actual client extent can differ from the requested size. Drag to resize, minimize and restore to exercise lifetime handling. No exclusive fullscreen, borderless toggle, target FPS limiter or GPU budget overlay is implemented here.

The fixture creates a desktop hardware D3D12 device using existing adapter selection, a direct queue, a triple-buffered DXGI flip-discard swap chain, one RTV per back buffer and a vertex/pixel pipeline compiled with system D3DCompile. A small root constant block describes the time-dependent rotation and aspect ratio. It draws an independently authored triangle with interpolated vertex colours over a dark background. The shader is ordinary desktop HLSL, not translated RDNA2 graphics code.

`BackBuffers` tracks DXGI's actual current index and last submission for each buffer. Before reuse, wait for the required fence and collect its timestamp result. A queue-wide fence limits GPU submissions to at most two outstanding. Each list transitions PRESENT to RENDER_TARGET and back, with timestamp queries resolved into dedicated per-index readback ranges. Resize drains the queue, collects timings, resets command lists and releases old back-buffer references before resizing and rebuilding RTVs. Minimized windows skip recording. Normal exit drains before destruction; exceptional exit attempts best-effort drain. Device removal or fence timeout fails the command.

Title-bar rates count successful `Present` calls (S_OK), not refreshes or displayed unique frames. The wall-clock rate includes minimized/resize time; CPU frame-loop samples include fence waits, command recording, Present and signal; GPU timestamps span recorded barriers, clear and draw but exclude compositor/scanout. No pixel oracle is performed for this animated fixture. Use `gpu-render` for the existing offscreen known-pixel test. Interval zero uses the tearing flag only if DXGI advertises support, and does not guarantee unlimited FPS. Alt+Enter exclusive fullscreen is disabled.

The new fixture does not integrate the previous copy queue, shader translator or texture pool into a unified graphics dispatcher. It does not implement Xbox ABI, console resource descriptors or game execution.

## Public references

- https://learn.microsoft.com/en-us/windows/win32/direct3d12/swap-chains
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present
- https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model
- https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ns-dxgi-dxgi_frame_statistics

## v0.18.0 additions

Fixed mode (`1`) retains the requested swap-chain buffer dimensions when the client changes size; DXGI stretches the output to the client. It is not exclusive fullscreen and not a guarantee of physical 4K scanout. A nonzero frame cap (1–1000) uses best-effort sleep after recording and Present. The loop now reports requested duration and exit reason (close, Escape, WM_QUIT, duration, or one-million submission safety limit). The previous shorter runs did not reach the old cap, so their cause is unknown. Use the separate scene fixture to verify exact offscreen pixels.
