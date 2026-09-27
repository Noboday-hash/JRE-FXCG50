# CGJRE

CGJRE is an in-progress CLDC/MIDP subset compatibility runtime for the Casio fx-CG50. The calculator add-in currently displays a smoke screen and exits; it cannot launch JARs. The shared M1 archive code can index ZIP files and extract stored entries.

## Host smoke build

```sh
cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
./build-host/cgjre-host --smoke
./build-host/cgjre-host --inspect path/to/file.jar
```

## Calculator smoke build

With the installed fxSDK, gint, and `sh-elf-gcc` configured:

```sh
fxsdk build-cg
```

The generated add-in is `CGJRE.g3a` at the repository root. The linked ELF and map are in `build-cg/`. On an fx-CG50, the smoke screen should appear and EXIT should return to the OS. Hardware execution has not yet been verified. The host `--inspect` command only reports ZIP entries and a stored manifest; it does not assess game compatibility. DEFLATE entries are indexed but extraction is pending.

See [SPEC.md](SPEC.md), [PROJECT_STATE.md](PROJECT_STATE.md), and [docs/toolchain.md](docs/toolchain.md) for scope and current evidence.
