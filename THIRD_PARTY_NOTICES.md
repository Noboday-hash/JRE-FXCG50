# Third-party notices

The two CG50 icon PNGs in `assets-cg/` came from the locally installed fxSDK 2.11.0 project template. Their provenance is recorded in `docs/toolchain.md`.

`third_party/miniz/` contains upstream miniz tinfl source and headers from commit `77d0dce8627735138c51770d1799a1ef48f2117d` of [richgel999/miniz](https://github.com/richgel999/miniz). `miniz_export.h` was added locally as an empty export macro for this static build. Upstream's MIT license is included at `third_party/miniz/LICENSE`. The build disables its ZIP, compressor, stdio, time, and malloc APIs, and uses only the low-level raw DEFLATE inflater.
