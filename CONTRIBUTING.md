# Contributing to Revenge

Revenge welcomes contributions to native execution research, PE/ABI compatibility, graphics translation, memory management, shader compilation and performance measurement. Read [LEGAL.md](LEGAL.md) before submitting changes.

## Source provenance and contribution rights

By submitting a contribution, you represent that you have the right to submit it under GPL-3.0-or-later or have identified compatible third-party terms and obtained the necessary permissions. Do not assume that material visible online is licensed for reuse.

1. Identify public specifications or authorised observations supporting the change. Include links and versions where available, and record any required attribution.
2. Write an independent behavioural specification and implementation. Never paste proprietary or confidential source or headers into issues, code or review discussions.
3. Identify any reused third-party code, its upstream URL, revision, affected files and licence. Include required copyright notices; update `THIRD_PARTY_NOTICES.md`. Obtain maintainer review before incorporating it.
4. Use self-authored or otherwise redistributable tests. Do not attach commercial game binaries, shader dumps, protected packages, confidential SDKs, keys, firmware, credentials or raw diagnostic dumps from restricted software.
5. Do not submit DRM, entitlement, authentication or hypervisor bypass functionality. If source rights or restrictions are uncertain, stop and consult the maintainer before proceeding.

## Development workflow

- Open an issue describing the problem, scope and expected behaviour before large architectural changes.
- Keep changes focused; add regression tests for new behaviour and failure cases.
- Use C++20 for the runtime and Python 3 for offline tools. Keep the portable core buildable outside Windows.
- Check errors and input bounds; preserve GPU resource lifetimes and required synchronisation. Avoid vendor assumptions; check capabilities at runtime.

### Build and test

On Windows, use Visual Studio 2022 C++ build tools and CMake:

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-Revenge.ps1
ctest --test-dir build -C Release --output-on-failure
py -3 tests\test_shaders.py
```

On Linux, for portable core testing:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 tests/test_shaders.py
```

## Performance and compatibility reports

Include the source revision, OS, CPU, GPU, driver, resolution, relevant settings and test commands. Compare the same authorised workload before and after the change; report cold and warm caches separately, median and tail frame times where applicable, and any correctness regressions. Do not present synthetic throughput as game FPS or imply unverified game support. Report untested hardware or platform paths explicitly.

## Pull request checklist

- [ ] I have the necessary rights to contribute the files and materials.
- [ ] I have documented source provenance and any third-party licences/notices.
- [ ] No unauthorised binaries, assets, SDK material or sensitive logs are included.
- [ ] I have added relevant tests and recorded which tests passed or remain untested.
- [ ] Performance claims include reproducible measurements and correctness checks.
- [ ] Documentation reflects actual behaviour and limitations.

Maintainers may decline changes when provenance, permissions or safety boundaries are unclear.
