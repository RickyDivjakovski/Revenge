# v0.31.0 validation

- Linux x64 Release configure and build: passed.
- CTest: 44/44 passed, including pe_page_layout and native_abi_services.
- Python unittest: 2/2 passed.
- Native services fixture: 4,096 mutex-protected memory updates across four workers; semaphore counts, ownership, timeouts, stop cancellation, binary storage roundtrip, buffer and key validation, isolation and cleanup passed.
- Synthetic PE page report: headers, shared W+X, gaps, zero fill, entry and IAT diagnostics passed.
- Windows/MSVC validation pending for this version; expected 51 CTest cases (including a Windows-only layout-plan on the independent client).
- Prior v0.30.0 Windows test logs supplied by the tester: 48/48 CTest; 64 GPU jobs, 5,005,312 verified pixels, 8 allocation sets and 56 reuses, peak 2 submitted-uncollected, 482.817 ms elapsed, zero retained resources; four additional GPU jobs checked 272,384 pixels. These are fixture results, not gameplay FPS or proof of simultaneous GPU execution.
- GPU code is unchanged by v0.31.0. No new hardware testing locally. No Xbox ABI implementation or game launch.
