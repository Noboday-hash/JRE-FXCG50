# fx-CG50 Java ME runtime — reusable master prompt

Copy from “Master prompt begins” through “Master prompt ends” into your coding assistant. Keep the resulting `SPEC.md` and `PROJECT_STATE.md` with the project. This brief specifies work; it does not claim that an implementation or sample binary has already been built or tested.

## Assumptions to correct before starting

- **Memory:** Treat 8 MiB as physical RAM, not a guaranteed application heap. In a December 2024 answer, gint's author described approximately 600–700 kB normally available on the CG50 and additional regions commonly used for another 2–3 MB. Determine safe availability for the actual OS and gint revision before allocating a Java heap. Do not hardcode an assumed free 8 MiB region. [Maintainer discussion](https://www.planet-casio.com/Fr/forums/topic18535-1-gint-programming-questions.html)
- **MMU:** “No MMU-backed OS” is inaccurate literally: MMU/TLB mappings are used for add-in code. The useful project constraint is that this is not a desktop process environment with demand paging, memory protection, POSIX threads, and an arbitrarily expandable heap. [Maintainer explanation](https://www.planet-casio.com/Fr/forums/topic13572-42-gint-un-noyau-pour-developper-des-add-ins.html)
- **Toolchain:** Recommend fxSDK + gint and `sh-elf-gcc`. Its CMake integration, graphics/input/timer support, and current ecosystem make it the practical primary choice. PrizmSDK is a fallback requiring a separate backend, not something to mix into the initial build. The maintained headers and installed template must be checked before committing to exact APIs. The upstream forge was inaccessible during preparation of this brief; no latest release number is asserted here. [Maintainer CMake tutorial](https://www.planet-casio.com/Fr/forums/topic16647-last-tutoriel-compiler-des-add-ins-avec-cmake-fxsdk.html)
- **CLDC 1.1:** It includes floating point. An integer-only first release is a deliberate compatibility subset. Even that release needs 64-bit `long` for timing and common Java APIs. [CLDC Double](https://docs.oracle.com/javame/config/cldc/ref-impl/cldc1.1/jsr139/java/lang/Double.html), [CLDC System](https://docs.oracle.com/javame/config/cldc/ref-impl/cldc1.1/jsr139/java/lang/System.html)
- **Threading:** Cooperative green threads are feasible, but arbitrary game loops may not yield. GameCanvas documentation even illustrates a continuous render loop. Implement actual thread state and monitors; calling `run()` synchronously or treating synchronization as a no-op is insufficient. Switching Java threads after an instruction quota is VM-level preemption, even without OS threads. [Thread API](https://docs.oracle.com/javame/config/cldc/ref-impl/cldc1.1/jsr139/java/lang/Thread.html), [GameCanvas API](https://docs.oracle.com/javame/config/cldc/ref-impl/midp2.0/jsr118/javax/microedition/lcdui/game/GameCanvas.html)
- **Game compatibility:** The difficult part is the combination of VM semantics, library methods, assets, and game assumptions. PNG decoding and resource streams are core requirements, not optional polish. MIDP Image explicitly specifies PNG support. Games may additionally require menus, RMS saves, vendor APIs, or sound API symbols even when the user does not want audible output. [Image API](https://docs.oracle.com/javame/config/cldc/ref-impl/midp2.0/jsr118/javax/microedition/lcdui/Image.html)
- **Feasibility:** A selected-game compatibility runtime is a reasonable engineering target; broad support for arbitrary J2ME games is not a promised outcome. Measure speed and memory on the calculator. A desktop harness cannot establish SH performance. Also verify the add-in size limit: the maintainer reports a 2 MB CG50 add-in limit, reinforcing the choice to keep game JARs external. [Maintainer discussion](https://www.planet-casio.com/Fr/forums/topic18535-1-gint-programming-questions.html)

---

## Master prompt begins

You are implementing an embedded Java ME compatibility runtime for the Casio fx-CG50. Work as an experienced C, virtual-machine, and embedded graphics engineer. Produce a buildable repository in incremental, testable milestones. This is an implementation assignment, not a request for architectural pseudocode.

### 1. Goal and definition of success

Build `CGJRE.g3a`, an fx-CG50 add-in that loads an external, ordinary ZIP-format `.jar` containing a supported CLDC/MIDP application and resources, selects its MIDlet, and runs a simple 2D game with graphics, keys, timing, sprites, and tiles. No game-specific translation or recompilation may be required to launch a compatible existing JAR. A host inspection tool may report compatibility, but must not become an undisclosed mandatory JAR conversion step.

Call the project a **CLDC/MIDP subset compatibility runtime**, not a conformant JRE or certified Java implementation. A small switch-dispatch interpreter is the baseline. No JIT or full JVM implementation is required.

Success means:

1. A clean documented build produces a real `.g3a`, not an ELF file renamed to `.g3a`.
2. The calculator loads the supplied external sample JAR and runs its game loop.
3. Movement, collision, frame animation, tile rendering, input, pause/resume, and exit work.
4. Supported semantics have meaningful host tests; hardware behavior has separately recorded evidence.
5. Unsupported files/features produce actionable diagnostics, never silent omission or fictitious success.

If hardware is unavailable, finish all feasible implementation and host validation, clearly mark hardware acceptance as pending, and supply precise testing instructions. Do not claim the whole project complete.

### 2. Hardware, platform, and toolchain

- Primary target: Casio fx-CG50 / Graph 90+E, Renesas SH7305 in the SH4A family. Do not silently extend support to other calculator models.
- Deliverable: `.g3a` add-in plus externally stored game JARs.
- Primary stack: fxSDK + gint + its compatible libc/runtime dependencies, CMake, and `sh-elf-gcc`. Use the SDK's target ABI, linker setup, startup code, and CPU flags; do not invent generic SH4 flags or assume an FPU.
- Implement the VM core in portable C11 with explicit-width integers. Keep gint references inside the calculator backend. Prefer `-Os` initially and profile before optimizing.
- Host development: Linux or Linux under WSL, host C compiler, CMake, Python 3, and a separately pinned Java ME-capable sample compilation toolchain.
- PrizmSDK/libfxcg: fallback only if a concrete fxSDK blocker is demonstrated and documented. Do not build two hardware backends before the primary implementation works.
- Treat usable RAM as measured, OS/version-dependent capacity. Begin with a configurable modest heap, for example 256 KiB if the measured total budget permits it. Add an extended-memory configuration only through a verified safe reservation mechanism.
- Plan around a 396 × 224 physical framebuffer with RGB565 output through gint; verify actual exposed dimensions in the pinned headers. One such 16-bit framebuffer consumes 177,408 bytes. Account for every extra buffer and image.
- No dependence on demand paging, `mmap`, OS processes, or native preemptive threads. Respect gint/Casio OS transitions and filesystem constraints.

First inspect available tools, installed headers, SDK templates, and documentation. Record exact versions or commits in `docs/toolchain.md`. If something is unavailable, say so and continue with the host implementation where possible. Never fabricate a download URL, SDK API, version, or successful build.

### 3. Compatibility boundaries

Maintain three independent labels: **required for baseline**, **required for extended profile**, and **out of scope**. Milestone order does not change these requirements.

**Baseline:** integer-based 2D MIDlets, byte/boolean/char/short/int/long/reference values and arrays, objects and inheritance, interface dispatch, exceptions, class initialization, garbage collection, cooperative Java threads, local JAR resources, PNG images, Canvas/GameCanvas, and the game-layer APIs specified below.

**Extended profile:** float/double bytecodes and library methods with correct Java numerical behavior; optional small file-backed RMS subset and minimal menu widgets if actual selected games require them. Float/double are required before describing this as a floating-point-capable CLDC 1.1 subset, but may follow the integer-game baseline.

**Out of scope:** audio playback, sound/media APIs, networking and Generic Connection Framework networking, Bluetooth, messaging, camera, 3D/M3G, vendor-specific Nokia/Siemens/etc. extensions, AWT/Swing, Java SE collections framework, reflection beyond specifically listed CLDC `Class` operations, user class loaders, JNI/dynamic native loading, serialization, finalizers, weak references, JIT, multiple simultaneous MIDlet suites, installation/signing/OTA infrastructure, and full security certification.

RMS and high-level LCDUI are not silently implemented as empty methods. They remain unsupported until added explicitly. Likewise, “no sound” does not automatically mean media classes exist as stubs. If a target game needs a silent media compatibility layer, propose it as a separately named extension and document its behavior; do not quietly add it or pretend playback succeeded.

For every API, record class, exact method/field descriptor, status, implemented semantics, limitations, and associated tests in `docs/api-matrix.csv`. Merely listing a class does not imply all its methods exist.

### 4. Repository and component boundaries

Provide at least these components, splitting source files further as needed:

- Root `CMakeLists.txt`, `README.md`, `SPEC.md`, `PROJECT_STATE.md`, `CHANGELOG.md`, license and third-party notices.
- `include/cgjre/`: VM/platform contracts, types, limits, and configuration.
- `src/vm/`: class parsing, linking, interpreter, frames, object model, exceptions, GC, scheduler, native bridge.
- `src/archive/`: bounded ZIP/JAR access and manifest parser.
- `src/classlib/` and optionally `classlib/java/`: actual built-in library implementations and deterministic embedding rules.
- `src/midp/`: lifecycle, event queue, graphics surfaces, image decoder integration, fonts, game layers.
- `src/platform/gint/`: display, keys, time, storage, safe memory acquisition, launcher, exit handling.
- `src/platform/host/`: headless deterministic backend and, optionally, SDL2 interactive backend.
- `assets-cg/`: valid selected/unselected icons and any native add-in assets.
- `tests/`: semantic fixtures, malformed input tests, rendering tests, scheduler and GC tests.
- `samples/TileBounce/`: complete Java source, manifest, resources, build configuration, and license.
- `tools/`: sample build, JAR inspection, asset generation, and release packaging scripts.
- `docs/`: architecture, toolchain, memory budget, opcode/API matrices, compatibility results, hardware testing, and decisions.

Hardware services must be injected through a small platform interface. The same interpreter and MIDP implementation must run on the host and calculator; do not maintain a fake host-only VM.

### 5. Archive and class-file loading

Implement these as distinct layers:

1. **JAR/ZIP reader:** central-directory indexing; stored and DEFLATE entries; bounded streaming decompression; CRC checks; support normal data-descriptor archives by using central-directory sizes; checked offsets and lengths; no whole-archive expansion. Reject encrypted, multi-volume, and ZIP64 archives initially with explicit errors. Limit entry count, inflated bytes, image dimensions, and per-resource allocation.
2. **Manifest:** `META-INF/MANIFEST.MF`, continuation lines, CRLF/LF, case-insensitive attribute names, `MIDlet-n`, MIDlet class selection, name/version/vendor, configuration/profile attributes. Launch with the MIDlet constructor and lifecycle, never a presumed `main()` method. Permit standalone JAR launch. Optional JAD support must specify precedence and validate any referenced filename/size.
3. **Resources:** package-relative and absolute `Class.getResourceAsStream` paths with owned stream lifetime; no extraction outside a managed cache and no path traversal.
4. **Class parser:** explicitly read big-endian values without unaligned pointer casts; parse the supported constant pool, fields, methods, descriptors, Code, exception tables, ConstantValue, and useful debugging attributes. Handle modified UTF-8, not ordinary UTF-8. Long/double constant entries consume two constant-pool indices.
5. **Version policy:** start with class format 45.3 for the controlled sample. Establish the exact accepted old-format versions from CLDC documentation and fixtures before broadening. Reject modern bytecode formats and constant-pool tags. Never “downgrade” by changing only version bytes.
6. **Preverification:** document the CLDC `StackMap` attribute separately from newer `StackMapTable`. If using runtime checked execution instead of the original split verifier, bound and parse/skip the old attribute deliberately, explain the trust limitations, and never claim full CLDC verification. Skipping metadata is not permission to omit execution checks.

Class metadata must contain ownership and lifetime information. Link superclasses/interfaces, lay out inherited instance fields and static fields, resolve members by name **and descriptor**, and cache resolution where useful. Implement `<init>` and `<clinit>` ordering, initialization-on-use, recursive initialization, failure state, and thread interaction. Built-in classes must resolve exactly as Java classes, including overridable callbacks. Prevent application classes from replacing built-in platform classes.

An inspection tool must report archive sizes, versions, opcode usage, referenced APIs, native declarations, and likely incompatibilities. Separate unreachable or unresolved optional references from known executed failures; do not promise that static inspection proves a game compatible.

### 6. Interpreter and object semantics

Use an explicit VM frame stack, not unbounded C recursion for Java calls. Specify the representation of 32-bit slots, two-slot category-2 values, references, locals, operand stacks, program counters, and return values. Use handles or another design that works on both 64-bit hosts and the 32-bit target; never truncate host pointers into Java slots.

Create `docs/opcodes.csv` with every opcode byte, mnemonic, operand format, profile, implementation status, and tests. Expand the families below into individual entries; an omitted opcode must not silently default to a no-op.

**Required baseline families:**

- Constants: `nop`, `aconst_null`, `iconst_*`, `lconst_*`, `bipush`, `sipush`, `ldc`, `ldc_w`, `ldc2_w` for supported constants.
- Locals: integer, long, and reference loads/stores and their `_0`–`_3` forms; `iinc`; `wide` for supported nested instructions.
- Stack operations: `pop`, `pop2`, `dup`, `dup_x1`, `dup_x2`, `dup2`, `dup2_x1`, `dup2_x2`, `swap`, enforcing category rules.
- Integer/long arithmetic: add/subtract/multiply/divide/remainder/negation, shifts, unsigned shifts, and/or/xor; `i2l`, `l2i`, `i2b`, `i2c`, `i2s`, `lcmp`.
- Branches: all integer zero/compare and reference compare branches, null branches, `goto`, `goto_w`, `tableswitch`, `lookupswitch`. Correctly handle switch padding and signed offsets relative to the opcode.
- Objects/arrays: `new`, `newarray`, `anewarray`, `multianewarray`, `arraylength`, relevant primitive and reference loads/stores including byte/boolean and char distinctions; `checkcast`, `instanceof`.
- Fields/calls: `getfield`, `putfield`, `getstatic`, `putstatic`, `invokevirtual`, `invokespecial`, `invokestatic`, `invokeinterface`, and `return`, `ireturn`, `lreturn`, `areturn`.
- Exceptions/locking: `athrow`, `monitorenter`, `monitorexit`; synchronized method entry/exit is also required even without explicit monitor opcodes.

**Extended floating point:** float/double constants, loads/stores, arrays, arithmetic, comparisons, conversions, and returns, including `fcmpl/g`, `dcmpl/g`, NaNs, infinities, signed zero, conversion saturation, and division/remainder behavior. Use software support where needed and measure its cost.

**Initially rejected:** `jsr`, `jsr_w`, `ret` including widened forms; legacy subroutines are a stated compatibility limitation. `invokedynamic`, modern dynamic constants/method handles, reserved opcodes, and implementation-specific opcodes are out of scope. If an actual candidate requires a rejected legacy instruction, flag it and propose the necessary scope change.

Implement Java wraparound integer arithmetic without C signed-overflow undefined behavior. Test masked shift counts, negative division/remainder, `MIN_VALUE / -1`, boolean/byte array access, long ordering, null and bounds checks, assignability, array covariance, and field/method lookup across inheritance.

Catchable Java exceptions must unwind frames and use exception tables correctly. Null dereferences, array bounds, bad casts, division by zero, illegal monitors, and native API argument errors must not crash C. Fatal unsupported-feature diagnostics should include JAR/class/method/descriptor, bytecode offset, opcode or missing member, and cause. Define which linkage/runtime errors are represented as Java objects and which terminate this subset runtime.

### 7. Memory management and native integration

Implement stop-the-world, non-moving mark-and-sweep with a free list and coalescing unless a measured reason justifies another simple collector. Use iterative marking or a bounded mark stack with a defined overflow strategy.

Explicit roots include live reference slots in every thread/frame, static fields, pending exceptions, class initialization state, strings retained by the VM, native handle scopes, MIDlet/display objects, event queue entries, and thread/monitor structures. Track reference types explicitly or explain another sound tracing method; never infer references merely because an integer looks like an address.

Native calls must register temporary roots before allocating or invoking Java. Define ownership for decoded pixels, surfaces, archive streams, and other native payloads attached to Java objects. Reclaim these with their owners despite the lack of Java finalizers.

Separate accounting for native runtime memory, class metadata, Java heap, stacks, archive/inflater buffers, images, and display surfaces. Track live bytes, high-water marks, allocation failures, fragmentation, GC count, and pause duration. Define failure behavior that still permits a diagnostic and safe exit under low memory.

Allow only an explicitly registered native bridge keyed by class/name/descriptor. Reject arbitrary native application methods. No JNI loader is required.

### 8. Minimum class library

Implement exact CLDC/MIDP signatures, not desktop Java substitutes. Use native bindings for platform-sensitive operations and Java or C for ordinary logic. If built-ins are represented entirely in C, specify their synthetic class metadata, inheritance, dispatch, and GC fields. Compile-only sample API stubs must not be mistaken for runtime implementations or packaged in the game JAR.

Baseline API groups:

- `java.lang`: `Object`, `Class` limited to required CLDC name/type/resource operations, `String`, `StringBuffer`, `System`, `Runtime` essentials, `Math` integer/long essentials, primitive wrappers used by the library, `Runnable`, `Thread`, `Throwable`/`Exception`/`RuntimeException` and the concrete exception/error types required by implemented operations.
- Strings: UTF-16 code units, constructors needed by samples, length/character access, equality/hash, substring, searching, character extraction, numeric conversion, and concatenation via `StringBuffer`. Define supported external encodings. String literals need stable interning behavior; hash codes cannot depend on moving C addresses.
- `System`: `currentTimeMillis(): long`, overlapping/type-checked `arraycopy`, relevant properties, and a GC request hook. Use a separate monotonic platform clock for scheduling and frame timing.
- `Runtime`: measured `freeMemory`/`totalMemory`, GC request, and controlled exit semantics as supported by the selected CLDC API.
- `java.util`: `Random` with specified Java algorithm, `Vector`, `Hashtable`, `Enumeration`; exact implemented methods in the matrix. Do not replace these with Java SE `ArrayList`/`HashMap` APIs. Timer/TimerTask may be deferred until a target game requires them.
- `java.io`: `InputStream`, `ByteArrayInputStream`, `DataInput`/`DataInputStream`, `IOException`, `EOFException`, and needed encoding errors. Support binary resource reading and exact big-endian integer/long data behavior. Additional output streams are extensions unless needed internally.
- `javax.microedition.midlet`: `MIDlet`, `MIDletStateChangeException`.
- `javax.microedition.lcdui`: `Display`, `Displayable`, `Canvas`, `Graphics`, `Image`, `Font`, plus minimal usable `Command`/`CommandListener` dispatch for exit/pause/soft keys.
- `javax.microedition.lcdui.game`: `GameCanvas`, `Layer`, `Sprite`, `TiledLayer`, `LayerManager`.

At the initial API milestone, publish exact descriptors for the selected methods. Any omitted overload remains explicitly unsupported. Extend according to fixtures and selected games, not guesses about unused desktop APIs.

### 9. MIDlet lifecycle, events, and cooperative threads

Implement create/start/pause/resume/destroy handling and application notifications, including `notifyDestroyed`, `notifyPaused`, `resumeRequest`, and `getAppProperty` as applicable. Distinguish application notifications from callbacks the runtime initiates. Do not block `startApp()` by synchronously running an application thread. Tear down threads, streams, surfaces, and VM allocations when the suite ends.

Use green-thread contexts with RUNNABLE, SLEEPING, WAITING, MONITOR_BLOCKED, and TERMINATED states. Implement `Thread.start`, `run` dispatch, `sleep(long)`, `yield`, `currentThread`, `isAlive`, `join`, and interruption behavior needed by blocking calls. Model priorities honestly; a simplified policy must be documented as a deviation.

Implement reentrant monitors, synchronized methods including class monitors for static methods, `Object.wait` variants, `notify`, and `notifyAll`. Waiting releases and later reacquires the monitor; sleeping does not. Never turn synchronization into a no-op.

Baseline Java thread switching is cooperative: sleep/yield/wait/blocking calls and explicitly documented MIDP service points. A render flush may be a documented scheduling point to accommodate common loops. Keep native operations bounded.

The interpreter must periodically return control to its native supervisor to poll emergency exit/input and inspect timers. This does **not** automatically authorize switching to another Java thread or invoking Java callbacks. A pure Java busy loop may starve other Java threads under this model; diagnose and document it. An optional instruction-quota scheduler would be VM-level preemption and requires an explicit recorded change to this constraint.

Serialize LCDUI callbacks through an event thread/queue. Implement repaint coalescing, `serviceRepaints`, `Display.callSerially`, display changes, and key delivery without recursive paint dispatch or deadlock. Timer interrupts only set flags/timestamps or enqueue safe native signals; they never interpret Java, allocate Java objects, or run GC.

### 10. Graphics, images, and keys

Build MIDP rendering around explicit surfaces and Graphics state. Native gint drawing functions are implementation aids, not a replacement for MIDP semantics.

- Mutable images and GameCanvas need their own retained offscreen surfaces. Preserve clip, translation, color, font, stroke style, and anchors per Graphics object.
- Baseline primitives: fill/draw rectangles, lines, text, images, `drawRegion`, `drawRGB`, and relevant pixel reads. Specify support for arcs, round rectangles, and triangles individually; implement them when declared supported, otherwise diagnose their use.
- Implement Image resource/byte-array loading, blank mutable image creation, copying, RGB creation, dimensions, mutability, and valid Graphics access. Use a runtime PNG decoder: fxconv's build-time conversion cannot decode an arbitrary newly loaded JAR.
- Choose a small dependency with a compatible license for inflate/PNG where practical. Support the controlled sample's PNGs first, then record support for indexed images, tRNS, RGB/RGBA, grayscale, bit depths, filters, and interlace individually. Reject unsupported encodings. Broad MIDP PNG compatibility is a separate milestone from decoding one sample image.
- Preserve full transparency. Specify partial-alpha behavior and report capabilities consistently; never treat transparent pixels as an arbitrary RGB565 key that collides with opaque colors.
- Implement GameCanvas's retained buffer, full/partial flush, visibility, and latched key-state behavior, including key-event suppression. Do not treat `getKeyStates()` as only an instantaneous hardware scan.
- Sprite: frame layout/sequence, position/reference pixel, visibility, all eight transforms, collision rectangle, bounding and pixel-level collision for supported overloads. Do not silently substitute bounding boxes for requested pixel collision.
- TiledLayer: static tiles, empty tile zero, animated tiles, cell operations, clipping and painting. LayerManager: insertion/removal/order, view window, painting. Preserve a common coordinate/transform model.
- Use a virtual game viewport. Default the sample to 176 × 208 centered at 1:1 on the physical display. Make alternate sizes configurable. Fixed 240 × 320 games require explicit fitting/downscaling or clipping with stated usability limits; do not misreport physical dimensions as their original phone canvas.
- Map arrows to directions, a documented key to FIRE, and calculator keys to numeric/star/pound/soft-key actions as feasible. Maintain separate MIDP key codes and game actions. Test simultaneous held keys and ghosting on hardware. Reserve a documented emergency exit combination.

### 11. Builds, commands, and artifacts

Provide exact executable commands and actual scripts, not “build using your preferred tools.” Separate toolchain installation from building this repository. Pin acquisition sources and licenses; if upstream cannot be reached, mark installation instructions unverified rather than inventing them.

For fxSDK scaffolding, inspect a throwaway project created with:

```sh
fxsdk new CGJRE-template
```

Adapt its current CMake conventions. The calculator branch of the root build must have this structure, with a **complete** real source list in the delivered file:

```cmake
cmake_minimum_required(VERSION 3.15)
project(CGJRE LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
option(CGJRE_HOST "Build portable host harness" OFF)

# Define the complete portable source list before these branches.
if(CGJRE_HOST)
  # Define cgjre-host, tests, host backend and CTest registration here.
else()
  include(GenerateG3A)
  include(Fxconv)
  find_package(Gint REQUIRED)
  # Define cgjre from the portable sources and gint backend here.
  target_compile_options(cgjre PRIVATE -Wall -Wextra -Os)
  target_link_libraries(cgjre Gint::Gint)
  generate_g3a(TARGET cgjre OUTPUT "CGJRE.g3a"
    NAME "CGJRE"
    ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
endif()
```

This snippet states the required arrangement, not a complete deliverable: replace its explanatory gaps with working code. Declare fxconv assets only where actually used. Validate `generate_g3a` syntax and output location against the pinned SDK. Link any required compiler/runtime/math dependencies explicitly as needed. Produce a linker map and size report. Supply the icons or a complete deterministic generator.

Required repository command interface:

```sh
# Portable build and semantic tests
cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure

# Build and inspect the controlled JAR
python3 tools/build_sample.py --sample TileBounce --output dist/samples/TileBounce.jar
python3 tools/inspect_jar.py dist/samples/TileBounce.jar

# Deterministic headless integration run
./build-host/cgjre-host --jar dist/samples/TileBounce.jar --headless --frames 300 --seed 1 --report dist/host-report.json

# Calculator build and release packaging
fxsdk build-cg
python3 tools/package_release.py
```

These custom scripts/options are requirements to implement, not pre-existing tools. Make host CMake output the executable at the path shown. Define headless frame/time advancement precisely. The packaging script must find the verified SDK output, fail if missing, and copy it to the following fixed release layout:

- `dist/CGJRE.g3a`
- `dist/samples/TileBounce.jar`
- `dist/samples/TileBounce.jad` if supplied
- `dist/build-info.json`, `dist/size-report.txt`, `dist/host-report.json`
- `dist/README-install.txt` and checksums

Keep compiler intermediates in `build-cg/` and `build-host/`; document the actual linked ELF and map paths. Do not commit fabricated binaries. Explain USB mass-storage transfer, the tested add-in location, game directory, launch selection, and exit behavior. Verify these instructions on the target firmware rather than assuming every Casio model uses the same directory convention.

### 12. Controlled sample and tests

Create an original, redistributable **TileBounce** MIDlet with source and tiny PNG resources. It must use the standard selected MIDP APIs, not private VM intrinsics. Include:

- A 176 × 208 GameCanvas with a game thread paced by `Thread.sleep`.
- Directional movement, one fire action, a Sprite with multiple frames, a TiledLayer including an animated tile, and a LayerManager.
- Collision, score text, a resource read through DataInputStream, manifest property lookup, pause/resume, and clean destroy.
- Fixed test seed/time controls supplied by the host platform for deterministic test execution, without changing the JAR's API dependencies.

Pin a compiler capable of producing the accepted old class format and a legal CLDC/MIDP compile-time API source. Verify output with `javap -verbose` or the independent inspector. Modern `javac` defaults are not acceptable; do not claim a modern compiler supports obsolete `-target` values without checking. If using a legacy JDK or suitable ECJ version, give exact version, commands, dependencies, and a reproducible acquisition procedure. Apply CLDC preverification where required by the chosen reference runtime and preserve its output in packaging.

Prove the same JAR runs in a named, pinned independent Java ME runtime/emulator and record its SHA-256 plus observations. Only then label that artifact **known-good**. Until independent execution occurs, call it a candidate fixture. Host execution on the new VM alone is insufficient evidence of sample correctness. An optional existing open-source game may supplement this fixture after license, source, exact binary, and dependencies are checked; never redistribute an unlicensed commercial game.

Test plan:

1. Parser tests: truncated files, invalid indices/descriptors, duplicate/oversized archive entries, CRC failures, malformed DEFLATE, modified UTF-8, two-slot constants, exception tables, unsupported versions/tags/opcodes, checked arithmetic overflow in lengths.
2. Interpreter tests: each supported instruction family and stack category form, numerical edge cases, switches, wide operands, polymorphism/interfaces, constructors, static initialization including failure, exceptions, array typing, null/bounds checks. Use independent expected results or differential tests where valid.
3. GC/native tests: forced collection during native calls and callbacks, cyclic objects, dead image buffers, roots across threads, low-memory failure, repeated launches, and fragmentation. Use host sanitizers where available.
4. Scheduler tests: sleeping thread plus UI events, wait/notify ownership and reacquisition, interruption, synchronized exceptions, join, busy-loop starvation diagnostics, emergency exit, and no Java execution inside interrupts.
5. Rendering tests: clipping/translation/anchors, mutable-image targets, transparency, all transforms, sprite collision, animated tiles, layer order, partial flushes, and key-state latching. Compare independent expected pixels or carefully reviewed golden images.
6. Integration: external stored and compressed JAR variants; deterministic host report; reference runtime behavior; then the identical game binary on the calculator.
7. Hardware: record model, OS, toolchain, clock configuration, safe RAM budget, heap/image/metadata peaks, frame-time distribution, GC pauses, file behavior, pause/resume, key combinations, repeated restart, and safe return to the OS.

Use 15–20 fps for TileBounce at default clock as an initial performance goal, not an asserted guarantee. Report actual measured results. If it is missed, profile dispatch, decompression, drawing, and display transfer before considering optimizations. No mandatory overclocking.

### 13. Deliver milestones in dependency order

| Milestone | Concrete deliverable | Gate |
|---|---|---|
| M0 | Toolchain inventory, pinned spec, constraints, memory plan, empty host build, genuine display/key/exit `.g3a` | Host builds; calculator build verified or clearly blocked |
| M1 | Bounded ZIP/manifest/class parser and JAR inspector | Valid fixtures parsed; malformed fixtures rejected |
| M2 | Integer/long VM, objects, linkage, exceptions, initial built-ins | Semantic fixtures execute correctly on host |
| M3 | GC, native roots, cooperative threads, monitors, lifecycle/events | Stress and scheduling tests pass |
| M4 | Surfaces, PNG subset, Canvas/GameCanvas, input and class-library coverage | Same simple MIDlet works through the shared host core |
| M5 | Sprite/tile/layer behavior, complete TileBounce, independent reference validation | Known-good JAR and deterministic integration report |
| M6 | Calculator integration, external loading, measured memory/performance, release packaging | Hardware acceptance or explicit outstanding hardware checklist |
| M7 | Extended float/double profile and evidence-driven compatibility additions | Updated profile tests and per-game compatibility records |

Port and compile incrementally; do not wait until M6 to discover that the VM core cannot link for SH. M0–M6 deliver the baseline; M7 is separately tracked. Do not declare a milestone complete merely because its interfaces exist.

### 14. Required output format and truthful progress

For every turn:

1. Read this spec, `PROJECT_STATE.md`, relevant decisions, and the actual repository state.
2. State the current milestone, concrete result to achieve this turn, and any changed assumption.
3. Implement a coherent slice with complete code and necessary assets/scripts. Favor a compiling boundary over emitting disconnected pieces of every module.
4. Run available builds/tests and report exact commands, exit results, and material failures. Distinguish NOT RUN, BLOCKED, FAIL, and PASS. Never claim compilation, emulator execution, or hardware testing without evidence.
5. Update `PROJECT_STATE.md`, matrices, and relevant documentation.

When replying with code in chat, delimit every new or changed text file as follows, using the entire actual contents with no omissions:

````text
===== BEGIN FILE: relative/path/file.c =====
```c
/* Complete real file contents go here. */
```
===== END FILE: relative/path/file.c =====
````

The example above illustrates formatting only. Your actual files must contain complete executable implementations. No pseudocode, ellipses, “same as before,” fake methods, or `TODO: implement` inside claimed-complete features. Future features may be absent or have explicit failing boundaries tracked as unsupported. If using workspace tools, write the actual files and give their paths; provide full-file text when file content is requested. For binary assets, provide real files or deterministic generation scripts with all inputs included.

If the turn cannot fit an entire milestone, complete a smaller valid slice, state what remains, and supply the next action. Do not claim the overall project is done because one turn ended. Do not restart or redesign working modules without a concrete reason and a recorded decision.

If a requirement is infeasible or contradictory, create a blocker containing: requirement, evidence, practical impact, alternatives, recommendation, and work that can continue. Never silently drop it or make required methods return fabricated success. Seek a scope decision only when a material tradeoff truly requires one.

### 15. Persistent state and continuation protocol

Keep `SPEC.md` as the stable requirements source. Record agreed changes in `docs/decisions.md` with date, reason, compatibility effect, and affected tests. `PROJECT_STATE.md` must include:

- Current milestone and completed acceptance criteria.
- Implemented vs pending opcode/API coverage.
- Toolchain/dependency versions and source provenance.
- Exact last build/test commands and outcomes.
- Known bugs, blockers, technical uncertainties, and unsupported behaviors.
- Memory/performance measurements, clearly separated from estimates.
- Artifact paths and checksums for tested JARs/builds.
- The next smallest actionable task and its acceptance test.

A subsequent user turn may simply say:

> Continue from SPEC.md and PROJECT_STATE.md. Inspect the existing files, implement the next coherent slice, run the applicable checks, and update state. Preserve the target, scope, and honest verification rules. Do not repeat completed work or claim unrun checks passed.

**Begin now with M0:** inspect available tools, identify the most consequential unresolved platform assumptions, create the project scaffold and state files, implement the native display/key/exit smoke test plus portable host test entry point, and run the builds available in your environment. End with concrete results and the exact next task.

## Master prompt ends
