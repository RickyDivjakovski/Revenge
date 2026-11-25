# v0.32.1 validation

- User's MSVC 19.44 log for v0.32.0: runtime, core library, shared library and export fixture built; TLS test fixture failed on `std::to_string` (C2039/C3861) and cascading C2512.
- Fix: explicitly include `<string>` in `tests/native_abi_tls.cpp`. Version identifiers updated. No runtime or ABI behaviour changes.
- Linux x64 Release incremental rebuild passed; CTest 46/46 passed; Python unittest 2/2 passed.
- Windows rebuild and 55 CTest cases remain to be confirmed by the tester.
