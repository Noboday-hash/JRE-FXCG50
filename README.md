# CGJRE

CGJRE is an in-progress CLDC/MIDP subset compatibility runtime for the Casio fx-CG50. The current M0 scaffold only displays a smoke screen and exits; it cannot load JARs.

## Host smoke build

```sh
cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
./build-host/cgjre-host --smoke
```

## Calculator smoke build

With the installed fxSDK, gint, and `sh-elf-gcc` configured:

```sh
fxsdk build-cg
```

The generated add-in is `CGJRE.g3a` at the repository root. The linked ELF and map are in `build-cg/`. On an fx-CG50, the smoke screen should appear and EXIT should return to the OS. Hardware execution has not yet been verified.

See [SPEC.md](SPEC.md), [PROJECT_STATE.md](PROJECT_STATE.md), and [docs/toolchain.md](docs/toolchain.md) for scope and current evidence.
