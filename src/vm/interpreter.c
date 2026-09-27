#include <cgjre/vm.h>
#include <cgjre/bytecode_scan.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const cgjre_classfile *file;
    const cgjre_class_member *method;
    uint16_t method_index;
    cgjre_vm_slot *locals;
    cgjre_vm_slot *stack;
    uint8_t *starts;
    uint32_t pc;
    uint32_t sp;
} cgjre_vm_frame;

typedef struct {
    cgjre_vm_frame frames[CGJRE_VM_MAX_FRAMES];
    unsigned depth;
} cgjre_vm_context;

static uint16_t read16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t read32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
        ((uint32_t)p[2] << 8) | p[3];
}

static int32_t as_signed(uint32_t bits)
{
    return bits <= INT32_MAX ? (int32_t)bits :
        -1 - (int32_t)(UINT32_MAX - bits);
}

static uint32_t as_bits(int32_t value)
{
    return (uint32_t)value;
}

static int32_t signed_byte(uint32_t bits)
{
    uint32_t value = bits & 0xffu;
    return value < 0x80u ? (int32_t)value : (int32_t)value - 0x100;
}

static int32_t signed_short(uint32_t bits)
{
    uint32_t value = bits & 0xffffu;
    return value < 0x8000u ? (int32_t)value : (int32_t)value - 0x10000;
}

static uint32_t arithmetic_shift(uint32_t bits, unsigned count)
{
    if(!count) return bits;
    uint32_t shifted = bits >> count;
    return (bits & 0x80000000u) ?
        shifted | ~(UINT32_MAX >> count) : shifted;
}

static int push_int(cgjre_vm_frame *frame, uint32_t bits)
{
    if(frame->sp >= frame->method->max_stack) return 0;
    frame->stack[frame->sp++] = (cgjre_vm_slot){bits, CGJRE_SLOT_INT};
    return 1;
}

static int pop_int(cgjre_vm_frame *frame, uint32_t *bits)
{
    if(!frame->sp || frame->stack[frame->sp - 1].kind != CGJRE_SLOT_INT)
        return 0;
    *bits = frame->stack[--frame->sp].bits;
    frame->stack[frame->sp].kind = CGJRE_SLOT_EMPTY;
    return 1;
}

static int load_int(cgjre_vm_frame *frame, unsigned index)
{
    return index < frame->method->max_locals &&
        frame->locals[index].kind == CGJRE_SLOT_INT &&
        push_int(frame, frame->locals[index].bits);
}

static int store_int(cgjre_vm_frame *frame, unsigned index)
{
    uint32_t bits;
    if(index >= frame->method->max_locals || !pop_int(frame, &bits))
        return 0;
    frame->locals[index] = (cgjre_vm_slot){bits, CGJRE_SLOT_INT};
    return 1;
}

static int increment(cgjre_vm_frame *frame, unsigned index, int32_t amount)
{
    if(index >= frame->method->max_locals ||
       frame->locals[index].kind != CGJRE_SLOT_INT) return 0;
    frame->locals[index].bits += as_bits(amount);
    return 1;
}

static int method_arguments(const cgjre_classfile *file,
    const cgjre_class_member *method, size_t *count)
{
    const cgjre_cp_entry *entry = &file->cp[method->descriptor_index];
    const uint8_t *text = file->bytes + entry->offset;
    size_t i = 1, n = 0;
    if(entry->length < 3 || text[0] != '(') return 0;
    while(i < entry->length && text[i] == 'I') { ++i; ++n; }
    if(i + 2 != entry->length || text[i] != ')' || text[i + 1] != 'I')
        return 0;
    *count = n;
    return 1;
}

static int has_class_initializer(const cgjre_classfile *file)
{
    for(uint16_t i = 0; i < file->method_count; ++i) {
        char name[16];
        if(cgjre_class_ascii(file, file->methods[i].name_index,
           name, sizeof(name)) == CGJRE_CLASS_OK &&
           strcmp(name, "<clinit>") == 0) return 1;
    }
    return 0;
}

