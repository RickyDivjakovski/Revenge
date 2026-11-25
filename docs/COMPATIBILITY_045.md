# v0.45.0: independent texture reference and faster development builds

Baseline: reported Windows v0.44.0 passed 82/82 regression tests. This update retains the prior loader fixtures, ABI and GPU checks.

## Texture reference

The CPU-only texture library accepts BC1, BC3, BC4 UNORM and BC5 UNORM blocks. BC4 and BC5 also participate in the existing bounded upload-footprint and independent Morton8 conversion logic. RGB565 endpoints expand to 8-bit channels; interpolation uses deterministic integer truncation. BC1 handles the three-colour/transparent branch, BC3 forces four-colour interpolation and independently decodes alpha, BC4/5 expose unsigned normalized channels. Exact hardware rounding parity is not established. No BC signed variants, BC6H/BC7, sRGB conversion or console texture descriptors/swizzles are implemented.

`sample_lod` takes a layer, normalized U/V, finite explicit LOD, independent address modes (clamp, wrap, mirror, transparent-black border), point or bilinear spatial filter and point or linear mip filter. LOD is clamped to the available levels. The existing explicit-mip `sample` API remains available. These functions are CPU references, not new native ABI operations or GPU sampling bindings.

Known-answer samples exercise all four formats, endpoint modes, transparency, partial blocks, bilinear filtering across blocks, both mip filters, array-layer selection, mixed address modes and border blends. Existing randomized layout tests include BC4 and BC5. Non-finite LODs are rejected.

## Build workflow

`revenge_win_backend` compiles `gpu_win.cpp` and `multimedia_win.cpp` once for reuse by both executable and DLL. Each linked module still has its own runtime state. The Windows target linkage is unverified locally. `REVENGE_ENABLE_IPO` defaults ON; use `-FastBuild` to disable IPO, not ordinary Release optimisation. `-BuildJobs 2` enables two concurrent build jobs; the original one-job default remains. The build logs elapsed build time. Keep `build` for incremental compilation and use a consistent optimization setting. Switching modes may trigger a substantial rebuild. Windows build-time improvement is not measured yet.

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1 -FastBuild -BuildJobs 2
```

No console-targeted executable or GDKX access is available. This release does not implement Xbox execution, guest TLS installation, console API bindings or console graphics translation. The next meaningful console milestone requires an authorised native console target and sufficient specifications.

## Results

Portable x64 Release, IPO disabled: 63/63 CTest cases, 2/2 Python tests and 79 new known-answer samples passed. The existing GCC filesystem `u8path` deprecation warning remains. Expected Windows/MSVC regression count: 83; Windows build scripts, shared object linkage and GPU regressions are pending validation.
