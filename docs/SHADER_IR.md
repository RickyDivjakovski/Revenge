# RDNA2 ALU research implementation (0.10.0)

The input is a self-authored, straight-line integer ALU fixture in public GFX10.3 encoding. No proprietary shader/container or game-specific interface was used. This is an initial compiler stage, not a complete console shader translator.

## Pipeline

Hex dwords -> bounded decoder -> register-based IR -> constant propagation/folding -> backwards live-register analysis -> SSA-like HLSL temporary names -> optional DXC or system D3DCompile -> desktop compute fixture readback.

Only v0 is an input and output in the synthetic ABI. All other registers must be written before read. s0..s105 and v0..v255 are distinct register banks, modeled as 32-bit unsigned words. Scalar moves use uniform constants or previously defined SGPRs. No special registers are accepted. All lanes are assumed active and independent; this is not Wave32/Wave64 emulation. The HLSL wrapper maps input to dispatch ID and output to UAV u0; this binding is not derived from an Xbox resource layout.

Supported encoding families: SOP1 s_mov_b32, VOP1 v_mov_b32, VOP2 integer subset (mul_u32_u24, min/max_u32, lshrrev/lshlrev_b32, and/or/xor/xnor_b32, add/sub/subrev_nc_u32), SOPP s_nop with valid 4-bit immediate and s_endpgm with zero immediate. NOP timing is omitted because the fixture has no timing-dependent effects. Integer inline constants, negative inline integers and 32-bit literals are supported; other source selectors are rejected. Shift counts are masked to 5 bits and arithmetic wraps at 32 bits. The decoder rejects malformed inputs, unsupported opcodes, undefined reads, special destinations, missing literals, missing termination and trailing instructions. It does not scan or extract machine code from a title.

The optimizer is valid only for the specified one-result fixture ABI without side effects. Constant propagation substitutes known values until a register is overwritten. Backward liveness removes only writes that cannot affect the final v0 result. HLSL emission uses distinct variable versions rather than dynamically indexed register arrays. No shader performance gain has been benchmarked.

## Validation

CPU tests: 4,096 input indices against independent formula 3*x+1, 6,500 deterministic randomized integer cases including register-dependent additions, rejection cases, overwrite handling, literal/terminator distinction and optimizer equality. CLI tests decode and emit the checked-in fixture. Optional Windows GPU test compiles the generated shader and verifies every output against both the CPU evaluator and independent formula. This test has not yet run on Windows. The GPU fixture only exercises a small addition program; other supported operations have CPU tests, not verified GPU semantics.

## Not implemented

Floating-point modes (NaN, denormal, precision, rounding); EXEC/VCC/SCC and wave operations; control flow and CFG reconstruction; memory, image/texture and LDS operations; exports, interpolation and vertex/pixel shader stages; barriers and wait counts; resource metadata and console binary containers; full DXIL validation, runtime integration, persistent PSO caching and game execution. Unsupported operations fail explicitly. Prior transfer tuning is separate from this compiler; no automatic game queue configuration is applied.

## Public references

- AMD RDNA2 ISA reference, document 70648 (encoding and semantics): https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture
- LLVM GFX10.3 public assembly syntax: https://rocmdocs.amd.com/projects/llvm-project/en/latest/LLVM/llvm/html/AMDGPU/AMDGPUAsmGFX1030.html
- LLVM GFX10 public assembler/disassembler test encodings (bitfield cross-checks, no implementation copied): https://github.com/llvm/llvm-project/tree/main/llvm/test/MC/AMDGPU
- Microsoft DXC: https://github.com/microsoft/DirectXShaderCompiler