static int direct_object_super(const cgjre_classfile *file)
{
    char name[32];
    uint16_t super = file->super_class;
    if(!super || super >= file->cp_count || file->cp[super].tag != 7)
        return 0;
    uint16_t name_index = read16(file->bytes + file->cp[super].offset);
    return cgjre_class_ascii(file, name_index, name,
        sizeof(name)) == CGJRE_CLASS_OK &&
        strcmp(name, "java/lang/Object") == 0;
}

static int branch(cgjre_vm_frame *frame, int64_t offset)
{
    int64_t target = (int64_t)frame->pc + offset;
    if(target < 0 || target >= frame->method->code_length ||
       !frame->starts[(size_t)target]) return 0;
    frame->pc = (uint32_t)target;
    return 1;
}

static int same_utf8(const cgjre_classfile *file, uint16_t first,
    uint16_t second)
{
    if(!first || !second || first >= file->cp_count ||
       second >= file->cp_count || file->cp[first].tag != 1 ||
       file->cp[second].tag != 1) return 0;
    const cgjre_cp_entry *a = &file->cp[first], *b = &file->cp[second];
    return a->length == b->length &&
        memcmp(file->bytes + a->offset, file->bytes + b->offset,
            a->length) == 0;
}

static cgjre_vm_status resolve_static_call(const cgjre_classfile *file,
    uint16_t cp_index, uint16_t *method_index, size_t *argument_count)
{
    const cgjre_cp_entry *reference, *name_type;
    const uint8_t *p;
    uint16_t owner, name, descriptor;
    if(!cp_index || cp_index >= file->cp_count ||
       file->cp[cp_index].tag != 10) return CGJRE_VM_INVALID_CODE;
    reference = &file->cp[cp_index];
    p = file->bytes + reference->offset;
    owner = read16(p);
    if(owner != file->this_class) return CGJRE_VM_UNSUPPORTED;
    name_type = &file->cp[read16(p + 2)];
    p = file->bytes + name_type->offset;
    name = read16(p);
    descriptor = read16(p + 2);
    for(uint16_t i = 0; i < file->method_count; ++i) {
        const cgjre_class_member *method = &file->methods[i];
        if(same_utf8(file, method->name_index, name) &&
           same_utf8(file, method->descriptor_index, descriptor)) {
            if(!(method->access_flags & 0x0008u) ||
               (method->access_flags & 0x0520u) ||
               method->exception_count || !method->code_length ||
               !method_arguments(file, method, argument_count))
                return CGJRE_VM_UNSUPPORTED;
            *method_index = i;
            return CGJRE_VM_OK;
        }
    }
    return CGJRE_VM_MISSING_MEMBER;
}

static cgjre_vm_status initialize_frame(cgjre_vm_frame *frame,
    const cgjre_classfile *file, uint16_t method_index)
{
    const cgjre_class_member *method = &file->methods[method_index];
    uint32_t counts[256];
    frame->file = file;
    frame->method = method;
    frame->method_index = method_index;
    if(method->max_stack > CGJRE_VM_MAX_SLOTS ||
       method->max_locals > CGJRE_VM_MAX_SLOTS)
        return CGJRE_VM_LIMIT;
    frame->locals = calloc(method->max_locals ? method->max_locals : 1u,
        sizeof(*frame->locals));
    frame->stack = calloc(method->max_stack ? method->max_stack : 1u,
        sizeof(*frame->stack));
    frame->starts = malloc(method->code_length);
    if(!frame->locals || !frame->stack || !frame->starts)
        return CGJRE_VM_NOMEM;
    if(cgjre_bytecode_scan_starts(file->bytes + method->code_offset,
       method->code_length, counts, frame->starts) != 0)
        return CGJRE_VM_INVALID_CODE;
    return CGJRE_VM_OK;
}

