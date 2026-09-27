# Architecture at M0

`src/smoke.c` is portable C11. It calls the injected `cgjre_platform` interface from `include/cgjre/platform.h`. The host and gint entry points provide the interface. This is only a display/key/exit smoke boundary; VM and MIDP interfaces will be designed against actual fixture needs in later milestones.

Planned ownership: `src/archive` reads JARs; `src/vm` parses and executes classes; `src/classlib` implements built-ins; `src/midp` owns lifecycle and rendering; `src/platform/gint` and `src/platform/host` provide hardware services. Empty directories mark planned components, not implemented ones.

M1 now has `src/archive/zip.c` and `manifest.c`. The ZIP reader receives a random-access read callback plus the known file size. It scans the bounded EOCD tail, indexes bounded central-directory metadata, checks local headers and payload extents, and allocates only index metadata or one requested stored entry. The host file adapter is outside the portable archive code. DEFLATE extraction is an explicit unsupported result until an inflater is selected and integrated.
