"""External two-class JAR fixtures for shared class loading and static calls."""

import io
import pathlib
import struct
import subprocess
import sys
import tempfile
import zipfile


def u2(value):
    return struct.pack(">H", value)


def u4(value):
    return struct.pack(">I", value)


def utf8(text):
    data = text.encode("ascii")
    return b"\x01" + u2(len(data)) + data


def method(name_index, descriptor_index, code):
    body = u2(2) + u2(0) + u4(len(code)) + code + u2(0) + u2(0)
    return (u2(0x0009) + u2(name_index) + u2(descriptor_index) +
            u2(1) + u2(7) + u4(len(body)) + body)


def klass(name, pool, methods, major=45, minor=3):
    return (u4(0xCAFEBABE) + u2(minor) + u2(major) +
            u2(len(pool) + 1) + b"".join(pool) +
            u2(0x0021) + u2(2) + u2(4) + u2(0) + u2(0) +
            u2(len(methods)) + b"".join(methods) + u2(0))


def caller(owner="Helper", twice=False):
    pool = [utf8("Caller"), b"\x07" + u2(1),
            utf8("java/lang/Object"), b"\x07" + u2(3),
            utf8("test"), utf8("()I"), utf8("Code"),
            utf8(owner), b"\x07" + u2(8), utf8("value"),
            b"\x0c" + u2(10) + u2(6),
            b"\x0a" + u2(9) + u2(11)]
    code = b"\xb8\x00\x0c"
    if twice:
        code += b"\xb8\x00\x0c\x60"
    code += b"\xac"
    return klass("Caller", pool, [method(5, 6, code)])


def helper(name="Helper", method_name="value", clinit=False, major=45,
           minor=3):
    pool = [utf8(name), b"\x07" + u2(1),
            utf8("java/lang/Object"), b"\x07" + u2(3),
            utf8(method_name), utf8("()I"), utf8("Code")]
    methods = [method(5, 6, b"\x08\xac")]
    if clinit:
        pool.extend([utf8("<clinit>"), utf8("()V")])
        methods.append(method(8, 9, b"\xb1"))
    return klass(name, pool, methods, major, minor)


def jar(entries, compression):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", compression=compression) as archive:
        archive.writestr("META-INF/MANIFEST.MF", "Manifest-Version: 1.0\r\n")
        for name, data in entries.items():
            archive.writestr(name, data)
    return buffer.getvalue()


def check(executable, folder, label, entries, expected,
          compression=zipfile.ZIP_DEFLATED, root="Caller"):
    path = folder / (label + ".jar")
    path.write_bytes(jar(entries, compression))
    run = subprocess.run([executable, "--eval-jar", str(path), root,
                          "test", "()I"], capture_output=True, text=True,
                         check=False)
    if isinstance(expected, int):
        if run.returncode != 0 or f"result={expected} " not in run.stdout:
            raise AssertionError(f"{label}: {run.returncode}\n{run.stdout}\n{run.stderr}")
    elif run.returncode == 0 or expected not in run.stderr:
        raise AssertionError(f"{label}: {run.returncode}\n{run.stdout}\n{run.stderr}")


def check_raw(executable, folder, label, data, expected):
    path = folder / (label + ".jar")
    path.write_bytes(data)
    run = subprocess.run([executable, "--eval-jar", str(path),
                          "Caller", "test", "()I"], capture_output=True,
                         text=True, check=False)
    if run.returncode == 0 or expected not in run.stderr:
        raise AssertionError(f"{label}: {run.returncode}\n{run.stdout}\n{run.stderr}")


def main(executable):
    with tempfile.TemporaryDirectory() as temporary:
        folder = pathlib.Path(temporary)
        both = {"Caller.class": caller(), "Helper.class": helper()}
        check(executable, folder, "compressed", both, 5)
        check(executable, folder, "stored", both, 5,
              compression=zipfile.ZIP_STORED)
        check(executable, folder, "cached",
              {"Caller.class": caller(twice=True),
               "Helper.class": helper()}, 10)
        check(executable, folder, "old_46_helper",
              {"Caller.class": caller(),
               "Helper.class": helper(major=46, minor=0)}, 5)
        check(executable, folder, "missing_helper",
              {"Caller.class": caller()},
              "missing class target=value()I class=Helper")
        check(executable, folder, "missing_member",
              {"Caller.class": caller(),
               "Helper.class": helper(method_name="other")},
              "missing method member target=value()I class=Helper")
        check(executable, folder, "wrong_class_name",
              {"Caller.class": caller(),
               "Helper.class": helper(name="Other")},
              "class loading error target=value()I class=Helper")
        check(executable, folder, "unsupported_helper_version",
              {"Caller.class": caller(),
               "Helper.class": helper(major=50, minor=0)},
              "class cause: unsupported class format")
        check(executable, folder, "oversized_class",
              {"Caller.class": caller(),
               "Helper.class": helper() + b"\x00" * (1024 * 1024)},
              "VM execution limit reached target=value()I class=Helper")
        check(executable, folder, "no_class_init",
              {"Caller.class": caller(),
               "Helper.class": helper(clinit=True)},
              "unsupported VM feature target=value()I class=Helper")
        check(executable, folder, "platform_override",
              {"Caller.class": caller(owner="java/lang/System"),
               "java/lang/System.class": helper(name="java/lang/System")},
              "unsupported VM feature target=value()I class=java/lang/System")
        check(executable, folder, "missing_root", both,
              "missing class", root="Unknown")
        corrupt = bytearray(jar(both, zipfile.ZIP_STORED))
        with zipfile.ZipFile(io.BytesIO(corrupt)) as archive:
            info = archive.getinfo("Helper.class")
            start = info.header_offset + 30 + len(info.filename) + len(info.extra)
        corrupt[start] ^= 1
        check_raw(executable, folder, "bad_crc", corrupt,
                  "archive cause: CRC mismatch")
    print("JAR VM fixtures passed")


if __name__ == "__main__":
    main(sys.argv[1])
