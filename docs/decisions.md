# Decisions

## 2026-09-27 — Scaffold boundary

Reason: the requested first deliverable is a reviewable M0 structure. Compatibility effect: none; JAR loading is explicitly unsupported. Tests: host smoke CTest and calculator build; hardware execution pending.

## 2026-09-27 — Raw DEFLATE dependency

Reason: the SH sysroot has no target zlib library, and M1 needs bounded extraction of ordinary compressed JARs. Use the low-level tinfl inflater from miniz commit `77d0dce8627735138c51770d1799a1ef48f2117d` under MIT. Compatibility effect: raw DEFLATE ZIP entries and data-descriptor archives can be read. Tests: compressed manifest, multi-chunk input, malformed stream, CRC, host sanitizer run with leak detection disabled, calculator cross-build. Hardware execution pending.

## 2026-09-27 — Toolchain decisions

Use [Eclipse ECJ 3.26.0](https://repo1.maven.org/maven2/org/eclipse/jdt/ecj/3.26.0/) for TileBounce compilation, targeting class format 45.3; [ECJ documents a 1.1/45.3 target](https://help.eclipse.org/latest/topic/org.eclipse.jdt.doc.user/tasks/task-using_batch_compiler.htm). Use the CLDC 1.1 and MIDP 2.0 compile-time API archives, preverifier, and Linux emulator from [Sun Java Wireless Toolkit 2.5.2_01](https://www.oracle.com/java/technologies/sun-java-wireless-toolkit.html). This pins one matching API and reference-emulator release and avoids compiling against Java SE classes. The toolkit is licensed separately and will not be redistributed. Installation, class output, preverification, and emulator execution remain unverified locally.
