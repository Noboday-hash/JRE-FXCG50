# Architecture at M0

`src/smoke.c` is portable C11. It calls the injected `cgjre_platform` interface from `include/cgjre/platform.h`. The host and gint entry points provide the interface. This is only a display/key/exit smoke boundary; VM and MIDP interfaces will be designed against actual fixture needs in later milestones.

Planned ownership: `src/archive` reads JARs; `src/vm` parses and executes classes; `src/classlib` implements built-ins; `src/midp` owns lifecycle and rendering; `src/platform/gint` and `src/platform/host` provide hardware services. Empty directories mark planned components, not implemented ones.
