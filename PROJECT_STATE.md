# Project state — 2026-09-27

## Current milestone

M1 archive loading, partial. M0 scaffold, host build/test, and calculator cross-build are complete; hardware acceptance remains pending. M1 now has bounded ZIP central-directory indexing, local-header and extent checks, stored-entry extraction with CRC, and bounded main-section manifest parsing. DEFLATE extraction, class parsing, and full JAR inspection are pending; M1 is not complete.

## Coverage

Opcode coverage: none. API coverage: none. `docs/opcodes.csv` and `docs/api-matrix.csv` have schema only. The smoke test is native code and does not execute Java. The host `--inspect` command reports archive entries and stored manifest attributes only; it cannot determine compatibility.

## Toolchain and dependencies

See `docs/toolchain.md` for observed versions and provenance. The ZIP indexer has no external dependency; a target-compatible DEFLATE/PNG dependency and Java ME compile toolchain are still unselected. CTest found Python 3.11.16 at `/home/ve1qu/.local/bin/python3.11`. The calculator build uses installed fxSDK/gint and `sh-elf-gcc`.

## Last checks

PASS: `cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug`; `cmake --build build-host --parallel`; `ctest --test-dir build-host --output-on-failure` (2/2, smoke and archive fixtures).

PASS: `fxsdk build-cg` (exit 0). It linked `build-cg/cgjre` as a 32-bit Renesas SH ELF, wrote `build-cg/cgjre.map`, and generated `CGJRE.g3a` via the installed `GenerateG3A` rule. `sh-elf-size build-cg/cgjre`: text 28,240, data 464, BSS 1,056 bytes. NOT RUN: calculator hardware and Java ME emulator.

## Bugs, blockers, and unknowns

- Usable calculator RAM and safe heap reservation are unmeasured.
- Target OS version, framebuffer dimensions exposed by the pinned headers, storage location, and actual hardware exit behavior need verification.
- Old-format Java ME sample compiler, compile-time API, and independent emulator are not pinned.
- DEFLATE extraction is explicitly unsupported. Class parsing, VM, class library, MIDP, full inspection, and packaging commands are absent. The existing host inspector is metadata-only.

## Memory and performance

No device measurements. Estimated physical framebuffer size: 177,408 bytes. No Java heap allocated.

## Artifacts

Source icon assets are present. Cross-built, hardware-untested `CGJRE.g3a`: SHA-256 `288774e9ac938eb5f425163b48d9bb62b0bd3c3db1e688bf77c29e5279fe295b`. No JAR or released add-in exists yet.

## Next smallest task

Select and integrate a small, licensed, target-compatible raw DEFLATE inflater with bounded streaming output, then add compressed-entry and data-descriptor fixtures. Acceptance: extracted bytes and CRC match for a compressed manifest on host, malformed streams fail explicitly, and the same portable source cross-compiles with `fxsdk build-cg`. Then implement the bounded class-file parser and extend inspection.
