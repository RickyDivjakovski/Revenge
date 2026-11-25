# v0.40.0 — platform probe and custom vertex research

## Scope

The v0.39.0 Arena was reported working by the user. No validation transcript was supplied for that run. This release does not represent ten completed milestones or a complete emulator. It introduces a source-level comparison target and additional restricted shader support. The 63 independent ABI operations and five exports are unchanged. No Xbox binary loader, console-specific system or graphics ABI, protected package handling, guest TLS or exception registration has been implemented.

## Single validation run

```bat
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1 -Play
```

The existing regression suite, integrated GPU gameplay and stress test are retained; the wrapper also checks the narrow vertex program with synchronous and asynchronous GPU readback and runs the independent host probe. `-Hardware` retains optional audio and physical controller probes. Send the single `validation-*/validation.log` for any failures (redact private paths and do not include restricted materials). No additional manual test is needed for this release's normal validation run. The Windows wrapper has not been executed locally.

## Shared gameplay source

`samples/game/arena_core.hpp` is factored from the independently authored Arena. The interactive game and source probe use the same collision, movement and pickup logic. The probe runs the wall test followed by four waypoints. Its discrete checkpoint contract is:

```
REVENGE-PROBE-V1 collision=1 masks=1,3,7,15 score=4
```

The independently linked `RevengePlatformProbe` uses the actual Revenge ABI to copy the checkpoint data via a checked nonzero-offset allocation, retain the final score through TLS, check monotonic time and release resources. It also supports `--compare trace.txt`. The comparator accepts a single exact trace line, with an optional debug prefix; rejects missing, mismatching or duplicate traces; bounds the input to 1 MiB. The trace is not an attestation of origin. Source-level logic agreement is not binary compatibility.

## Official GDKX test (requires authorised access)

Microsoft's public documentation distinguishes Windows GDK and Xbox console GDKX target platforms. Use your authorised GDKX and approved test environment. No proprietary headers, libraries or tools are shipped in this archive. The sample is an independently authored **source file**, not a complete Xbox project or a binary. It has not been built against the console toolchain.

1. Create a new authorised Series X|S game project using the official Visual Studio GDKX template, targeting `Gaming.Xbox.Scarlett.x64`. Keep the template's normal runtime lifecycle, configuration, libraries and deployment requirements. Do not substitute Revenge for its runtime.
2. Add `samples/platform/gdk_probe.cpp` as a source file, retaining the relative layout to `samples/platform/probe_trace.hpp` and `samples/game/arena_core.hpp`. Enable C++17 or newer and C++ exceptions for this source; configure its precompiled-header setting if the template uses one.
3. Declare `extern "C" int RunRevengePlatformProbe() noexcept;` in the title source. Invoke it once after the template successfully initialises the real GDK runtime. Handle a nonzero result as a probe failure. The sample does not replace the entry point, display graphics or initialise/uninitialise the real runtime itself.
4. Build and deploy using the official toolchain and approved hardware. Capture the single `REVENGE-PROBE-V1` line in the official debugger's output and save it to a plain UTF-8 text file. The optional `REVENGE-GDK-PROBE device-type=...` line describes what the real API reports; no identity is forged. Do not share binaries or SDK materials unless authorised.
5. To compare the checkpoint trace with Revenge in the same automated run, use `-ProbeTrace path-to-trace.txt`. For offline analysis of an independently authored, authorised PE64 target only, the existing `-AuthorisedTarget` and optional `-DependencyDlls` arguments remain available. This inspects but does not launch or bind the target. `run-native` remains for trusted ordinary Windows executables, not Xbox binaries.

Public references:
- https://learn.microsoft.com/en-us/gaming/gdk/docs/gdk-dev/intro/introduction
- https://learn.microsoft.com/en-us/gaming/gdk/docs/gdk-dev/console-dev/overviews/gdkx-new-layout
- https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/xsystem/functions/xsystemgetdevicetype

## Custom vertex programs

`RV_GFX_LOAD_CUSTOM` (62) now accepts independently authored, validated vertex variants as well as pixel variants, with the original fixed compute program. No request layout, opcode or export changes are needed. The VS input/PS linkage, single texture/sampler, instruction whitelist, draw format and transfer limits are unchanged. Known-answer `RV_GFX_LOAD`, `RV_GFX_DRAW` and `RV_GFX_RUN` continue to require the original exact fixture.

`package-create-narrow` emits a package with a `v_mul_f32` on clip-space X before export, scaling X by 0.5. The portable test verifies output coordinates and unchanged other outputs, package roundtrip and rejection by the known-answer package gate. The command test generates an independent narrow framebuffer reference, checks interior colours and depth, rejects corruption, then exercises the ABI validation job. The optional GPU test checks the same oracle against actual synchronous and asynchronous output. This is restricted stage-program support, not D3D12.x or general RDNA2 ISA compatibility.

## Validation and blockers

Linux Release 53/53 CTest and 2/2 Python tests passed. The expected Windows CTest total is 65; the custom vertex GPU comparison and previously validated integrated GPU workloads are also run by the wrapper. Windows compiler and GPU results for these changes remain pending. The GDKX sample is source-only and untested, and neither a GDKX environment nor an authorised native Series executable has been made available for this release. Actual Xbox initialisation, system ABI, D3D12.x integration and commercial gameplay are still major engineering milestones. Do not treat the source probe, desktop build or diagnostic output as evidence of console binary execution.
