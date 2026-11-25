# Restricted GFX10 buffer fixture compiler

## Scope

The new compiler accepts at most 256 numeric hexadecimal instruction dwords. It uses a separate fixed fixture ABI, not a general compute shader ABI or console resource descriptor format. Supported instructions:

- `buffer_load_dword`: idxen only; index v0; inline scalar offset zero; s[0:3] read-only binding; aligned immediate offsets 0..4092 bytes.
- `s_waitcnt vmcnt(0)`: marks outstanding read destinations available. It is a dependency validation mechanism for this straight-line model; HLSL carries the actual dependencies. No general memory-barrier translation is implied.
- `v_add_nc_u32`: selected vector, inline integer and literal sources, modulo 2^32; v0 cannot be modified.
- `buffer_store_dword`: idxen, v0, s[4:7], zero offsets; one store per invocation, then `s_endpgm`.

All other encodings and modifiers are rejected. The fixture assumes all lanes active, nonaliasing buffers, 4-byte strides, exactly 4,096 logical elements and zero for loads past that logical length. These are explicit fixture contracts, not inferred descriptor properties or a complete model of RDNA2 out-of-range rules. No inter-thread dependencies, atomics, shared memory, general memory ordering or vector-store wait counters are modelled. The host must dispatch exactly 64 groups of 64 threads and allocate a suitably padded input buffer (4,096 logical elements plus 1,023 zero elements) and at least 4,096 output elements. The generated HLSL also checks logical bounds. The GPU harness uses an upload heap root SRV and a default heap root UAV, with a fence before readback. No GPU is needed for CPU tests.

## Known-answer fixture

For each element i, output is input[i] + input[i+1] modulo 2^32, using zero for the final out-of-range load. CPU tests compare 4,096 deterministic values and 16 randomized sets of 4,096 values, totalling 69,632 outputs. Windows GPU test uses the deterministic set. Malformed instruction, missing wait, uninitialized register, invalid binding, unsupported offset and immutable-index checks are tested. This validates the fixture model, not arbitrary shader execution.

## References

- [AMD RDNA2 shader ISA](https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/instruction-set-architectures/rdna2-shader-instruction-set-architecture.pdf), MUBUF, VOP2 and SOPP instruction documentation.
- [LLVM GFX10 MUBUF encoding examples](https://github.com/llvm/llvm-project/blob/main/llvm/test/MC/AMDGPU/gfx10_asm_mubuf.s), cross-check of public instruction encodings; no upstream implementation included.
- [LLVM GFX10 wait-counter examples](https://llvm.org/docs/AMDGPU/gfx1030_waitcnt.html).
- [Microsoft root signatures](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures-overview).

## Remaining work

Decode actual resource descriptors and address modes using authorised independently reproducible fixtures; integrate control flow and masks with memory operations; implement typed buffers, image instructions, loops, atomics, memory ordering, real DXIL validation and adapter-specific PSO persistence. No Xbox game execution is provided.
