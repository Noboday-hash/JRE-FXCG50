# Compatibility results

The user supplied two old-phone MIDlet JARs as **local-only** compatibility probes. They are ignored by Git and are not release inputs. Static inspection does not show which references execute, and neither game has run on CGJRE or an independent emulator. Neither is a known-good fixture.

| Local JAR | SHA-256 | Archive and class evidence | Static result |
|---|---|---|---|
| `samples/farm2.jar` | `d4632cbb421c9fdefc0638a693258c7e4d1eb39e53486d4ad0cedf2f200dc129` | 558,357 bytes; 215 DEFLATE entries; 42 classes, all 45.3; manifest CLDC-1.0/MIDP-2.0, MIDlet `com.spl.j2me.Game.fApplication` | Full archive/class inspection passes. References LCDUI/GameCanvas, `Vector`, `DataInputStream`, RMS, and media Player APIs. |
| `samples/gish.jar` | `f904f96303b1c1055613f2dcb36a6edb159178098f839081226e5698e12938a1` | 449,614 bytes; 193 entries (172 DEFLATE, 21 stored); 60 classes (32 version 45.3, 28 version 46.0); manifest CLDC-1.0/MIDP-2.0, MIDlet `com.hardwire.blob.Main` | Full archive/class inspection passes after explicit 46.0 support. References Nokia UI, Bluetooth, RMS, media Player, and LCDUI APIs. |

Farm Frenzy 2 is the first compatibility target by user choice. Its static opcode counts are dominated by reference loads, field access, virtual calls, and arrays. The VM now executes a bounded primitive-array subset in static integer fixtures; object fields, reference arrays, and virtual calls remain unsupported. Exact RMS and media descriptors referenced by this JAR are tracked as planned extensions in `docs/api-matrix.csv`; no save or media behavior is implemented. Gish remains a secondary probe. Its Bluetooth permission and references may be optional; no execution path has been established.

TileBounce is not yet a candidate fixture. The selected Wireless Toolkit emulator has not been installed or run.
