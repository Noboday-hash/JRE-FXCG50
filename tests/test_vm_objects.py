"""Independently assembled old-format object and constructor fixtures."""

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


def utf8(value):
    data = value.encode("ascii")
    return b"\x01" + u2(len(data)) + data


def member(flags, name, descriptor, code=None, stack=2, locals_count=2):
    prefix = u2(flags) + u2(name) + u2(descriptor)
    if code is None:
        return prefix + u2(0)
    body = (u2(stack) + u2(locals_count) + u4(len(code)) +
            code + u2(0) + u2(0))
    return prefix + u2(1) + u2(7) + u4(len(body)) + body


def klass(pool, fields, methods):
    return (u4(0xCAFEBABE) + u2(3) + u2(45) +
            u2(len(pool) + 1) + b"".join(pool) +
            u2(0x0021) + u2(2) + u2(4) + u2(0) +
            u2(len(fields)) + b"".join(fields) +
            u2(len(methods)) + b"".join(methods) + u2(0))


def box(field="value", descriptor="I", write=True,
        constructor_name="<init>", constructor_code=None, field_flags=1):
    pool = [utf8("Box"), b"\x07" + u2(1),
            utf8("java/lang/Object"), b"\x07" + u2(3),
            utf8(constructor_name), utf8("(I)V"), utf8("Code"),
            utf8("()V"), b"\x0c" + u2(5) + u2(8),
            b"\x0a" + u2(4) + u2(9),
            utf8(field), utf8(descriptor),
            b"\x0c" + u2(11) + u2(12),
            b"\x09" + u2(2) + u2(13)]
    if constructor_code is None:
        constructor_code = b"\x2a\xb7\x00\x0a"
        if write:
            constructor_code += b"\x2a\x1b\xb5\x00\x0e"
        constructor_code += b"\xb1"
    return klass(pool, [member(field_flags, 11, 12)],
                 [member(1, 5, 6, constructor_code)])


def caller(code=None, field="value", descriptor="I"):
    pool = [utf8("Caller"), b"\x07" + u2(1),
            utf8("java/lang/Object"), b"\x07" + u2(3),
            utf8("test"), utf8("(I)I"), utf8("Code"),
            utf8("Box"), b"\x07" + u2(8),
            utf8("<init>"), utf8("(I)V"),
            b"\x0c" + u2(10) + u2(11),
            b"\x0a" + u2(9) + u2(12),
            utf8(field), utf8(descriptor),
            b"\x0c" + u2(14) + u2(15),
            b"\x09" + u2(9) + u2(16)]
    if code is None:
        code = (b"\xbb\x00\x09\x59\x1a\xb7\x00\x0d"
                b"\x4c\x2b\xb4\x00\x11\xac")
    return klass(pool, [], [member(0x0009, 5, 6, code,
                                   stack=3, locals_count=2)])


def check(executable, folder, label, caller_bytes, box_bytes,
          expected, argument=41):
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as jar:
        jar.writestr("Caller.class", caller_bytes)
        jar.writestr("Box.class", box_bytes)
    path = folder / (label + ".jar")
    path.write_bytes(output.getvalue())
    run = subprocess.run([executable, "--eval-jar", str(path),
                          "Caller", "test", "(I)I", str(argument)],
                         capture_output=True, text=True, check=False)
    if isinstance(expected, int):
        if run.returncode != 0 or f"result={expected} " not in run.stdout:
            raise AssertionError(f"{label}: {run.returncode}\n{run.stdout}\n{run.stderr}")
    elif run.returncode == 0 or expected not in run.stderr:
        raise AssertionError(f"{label}: {run.returncode}\n{run.stdout}\n{run.stderr}")


def main(executable):
    with tempfile.TemporaryDirectory() as temporary:
        folder = pathlib.Path(temporary)
        check(executable, folder, "int_field", caller(), box(), 41)
        check(executable, folder, "negative_field", caller(), box(),
              -7, argument=-7)
        check(executable, folder, "final_set_in_constructor",
              caller(), box(field_flags=0x0011), 41)
        late_write = (b"\xbb\x00\x09\x59\x1a\xb7\x00\x0d\x4c"
                      b"\x2b\x08\xb5\x00\x11\x2b\xb4\x00\x11\xac")
        check(executable, folder, "final_write_after_constructor",
              caller(late_write), box(field_flags=0x0011),
              "invalid bytecode or stack state")
        default_code = (b"\xbb\x00\x09\x59\x03\xb7\x00\x0d"
                        b"\xb4\x00\x11\xac")
        check(executable, folder, "default_zero", caller(default_code),
              box(write=False), 0)
        check(executable, folder, "null_get",
              caller(b"\x01\xb4\x00\x11\xac"), box(), "null reference")
        check(executable, folder, "null_put",
              caller(b"\x01\x04\xb5\x00\x11\x03\xac"), box(),
              "null reference")
        check(executable, folder, "uninitialized_read",
              caller(b"\xbb\x00\x09\x59\xb4\x00\x11\xac"), box(),
              "uninitialized object")
        check(executable, folder, "missing_field",
              caller(descriptor="Z"), box(),
              "missing field member target=valueZ class=Box")
        check(executable, folder, "missing_constructor",
              caller(), box(constructor_name="init"),
              "missing method member target=<init>(I)V class=Box")
        check(executable, folder, "missing_super_call",
              caller(), box(constructor_code=b"\xb1"),
              "uninitialized object")
        check(executable, folder, "unsupported_long_field",
              caller(), box(descriptor="J", write=False),
              "unsupported VM feature")
        ref_code = (b"\xbb\x00\x09\x59\x03\xb7\x00\x0d\x4b"
                    b"\x2a\x04\xbc\x0a\xb5\x00\x11"
                    b"\x2a\xb4\x00\x11\xbe\xac")
        check(executable, folder, "object_reference_field",
              caller(ref_code, field="peer", descriptor="Ljava/lang/Object;"),
              box(field="peer", descriptor="Ljava/lang/Object;", write=False),
              1)
        check(executable, folder, "reference_type_mismatch",
              caller(ref_code, field="peer", descriptor="LBox;"),
              box(field="peer", descriptor="LBox;", write=False),
              "invalid bytecode or stack state")
    print("object VM fixtures passed")


if __name__ == "__main__":
    main(sys.argv[1])
