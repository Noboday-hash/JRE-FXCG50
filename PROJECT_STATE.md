# Project state — 2026-09-27

Risk summary: DEFLATE extraction on diverse JARs, class-file parsing beyond controlled fixtures, and the unimplemented bytecode VM are the three largest unresolved scope risks.

## Current milestone

M1 host gate passes on controlled fixtures: the portable reader indexes bounded ZIP/JAR files, extracts stored and raw DEFLATE entries with length and CRC checks, parses the main manifest and MIDlet declarations, and parses class format 45.3 metadata. The host inspector reports sizes, versions, opcode usage, native declarations, static member references, and selected likely incompatibilities. Malformed archive, manifest, class, and bytecode fixtures fail explicitly. M2 VM execution is not started.

## Coverage

Executable Java opcode coverage: none. The M1 bytecode scanner counts instruction starts and checks operand boundaries; it is not an interpreter or verifier. API implementation coverage: none. `docs/opcodes.csv` and `docs/api-matrix.csv` still have schema only and must be populated alongside M2. The inspector does not prove that a game can run. Only class version 45.3 is accepted. CLDC `StackMap` is bounded and skipped; it does not establish CLDC verification.

## Toolchain and dependencies

See `docs/toolchain.md`, `docs/decisions.md`, and `THIRD_PARTY_NOTICES.md`. The raw DEFLATE inflater is pinned miniz tinfl commit `77d0dce8627735138c51770d1799a1ef48f2117d` under MIT. CTest uses Python 3.11.16; the shell `python3` is 3.14.7. ECJ 3.26.0 and Sun Wireless Toolkit 2.5.2_01 are selected for the sample but are not installed or verified locally.

## Last checks

PASS: `cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug`; `cmake --build build-host --parallel`; `ctest --test-dir build-host --output-on-failure` (3/3: smoke, archive, classfile). PASS: `python3 tools/inspect_jar.py /tmp/cgjre-inspect.jar` on a generated compressed-manifest fixture.

PASS: `fxsdk build-cg`; linked `build-cg/cgjre` and `build-cg/cgjre.map`, and generated the real add-in `CGJRE.g3a`. `sh-elf-size build-cg/cgjre`: text 39,668, data 464, BSS 1,072 bytes. The calculator source still contains only the smoke screen.

PASS: `cmake -S . -B /tmp/cgjre-asan -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'`; build; `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/cgjre-asan --output-on-failure` (3/3). Without `detect_leaks=0`, LeakSanitizer fails in this ptrace sandbox before tests run; this is an environment limitation, not a passing leak check.

NOT RUN: calculator hardware execution and independent Java ME emulator.

## Bugs, blockers, and unknowns

- Usable calculator RAM, safe heap reservation, target OS, storage location, and actual display/key/exit behavior are unmeasured.
- M1 class parser has no full verifier; valid static structure is not executable correctness. Static references can be unreachable or unresolved.
- The resource limit of 4 MiB is a bound, not measured available calculator memory. A device-specific lower limit may be needed.
- The selected Java ME compiler, API archives, and emulator have not been installed or tested locally.
- VM, class library, MIDP, game sample, and packaging remain pending.

## Memory and performance

No device measurements. One estimated physical RGB565 framebuffer is 177,408 bytes. M1 archive allocation bounds are recorded in `docs/memory-budget.md`; no Java heap exists yet.

## Artifacts

Cross-built `CGJRE.g3a`: SHA-256 `0a196c64610ee3834a088a73ccd75b2fe147deb4778585e713f8f89f7df07996`. No game JAR or released add-in exists.

## Next smallest task

Begin M2 with a documented 32-bit slot and handle model and a small explicit-frame interpreter for integer constants, local loads/stores, arithmetic, branches, and returns. Populate corresponding rows of `docs/opcodes.csv` and add independent semantic fixtures. Acceptance: a class format 45.3 fixture executes a method returning an independently computed integer on the host, with errors for unsupported instructions; cross-compile the same core for SH before expanding opcode coverage.
