# Offline export inventory and native TLS (v0.32.0)

`exports <authorised.dll>` reads the PE32+ export directory, file-backed export address/name/ordinal tables and printable bounded strings. It reports aliases, ordinal-only entries, absent slots, and forwarded export declarations. Ordinal and name imports are distinguished explicitly, including names starting with `#`. A nonzero ordinary export RVA must fall in a section. Forwarders must terminate inside the export directory. Limits: 512 MiB per file, 65,536 function slots and names, 4,096 bytes per string. This is a bounded diagnostic subset, not a complete PE validity test.

`dependency-plan <authorised.exe> <explicit.dll> [...]` compares direct imports against the supplied files, identified by case-insensitive basenames (up to 64 files). Export names are case-sensitive. Reports distinguish direct export present, forwarder declared, symbol absent, and module not supplied. A forwarded declaration is not resolved. Export presence does not establish signature, calling convention, binary identity, runtime behavior, or compatibility. No recursive dependencies, API-set mapping, delay imports, search path discovery, loader simulation, import patching, module loading or execution. No commercial binaries are included. Use independently authored or otherwise authorised files only.

## Native ABI operations

All use the existing `rv_request` structure and five exported functions. Unused request fields and buffer capacity must be zero. Handles are session-owned opaque IDs; values are uint64 scalars, never interpreted as pointers by the runtime.

| Operation | Arguments | Result |
|---|---|---|
| RV_TLS_CREATE (45) | none | slot handle |
| RV_TLS_SET (46) | handle, arg0=value | zero; value 0 erases the entry |
| RV_TLS_GET (47) | handle | current thread value, zero if unset |
| RV_TLS_RELEASE (48) | handle | zero; removes slot and all values |
| RV_TLS_DETACH (49) | none | zero; removes current thread values from all session slots |
| RV_TLS_COUNT (50) | none | slot count |

There are at most 64 slots per session and 128 stored nonzero thread values per slot. Native threads receive a unique process-local identity, not a recycled OS thread ID. A thread that exits without DETACH does not expose its old value to later threads, but its entry consumes capacity until release or stop. Clients must detach before exit. The runtime does not run destructors for scalar values. Operations are serialized by a per-session mutex; stop clears slots and rejects subsequent operations. As before, callers must finish concurrent calls before closing the session. No PE TLS directory initialization, guest thread creation, exception dispatch or unwind registration is implemented.
