# Revenge Native 0.6.0 validation

- Linux CMake/GCC C++20 Release build passed.
- CTest passed: PE name/ordinal imports, bounds checks, DIR64 relocation fixup, unsupported relocation and absent relocation rejection, direct IAT slot indexing, dirty-page tracking, folder candidate selection.
- Python shader tests: 2 passed (structural container validation and deduplication).
- Windows MSVC compilation, Windows folder picker, Direct3D 12 GPU execution, real DXC compilation and Xbox ABI compatibility not tested. No Windows executable is included.
- This is a non-executable data-buffer preparation step, not a native Xbox loader. No imports bound, TLS initialized, unwind table registered or title entry point invoked. No commercial game compatibility or FPS demonstrated.

Public validation notes exclude title-specific diagnostic reports. Use independently authored fixtures for reproducible public tests.
