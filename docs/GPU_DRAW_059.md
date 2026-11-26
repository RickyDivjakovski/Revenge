# Extended resident drawing (0.59.0)

The independent native ABI version 1 now exposes RV_GPU_DRAW_EXTENDED (77), advertised by RV_GFX_RESIDENT_EXTENDED (512). This is a desktop D3D12 subset, not a console shader ABI.

Use the same request fields as RV_GPU_DRAW_INDEXED: target handle, source texture in offset, index count, vertex and index handles in arg0 and arg1. Supply exactly one rv_gpu_draw_options packet as the buffer. second_texture must equal offset for modes other than 2. Mode 0 copies, 1 inverts RGB preserving alpha, 2 averages both RGBA textures, 3 multiplies RGBA by the packed tint (R in least significant byte). Both textures must be initialized and match target dimensions. Target cannot alias either source. Legacy draws still copy. The cached pipeline uses two SRVs and two 32-bit root constants, not arbitrary shaders or a general constant buffer. Each submission waits for its fence.

The extended hardware fixture compares ten additional full-target draws (2025 pixels), switches modes back to copy, and checks invalid mode and target aliasing. The previous 4860-pixel test and partial geometry checks remain. Hardware validation pending.
