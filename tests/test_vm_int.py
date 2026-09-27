"""Independent class bytes and expected integer results for the M2 VM slice."""

import pathlib
import struct
import subprocess
import sys
import tempfile


def u2(number):
    return struct.pack(">H", number)


def u4(number):
    return struct.pack(">I", number & 0xFFFFFFFF)


def utf8(value):
    encoded = value.encode("ascii")
    return b"\x01" + u2(len(encoded)) + encoded


def make_class(code, descriptor="()I", max_stack=4, max_locals=4,
               constant=None, static=True, synchronized=False, exception=b""):
    pool = [utf8("Arithmetic"), b"\x07" + u2(1),
            utf8("java/lang/Object"), b"\x07" + u2(3),
            utf8("test"), utf8(descriptor), utf8("Code")]
    if constant is not None:
        pool.append(b"\x03" + u4(constant))
    code_attribute = (u2(max_stack) + u2(max_locals) + u4(len(code)) +
                      code + u2(len(exception) // 8) + exception + u2(0))
    flags = (0x0009 if static else 0x0001) | (0x0020 if synchronized else 0)
    method = (u2(flags) + u2(5) + u2(6) +
              u2(1) + u2(7) + u4(len(code_attribute)) + code_attribute)
    return (u4(0xCAFEBABE) + u2(3) + u2(45) + u2(len(pool) + 1) +
            b"".join(pool) + u2(0x0021) + u2(2) + u2(4) +
            u2(0) + u2(0) + u2(1) + method + u2(0))


def check(executable, folder, label, code, expected, descriptor="()I",
          arguments=(), **options):
    path = folder / (label + ".class")
    path.write_bytes(make_class(code, descriptor, **options))
    command = [executable, "--eval-class", str(path), "test", descriptor]
    command.extend(str(value) for value in arguments)
    run = subprocess.run(command, capture_output=True, text=True, check=False)
    if isinstance(expected, int):
        if run.returncode != 0 or f"result={expected} " not in run.stdout:
            raise AssertionError(f"{label}: {run.returncode}\n{run.stdout}\n{run.stderr}")
    elif run.returncode == 0 or expected not in run.stderr:
        raise AssertionError(f"{label}: {run.returncode}\n{run.stdout}\n{run.stderr}")


def main(executable):
    with tempfile.TemporaryDirectory() as directory:
        folder = pathlib.Path(directory)
        check(executable, folder, "add", b"\x05\x06\x60\xac", 5)
        check(executable, folder, "args_multiply", b"\x1a\x1b\x68\xac",
              -21, descriptor="(II)I", arguments=(7, -3))
        check(executable, folder, "subtract", b"\x07\x05\x64\xac", 2)
        check(executable, folder, "signed_divide",
              b"\x10\xf9\x06\x6c\xac", -2)
        check(executable, folder, "signed_remainder",
              b"\x10\xf9\x06\x70\xac", -1)
        check(executable, folder, "negate_min",
              b"\x12\x08\x74\xac", -2147483648,
              constant=0x80000000)
        check(executable, folder, "logical_shift",
              b"\x02\x04\x7c\xac", 2147483647)
        check(executable, folder, "and", b"\x06\x05\x7e\xac", 2)
        check(executable, folder, "or", b"\x06\x05\x80\xac", 3)
        check(executable, folder, "xor", b"\x06\x05\x82\xac", 1)
        check(executable, folder, "dup", b"\x06\x59\x60\xac", 6)
        check(executable, folder, "pop", b"\x04\x05\x57\xac", 1)
        check(executable, folder, "swap", b"\x05\x06\x5f\x64\xac", 1)
        check(executable, folder, "explicit_local",
              b"\x08\x36\x01\x15\x01\xac", 5)
        check(executable, folder, "local_forms",
              b"\x08\x3b\x1a\xac", 5)
        for index in range(4):
            code = bytes([0x08, 0x3B + index, 0x1A + index, 0xAC])
            check(executable, folder, f"local_form_{index}", code, 5)
        check(executable, folder, "wide_local",
              b"\x08\xc4\x36\x01\x00\xc4\x15\x01\x00\xac",
              5, max_locals=257)
        check(executable, folder, "ldc_w",
              b"\x13\x00\x08\xac", 123456, constant=123456)
        check(executable, folder, "bipush_negative",
              b"\x10\x80\xac", -128)
        check(executable, folder, "sipush_negative",
              b"\x11\xff\x7f\xac", -129)
        check(executable, folder, "i2c", b"\x02\x92\xac", 65535)
        check(executable, folder, "i2s",
              b"\x12\x08\x93\xac", -1, constant=65535)
        check(executable, folder, "nop", b"\x00\x04\xac", 1)
        check(executable, folder, "wrap", b"\x12\x08\x04\x60\xac",
              -2147483648, constant=2147483647)
        check(executable, folder, "min_divide",
              b"\x12\x08\x02\x6c\xac", -2147483648,
              constant=0x80000000)
        check(executable, folder, "min_remainder",
              b"\x12\x08\x02\x70\xac", 0,
              constant=0x80000000)
        check(executable, folder, "zero_divide",
              b"\x04\x03\x6c\xac", "integer division by zero")
        check(executable, folder, "negative_branch",
              b"\x1a\x9b\x00\x05\x04\xac\x02\xac", -1,
              descriptor="(I)I", arguments=(-3,))
        check(executable, folder, "positive_branch",
              b"\x1a\x9b\x00\x05\x04\xac\x02\xac", 1,
              descriptor="(I)I", arguments=(3,))
        check(executable, folder, "goto",
              b"\xa7\x00\x04\x03\x08\xac", 5)
        check(executable, folder, "goto_w",
              b"\xc8\x00\x00\x00\x06\x03\x08\xac", 5)
        unary_cases = [(0x99, 0, 1), (0x9A, 1, 0),
                       (0x9B, -1, 0), (0x9C, 0, -1),
                       (0x9D, 1, 0), (0x9E, 0, 1)]
        for opcode, true_value, false_value in unary_cases:
            code = bytes([0x1A, opcode, 0, 5, 0x03, 0xAC, 0x04, 0xAC])
            check(executable, folder, f"unary_{opcode:02x}_true", code, 1,
                  descriptor="(I)I", arguments=(true_value,))
            check(executable, folder, f"unary_{opcode:02x}_false", code, 0,
                  descriptor="(I)I", arguments=(false_value,))
        compare_cases = [(0x9F, (2, 2), (2, 3)),
                         (0xA0, (2, 3), (2, 2)),
                         (0xA1, (-1, 1), (1, -1)),
                         (0xA2, (1, -1), (-1, 1)),
                         (0xA3, (1, -1), (-1, 1)),
                         (0xA4, (-1, 1), (1, -1))]
        for opcode, true_args, false_args in compare_cases:
            code = bytes([0x1A, 0x1B, opcode, 0, 5, 0x03, 0xAC, 0x04, 0xAC])
            check(executable, folder, f"compare_{opcode:02x}_true", code, 1,
                  descriptor="(II)I", arguments=true_args)
            check(executable, folder, f"compare_{opcode:02x}_false", code, 0,
                  descriptor="(II)I", arguments=false_args)
        check(executable, folder, "wide_increment",
              b"\x84\x00\x7f\xc4\x84\x00\x00\x01\x2c\x1a\xac",
              432, descriptor="(I)I", arguments=(5,))
        check(executable, folder, "masked_shift",
              b"\x02\x10\x20\x78\xac", -1)
        check(executable, folder, "arithmetic_shift",
              b"\x02\x04\x7a\xac", -1)
        check(executable, folder, "narrow_byte",
              b"\x11\xff\x7f\x91\xac", 127)
        check(executable, folder, "stack_overflow",
              b"\x04\x05\x60\xac", "invalid bytecode or stack state",
              max_stack=1)
        check(executable, folder, "bad_branch",
              b"\xa7\x00\x01\xac", "invalid bytecode or stack state")
        check(executable, folder, "unsupported",
              b"\x09\xac", "unsupported VM feature")
        check(executable, folder, "instance_method",
              b"\x04\xac", "unsupported VM feature", static=False)
        check(executable, folder, "synchronized_method",
              b"\x04\xac", "unsupported VM feature", synchronized=True)
        check(executable, folder, "exception_handler",
              b"\x04\xac", "unsupported VM feature",
              exception=u2(0) + u2(1) + u2(1) + u2(0))
        check(executable, folder, "busy_loop",
              b"\xa7\x00\x00", "VM execution limit reached")
    print("integer VM fixtures passed")


if __name__ == "__main__":
    main(sys.argv[1])