static void destroy_frame(cgjre_vm_frame *frame)
{
    free(frame->starts);
    free(frame->stack);
    free(frame->locals);
    memset(frame, 0, sizeof(*frame));
}

const char *cgjre_vm_status_name(cgjre_vm_status status)
{
    switch(status) {
    case CGJRE_VM_OK: return "ok";
    case CGJRE_VM_BAD_INPUT: return "invalid VM request";
    case CGJRE_VM_INVALID_CODE: return "invalid bytecode or stack state";
    case CGJRE_VM_UNSUPPORTED: return "unsupported VM feature";
    case CGJRE_VM_MISSING_MEMBER: return "missing method member";
    case CGJRE_VM_DIVIDE_BY_ZERO: return "integer division by zero";
    case CGJRE_VM_LIMIT: return "VM execution limit reached";
    case CGJRE_VM_NOMEM: return "out of memory";
    }
    return "unknown VM error";
}

cgjre_vm_result cgjre_vm_execute_int(const cgjre_classfile *file,
    uint16_t method_index, const int32_t *arguments, size_t argument_count,
    uint32_t step_limit)
{
    cgjre_vm_result result = {.status = CGJRE_VM_BAD_INPUT,
        .method_index = method_index};
    cgjre_vm_context context = {0};
    cgjre_vm_frame *frame = &context.frames[0];
    const cgjre_class_member *method;
    const uint8_t *code;
    size_t expected_arguments;
    if(!file || method_index >= file->method_count ||
       (!arguments && argument_count) || !step_limit) return result;
    method = &file->methods[method_index];
    if(!(method->access_flags & 0x0008u) ||
       (method->access_flags & 0x0520u) || method->exception_count ||
       !method->code_length ||
       !method_arguments(file, method, &expected_arguments) ||
       expected_arguments != argument_count ||
       has_class_initializer(file) || !direct_object_super(file)) {
        result.status = CGJRE_VM_UNSUPPORTED;
        return result;
    }
    if(argument_count > method->max_locals) {
        result.status = CGJRE_VM_INVALID_CODE;
        return result;
    }
    context.depth = 1;
    result.status = initialize_frame(frame, file, method_index);
    if(result.status != CGJRE_VM_OK) goto done;
    for(size_t i = 0; i < argument_count; ++i)
        frame->locals[i] = (cgjre_vm_slot){as_bits(arguments[i]), CGJRE_SLOT_INT};
    for(;;) {
        uint8_t op;
        uint32_t a, b;
        uint32_t next;
        frame = &context.frames[context.depth - 1u];
        method = frame->method;
        code = file->bytes + method->code_offset;
        result.method_index = frame->method_index;
        result.cp_index = 0;
        if(frame->pc >= method->code_length || !frame->starts[frame->pc]) {
            result.status = CGJRE_VM_INVALID_CODE;
            goto done;
        }
        if(result.steps == step_limit) {
            result.status = CGJRE_VM_LIMIT;
            goto done;
        }
        op = code[frame->pc];
        result.pc = frame->pc;
        result.opcode = op;
        ++result.steps;
        next = frame->pc + 1u;
#define INVALID() do { result.status = CGJRE_VM_INVALID_CODE; goto done; } while(0)
        switch(op) {
        case 0x00: break;
        case 0x02: case 0x03: case 0x04: case 0x05:
        case 0x06: case 0x07: case 0x08:
            if(!push_int(frame, as_bits((int32_t)op - 3))) INVALID();
            break;
        case 0x10:
            if(!push_int(frame,
                as_bits(signed_byte(code[frame->pc + 1])))) INVALID();
            next += 1;
            break;
        case 0x11:
            if(!push_int(frame,
                as_bits(signed_short(read16(code + frame->pc + 1)))))
                INVALID();
            next += 2;
            break;
        case 0x12: case 0x13: {
            uint16_t index = op == 0x12 ? code[frame->pc + 1] :
                read16(code + frame->pc + 1);
            if(!index || index >= file->cp_count || file->cp[index].tag != 3) {
                result.status = CGJRE_VM_UNSUPPORTED;
                goto done;
            }
            if(!push_int(frame, read32(file->bytes + file->cp[index].offset)))
                INVALID();
            next += op == 0x12 ? 1u : 2u;
            break;
        }
        case 0x15:
            if(!load_int(frame, code[frame->pc + 1])) INVALID();
            next += 1;
            break;
        case 0x1a: case 0x1b: case 0x1c: case 0x1d:
            if(!load_int(frame, op - 0x1a)) INVALID();
            break;
        case 0x36:
            if(!store_int(frame, code[frame->pc + 1])) INVALID();
            next += 1;
            break;
        case 0x3b: case 0x3c: case 0x3d: case 0x3e:
            if(!store_int(frame, op - 0x3b)) INVALID();
            break;
        case 0x57:
            if(!pop_int(frame, &a)) INVALID();
            break;
        case 0x59:
            if(!frame->sp || frame->stack[frame->sp - 1].kind != CGJRE_SLOT_INT ||
               !push_int(frame, frame->stack[frame->sp - 1].bits)) INVALID();
            break;
        case 0x5f:
            if(frame->sp < 2 || frame->stack[frame->sp - 1].kind != CGJRE_SLOT_INT ||
               frame->stack[frame->sp - 2].kind != CGJRE_SLOT_INT) INVALID();
            a = frame->stack[frame->sp - 1].bits;
            frame->stack[frame->sp - 1].bits = frame->stack[frame->sp - 2].bits;
            frame->stack[frame->sp - 2].bits = a;
            break;
        case 0x60: case 0x64: case 0x68: case 0x6c: case 0x70:
        case 0x78: case 0x7a: case 0x7c: case 0x7e: case 0x80: case 0x82:
            if(!pop_int(frame, &b) || !pop_int(frame, &a)) INVALID();
            switch(op) {
            case 0x60: a += b; break;
            case 0x64: a -= b; break;
            case 0x68: a *= b; break;
            case 0x6c: case 0x70: {
                int32_t left = as_signed(a), right = as_signed(b);
                if(!right) {
                    result.status = CGJRE_VM_DIVIDE_BY_ZERO;
                    goto done;
                }
                if(left == INT32_MIN && right == -1)
                    a = op == 0x6c ? as_bits(INT32_MIN) : 0;
                else a = as_bits(op == 0x6c ? left / right : left % right);
                break;
            }
            case 0x78: a <<= (b & 31u); break;
            case 0x7a: a = arithmetic_shift(a, b & 31u); break;
            case 0x7c: a >>= (b & 31u); break;
            case 0x7e: a &= b; break;
            case 0x80: a |= b; break;
            case 0x82: a ^= b; break;
            }
            if(!push_int(frame, a)) INVALID();
            break;
        case 0x74:
            if(!pop_int(frame, &a) || !push_int(frame, 0u - a)) INVALID();
            break;
        case 0x84:
            if(!increment(frame, code[frame->pc + 1],
                signed_byte(code[frame->pc + 2]))) INVALID();
            next += 2;
            break;
        case 0x91: case 0x92: case 0x93:
            if(!pop_int(frame, &a)) INVALID();
            if(op == 0x91) a = as_bits(signed_byte(a));
            else if(op == 0x92) a = (uint16_t)a;
            else a = as_bits(signed_short(a));
            if(!push_int(frame, a)) INVALID();
            break;
        case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: {
            int32_t value;
            int take;
            if(!pop_int(frame, &a)) INVALID();
            value = as_signed(a);
            take = (op == 0x99 && value == 0) ||
                (op == 0x9a && value != 0) ||
                (op == 0x9b && value < 0) ||
                (op == 0x9c && value >= 0) ||
                (op == 0x9d && value > 0) ||
                (op == 0x9e && value <= 0);
            if(take) {
                if(!branch(frame, signed_short(read16(code + frame->pc + 1))))
                    INVALID();
                continue;
            }
            next += 2;
            break;
        }
        case 0x9f: case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: {
            int32_t left, right;
            int take;
            if(!pop_int(frame, &b) || !pop_int(frame, &a)) INVALID();
            left = as_signed(a); right = as_signed(b);
            take = (op == 0x9f && left == right) ||
                (op == 0xa0 && left != right) ||
                (op == 0xa1 && left < right) ||
                (op == 0xa2 && left >= right) ||
                (op == 0xa3 && left > right) ||
                (op == 0xa4 && left <= right);
            if(take) {
                if(!branch(frame, signed_short(read16(code + frame->pc + 1))))
                    INVALID();
                continue;
            }
            next += 2;
            break;
        }
        case 0xa7:
            if(!branch(frame, signed_short(read16(code + frame->pc + 1))))
                INVALID();
            continue;
        case 0xac:
            if(frame->sp != 1 || !pop_int(frame, &a)) INVALID();
            if(context.depth == 1) {
                result.value = as_signed(a);
                result.status = CGJRE_VM_OK;
                goto done;
            }
            destroy_frame(frame);
            --context.depth;
            frame = &context.frames[context.depth - 1u];
            if(!push_int(frame, a)) {
                result.method_index = frame->method_index;
                result.pc = frame->pc;
                result.opcode = 0xb8;
                INVALID();
            }
            continue;
        case 0xb8: {
            uint16_t index = read16(code + frame->pc + 1);
            uint16_t callee_index;
            size_t callee_arguments;
            cgjre_vm_status status = resolve_static_call(file, index,
                &callee_index, &callee_arguments);
            result.cp_index = index;
            if(status != CGJRE_VM_OK) {
                result.status = status;
                goto done;
            }
            if(context.depth >= CGJRE_VM_MAX_FRAMES) {
                result.status = CGJRE_VM_LIMIT;
                goto done;
            }
            if(callee_arguments > frame->sp ||
               callee_arguments > file->methods[callee_index].max_locals)
                INVALID();
            for(size_t i = 0; i < callee_arguments; ++i)
                if(frame->stack[frame->sp - callee_arguments + i].kind !=
                   CGJRE_SLOT_INT) INVALID();
            cgjre_vm_frame *callee = &context.frames[context.depth++];
            status = initialize_frame(callee, file, callee_index);
            if(status != CGJRE_VM_OK) {
                result.status = status;
                goto done;
            }
            for(size_t i = 0; i < callee_arguments; ++i) {
                callee->locals[i] =
                    frame->stack[frame->sp - callee_arguments + i];
                frame->stack[frame->sp - callee_arguments + i].kind =
                    CGJRE_SLOT_EMPTY;
            }
            frame->sp -= (uint32_t)callee_arguments;
            frame->pc += 3;
            continue;
        }
        case 0xc4: {
            uint8_t nested = code[frame->pc + 1];
            unsigned index = read16(code + frame->pc + 2);
            if(nested == 0x15) {
                if(!load_int(frame, index)) INVALID();
                next += 3;
            }
            else if(nested == 0x36) {
                if(!store_int(frame, index)) INVALID();
                next += 3;
            }
            else if(nested == 0x84) {
                if(!increment(frame, index,
                    signed_short(read16(code + frame->pc + 4)))) INVALID();
                next += 5;
            }
            else { result.status = CGJRE_VM_UNSUPPORTED; goto done; }
            break;
        }
        case 0xc8: {
            uint32_t bits = read32(code + frame->pc + 1);
            int64_t signed_offset = bits <= INT32_MAX ? bits :
                (int64_t)bits - 0x100000000ll;
            if(!branch(frame, signed_offset)) INVALID();
            continue;
        }
        default:
            result.status = CGJRE_VM_UNSUPPORTED;
            goto done;
        }
        frame->pc = next;
#undef INVALID
    }
done:
    while(context.depth)
        destroy_frame(&context.frames[--context.depth]);
    return result;
}
