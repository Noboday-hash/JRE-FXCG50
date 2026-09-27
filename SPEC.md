# CGJRE specification

`project.md` is the full project brief and authoritative requirements source. This file is the stable entry point for continuation. The target is an fx-CG50 CLDC/MIDP subset compatibility runtime that runs selected external JARs without conversion. The milestone order and success criteria are in `project.md` sections 1–15.

Current work is M0 only. The scaffold includes the required component directories, host entry point, and calculator display/key/exit smoke test. Archive loading, VM semantics, class libraries, MIDP rendering, sample compilation, and release packaging are pending.

Any agreed change to the brief belongs in `docs/decisions.md`.
