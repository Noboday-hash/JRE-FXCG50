"""Black-box ZIP indexing and stored-entry checks for the shared C reader."""

import io
import pathlib
import struct
import subprocess
import sys
import tempfile
import zipfile


def make_zip(compression):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", compression=compression) as archive:
        archive.writestr("META-INF/MANIFEST.MF", "Manifest-Version: 1.0\r\n")
        archive.writestr("game/Level.dat", b"\x00\x01\x02")
    return buffer.getvalue()


def manifest_zip(contents):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_STORED) as archive:
        archive.writestr("META-INF/MANIFEST.MF", contents)
    return buffer.getvalue()


def check(executable, folder, label, data, expected_success, message=None):
    path = folder / (label + ".jar")
    path.write_bytes(data)
    result = subprocess.run([executable, "--inspect", str(path)],
                            capture_output=True, text=True, check=False)
    if (result.returncode == 0) != expected_success:
        raise AssertionError(f"{label}: exit {result.returncode}: {result.stderr}")
    if message and message not in result.stdout + result.stderr:
        raise AssertionError(f"{label}: missing {message!r}: {result.stderr}")


def main(executable):
    stored = make_zip(zipfile.ZIP_STORED)
    deflated = make_zip(zipfile.ZIP_DEFLATED)
    central = stored.index(b"PK\x01\x02")
    eocd = stored.rindex(b"PK\x05\x06")
    with tempfile.TemporaryDirectory() as temporary:
        folder = pathlib.Path(temporary)
        check(executable, folder, "stored", stored, True, "Manifest bytes: 23")
        check(executable, folder, "deflated", deflated, False,
              "unsupported ZIP feature")
        check(executable, folder, "continuation",
              manifest_zip("MIDlet-1: TileBounce, /icon.png, game.\r\n Main\r\n\r\n"),
              True, "MIDlet-1: TileBounce, /icon.png, game.Main")
        check(executable, folder, "bad_manifest",
              manifest_zip(" Broken continuation\n"), False,
              "malformed manifest")
        check(executable, folder, "truncated", stored[:-4], False,
              "malformed ZIP")
        check(executable, folder, "short", b"PK", False, "malformed ZIP")
        encrypted = bytearray(stored)
        encrypted[central + 8] |= 1
        check(executable, folder, "encrypted", encrypted, False,
              "unsupported ZIP feature")
        zip64 = bytearray(stored)
        struct.pack_into("<H", zip64, eocd + 10, 0xFFFF)
        check(executable, folder, "zip64", zip64, False,
              "unsupported ZIP feature")
        out_of_bounds = bytearray(stored)
        struct.pack_into("<I", out_of_bounds, eocd + 16, 0xFFFFFF00)
        check(executable, folder, "offset", out_of_bounds, False,
              "malformed ZIP")
        bad_local = bytearray(stored)
        struct.pack_into("<I", bad_local, central + 42, 0xFFFFFF00)
        check(executable, folder, "local_offset", bad_local, False,
              "malformed ZIP")
        bad_payload_size = bytearray(stored)
        struct.pack_into("<I", bad_payload_size, central + 20, 0xFFFFF)
        check(executable, folder, "payload_size", bad_payload_size, False,
              "malformed ZIP")
        bad_local_name = bytearray(stored)
        bad_local_name[30] = ord("X")
        check(executable, folder, "local_name", bad_local_name, False,
              "malformed ZIP")
        bad_crc = bytearray(stored)
        struct.pack_into("<I", bad_crc, central + 16, 0)
        check(executable, folder, "crc", bad_crc, False, "CRC mismatch")
        duplicate = io.BytesIO()
        with zipfile.ZipFile(duplicate, "w") as archive:
            import warnings
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", UserWarning)
                archive.writestr("same.txt", "one")
                archive.writestr("same.txt", "two")
        check(executable, folder, "duplicate", duplicate.getvalue(), False,
              "malformed ZIP")
        traversal = io.BytesIO()
        with zipfile.ZipFile(traversal, "w") as archive:
            archive.writestr("../escape.txt", "bad")
        check(executable, folder, "traversal", traversal.getvalue(), False,
              "malformed ZIP")
        directory = io.BytesIO()
        with zipfile.ZipFile(directory, "w") as archive:
            archive.writestr("game/", "")
            archive.writestr("game/Main.class", b"test")
        check(executable, folder, "directory", directory.getvalue(), True,
              "ZIP entries: 2")
    print("archive fixtures passed")


if __name__ == "__main__":
    main(sys.argv[1])
