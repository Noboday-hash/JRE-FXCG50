# Decisions

## 2026-09-27 — Scaffold boundary

Reason: the requested first deliverable is a reviewable M0 structure. Compatibility effect: none; JAR loading is explicitly unsupported. Tests: host smoke CTest and calculator build; hardware execution pending.

## 2026-09-27 — Raw DEFLATE dependency

Reason: the SH sysroot has no target zlib library, and M1 needs bounded extraction of ordinary compressed JARs. Use the low-level tinfl inflater from miniz commit `77d0dce8627735138c51770d1799a1ef48f2117d` under MIT. Compatibility effect: raw DEFLATE ZIP entries and data-descriptor archives can be read. Tests: compressed manifest, multi-chunk input, malformed stream, CRC, host sanitizer run with leak detection disabled, calculator cross-build. Hardware execution pending.
