# Revenge Native 0.28.0 validation

- User-reported Windows v0.21.0: 32/32 CTest passed; both controller sticks responded, snapshot write/read each verified 16,384 bytes. Audio sample-count assertion failed; buttons, triggers, rumble and audibility unconfirmed.
- v0.21.1: use XAudio2 OnStreamEnd and empty queue for completion, report SamplesPlayed as diagnostic only, propagate voice errors and retain timeout. Build script defaults to one build job. Tester subsequently reported successful OnStreamEnd, 48,000 submitted stereo frames, zero queued buffers and reset counter. Audibility unconfirmed.

- User-reported Windows v0.22.0: 35/35 CTest, all 4,096 loop and 8,192 logical wave GPU results correct. Logical wave masks are not native opcode translation.

- User-reported Windows v0.23.0: 38/38 CTest in 2.76 s, 2,046,676 and 8,240,476 verified interior GPU pixels, translated compute/vertex/pixel stages.

- User-reported Windows v0.24.0: 41/41 CTest in 2.89 s; 10,287,152 verified interior pixels, successful cache miss/save and accepted cache hit (1.1983 vs 1.5978 ms creation; no speedup shown), 70 direct import candidates and 41 validated relocations, zero bound imports, zero tracked application resources after shutdown.

- User-reported Windows v0.25.0: 43/43 CTest, both independent C/C++ clients passed, 4,096 worker results, zero tracked resources after stop. 83 IAT slots, 76 host candidates, 7 unverified (five Revenge declarations and two atomic-wait imports), 39 validated relocations; none bound by inspector.

- User-reported Windows v0.26.0: 44/44 CTest in 2.98 s; 10,287,152 checked GPU interior pixels, native ABI graphics and static DXGI submission passed, zero package handles remaining; 83 imports classified including five Revenge declarations, zero bound. Visual confirmation of the window not supplied.

- User-reported Windows v0.27.0: 45/45 CTest in 2.96 s; continuous first/final oracles passed, all 57,935 submitted frames reported successful Presents across four runs. Uncapped rates 3,684.53/s at 1080p and 3,222.82/s at 4K for the lightweight fixture, not displayed frames or gameplay. Three runs ended early. Input responsiveness not confirmed.

## Executed locally

- v0.28.0: Linux Release build, 40/40 CTest (0.30 s final run) and 2/2 Python tests. Independent command client validated two draw ranges, bad range and wrong resource kind rejection, stale/foreign handle rejection, invalid texture dimensions, nonfinite vertex rejection, malformed command header, readback size, 64-resource and 8 MiB payload budgets, STOP cleanup and unsupported backend handling. Client oracle checked 68,096 synthetic reference pixels and rejected a one-byte corruption. GPU submission is optional and was not executed locally. Windows registers 46 CTest cases.

- v0.27.0: Linux Release build, 39/39 CTest and 2/2 Python; 360,556 shifted CPU oracle pixels and 30,000 deterministic motion offsets checked. Continuous ABI request validation rejects invalid duration, cap, buffer size, foreign/stale handles; valid non-Windows GPU request is unsupported. Existing tests pass. Windows build, continuous rendering, timing and input controls require hardware validation. Windows registers 45 tests. Initial motion corruption test accidentally selected an excluded boundary pixel and was corrected to use a checked pixel; full suite rerun passed.

- v0.26.0: Linux Release build, 38/38 CTest and 2/2 Python. Independent graphics ABI client loads a generated RSP1 package, rejects checksum corruption, wrong length, invalid dimensions, foreign/stale/wrong-kind handles, exhausted 16-package capacity and post-stop loading. Non-Windows GPU request returns unsupported; STOP releases all packages. Existing clients pass unchanged. Windows DLL renderer, offscreen GPU verification and DXGI static-frame presentation not executed locally.

- v0.25.0: Linux Release C/C++20 build; 37/37 CTest and 2/2 Python. Two separately compiled clients dynamically linked to the new shared library. C++ client verified 4,096 worker outputs, event operations, lifecycle, invalid versions/requests, bounds and handle rejection. Plain C client verified layout, five-argument calls and nonzero-offset buffers. CPU scene command validation only. Windows DLL and Microsoft x64 ABI untested.

- v0.24.0: Linux Release C++20 build; 35/35 CTest, Python 2/2. RSP1 128-byte fixture round-trip, all 128 truncated lengths and 128 single-bit byte-position mutations rejected, trailing bytes and invalid stages rejected, 4,096 CPU outputs verified. Separate package CLI invocations and application resource cleanup tested. Synthetic import plan tested with duplicate IAT rejection. Windows compilation, scene PSO caching, GPU integration and native executable fixture import-plan pending.

- v0.23.0: Linux Release C++20 build, 33/33 CTest, Python 2/2. Eight vertex outputs, 4,096 CPU sampled pixel outputs and 71,836 interior BGR framebuffer pixels verified. Malformed stage, export mask, resource descriptor, missing wait, nonzero/unknown LOD and undefined register rejection tested. Windows build and GPU execution pending.

- v0.22.0: Linux C++20 Release build; 30/30 CTest, Python 2/2. 4,096 bounded-loop results and 8,192 logical wave-mask outputs matched independent CPU oracles. Tested SCC carry and no-carry, invalid branch targets, undefined SCC, old-mode rejection, and instruction-budget exhaustion. Generated loop HLSL inspected. Windows compilation and GPU execution of new tests are unverified.

