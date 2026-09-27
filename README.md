# CGJRE

CGJRE is an in-progress CLDC/MIDP subset compatibility runtime for the Casio fx-CG50, aimed at ordinary old-phone MIDlet JARs. The calculator add-in currently displays a smoke screen and exits; it cannot launch JARs. The shared core reads bounded stored and DEFLATE entries and class format 45.3/46.0 metadata. Farm Frenzy 2 is the first local compatibility probe; the supplied game JARs stay local-only and are not committed.

## Host smoke build

```sh
cmake -S . -B build-host -DCGJRE_HOST=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
./build-host/cgjre-host --smoke
./build-host/cgjre-host --inspect path/to/file.jar
python3 tools/inspect_jar.py path/to/file.jar
./build-host/cgjre-host --eval-class path/to/Arithmetic.class test '(I)I' 41
./build-host/cgjre-host --eval-jar path/to/TwoClass.jar Caller test '(I)I' 41
```

## Calculator smoke build

With the installed fxSDK, gint, and `sh-elf-gcc` configured:

```sh
fxsdk build-cg
```

The generated add-in is `CGJRE.g3a` at the repository root. The linked ELF and map are in `build-cg/`. On an fx-CG50, the smoke screen should appear and EXIT should return to the OS. Hardware execution has not yet been verified. The host inspector reports ZIP sizes, manifest attributes, class versions, opcode usage, native methods, and referenced members. Static inspection cannot prove game compatibility.

`--eval-class` runs a limited static integer method directly from a 45.3 class file for M2 semantic tests. It does not initialize or launch a MIDlet. See `PROJECT_STATE.md` for its supported scope.
`--eval-jar` runs the same limited method from an external JAR and can resolve static integer calls to other classes in that JAR. The fixture VM also supports bounded primitive arrays and simple constructed objects with integer/reference fields. These commands do not launch a MIDlet.

See [SPEC.md](SPEC.md), [PROJECT_STATE.md](PROJECT_STATE.md), and [docs/toolchain.md](docs/toolchain.md) for scope and current evidence.
