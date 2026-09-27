# Tests

CTest runs the M0 smoke check and Python-generated ZIP, manifest, and class fixtures through the host executable. Archive fixtures cover stored and raw DEFLATE extraction, multi-chunk input, data descriptors, CRC mismatch, malformed streams, truncated files, ZIP64, encryption, duplicate names, invalid offsets/local headers, and manifest continuation/errors. Class fixtures cover class format 45.3, old-version policy, descriptor and modified UTF-8 failures, truncated Code, exception ranges, unsupported tags, and two-slot long constants. Semantic, scheduler, GC, and rendering tests begin with the related implementation milestones.

The class fixtures also cover SourceFile and line metadata, opcode counting, switches, and malformed instruction lengths. Tests use independently constructed ZIP and class bytes rather than a game JAR. Hardware tests and an independent Java ME emulator run remain pending.
