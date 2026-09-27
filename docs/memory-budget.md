# Memory budget — M0 estimates only

Physical RAM is 8 MiB; usable runtime memory depends on OS and gint configuration. A 396 × 224 RGB565 framebuffer is 177,408 bytes. A possible initial Java heap is 256 KiB only if measured available memory permits it. No Java heap, image surface, or extended memory region is allocated by the M0 smoke test.

Before M1/M2 allocations, record actual calculator model, OS, gint configuration, safe free regions, stack and VRAM placement, and failure behavior. The installed gint `config.h` documents use of part of the OS stack when `GINT_NO_OS_STACK` is unset; this is not a measured CGJRE memory allowance.
