# Memory budget — estimates only

Physical RAM is 8 MiB; usable runtime memory depends on OS and gint configuration. A 396 × 224 RGB565 framebuffer is 177,408 bytes. A possible initial Java heap is 256 KiB only if measured available memory permits it. No Java heap, image surface, or extended memory region is allocated by the M0 smoke test.

Before M1/M2 allocations, record actual calculator model, OS, gint configuration, safe free regions, stack and VRAM placement, and failure behavior. The installed gint `config.h` documents use of part of the OS stack when `GINT_NO_OS_STACK` is unset; this is not a measured CGJRE memory allowance.

M1 archive limits are 1,024 entries, 1 MiB central-directory metadata, 4 MiB inflated bytes per entry, and 1 MiB per class file. The reader allocates a maximum 65,557-byte EOCD tail, entry metadata, one extracted resource, a 4 KiB DEFLATE input buffer, and the miniz state. These are hard limits and possible peak allocations, not a claim that the calculator can reserve all of them. Failed allocations return explicit errors. A lower measured calculator budget must be configured before game loading is enabled.

The M2 fixture interpreter caps a frame at 4,096 locals and 4,096 operand slots and allocates an instruction-boundary byte per method byte. It has a 32-frame container but currently activates only one frame. These limits are not a Java heap budget; no objects or GC exist yet.
