# Decisions

## 2026-09-27 — Scaffold boundary

Reason: the requested first deliverable is a reviewable M0 structure. Compatibility effect: none; JAR loading is explicitly unsupported. Tests: host smoke CTest and calculator build; hardware execution pending.

## 2026-09-27 — Raw DEFLATE dependency

Reason: the SH sysroot has no target zlib library, and M1 needs bounded extraction of ordinary compressed JARs. Use the low-level tinfl inflater from miniz commit `77d0dce8627735138c51770d1799a1ef48f2117d` under MIT. Compatibility effect: raw DEFLATE ZIP entries and data-descriptor archives can be read. Tests: compressed manifest, multi-chunk input, malformed stream, CRC, host sanitizer run with leak detection disabled, calculator cross-build. Hardware execution pending.

## 2026-09-27 — Toolchain decisions

Use [Eclipse ECJ 3.26.0](https://repo1.maven.org/maven2/org/eclipse/jdt/ecj/3.26.0/) for TileBounce compilation, targeting class format 45.3; [ECJ documents a 1.1/45.3 target](https://help.eclipse.org/latest/topic/org.eclipse.jdt.doc.user/tasks/task-using_batch_compiler.htm). Use the CLDC 1.1 and MIDP 2.0 compile-time API archives, preverifier, and Linux emulator from [Sun Java Wireless Toolkit 2.5.2_01](https://www.oracle.com/java/technologies/sun-java-wireless-toolkit.html). This pins one matching API and reference-emulator release and avoids compiling against Java SE classes. The toolkit is licensed separately and will not be redistributed. ECJ produced a 45.3 Java SE fixture locally; toolkit installation, sample output against its APIs, preverification, and emulator execution remain unverified.

## 2026-09-27 — Integer VM slot model

Use 32-bit value bits plus an explicit kind per Java slot and bounded explicit frames. Reserve zero as the future null reference handle and adjacent tagged high/low slots for category-2 values. This prevents host pointer truncation and makes reference roots identifiable for later GC. The first executable slice accepts only static integer methods without class initialization, monitors, or exception tables; tests cover wraparound, signed division, branches, local slots, malformed code, and explicit unsupported results. Long values, virtual calls, objects, and Java exception delivery remain pending.

## 2026-09-27 — Class format 46.0

Gish Reloaded includes 28 class files with version 46.0. [Oracle's class-file version table](https://docs.oracle.com/javase/specs/jvms/se10/html/jvms-4.html) identifies 46.0 as Java 1.2 format. Accept exact 46.0 alongside 45.3 after a dedicated fixture and full static inspection of the local Gish JAR; retain rejection of other versions. This changes parsing only, not execution coverage or the CLDC verification claim.

## 2026-09-27 — Farm Frenzy 2 compatibility extensions

Use Farm Frenzy 2 as the first user-supplied old-phone MIDlet compatibility target. It statically references RMS saves and media Player APIs. With user approval, plan a small file-backed RMS subset and a silent media compatibility layer with no audio playback, using the exact referenced descriptors in `docs/api-matrix.csv`. These are named extensions to the original baseline scope, not implemented features yet. Test record persistence and media state/error behavior against the selected reference emulator before claiming game compatibility. The supplied game JARs remain local-only and must not be committed or redistributed.

## 2026-09-27 — JAR class ownership and resolution

Use an on-demand class repository backed by the bounded ZIP reader. It owns class bytes and metadata for the suite lifetime, caches up to 128 classes, validates requested internal names, and rejects application classes in `java/` or `javax/`. The VM resolves cross-class `invokestatic` through an injected callback, preserving the shared host/calculator core. Compatibility effect: static integer calls across ordinary external JAR classes can execute; initialization, inherited lookup, objects, and MIDlet launch remain unsupported. Tests: stored and compressed two-class JARs, repeated resolution, missing/mismatched/protected classes, size limit, and a real ECJ-built two-class JAR.

## 2026-09-27 — Primitive-array handle slice

Use numbered 32-bit handles for VM arrays, with null at zero and explicit reference slot tags. Bound the temporary fixture heap to 255 handles, 16,384 elements per array, and 256 KiB of payloads; release allocations when the run ends. This is a portable stepping stone for object handles and GC, not a measured calculator heap. Compatibility effect: boolean, byte, char, short, and int arrays, reference locals, reference comparison, and null/bounds/negative-size diagnostics work within static integer methods. Java exception objects, array reference covariance, long/float arrays, and GC remain pending. Tests: independent class and JAR fixtures, real ECJ int-array method, sanitizer and SH builds.

## 2026-09-27 — Direct Object instance slice

Extend the handle table to instances of classes directly extending `java/lang/Object`, with one tagged slot per supported integer or reference field. Track uninitialized aliases until a matching constructor completes; the built-in `Object.<init>()V` only marks the superclass call. This follows the [old-format constructor constraint](https://docs.oracle.com/javase/specs/jvms/se6/html/ClassFile.doc.html) without claiming full verification. Compatibility effect: `new`, `invokespecial` constructors, `getfield`, `putfield`, and void constructor return work for this subset. Inheritance, virtual dispatch, category-2 fields, and GC remain pending. Tests: independent object JAR fixtures and an ECJ 45.3 `Box` constructor/field program, plus sanitizer and SH builds.

## 2026-09-27 — Inheritance and virtual dispatch slice

Load class chains on demand with a 16-class limit, lay out base fields before derived fields, and retain owner-specific slots when names are hidden. Complete each constructor's direct-super call before promoting its `this` reference. Add integer-return `invokevirtual` with symbolic owner resolution and most-derived override selection, plus concrete reference arrays and supported class/array casts. This addresses high-frequency field, call, and array bytecodes seen in the local Farm Frenzy 2 JAR without claiming game execution. Tests: independent hierarchy/error fixtures, an ECJ 45.3 `Base`/`Child`/`Base[]` JAR, sanitizer and SH builds. Interface dispatch, class initialization, full assignability, and GC remain pending.
