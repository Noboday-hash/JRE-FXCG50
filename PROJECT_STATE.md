# Project state — 2026-09-27

Risk summary: DEFLATE extraction on diverse JARs, class-file parsing beyond controlled fixtures, and the incomplete bytecode VM are the three largest unresolved scope risks.

## Current milestone

M1 host gate passes on controlled fixtures. M2 has a first shared integer interpreter slice: static `(I...)I` methods in format 45.3 classes can execute without class initialization, calls, monitors, or exception handlers. The host fixture runner evaluates independent synthetic classes and one class compiled by ECJ. Calculator source still launches only the smoke screen.

## Coverage

`docs/opcodes.csv` now lists all 256 opcode bytes: 54 are marked `m2_integer_slice`, three `partial_integer_only`, and the rest are not implemented or rejected. All 57 slice entries have host fixtures, but only under the restrictions in `docs/architecture.md`; this is not complete baseline opcode support. No Java class-library API is implemented, and `docs/api-matrix.csv` remains schema-only. The parser accepts only class version 45.3. CLDC `StackMap` is bounded and skipped; there is no full verifier or Java exception delivery.

## Toolchain and dependencies

See `docs/toolchain.md`, `docs/decisions.md`, and `THIRD_PARTY_NOTICES.md`. The raw DEFLATE inflater is pinned miniz tinfl commit `77d0dce8627735138c51770d1799a1ef48f2117d` under MIT. CTest uses Python 3.11.16; the shell `python3` is 3.14.7. ECJ 3.26.0 is downloaded and verified for a 45.3 Java SE fixture. Sun Wireless Toolkit 2.5.2_01 remains selected but uninstalled.

## Last checks

PASS: `cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug`; `cmake --build build-host --parallel`; `ctest --test-dir build-host --output-on-failure` (4/4: smoke, archive, classfile, vm_int). PASS: ECJ 3.26.0 compiled `/tmp/cgjre-tools/Arithmetic.java` with `-source 1.3 -target 1.1`; `javap -verbose` showed 45.3, and `./build-host/cgjre-host --eval-class /tmp/cgjre-tools/classes/Arithmetic.class test '(I)I' 41` returned `result=42 steps=4`.

PASS: `fxsdk build-cg`; linked `build-cg/cgjre` and `build-cg/cgjre.map`, and generated the real add-in `CGJRE.g3a`. `sh-elf-size build-cg/cgjre`: text 44,328, data 464, BSS 1,072 bytes. The calculator source still contains only the smoke screen.

PASS: `cmake -S . -B /tmp/cgjre-asan -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'`; build; `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/cgjre-asan --output-on-failure` (4/4). Leak detection is disabled because LeakSanitizer fails in this ptrace sandbox before tests run; no passing leak check is claimed.

NOT RUN: calculator hardware execution and independent Java ME emulator.

## Bugs, blockers, and unknowns

- Usable calculator RAM, safe heap reservation, target OS, storage location, and actual display/key/exit behavior are unmeasured.
- M1 class parser has no full verifier; valid static structure is not executable correctness. Static references can be unreachable or unresolved.
- The resource limit of 4 MiB is a bound, not measured available calculator memory. A device-specific lower limit may be needed.
- The selected Wireless Toolkit API archives, preverifier, and emulator have not been installed or tested locally.
- Long and reference values, method calls, linkage, objects, Java exceptions, class library, MIDP, game sample, and packaging remain pending. The integer fixture runner returns explicit errors for unsupported semantics.

## Memory and performance

No device measurements. One estimated physical RGB565 framebuffer is 177,408 bytes. Archive and frame allocation bounds are recorded in `docs/memory-budget.md`; no Java heap exists yet.

## Artifacts

Cross-built `CGJRE.g3a`: SHA-256 `305b230c41b850e7f9c733364d625d2b143d0e6285bbfb94496f8c477b4bcf04`. No game JAR or released add-in exists.

## Next smallest task

Extend the explicit frame stack with same-class `invokestatic` integer calls, method resolution by name and descriptor, and checked return propagation. Acceptance: nested and recursive class format 45.3 fixtures execute with a bounded frame count on the host, invalid members fail with context, and the shared core still cross-compiles for SH.
