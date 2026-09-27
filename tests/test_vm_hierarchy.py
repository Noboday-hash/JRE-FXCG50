"""Independent JAR fixtures for inheritance, virtual calls and reference arrays."""

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
    value = text.encode("ascii")
    return b"\x01" + u2(len(value)) + value


def method(name, descriptor, code, stack=2, locals_count=2, flags=1):
    body = u2(stack) + u2(locals_count) + u4(len(code)) + code + u2(0) + u2(0)
    return (u2(flags) + u2(name) + u2(descriptor) + u2(1) +
            u2(7) + u4(len(body)) + body)


def field(name, descriptor):
    return u2(4) + u2(name) + u2(descriptor) + u2(0)


def klass(pool, fields, methods):
    return (u4(0xCAFEBABE) + u2(3) + u2(45) +
            u2(len(pool) + 1) + b"".join(pool) +
            u2(0x0021) + u2(2) + u2(4) + u2(0) +
            u2(len(fields)) + b"".join(fields) +
            u2(len(methods)) + b"".join(methods) + u2(0))


def base(include_score=True, cycle=False):
    pool = [utf8("Base"), b"\x07" + u2(1),
            utf8("Child" if cycle else "java/lang/Object"), b"\x07" + u2(3),
            utf8("<init>"), utf8("(I)V"), utf8("Code"),
            utf8("()V"), b"\x0c" + u2(5) + u2(8),
            b"\x0a" + u2(4) + u2(9),
            utf8("value"), utf8("I"), b"\x0c" + u2(11) + u2(12),
            b"\x09" + u2(2) + u2(13),
            utf8("score"), utf8("()I")]
    methods = [method(5, 6, b"\x2a\xb7\x00\x0a\x2a\x1b\xb5\x00\x0e\xb1")]
    if include_score:
        methods.append(method(15, 16, b"\x2a\xb4\x00\x0e\xac",
                              stack=1, locals_count=1))
    return klass(pool, [field(11, 12)], methods)


def child(override=True, hidden_field=False, bad_super=False):
    pool = [utf8("Child"), b"\x07" + u2(1),
            utf8("Base"), b"\x07" + u2(3),
            utf8("<init>"), utf8("(I)V"), utf8("Code"),
            b"\x0c" + u2(5) + u2(6), b"\x0a" + u2(4) + u2(8),
            utf8("score"), utf8("()I"),
            utf8("value"), utf8("I"),
            b"\x0c" + u2(12) + u2(13),
            b"\x09" + u2(4) + u2(14)]
    code = b"\x2a\x1b\xb7\x00\x09\xb1"
    if bad_super:
        pool.extend([utf8("java/lang/Object"), b"\x07" + u2(16),
                     utf8("()V"), b"\x0c" + u2(5) + u2(18),
                     b"\x0a" + u2(17) + u2(19)])
        code = b"\x2a\xb7\x00\x14\xb1"
    methods = [method(5, 6, code)]
    if override:
        methods.append(method(10, 11,
                              b"\x2a\xb4\x00\x0f\x04\x60\xac",
                              stack=2, locals_count=1))
    fields = [field(12, 13)] if hidden_field else []
    return klass(pool, fields, methods)


def caller(code=None):
    pool = [utf8("Caller"), b"\x07" + u2(1),
            utf8("java/lang/Object"), b"\x07" + u2(3),
            utf8("test"), utf8("(I)I"), utf8("Code"),
            utf8("Base"), b"\x07" + u2(8),
            utf8("Child"), b"\x07" + u2(10),
            utf8("<init>"), utf8("(I)V"),
            b"\x0c" + u2(12) + u2(13),
            b"\x0a" + u2(11) + u2(14),
            utf8("score"), utf8("()I"),
            b"\x0c" + u2(16) + u2(17),
            b"\x0a" + u2(9) + u2(18),
            b"\x0a" + u2(9) + u2(14),
            utf8("[LBase;"), b"\x07" + u2(21),
            utf8("[LChild;"), b"\x07" + u2(23),
            utf8("[I"), b"\x07" + u2(25),
            utf8("[B"), b"\x07" + u2(27)]
    if code is None:
        code = (b"\x04\xbd\x00\x09\x4c\x2b\x03"
                b"\xbb\x00\x0b\x59\x1a\xb7\x00\x0f\x53"
                b"\x2b\x03\x32\xb6\x00\x13\xac")
    return klass(pool, [], [method(5, 6, code,
                                   stack=5, locals_count=2, flags=0x0009)])


