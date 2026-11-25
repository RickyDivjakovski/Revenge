# Third-party notices and provenance register

This register describes the Native v0.10.0 source snapshot, not all historical Revenge releases. It is a source inventory, not a formal legal clearance.

## Bundled material

No vendored third-party implementation, Xbox SDK files, firmware, game binaries or compiled shader blobs were identified in the reviewed Native source snapshot. The GNU GPL licence text is reproduced verbatim from the Free Software Foundation's licence document; its copyright notice is in `LICENSE`.

The earlier Rust-based Revenge releases were derived from VIctory and related forks. Their original licence and notice obligations still apply to those releases and any code retained or reintroduced from them. The current Native branch is a separate C++ implementation. This register does not certify a formal clean-room process.

## External build/runtime dependencies (not bundled)

| Dependency | Use | Source / applicable terms |
| --- | --- | --- |
| Microsoft Windows SDK and system libraries | Win32, DXGI, D3D12, D3DCompile, COM and shell interfaces | [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/); applicable Microsoft terms |
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

## Runtime research references

- [Windows job objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects) and [debugger exception handling](https://learn.microsoft.com/en-us/windows/win32/debug/debugger-exception-handling).
- GitHub Actions checkout is an external CI dependency; no action implementation is bundled.

## Future additions

For each incorporated component, record: name, upstream URL, exact version or commit, licence/SPDX identifier, copyright holders, affected files, changes made and location of required licence/notice text. Preserve upstream notices without alteration.

Optional Agility SDK integration uses the documented app-local exports; no SDK components are bundled. See [Microsoft setup guidance](https://devblogs.microsoft.com/directx/gettingstarted-dx12agility/) and the applicable package terms.

The integer ALU decoder uses public AMD RDNA2 ISA reference (document 70648, sections 12.3, 12.5, 12.7 and 12.8) and public LLVM GFX10 encoding examples for cross-checking bitfields. No LLVM or AMD implementation source is bundled. See `docs/SHADER_IR.md`.

The v0.11.0 flow decoder uses AMD public document 70648, SOPC/SOPP and VOP2 tables; the independent fixture and CPU lane-mask tests use no proprietary code. See `docs/SHADER_FLOW.md`.

The v0.12.0 buffer fixture uses public AMD RDNA2 ISA documentation and publicly documented LLVM GFX10 MUBUF and wait-counter encodings as research references only. The fixture and implementation are independently written. See `docs/SHADER_MEMORY.md`.

The v0.13.0 mixed fixture builds on the independent public-ISA subset and documented Microsoft D3D12 cached PSO APIs; see `docs/SHADER_UNIFIED.md`.

The v0.14.0 texture model and Morton8 fixture are independently authored. Public Microsoft texture upload and cached PSO documentation are research references; see `docs/TEXTURE.md`.

The v0.15.0 virtual memory arena uses documented Win32 and Linux memory interfaces; no external implementation code is bundled. See `docs/VIRTUAL_MEMORY.md` for references.

The v0.16.0 transfer fixture is independently authored using public Microsoft D3D12 resource barrier, readback and fence documentation as references. See `docs/TRANSFERS.md`.

The v0.17.0 presentation fixture is independently authored using documented Win32, Direct3D 12 and DXGI swap-chain APIs. See `docs/PRESENTATION.md` for public references; no Microsoft sample implementation code is bundled.

The v0.18.0 scene and presentation changes are independently authored using public Microsoft D3D12/DXGI API documentation (references in `docs/SCENE.md` and `docs/PRESENTATION.md`). No third-party implementation code is bundled by this change.

The v0.21.0 multimedia and snapshot fixtures use publicly documented XAudio2, XInput and Win32 file replacement APIs as references; no Microsoft implementation code is bundled. See `docs/MULTIMEDIA.md`.

The v0.22.0 bounded loop mode uses public AMD SOPP and SOP2 documentation and LLVM GFX10 encoding examples as references; logical wave-mask fixtures are independently authored. See `docs/SHADER_LOOPS.md`.

The v0.23.0 graphics-stage fixtures use public AMD GFX10.3 VOP, EXP and MIMG ISA references and public LLVM encoding examples. See `docs/SHADER_STAGES.md`. No third-party implementation source is bundled.

The v0.24.0 RSP1 shader package is independently defined. Scene PSO caching reuses the existing Windows D3D12 backend; no third-party implementation code or console files are bundled.
