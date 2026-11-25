# v0.43.0 validation and scope

Baseline: v0.42.0 Windows report confirmed 74/74 CTest cases, 32 managed reference threads, 8,192 TLS comparisons, 8,192 exception checks, and the logging fix. No console binary was tested.

## Static dependency graph

`Revenge dependency-graph <authorised.exe> <explicit.dll> [...]` inventories up to 64 explicit dependency files. Filenames are case-insensitive ASCII basenames with a restricted character set. Each reached file is visited once. Repeated module edges within a file are merged; symbol references are counted including repetitions. Unprovided module names are aggregated. Unreachable supplied modules are reported separately. Invalid imported names are counted and not traversed. This graph concerns ordinary static direct imports only; delay imports, forwarded exports, runtime loading and API-set mapping are separate diagnostics. A missing module is not a missing-symbol verdict. No modules are loaded.

## PE metadata

`init-plan` now reports the raw TLS template extent and whether its modelled pages are readable and occupied. Version-1 x64 unwind slots are grouped into operations, checking order against the declared prologue, extra slot availability, reserved opcodes and selected operand constraints. Local stack adjustment includes pushes, allocation and machine frames; it does not accumulate chained records or simulate prologues. Other versions are skipped by the new slot analysis. Results are advisory, not proof of ABI compatibility. No runtime unwinding, TLS callback execution or native registration is added.

Reference: Microsoft x64 exception handling documentation, https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64 .

## Graphics instruction subset

The existing GFX10.3 independent graphics compiler supports VOP2 opcodes 15 and 16 (min/max), plus FP inline selectors 240–247. Encoding reference: LLVM GFX10 assembler tests, https://github.com/llvm/llvm-project/blob/main/llvm/test/MC/AMDGPU/gfx10_asm_vop2.s . Only the existing fixed vertex/pixel ABI is supported. Normal finite FP32 operands are the intended scope; NaN, signed-zero, denormals and mode-specific behaviour are not validated. No new console descriptors, hardware wave control or native container parsing are added.

`package-create-minmax` generates a shader that scales x by 0.5 and compares it with offset values using min/max. The GPU comparison checks equivalent geometry and framebuffer output through the existing command ABI; the HLSL compiler may optimise these expressions. No change to the 63-operation ABI or RSP1 format.

## Results

- Linux x64 C++20 Release: 60/60 CTest passed; 2/2 Python tests passed.
- New portable fixtures: direct dependency graph, unwind slot diagnostics, stage min/max and constants, package command integration.
- 4,098 vertex comparisons, 64 pixel comparisons, 8 inline constants, and RSP1 roundtrip passed.
- Windows: expected 79 CTest cases, including actual DLL graph check; automatic min/max GPU comparison pending. Existing GPU regression checks retained.
- One command: `powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1`.

Actual Xbox executable execution, system/graphics ABI translation and guest initialization are not implemented. No authorised console build or GDKX was available. The release is numbered 0.43.0 rather than claiming the whole 0.52 roadmap.
