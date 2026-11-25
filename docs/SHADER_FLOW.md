# Forward-control-flow and FP32 research (0.11.0)

## Scope and contract

This module is independent of the prior straight-line integer optimizer and operates on explicit numeric GFX10.3 dwords in a self-authored fixture. `v0 = dispatch index`, `s0 = index / 64` (uniform per 64-thread group); result is v0 at end, with all lanes active. The built-in dispatch is exactly 4,096 threads. There are no console bindings. This is not a console shader container decoder.

| Instruction category | Supported subset |
| --- | --- |
| SOP1 / VOP1 | 32-bit scalar/vector moves to ordinary registers |
| VOP2 | add_u32, or_b32, add_f32, sub_f32, subrev_f32, mul_f32 |
| SOPC | equality/inequality (both aliases), unsigned greater/greater-equal/less/less-equal |
| SOPP | nop, endpgm, branch, cbranch_scc0, cbranch_scc1 |

Public reference: [AMD RDNA2 ISA reference, document 70648](https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/instruction-set-architectures/rdna2-shader-instruction-set-architecture.pdf), sections 12.3–12.8, particularly SOPC, SOPP and VOP2. [LLVM GFX1030 instruction syntax](https://rocm.docs.amd.com/projects/llvm-project/en/latest/reference/AMDGPU/AMDGPUAsmGFX1030.html) is an additional public reference. No proprietary implementation source is incorporated.

## Control flow and verification

The decoder records dword boundaries, resolves branch offsets from the following word, and rejects offsets outside the stream, backwards branches, literal targets, unknown encodings and unreachable instructions. The last instruction must be endpgm. A single topological pass intersects the definitely defined registers at each merge and rejects reads without a definition on every incoming path, including SCC. The HLSL backend uses a monotonically advancing program counter with ordered guards; this is a bounded baseline, not an optimized general CFG compiler. Integer optimizer from 0.10.0 remains separate.

The fixture constructs binary32 1.x from the thread index. Group IDs below 32 add 1.0; remaining groups multiply by 2.0. The expected output is derived independently from binary32 mantissa arithmetic with tie-to-even rounding. The CPU tests compare all 4,096 values, additional sub/subrev cases, both branch conditions and malformed inputs. Windows `shader-flow-gpu-test` uses the same expected array and compiles HLSL with IEEE strictness. The new GPU test has not been executed locally.

FP32 CPU evaluation rejects non-normal operands/results. HLSL emission does not prove the FP domain for arbitrary input programs and does not trap on non-normal values; only use it with fixtures whose inputs and intermediates are within the documented domain. Special FP modes (including denormals, exceptions, signed zero, NaN, infinity, rounding-mode controls) remain unsupported. No general validation of arbitrary emitted GPU floating-point behavior is implied.

The CPU `WaveMask` class models an explicit 32- or 64-lane active bitmask, equality masks and writes only to active lanes. It is a standalone test abstraction and does not decode EXEC/VCC machine instructions, model divergent branches, wave scheduling, or implement wave operations in HLSL. No Xbox GPU or commercial shader is needed for these tests.

## Next work

Structured CFG lowering and optimization, loops, special-register decoding, divergent wave execution and GPU oracle tests, memory/resource instructions, texture semantics, DXC execution validation, pipeline disk persistence, console bindings. These are not implemented by this release.
