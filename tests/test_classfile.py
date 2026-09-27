"""Independent old-format class fixtures for the bounded class parser."""

import io
import pathlib
import struct
import subprocess
import sys
import tempfile
import zipfile


def u1(n):
    return struct.pack(">B", n)


def u2(n):
    return struct.pack(">H", n)


def u4(n):
    return struct.pack(">I", n)


def utf8(value):
    return b"\x01" + u2(len(value)) + value


def make_class(major=45, minor=3, class_name=b"Example", descriptor=b"()V",
               code=b"\x2a\xb7\x00\x09\xb1", exception=b"", extra_pool=b"",
               extra_slots=0, debug=False):
    pool = [
        utf8(class_name), b"\x07" + u2(1),
        utf8(b"java/lang/Object"), b"\x07" + u2(3),
        utf8(b"<init>"), utf8(descriptor), utf8(b"Code"),
        b"\x0c" + u2(5) + u2(6),
        b"\x0a" + u2(4) + u2(8),
    ]
    nested = b""
    class_attributes = b"\x00\x00"
    if debug:
        assert extra_slots == 0
        extra_pool = (utf8(b"SourceFile") + utf8(b"Example.java") +
                      utf8(b"LineNumberTable"))
        extra_slots = 3
        nested = u2(12) + u4(6) + u2(1) + u2(0) + u2(42)
        class_attributes = u2(1) + u2(10) + u4(2) + u2(11)
    code_attribute = (u2(1) + u2(1) + u4(len(code)) + code +
                      u2(len(exception) // 8) + exception + u2(bool(debug)) + nested)
    method = (u2(1) + u2(5) + u2(6) + u2(1) +
              u2(7) + u4(len(code_attribute)) + code_attribute)
    return (u4(0xCAFEBABE) + u2(minor) + u2(major) + u2(10 + extra_slots) +
            b"".join(pool) + extra_pool + u2(0x21) + u2(2) + u2(4) +
            u2(0) + u2(0) + u2(1) + method + class_attributes)


def inspect(executable, folder, label, class_bytes, success, expected):
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as jar:
        jar.writestr("Example.class", class_bytes)
    path = folder / (label + ".jar")
    path.write_bytes(output.getvalue())
    result = subprocess.run([executable, "--inspect", str(path)],
                            capture_output=True, text=True, check=False)
    if (result.returncode == 0) != success or expected not in result.stdout + result.stderr:
        raise AssertionError(f"{label}: {result.returncode}\n{result.stdout}\n{result.stderr}")


def main(executable):
    with tempfile.TemporaryDirectory() as directory:
        folder = pathlib.Path(directory)
        valid = make_class()
        inspect(executable, folder, "valid", valid, True,
                "class version=45.3 fields=0 methods=1")
        inspect(executable, folder, "opcode", valid, True,
                "opcode 0xb7 count=1")
        table = (b"\xaa\x00\x00\x00" + u4(20) + u4(1) + u4(1) +
                 u4(20) + b"\xb1")
        inspect(executable, folder, "tableswitch",
                make_class(code=table), True, "opcode 0xaa count=1")
        inspect(executable, folder, "bad_bytecode",
                make_class(code=b"\x10"), False, "malformed bytecode")
        lookup = (b"\xab\x00\x00\x00" + u4(28) + u4(2) +
                  u4(5) + u4(28) + u4(4) + u4(28) + b"\xb1")
        inspect(executable, folder, "unsorted_lookup",
                make_class(code=lookup), False, "malformed bytecode")
        inspect(executable, folder, "long_constant",
                make_class(extra_pool=b"\x05" + b"\x00" * 8, extra_slots=2),
                True, "class version=45.3")
        inspect(executable, folder, "debug", make_class(debug=True),
                True, "line entries=1 first=42")
        inspect(executable, folder, "modern", make_class(major=50), False,
                "unsupported class format")
        inspect(executable, folder, "old_46",
                make_class(major=46, minor=0), True,
                "class version=46.0")
        inspect(executable, folder, "bad_minor_46",
                make_class(major=46, minor=3), False,
                "unsupported class format")
        inspect(executable, folder, "bad_descriptor",
                make_class(descriptor=b"(V)V"), False, "malformed class file")
        inspect(executable, folder, "bad_utf8",
                make_class(class_name=b"\xc0A"), False, "malformed class file")
        inspect(executable, folder, "bad_exception",
                make_class(exception=u2(4) + u2(2) + u2(0) + u2(0)),
                False, "malformed class file")
        inspect(executable, folder, "truncated", valid[:-3], False,
                "malformed class file")
        bad_tag = bytearray(valid)
        bad_tag[10] = 18
        inspect(executable, folder, "tag", bad_tag, False,
                "unsupported class format")
    print("class fixtures passed")


if __name__ == "__main__":
    main(sys.argv[1])
