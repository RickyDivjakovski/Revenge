# Revenge Native 0.6.0 validation

- Linux CMake/GCC C++20 Release build passed.
- CTest passed: PE name/ordinal imports, bounds checks, DIR64 relocation fixup, unsupported relocation and absent relocation rejection, direct IAT slot indexing, dirty-page tracking, folder candidate selection.
- Python shader tests: 2 passed (structural container validation and deduplication).
- User-provided executable was read from its ZIP, prepared at preferred base 0x140000000 and alternate base 0x180000000. Both completed. 184,995,840 mapped bytes, 598,282 DIR64 relocations, 587 direct IAT slots, 5,248 bytes of TLS initial data, two TLS callbacks, 299,257 x64 runtime function entries. No title code executed. No title files are distributed.
- Windows MSVC compilation, Windows folder picker, Direct3D 12 GPU execution, real DXC compilation and Xbox ABI compatibility not tested. No Windows executable is included.
- This is a non-executable data-buffer preparation step, not a native Xbox loader. No imports bound, TLS initialized, unwind table registered or title entry point invoked. No commercial game compatibility or FPS demonstrated.
