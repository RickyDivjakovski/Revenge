# v0.32.0 validation

- Linux x64 Release build: passed.
- CTest: 46/46 passed, including synthetic PE export tables and native TLS.
- Python unittest: 2/2 passed.
- Export fixture: names, aliases, ordinal-only entry, hole, forwarder, case-sensitive names, case-insensitive module names, literal hash-prefixed name, missing modules/symbols, duplicate modules and seven malformed variants checked.
- Native TLS fixture: 4,096 isolated set/get pairs across eight workers; detach, non-recycled identities, 64-slot capacity, 128 stored thread values, stale/foreign handles and stop cleanup checked.
- Windows/MSVC validation pending. Expected 55 CTest cases, including two new Windows-only commands against the independently built Revenge ABI DLL. The dependency plan explicitly supplies only that DLL; all other imported modules will be reported as not supplied.
- Prior v0.31.0 tester logs: 51/51 Windows CTest, 4,096 protected updates, 39 validated DIR64 relocations and 96 unwind table entries. This release does not change the GPU implementation.
- No guest PE TLS initialization, thread creation services, unwind registration, console API binding or Xbox execution is implemented. The native scalar slots are an independent ABI service, and the offline export report does not establish callable ABI compatibility.
