# v0.44.0 scope and validation

Baseline: reported v0.43.0 Windows run passed 79/79 CTest cases, min/max vertex GPU comparison (136,192 oracle pixels), 64-job GPU stress and earlier fixed image and DLL references. Actual console execution remains untested. The C4456 local variable shadowing in init metadata is corrected.

## Offline TLS template model

`tls-plan <authorised.exe> [--rebase]` extracts data from a non-executable prepared PE image. It checks readable occupied pages, an aligned writable index field, the declared alignment nibble and a maximum raw + zero-fill size of 1 MiB. The result can be copied into independent byte vectors with a zero-filled tail. No OS TLS storage, index patching, TEB modification, callback execution or guest TLS lifecycle is implemented. The declared alignment is reported, not applied to vector allocation. The portable test uses a synthetic plan, eight workers, and independent vector pairs, and rejects seven malformed inputs. It is not compiler TLS execution.

## Fixed native unwind record

The existing fixture now embeds one version-1 RUNTIME_FUNCTION/UNWIND_INFO record for the self-authored import wrapper. Static metadata describes 40 bytes of local stack reservation. On Windows the fixture registers the table with RtlAddFunctionTable, checks RtlLookupFunctionEntry and RtlVirtualUnwind against a synthetic stack, executes the same fixed host call, and deregisters with RtlDeleteFunctionTable before unmapping. This is a fixed test, not a general exception dispatcher, guest unwind registration or external PE loader. Windows-specific code is not locally tested.

Reference: https://learn.microsoft.com/windows/win32/api/winnt/nf-winnt-rtladdfunctiontable and https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64 .

## Shader operands

The independent GFX10 graphics source decoder adds selectors 129–192 and 193–208, the 64 positive and 16 negative integer inline encodings. Selector 128 (zero) was already supported. The reference test checks 81 bit-exact CPU MOV outputs and emitted HLSL asfloat expressions, plus reserved-selector rejection. Raw integer bits are not conversion to FP numeric values. Negative patterns can encode NaNs; GPU exceptional FP and denormal handling remain unvalidated. No new GPU instructions or ABI operations are claimed.

Reference: https://llvm.org/docs/AMDGPUOperandSyntax.html .

## Validation

Run `powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1`. All previous regression and GPU tests remain. Portable Release: 62/62 CTest and 2/2 Python passed. Expected Windows CTest count: 82. New CTest cases: pe_tls_template, stage_integer_constants, runtime_tls_template (Windows). The existing native_fixed_image test also exercises fixed unwind registration on Windows. Send the combined validation.log.

No authorised console-targeted build or GDKX environment is available in this development environment. General executable loading, platform API translation, D3D12.x translation, guest TLS initialization, and genuine Xbox gameplay remain outstanding.
