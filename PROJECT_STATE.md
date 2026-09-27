# Project state — 2026-09-27

Risk summary: DEFLATE extraction on diverse JARs, class-file parsing beyond controlled fixtures, and the incomplete bytecode VM are the three largest unresolved scope risks.

## Current milestone

M1 parses format 45.3 and 46.0 classes, including both local old-phone MIDlet JARs in static inspection. M2 loads classes from an external JAR and executes bounded static integer calls, superclass constructor chains, inherited fields, integer-return virtual calls, and primitive/reference arrays. This remains a restricted fixture VM; GC, class initialization, and MIDlet launch are pending. Farm Frenzy 2 is the first compatibility target by user choice. Calculator source still launches only the smoke screen.

## Coverage

`docs/opcodes.csv` lists all 256 opcode bytes: 94 have bounded executable or partial behavior, 104 are not implemented, and 58 are rejected. The executable subset is limited by `docs/architecture.md`; this is not complete baseline support. `java/lang/Object.<init>()V` has a narrow built-in in `docs/api-matrix.csv`; no broader Java class library is implemented. The matrix also lists planned RMS and silent media descriptors referenced by Farm Frenzy 2. The parser accepts exact class versions 45.3 and 46.0. CLDC `StackMap` is bounded and skipped; there is no full verifier or Java exception delivery.

## Toolchain and dependencies

See `docs/toolchain.md`, `docs/decisions.md`, and `THIRD_PARTY_NOTICES.md`. The raw DEFLATE inflater is pinned miniz tinfl commit `77d0dce8627735138c51770d1799a1ef48f2117d` under MIT. CTest uses Python 3.11.16; the shell `python3` is 3.14.7. ECJ 3.26.0 is in ignored `build-host/deps/` and verified for a 45.3 Java SE fixture. Sun Wireless Toolkit 2.5.2_01 remains selected but uninstalled.

## Last checks

PASS: `cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug`; `cmake --build build-host --parallel`; `ctest --test-dir build-host --output-on-failure` (8/8: smoke, archive, classfile, vm_int, vm_jar, vm_arrays, vm_objects, vm_hierarchy). PASS: `python3 tools/inspect_jar.py` on both local game JARs; this is static parsing only. PASS: ECJ 3.26.0 compiled `build-host/deps/Base.java`, `Child.java`, and `Caller.java` with `-source 1.3 -target 1.1`; `./build-host/cgjre-host --eval-jar build-host/deps/HierarchyFixture.jar Caller test '(I)I' 41` returned `result=42 steps=30` through superclass construction, inherited fields, `Base[]`, and overridden virtual dispatch.

PASS: `fxsdk build-cg`; linked `build-cg/cgjre` and `build-cg/cgjre.map`, and generated the real add-in `CGJRE.g3a`. `sh-elf-size build-cg/cgjre`: text 56,276, data 464, BSS 1,072 bytes. The calculator source still contains only the smoke screen.

PASS: `cmake -S . -B /tmp/cgjre-hierarchy-asan -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'`; build; `ASAN_OPTIONS=detect_leaks=1 ctest --test-dir /tmp/cgjre-hierarchy-asan --output-on-failure` (8/8). A first configure attempt at `/tmp/cgjre-asan` failed because that directory held a cache for another source tree; the new directory completed normally.

NOT RUN: calculator hardware execution and independent Java ME emulator.

## Bugs, blockers, and unknowns

- Usable calculator RAM, safe heap reservation, target OS, storage location, and actual display/key/exit behavior are unmeasured.
- M1 class parser has no full verifier; valid static structure is not executable correctness. Static references can be unreachable or unresolved.
- The resource limit of 4 MiB is a bound, not measured available calculator memory. A device-specific lower limit may be needed.
- The selected Wireless Toolkit API archives, preverifier, and emulator have not been installed or tested locally.
- Long values, interface dispatch, broader method signatures, full array covariance, class initialization, Java exceptions, GC, most class-library methods, MIDP, game sample, and packaging remain pending. Hierarchy and virtual calls are bounded slices; the JAR fixture runner returns explicit errors for unsupported semantics.
- Farm Frenzy 2 statically references RMS and media Player APIs. A file-backed RMS subset and silent media compatibility layer are user-approved planned extensions, not implemented behavior. Gish additionally references Nokia and Bluetooth APIs that remain out of scope.

## Memory and performance

No device measurements. One estimated physical RGB565 framebuffer is 177,408 bytes. Archive, class-cache, frame, class-depth, and temporary array/object limits are recorded in `docs/memory-budget.md`; no GC exists yet.

## Artifacts

Cross-built `CGJRE.g3a`: SHA-256 `a01732088c2c53fd8d03fb09e331cbe4abe247d766527e40e2b2058fdfc05ab5`. The user-supplied `samples/farm2.jar` and `samples/gish.jar` are local-only and ignored by Git; their checksums and static results are in `docs/compatibility-results.md`. No released add-in exists.

## Next smallest task

Add static field storage and `<clinit>` initialization-on-use with superclass ordering and failure state. Acceptance: external 45.3 JAR fixtures exercise `getstatic`, `putstatic`, recursive initialization, and failure diagnostics on the shared host core, while the same source cross-compiles for SH. Then add category-2 `long` operations needed by timing APIs.
