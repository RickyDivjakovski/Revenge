# v0.30.0 validation

- Linux x64 Release configure and build: passed.
- CTest: 42/42 passed (including native_abi_async_stress).
- Python unittest: 2/2 passed.
- New stress mode: 64 validation jobs, four retained at a time, two dimensions, changing texture snapshots, fifth-job rejection, out-of-order read/retirement, stale handles and zero retained resources after stop. Validation mode checks command snapshots, not GPU output.
- Windows/MSVC build and new GPU slot code: not tested locally. Expected 48 Windows CTest cases. Run both optional GPU modes to verify allocation reuse, fence ordering and pixel outputs. No performance claims or actual GPU concurrency measurements yet.
- Previous v0.29.0 Windows validation reported by user: 47/47 CTest cases, four async GPU jobs with 272,384 checked pixels plus two synchronous submissions with 136,192 checked pixels. This is prior-release evidence, not v0.30.0 GPU validation.
