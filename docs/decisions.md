# Decisions

## 2026-09-27 — Scaffold boundary

Reason: the requested first deliverable is a reviewable M0 structure. Compatibility effect: none; JAR loading is explicitly unsupported. Tests: host smoke CTest and calculator build; hardware execution pending.

## 2026-09-27 — Raw DEFLATE dependency

Reason: the SH sysroot has no target zlib library, and M1 needs bounded extraction of ordinary compressed JARs. Use the low-level tinfl inflater from miniz commit `77d0dce8627735138c51770d1799a1ef48f2117d` under MIT. Compatibility effect: raw DEFLATE ZIP entries and data-descriptor archives can be read. Tests: compressed manifest, multi-chunk input, malformed stream, CRC, host sanitizer run with leak detection disabled, calculator cross-build. Hardware execution pending.

## 2026-09-27 — Toolchain decisions

Use [Eclipse ECJ 3.26.0](https://repo1.maven.org/maven2/org/eclipse/jdt/ecj/3.26.0/) for TileBounce compilation, targeting class format 45.3; [ECJ documents a 1.1/45.3 target](https://help.eclipse.org/latest/topic/org.eclipse.jdt.doc.user/tasks/task-using_batch_compiler.htm). Use the CLDC 1.1 and MIDP 2.0 compile-time API archives, preverifier, and Linux emulator from [Sun Java Wireless Toolkit 2.5.2_01](https://www.oracle.com/java/technologies/sun-java-wireless-toolkit.html). This pins one matching API and reference-emulator release and avoids compiling against Java SE classes. The toolkit is licensed separately and will not be redistributed. ECJ produced a 45.3 Java SE fixture locally; toolkit installation, sample output against its APIs, preverification, and emulator execution remain unverified.

## 2026-09-27 — Integer VM slot model

Use 32-bit value bits plus an explicit kind per Java slot and bounded explicit frames. Reserve zero as the future null reference handle and adjacent tagged high/low slots for category-2 values. This prevents host pointer truncation and makes reference roots identifiable for later GC. The first executable slice accepts only static integer methods without class initialization, monitors, or exception tables; tests cover wraparound, signed division, branches, local slots, malformed code, and explicit unsupported results. Long values, cross-class and virtual calls, objects, and Java exception delivery remain pending.

## 2026-09-27 — Class format 46.0

Gish Reloaded includes 28 class files with version 46.0. [Oracle's class-file version table](https://docs.oracle.com/javase/specs/jvms/se10/html/jvms-4.html) identifies 46.0 as Java 1.2 format. Accept exact 46.0 alongside 45.3 after a dedicated fixture and full static inspection of the local Gish JAR; retain rejection of other versions. This changes parsing only, not execution coverage or the CLDC verification claim.

## 2026-09-27 — Farm Frenzy 2 compatibility extensions

Use Farm Frenzy 2 as the first user-supplied old-phone MIDlet compatibility target. It statically references RMS saves and media Player APIs. With user approval, plan a small file-backed RMS subset and a silent media compatibility layer with no audio playback, using the exact referenced descriptors in `docs/api-matrix.csv`. These are named extensions to the original baseline scope, not implemented features yet. Test record persistence and media state/error behavior against the selected reference emulator before claiming game compatibility. The supplied game JARs remain local-only and must not be committed or redistributed.
