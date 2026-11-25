# v0.19.0 integration fixture

`gpu-integrated-test [width] [height] [output.bmp]` reuses the v0.18.0 scene and oracle but replaces the CPU palette upload. The GFX10.3 ALU decoder and optimiser generate a callable HLSL function; for input indices 0–3 it must return 1, 4, 7, 10. A fixture wrapper maps those results to packed RGBA8 words and stores them in a raw UAV buffer using the D3D12 texture footprint row pitch. Unexpected results map to magenta, which the scene oracle rejects. This validates composition of the existing compiler with graphics, not actual image instructions or a console shader ABI. The HLSL wrapper is independently written.

A compute command list dispatches four lanes and transitions the buffer from UAV to COMMON, then signals its own fence. The direct queue waits on that fence before copying the buffer (implicit promotion to COPY_SOURCE) into the 2x2 texture. The graphics command list transitions the texture to PIXEL_SHADER_RESOURCE and renders the two near-first indexed draws with depth testing. Graphics completion precedes framebuffer readback and descriptor reuse. All native objects are retained across the handoff. The CPU verifies the same interior pixels as the previous scene. The offscreen framebuffer is not presented in the window.

The registry tracks typed handles, registry identity, generation, descriptor index, last use and retirement. It is thread-confined and uses a single graphics queue fence timeline. The scene deliberately allocates slot zero as a placeholder so the texture uses a nonzero descriptor index. This is descriptor slot management, not a generalized GPU object registry, cross-queue hazard tracker or heap allocator.

`gpu-present` keeps the previous desktop triangle and uses a per-process high-resolution waitable timer if available. The timer sleeps until approximately 250 microseconds before the deadline and yields for the remainder; fallback timing is reported. It does not change the system-wide timer period. Windows scheduling and Present may still limit the rate. Let the window remain open for the requested duration; use Escape to exit early.

References (public Microsoft documentation):
- https://learn.microsoft.com/en-us/windows/win32/direct3d12/user-mode-heap-synchronization
- https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-wait
- https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw

Portable tests passed. Windows compilation, driver behaviour and actual timing remain unverified.
