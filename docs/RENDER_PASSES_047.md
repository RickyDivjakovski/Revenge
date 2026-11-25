# Independent multipass graphics reference

The `render-pass-gpu-test` command constructs its own fixed 16×16 RGBA8 textures and D24S8 depth/stencil target. No external shaders or executable input are accepted. This test uses ordinary desktop D3D12, not D3D12.X.

The first draw writes red and green into two render targets over a scissor rectangle (10×8 pixels), sets stencil to one and writes depth 0.25. The second draw has depth 0.5 and stencil equality; neither the covered nor uncovered region should change. The third draw has depth 0.125, stencil equality, source-alpha blending on target zero and a zero colour-write mask on target one. The composition pass loads the two previous render targets through SRVs into a third texture. Explicit transitions are recorded before sampling and copying. All three color surfaces are read back after a fence and compared to a portable reference with a one-byte per-channel tolerance. No depth/stencil readback or depth sampling is performed.

The test uses two root signatures and four PSOs, with fixed HLSL and root constants. There is no general render graph or guest command translation. The current portable test verifies all 768 pixels and independent known answers, not the Windows shader compiler or GPU. Run the normal focused validation on Windows to exercise both this and the prior sampling test.