def check(executable, folder, label, entries, expected):
    output = io.BytesIO()
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as jar:
        for name, data in entries.items():
            jar.writestr(name, data)
    path = folder / (label + ".jar")
    path.write_bytes(output.getvalue())
    run = subprocess.run([executable, "--eval-jar", str(path), "Caller",
                          "test", "(I)I", "41"], capture_output=True,
                         text=True, check=False)
    if isinstance(expected, int):
        if run.returncode != 0 or f"result={expected} " not in run.stdout:
            raise AssertionError(f"{label}: {run.returncode}\n{run.stdout}\n{run.stderr}")
    elif run.returncode == 0 or expected not in run.stderr:
        raise AssertionError(f"{label}: {run.returncode}\n{run.stdout}\n{run.stderr}")


def main(executable):
    with tempfile.TemporaryDirectory() as temporary:
        folder = pathlib.Path(temporary)
        all_classes = {"Base.class": base(), "Child.class": child(),
                       "Caller.class": caller()}
        check(executable, folder, "overridden", all_classes, 42)
        check(executable, folder, "inherited_method",
              {"Base.class": base(), "Child.class": child(override=False),
               "Caller.class": caller()}, 41)
        check(executable, folder, "hidden_field",
              {"Base.class": base(), "Child.class": child(hidden_field=True),
               "Caller.class": caller()}, 42)
        check(executable, folder, "missing_declared_method",
              {"Base.class": base(include_score=False),
               "Child.class": child(), "Caller.class": caller()},
              "missing method member target=score()I class=Base")
        check(executable, folder, "null_virtual",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(b"\x01\xb6\x00\x13\xac")},
              "null reference")
        check(executable, folder, "uninitialized_virtual",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(b"\xbb\x00\x0b\xb6\x00\x13\xac")},
              "uninitialized object")
        check(executable, folder, "array_store_type",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\x04\xbd\x00\x0b\x4c\x2b\x03"
                   b"\xbb\x00\x09\x59\x1a\xb7\x00\x14\x53"
                   b"\x03\xac")},
              "incompatible array element")
        check(executable, folder, "reference_array_length",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(b"\x04\xbd\x00\x09\xbe\xac")},
              1)
        check(executable, folder, "default_null_element",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\x04\xbd\x00\x09\x03\x32\xc6\x00\x05"
                   b"\x03\xac\x04\xac")},
              1)
        check(executable, folder, "null_array_store",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(b"\x01\x03\x01\x53\x03\xac")},
              "null reference")
        check(executable, folder, "subclass_instanceof_base",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\xbb\x00\x0b\x59\x1a\xb7\x00\x0f"
                   b"\xc1\x00\x09\xac")},
              1)
        check(executable, folder, "base_not_instanceof_child",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\xbb\x00\x09\x59\x1a\xb7\x00\x14"
                   b"\xc1\x00\x0b\xac")},
              0)
        check(executable, folder, "subclass_checkcast_base",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\xbb\x00\x0b\x59\x1a\xb7\x00\x0f"
                   b"\xc0\x00\x09\xb6\x00\x13\xac")},
              42)
        check(executable, folder, "bad_class_cast",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\xbb\x00\x09\x59\x1a\xb7\x00\x14"
                   b"\xc0\x00\x0b\x03\xac")},
              "class cast failed")
        check(executable, folder, "null_checkcast",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\x01\xc0\x00\x09\xc6\x00\x05"
                   b"\x03\xac\x04\xac")},
              1)
        check(executable, folder, "array_covariant_instanceof",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\x04\xbd\x00\x0b\xc1\x00\x16\xac")},
              1)
        check(executable, folder, "array_not_reverse_covariant",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\x04\xbd\x00\x09\xc1\x00\x18\xac")},
              0)
        check(executable, folder, "primitive_array_instanceof_object",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\x04\xbc\x0a\xc1\x00\x04\xac")},
              1)
        check(executable, folder, "primitive_array_exact_type",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\x04\xbc\x0a\xc1\x00\x1a\xac")},
              1)
        check(executable, folder, "primitive_array_bad_cast",
              {"Base.class": base(), "Child.class": child(),
               "Caller.class": caller(
                   b"\x04\xbc\x0a\xc0\x00\x1c\x03\xac")},
              "class cast failed")
        check(executable, folder, "bad_super_constructor",
              {"Base.class": base(), "Child.class": child(bad_super=True),
               "Caller.class": caller()},
              "invalid bytecode or stack state")
        check(executable, folder, "cycle",
              {"Base.class": base(cycle=True), "Child.class": child(),
               "Caller.class": caller()},
              "invalid bytecode or stack state")
    print("hierarchy VM fixtures passed")


if __name__ == "__main__":
    main(sys.argv[1])
