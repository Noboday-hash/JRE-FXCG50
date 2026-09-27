# Project state — 2026-09-27

## Current milestone

M0 scaffold and smoke boundary. Completed criteria: repository component layout, portable C11 source shared by host and calculator targets, host smoke entry point, gint display/key/exit source, valid local template icons, toolchain inventory, host build/test, and calculator cross-build. Hardware acceptance remains pending.

## Coverage

Opcode coverage: none. API coverage: none. `docs/opcodes.csv` and `docs/api-matrix.csv` have schema only. The smoke test is native code and does not execute Java.

## Toolchain and dependencies

See `docs/toolchain.md` for observed versions and provenance. No ZIP, PNG, or Java ME compile dependency is selected. The host build uses the system C compiler. The calculator build uses installed fxSDK/gint and `sh-elf-gcc`.

## Last checks

PASS: `cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug`; `cmake --build build-host --parallel`; `ctest --test-dir build-host --output-on-failure` (1/1); `./build-host/cgjre-host --smoke` (exit 0).

PASS: `fxsdk build-cg` (exit 0). It linked `build-cg/cgjre` as a 32-bit Renesas SH ELF, wrote `build-cg/cgjre.map`, and generated `CGJRE.g3a` via the installed `GenerateG3A` rule. `sh-elf-size build-cg/cgjre`: text 24,832, data 464, BSS 1,056 bytes. NOT RUN: calculator hardware and Java ME emulator.

## Bugs, blockers, and unknowns

- Usable calculator RAM and safe heap reservation are unmeasured.
- Target OS version, framebuffer dimensions exposed by the pinned headers, storage location, and actual hardware exit behavior need verification.
- Old-format Java ME sample compiler, compile-time API, and independent emulator are not pinned.
- Archive, VM, class library, MIDP, inspection, and packaging commands are absent by milestone design.

## Memory and performance

No device measurements. Estimated physical framebuffer size: 177,408 bytes. No Java heap allocated.

## Artifacts

Source icon assets are present. Cross-built, hardware-untested `CGJRE.g3a`: SHA-256 `5aa15903a982b52f934a85e91e153552e5751c033f66e5bca2f6432059746cfa`. No JAR or released add-in exists yet.

## Next smallest task

After review, begin M1 with a bounded ZIP central-directory reader and malformed input fixtures. Acceptance: valid stored/DEFLATE archive metadata is read, and truncated, ZIP64, encrypted, and out-of-bounds structures fail with explicit errors. Continue calculator compilation during M1.
