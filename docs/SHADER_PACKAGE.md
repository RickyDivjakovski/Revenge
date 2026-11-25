# RSP1 shader package (v0.24.0)

Independent research format. It is not derived from an Xbox container and has no encryption, signing, platform descriptor layout or guest execution mechanism.

## Binary layout

Eight little-endian uint32 fields (32-byte header): magic `RSP1` (0x31505352), format version 1, ABI version 1, compute word count, vertex word count, pixel word count, reserved zero, CRC32. Payload is the three concatenated instruction streams in that order, each little-endian uint32. The CRC32 uses polynomial 0xedb88320 and covers the first 28 header bytes followed by the payload. Each stream must be 1..4096 words. Maximum package length 49,184 bytes. Exact total length required. CRC is accidental corruption detection, not authentication. The decoder calls the existing limited stage translators and rejects unsupported instructions.

## Application / GPU paths

`package-create` accepts no shader arguments (built-in fixture) or three authorised hex streams (compute, vertex, pixel). Use a fresh output path; existing files are rejected under normal single-process use. This is not a concurrent secure file creation primitive. `package-inspect` validates and reports stage lengths. `package-app-test` runs the existing application lifecycle with the package's compute stream and round-trips encoded package bytes through application storage. `package-gpu-test` additionally passes the loaded stage streams to D3D12 compute texture generation and graphics compilation; for the fixed pixel oracle only the exact known-answer package is accepted. HLSL remains SM5.1 DXBC. No DXIL or additional instruction coverage in this release.

Optional PSO cache is disposable, local and trusted; use a distinct cache path and output path, never point them at original source or input files. Adapter/driver, root signature, shader bytes and fixed state schema identify cache compatibility. The driver is authoritative; rejection retries without cached bytes. Saving occurs after successful pixel verification. It does not skip HLSL compilation. No speedup is asserted. The compute-generated texture PSO is not cached by this option.

## Offline executable diagnostics

`import-plan` uses the existing non-executable PE preparation path and lists IAT slot RVAs and names with DLL classifications. It indicates eligibility for a separate host audit, not actual resolution. It does not load modules, patch imports, invoke TLS, register unwind data or run entry points. Delay imports are flagged but not resolved. Test with the project's own Windows fixture. The existing native Windows runner is unchanged.

## Validation

Linux Release 35/35 CTest and Python 2/2 passed. Windows GPU execution, cache reuse and executable fixture import plan require Windows validation. General console descriptors, native wave opcodes, SM6 DXIL, actual Xbox ABI and live translated rendering remain outstanding.
