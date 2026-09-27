# Toolchain inventory (2026-09-27)

Locally observed:

| Component | Version or revision | Evidence |
|---|---|---|
| fxSDK | 2.11.0 | `fxsdk --version` |
| gint | 2.11.0, commit `badbd0fd2bd8ac796fd55d49b93691741bd8a139` | installed source `CMakeLists.txt`, git revision, generated `gint/config.h` |
| SH compiler | GCC 14.1.0 | `sh-elf-gcc --version` |
| Host C compiler | GCC 16.2.1 | CMake compiler identification during host configure |
| CMake | 4.4.3 | `cmake --version` |
| Python | 3.14.7 | `python3 --version` |
| Java compiler | 21.0.12.1 | `javac -version`; not suitable by default for the required old class format |

`fxsdk new CGJRE-template` was run in `/tmp`. Its CMake template uses `GenerateG3A`, `Fxconv`, and `Gint::Gint`, and its main source uses `dclear`, `dtext`, `dupdate`, and `getkey`. The installed `GenerateG3A.cmake` accepts `TARGET`, `OUTPUT`, `NAME`, and two `ICONS`; an explicit `OUTPUT` resolves relative to the source root. The two 92 × 64 PNGs in `assets-cg/` were copied from that local template. The project does not invoke fxconv because it has no compiled image assets.

No Java ME-capable compiler, API source, independent emulator, or calculator hardware has been verified. Acquisition sources and licenses for those remain to be pinned before building TileBounce. `fxconv --version` is not supported by the installed CLI, so no independent fxconv version is asserted.
