# v0.41.0 — native image execution experiment

## Scope and evidence

The supplied v0.40.0 Windows validation excerpt reports 619 Arena frames in 15.189 seconds, four pickups, 136,192 GPU oracle pixels for the custom vertex test, and 64 completed stress jobs with 5,005,312 verified pixels. The exact CTest count is not visible in that excerpt; audio and physical controller checks were not requested. This is independent Windows gameplay, not Xbox execution.

The proposed v0.41–v0.50 roadmap is **not completed**. This release focuses on two missing prerequisites: native execution of a fixed image and offline target inventory. No authorised native Xbox target or GDKX toolchain was available. The 63-operation host ABI and existing graphics paths remain unchanged. No console runtime, GDKX library, protected content, or external executable loader is included.

## Fixed native PE fixture

`tests/native_image.cpp` constructs a tiny deterministic AMD64 PE32+ image in source. It never reads an executable from disk or accepts a target path. It prepares two separate mappings through the existing data-model routine, checks one DIR64 relocation, zero-fill, and exact import declaration. It supplies only its own local `fixture_answer()` function for the single matching test IAT slot. No DLL is loaded, and no dynamic symbol discovery or generic binding is performed. The two entries use x64 machine instructions and an ordinary host calling convention. The first increments a per-mapping counter; the second invokes the local host function using an aligned stack and Win64 shadow space. There is no exception propagation through mapped code, guest TLS, or unwind registration.

The five 4 KiB pages are read-only headers, read-execute code, read-write data, no-access gap, and read-only relocation data. Windows uses VirtualAlloc, VirtualProtect, VirtualQuery and FlushInstructionCache; Linux x64 uses mmap, mprotect and cache synchronisation. No page is simultaneously writable and executable. A restrictive host policy that disallows executable allocation can prevent the test from running; it does not attempt to alter that policy. Mapping is released at scope exit. This fixture is deliberately separate from the ordinary CLI and external target inspection. It is not a general-purpose PE loader or sandbox.

Expected results: separate counters return 42, 43, 44 and 42, 43 respectively; both fixed import calls return 0x5256. Unsupported host architectures skip the CTest case.

## Offline target inventory

`Revenge target-plan <authorised.exe>` prepares only a byte vector and reports machine/subsystem, COFF and DLL flags, section/file alignment, stack and heap reservation/commit sizes, direct dependency categories, RVA-form delay import descriptors and named/ordinal thunks, load config size and guard flags where present, CLR directory, TLS, unwind and relocation counts. Malformed or unsupported delay records fail rather than silently guessing. Delay symbols are bounded to 100,000 and report output to approximately 8 MiB. Import names and descriptor counts are bounded; VA-format delay descriptors are unsupported. The existing direct-import heuristic is not a compatibility or security verdict. Dynamic and transitive dependencies and calling signatures are not enumerated. No protections are changed in this command.

The portable synthetic test covers named and ordinal imports and five malformed inputs. The Windows/MSVC-specific test creates an independent ordinary Windows executable with a delay-loaded user32 import and checks that the real linker output is recognised. This Windows fixture has not yet been built locally.

## Validation

```bat
powershell -ExecutionPolicy Bypass -File .\Build-And-Test-Revenge.ps1
```

The wrapper runs the full regression suite and existing hardware GPU comparisons, prints the new fixed-image fixture results, and inventories the host probe. `-Play` is optional; `-Hardware` enables audio/controller checks. For an independently authored authorised PE64 development target, optionally specify `-AuthorisedTarget "C:\path\to\probe.exe"`. That option performs offline reports only; no external image is loaded or invoked. The prior `-DependencyDlls` and `-ProbeTrace` options remain available. Keep proprietary or restricted files out of shared reports.

Linux x64 Release: 55/55 CTest, 2/2 Python, including actual execution of the fixed machine-code fixture. Expected Windows/MSVC count is 69, plus hardware tests. Windows build, GPU regressions and native page-protection behaviour for this release remain to be confirmed. The separate `run-native` command still only uses the ordinary Windows loader for trusted Windows applications. Actual Xbox system/graphics API translation, console initialisation and execution still require an authorised test target, real platform contracts and substantial engineering.
