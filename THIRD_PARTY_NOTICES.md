# Third-party notices and provenance register

This register describes the Native v0.6.0 source snapshot, not all historical Revenge releases. It is a source inventory, not a formal legal clearance.

## Bundled material

No vendored third-party implementation, Xbox SDK files, firmware, game binaries or compiled shader blobs were identified in the reviewed Native source snapshot. The GNU GPL licence text is reproduced verbatim from the Free Software Foundation's licence document; its copyright notice is in `LICENSE`.

The earlier Rust-based Revenge releases were derived from VIctory and related forks. Their original licence and notice obligations still apply to those releases and any code retained or reintroduced from them. The current Native branch is a separate C++ implementation. This register does not certify a formal clean-room process.

## External build/runtime dependencies (not bundled)

| Dependency | Use | Source / applicable terms |
| --- | --- | --- |
| Microsoft Windows SDK and system libraries | Win32, DXGI, D3D12, COM and shell interfaces | [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/); applicable Microsoft terms |
| Visual Studio / MSVC | Windows compiler and toolchain | [Visual Studio](https://visualstudio.microsoft.com/); applicable Microsoft terms |
| CMake | Build configuration | [CMake](https://cmake.org/); upstream licence |
| Python 3 | Offline shader tools and tests | [Python](https://www.python.org/); upstream licence |
| DirectX Shader Compiler (DXC) | Optional external PC HLSL compiler | [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler); upstream licence and notices |
| Shields.io | README badge images loaded from an external service | [Shields.io](https://shields.io/); not part of runtime |

External tools and libraries must be obtained through authorised channels. If bundled in a future release, review their actual versions, redistribution conditions and notice requirements before shipping.

## Research references (not a declaration of incorporated code)

- [Microsoft PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format), [DirectX specifications](https://github.com/microsoft/DirectX-Specs), [public GDK repository](https://github.com/microsoft/GDK), and [Direct3D 12 documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics).
- [AMD GPU ISA specifications](https://gpuopen.com/machine-readable-isa/).
- [Xenia](https://github.com/xenia-project/xenia), [shadPS4](https://github.com/shadps4-emu/shadPS4) and [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton) as architectural references.
- [VIctory](https://github.com/Marvisak/VIctory) and its forks as historical references to the prior Rust-based development effort.

See `docs/ARCHITECTURE.md` for further technical references. These links do not imply affiliation, endorsement, or permission to copy any particular implementation.

## Future additions

For each incorporated component, record: name, upstream URL, exact version or commit, licence/SPDX identifier, copyright holders, affected files, changes made and location of required licence/notice text. Preserve upstream notices without alteration.
