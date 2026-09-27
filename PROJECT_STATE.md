# Project state — 2026-09-27

Risk summary: DEFLATE extraction on diverse JARs, class-file parsing beyond controlled fixtures, and the incomplete bytecode VM are the three largest unresolved scope risks.

## Current milestone

M1 parses format 45.3 and 46.0 classes, including both local old-phone MIDlet JARs in static inspection. M2 loads classes from an external JAR and executes bounded static integer calls across them. The VM now has typed reference slots and bounded boolean, byte, char, short, and int arrays. Objects, GC, and MIDlet launch are pending. Farm Frenzy 2 is the first compatibility target by user choice. Calculator source still launches only the smoke screen.

## Coverage

`docs/opcodes.csv` lists all 256 opcode bytes: 51 `m2_integer_slice`, 15 `m2_reference_slice`, nine `m2_array_slice`, three `m2_category1_slice`, and five partially supported entries (`ldc`, `ldc_w`, `wide`, `invokestatic`, `newarray`). The rest are not implemented or rejected. These slice entries have host fixtures under the restrictions in `docs/architecture.md`; this is not complete baseline support. No Java class-library API is implemented. `docs/api-matrix.csv` lists exact planned RMS and silent media descriptors referenced by Farm Frenzy 2. The parser accepts exact class versions 45.3 and 46.0. CLDC `StackMap` is bounded and skipped; there is no full verifier or Java exception delivery.

## Toolchain and dependencies

See `docs/toolchain.md`, `docs/decisions.md`, and `THIRD_PARTY_NOTICES.md`. The raw DEFLATE inflater is pinned miniz tinfl commit `77d0dce8627735138c51770d1799a1ef48f2117d` under MIT. CTest uses Python 3.11.16; the shell `python3` is 3.14.7. ECJ 3.26.0 is in ignored `build-host/deps/` and verified for a 45.3 Java SE fixture. Sun Wireless Toolkit 2.5.2_01 remains selected but uninstalled.

## Last checks

PASS: `cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug`; `cmake --build build-host --parallel`; `ctest --test-dir build-host --output-on-failure` (6/6: smoke, archive, classfile, vm_int, vm_jar, vm_arrays). PASS: `python3 tools/inspect_jar.py` on both local game JARs; this is static parsing only. PASS: ECJ 3.26.0 compiled `build-host/deps/ArrayFixture.java` with `-source 1.3 -target 1.1`; `./build-host/cgjre-host --eval-class build-host/deps/classes/ArrayFixture.class test '(I)I' 41` returned `result=41 steps=11` for a real `int[]` store/load.

PASS: `fxsdk build-cg`; linked `build-cg/cgjre` and `build-cg/cgjre.map`, and generated the real add-in `CGJRE.g3a`. `sh-elf-size build-cg/cgjre`: text 47,776, data 464, BSS 1,072 bytes. The calculator source still contains only the smoke screen.

PASS: `cmake -S . -B /tmp/cgjre-asan -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'`; build; `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/cgjre-asan --output-on-failure` (6/6). Leak detection is disabled because LeakSanitizer fails in this ptrace sandbox before tests run; no passing leak check is claimed.

NOT RUN: calculator hardware execution and independent Java ME emulator.

## Bugs, blockers, and unknowns

- Usable calculator RAM, safe heap reservation, target OS, storage location, and actual display/key/exit behavior are unmeasured.
- M1 class parser has no full verifier; valid static structure is not executable correctness. Static references can be unreachable or unresolved.
- The resource limit of 4 MiB is a bound, not measured available calculator memory. A device-specific lower limit may be needed.
- The selected Wireless Toolkit API archives, preverifier, and emulator have not been installed or tested locally.
- Long values, object and reference-array semantics, inherited/virtual calls, class initialization, Java exceptions, GC, class library, MIDP, game sample, and packaging remain pending. The JAR fixture runner returns explicit errors for unsupported semantics.
- Farm Frenzy 2 statically references RMS and media Player APIs. A file-backed RMS subset and silent media compatibility layer are user-approved planned extensions, not implemented behavior. Gish additionally references Nokia and Bluetooth APIs that remain out of scope.

## Memory and performance

No device measurements. One estimated physical RGB565 framebuffer is 177,408 bytes. Archive, class-cache, frame, and temporary array limits are recorded in `docs/memory-budget.md`; no managed Java object heap or GC exists yet.

## Artifacts

Cross-built `CGJRE.g3a`: SHA-256 `134dcbdb4a14472c6a3f490284f037d39720549d846d3d2acd6f8cf7f2169a40`. The user-supplied `samples/farm2.jar` and `samples/gish.jar` are local-only and ignored by Git; their checksums and static results are in `docs/compatibility-results.md`. No released add-in exists.

## Next smallest task

Extend the handle table to instances of classes directly extending `java/lang/Object`, with checked integer/reference field layout and constructor execution. Acceptance: a format 45.3 external JAR fixture performs `new`, `invokespecial <init>`, `putfield`, and `getfield` through the shared VM; field resolution uses name and descriptor, null and invalid member cases fail explicitly, and the source cross-compiles for SH. Then add inherited layout and virtual dispatch.
