"""Primitive-array bytecode behavior through the shared host VM."""

import pathlib
import sys
import tempfile
import zipfile

from test_vm_int import check, make_class


def main(executable):
    with tempfile.TemporaryDirectory() as temporary:
        folder = pathlib.Path(temporary)
        check(executable, folder, "int_roundtrip",
              b"\x06\xbc\x0a\x4b\x2a\x04\x10\xf9\x4f"
              b"\x2a\x04\x2e\xac", -7, max_stack=3)
        check(executable, folder, "array_length",
              b"\x06\xbc\x0a\xbe\xac", 3)
        check(executable, folder, "zero_length",
              b"\x03\xbc\x0a\xbe\xac", 0)
        check(executable, folder, "default_zero",
              b"\x04\xbc\x0a\x03\x2e\xac", 0)
        check(executable, folder, "byte_roundtrip",
              b"\x04\xbc\x08\x4b\x2a\x03\x02\x54"
              b"\x2a\x03\x33\xac", -1, max_stack=3)
        check(executable, folder, "boolean_byte_store",
              b"\x04\xbc\x04\x4b\x2a\x03\x08\x54"
              b"\x2a\x03\x33\xac", 5, max_stack=3)
        check(executable, folder, "char_zero_extend",
              b"\x04\xbc\x05\x4b\x2a\x03\x11\xff\xff\x55"
              b"\x2a\x03\x34\xac", 65535, max_stack=3)
        check(executable, folder, "short_sign_extend",
              b"\x04\xbc\x09\x4b\x2a\x03\x11\xff\xff\x56"
              b"\x2a\x03\x35\xac", -1, max_stack=3)
        check(executable, folder, "null_length",
              b"\x01\xbe\xac", "null reference")
        check(executable, folder, "null_load",
              b"\x01\x03\x2e\xac", "null reference")
        check(executable, folder, "null_store",
              b"\x01\x03\x04\x4f\x03\xac", "null reference")
        check(executable, folder, "negative_size",
              b"\x02\xbc\x0a", "negative array size")
        check(executable, folder, "upper_bound",
              b"\x04\xbc\x0a\x04\x2e\xac",
              "array index out of bounds")
        check(executable, folder, "negative_index",
              b"\x04\xbc\x0a\x02\x04\x4f\xac",
              "array index out of bounds")
        check(executable, folder, "wrong_array_kind",
              b"\x04\xbc\x08\x03\x2e\xac",
              "invalid bytecode or stack state")
        check(executable, folder, "unsupported_type",
              b"\x04\xbc\x07", "unsupported VM feature")
        check(executable, folder, "array_size_limit",
              b"\x11\x40\x01\xbc\x0a", "VM execution limit reached")
        check(executable, folder, "handle_limit",
              (b"\x04\xbc\x0a\x57" * 256) + b"\x03\xac",
              "VM execution limit reached", max_stack=1)
        check(executable, folder, "total_byte_limit",
              (b"\x11\x40\x00\xbc\x0a\x57" * 5) + b"\x03\xac",
              "VM execution limit reached", max_stack=1)
        check(executable, folder, "ifnull",
              b"\x01\xc6\x00\x05\x03\xac\x04\xac", 1)
        check(executable, folder, "ifnull_false",
              b"\x04\xbc\x0a\xc6\x00\x05\x03\xac\x04\xac", 0)
        check(executable, folder, "ifnonnull",
              b"\x04\xbc\x0a\xc7\x00\x05\x03\xac\x04\xac", 1)
        check(executable, folder, "ifnonnull_false",
              b"\x01\xc7\x00\x05\x03\xac\x04\xac", 0)
        check(executable, folder, "same_ref",
              b"\x04\xbc\x0a\x4b\x2a\x2a\xa5\x00\x05"
              b"\x03\xac\x04\xac", 1)
        check(executable, folder, "same_ref_not_unequal",
              b"\x04\xbc\x0a\x4b\x2a\x2a\xa6\x00\x05"
              b"\x03\xac\x04\xac", 0)
        check(executable, folder, "different_refs",
              b"\x04\xbc\x0a\x04\xbc\x0a\xa6\x00\x05"
              b"\x03\xac\x04\xac", 1)
        check(executable, folder, "different_refs_not_equal",
              b"\x04\xbc\x0a\x04\xbc\x0a\xa5\x00\x05"
              b"\x03\xac\x04\xac", 0)
        check(executable, folder, "reference_dup",
              b"\x04\xbc\x0a\x59\x4b\xbe\xac", 1)
        check(executable, folder, "reference_pop",
              b"\x04\xbc\x0a\x57\x08\xac", 5)
        check(executable, folder, "reference_swap",
              b"\x04\xbc\x0a\x08\x5f\xbe\x60\xac", 6)
        check(executable, folder, "explicit_reference_local",
              b"\x04\xbc\x0a\x3a\x01\x19\x01\xbe\xac", 1)
        check(executable, folder, "uninitialized_reference_local",
              b"\x2a\xbe\xac", "invalid bytecode or stack state")
        check(executable, folder, "wrong_reference_store",
              b"\x04\x4b\x03\xac", "invalid bytecode or stack state")
        for index in range(4):
            code = bytes([0x04, 0xBC, 0x0A, 0x4B + index,
                          0x2A + index, 0xBE, 0xAC])
            check(executable, folder, f"reference_local_{index}", code, 1)
        check(executable, folder, "wide_reference_local",
              b"\x04\xbc\x0a\xc4\x3a\x01\x00"
              b"\xc4\x19\x01\x00\xbe\xac", 1,
              max_locals=257)
        jar_path = folder / "ArrayFixture.jar"
        with zipfile.ZipFile(jar_path, "w",
                             compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("Arithmetic.class", make_class(
                b"\x04\xbc\x0a\xbe\xac"))
        import subprocess
        run = subprocess.run([executable, "--eval-jar", str(jar_path),
                              "Arithmetic", "test", "()I"],
                             capture_output=True, text=True, check=False)
        if run.returncode or "result=1 " not in run.stdout:
            raise AssertionError(f"jar_array: {run.returncode}\n{run.stdout}\n{run.stderr}")
    print("array VM fixtures passed")


if __name__ == "__main__":
    main(sys.argv[1])
