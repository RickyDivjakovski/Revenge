# v0.29.0 validation

Linux x64 Release build: passed, 41/41 CTest cases and 2/2 Python unittest cases. New native_abi_async test checks four retained jobs, fifth-job rejection, mutable texture replacement, foreign/stale handles, wait limit, source resource/package release before collection, draw count and cleanup. Portable jobs validate command snapshots, not pixels. The existing CPU oracle checks 68,096 reference pixels and corruption rejection.

Windows build and GPU execution: not tested in the development environment. Expected 47 Windows CTest cases. Run the optional --async-gpu test to independently verify four 320x240 readbacks, expected 272,384 checked pixels, and observe reuse of the translated graphics pipeline after the first job. GPU device, root signature, PSO and direct queue reuse are implemented but not validated on hardware. No performance claims. No Xbox ABI or game execution.
