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

The Java ME sample choices are pinned in `docs/decisions.md`. ECJ 3.26.0 was downloaded from [Maven Central](https://repo.maven.apache.org/maven2/org/eclipse/jdt/ecj/3.26.0/ecj-3.26.0.jar) to ignored `build-host/deps/ecj-3.26.0.jar` (SHA-256 `ac0ba5876eaf7ebb47749a0d1be179c51f194b9dd0b875d1c09e1b530f5a2db5`). `java -jar ... -version` reported 3.26.0. `java -jar ... -source 1.3 -target 1.1 -d build-host/deps/classes build-host/deps/Arithmetic.java` produced a class that `javap -verbose` identified as major 45, minor 3; `cgjre-host --eval-class` returned 42 for a method using `invokestatic`. This checks a Java SE boot-class fixture, not the selected CLDC/MIDP API archives or preverification. The Wireless Toolkit is not installed or verified locally. Calculator hardware has not been verified. `fxconv --version` is not supported by the installed CLI, so no independent fxconv version is asserted.

The raw DEFLATE inflater is miniz `tinfl`, pinned to upstream commit `77d0dce8627735138c51770d1799a1ef48f2117d`. Source: [official miniz repository](https://github.com/richgel999/miniz). License: MIT, included in `third_party/miniz/LICENSE`. `miniz_tinfl.c` SHA-256: `2296ebd21ef9af5ebbefd5b454d4bb67d8fb4af2d24945e7fb32114374f75a76`. The source was cross-compiled with this fxSDK toolchain; hardware performance and memory use remain unmeasured.