- v0.21.0: Linux C++20 Release build, 27/27 CTest, Python 2/2. Multimedia core verified 96,000 PCM samples (48,000 stereo frames), saturation, 10,000 random stick inputs, 16 KiB persistence, 1 MiB snapshot round trip, checksum and unrelated-file rejection. Separate CLI write and read processes each verified 16,384 bytes. Windows audio, controller and snapshot replacement not executed.

- v0.20.0 Windows tester reported 31/31 tests, three application runs (12,288 CPU ALU results), 10,287,152 checked GPU pixels and zero remaining tracked resources. These are user-reported independent fixtures, not console compatibility.


- v0.20.0: Linux C++20 Release build, 26/26 CTest, Python 2/2. Application services exercised 2,000 allocation cycles, logical bounds, stale/foreign/wrong-kind handles, manual-reset event wait and reset, storage limits, lifecycle cleanup and graphics callback dispatch. CPU application fixture verified 4,096 ALU outputs and a 16,384-byte temporary storage round trip. No tracked resources left after stop.

- v0.19.0: Linux C++20 Release build, 24/24 CTest; Python 2/2. Resource registry checked 10,000 retirement cycles and 4,096 translated CPU ALU results, texture packing, capacity, stale/foreign/wrong-kind handles and invalid fences. No Windows compiler or GPU execution used.

- v0.18.0: Linux C++20 Release build; 23/23 CTest, Python 2/2. Scene tests checked 1,889,661 synthetic CPU oracle interior pixels across four extents; checked malformed command streams, invalid indices, corrupted pixels and short images. This is not GPU validation.

- v0.17.0: Linux C++20 Release build, 22/22 CTest cases passed; Python 2/2 passed. Back-buffer tracker exercised 12,000 index and resize iterations; invalid fences, indices, premature reuse and resize rejected; aspect ratio and rotation constants checked.
- v0.16.0 results retained below.

- Linux C++20 Release build; 21/21 CTest cases passed. Python 2/2 passed.
- Transfer pool: 8,000 simulated submissions across 1–8 slots, invalid state and fence rejection, budget and retirement checks, stale/corrupted payload detection.
- Prior v0.15.0 local results below are retained for comparison.
- Native virtual memory tests: aligned reservation, checked protection, zero-filled recommit, stale and cross-arena handle rejection, commit budget, logical retirement, fragmentation and 2,000 concurrent allocation cycles.
- `vm-bench 8 4`: 33,554,432 bytes verified, 4096-byte host page, peak committed 33,554,432 bytes, 26.8537 ms elapsed on the local Linux environment, no reservations left. Timing is illustrative.

## Prior Windows validation reported by tester

- v0.7.0: 6/6 CTest.
- v0.8.0: CPU detection and verified transfer tuning.
- v0.9.0: desktop graphics fixture pixel verification and BMP export.
- v0.10.0: 4,096 integer GPU outputs matched.
- v0.11.0: 16/16 CTest and 4,096 FP32/branch outputs matched.
- v0.12.0: 19/19 CTest and two GPU runs each verified 4,096 outputs.
- v0.13.0: 22/22 CTest in 2.39 seconds; two GPU runs each verified 4,096 mixed outputs. Compute PSO cache saved then accepted by the RTX 5070 Ti driver, reported creation times 21.3028 ms and 0.8868 ms. Warm-up may contribute.

- v0.14.0: 23/23 CTest in 2.43 seconds; 4,096 synthetic texture outputs matched. Graphics PSO cache saved and accepted, 1.3099 ms miss versus 1.6323 ms hit; no demonstrated creation speedup. Both render fixtures verified five pixels.

- v0.15.0: 25/25 Windows CTest in 2.49 seconds. Verified 32, 256 and 512 MiB in 26.1124, 182.87 and 436.958 ms respectively. All reservations released.

- v0.16.0: 26/26 Windows CTest in 2.48 seconds; 320 round trips, 2,816 MiB verified, all tracked allocations released. Comparable 4 MiB workloads reported 372.982, 385.171 and 370.846 MiB/s for 1, 3 and 6 slots; 16 MiB stress workload reported 388.943 MiB/s.

- v0.17.0: 27/27 Windows CTest in 2.49 s; 64,353 successful Present calls across four runs. Larger client extents were clamped to 2560×1421 and 3840×1421. All durations were shorter than requested; cause unknown. The triangle has no pixel oracle.

- v0.18.0: 28/28 Windows CTest in 2.54 s, 2,046,676 checked interior pixels at 1920x1080 and 8,240,476 at 3840x2160. Fixed 4K swap chain confirmed; both presentation tests ended via window-close events. The 120 cap achieved 53.0029 successful Present calls/s in a short run.

- v0.19.0: tester reported 29/29 Windows CTest in 2.54 s; integrated GPU fixture verified 2,046,676 and 8,240,476 interior pixels. Both frame pacing tests used high-resolution waitable timers but exited via window close; 120 cap yielded 90.8029 Present/s and 240 cap 147.233 Present/s. These are short, different-client-size synthetic runs.

## Not executed for v0.20.0

Windows compilation and the new application-to-GPU service path remain unverified locally. Frame pacing remains only partially validated. Xbox ABI, general graphics dispatch, translated graphics shader stages and gameplay remain unimplemented.

- v0.21.1 portable retest: Linux Release build succeeded, 27/27 CTest, 2/2 Python tests. Windows-specific callback and build script not executed here.
