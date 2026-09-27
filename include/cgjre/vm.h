#ifndef CGJRE_VM_H
#define CGJRE_VM_H

#include <cgjre/classfile.h>
#include <stddef.h>
#include <stdint.h>

#define CGJRE_VM_MAX_FRAMES 32u
#define CGJRE_VM_MAX_SLOTS 4096u

typedef enum {
    CGJRE_SLOT_EMPTY = 0,
    CGJRE_SLOT_INT,
    CGJRE_SLOT_REFERENCE,
    CGJRE_SLOT_LONG_HIGH,
    CGJRE_SLOT_LONG_LOW
} cgjre_slot_kind;

typedef struct {
    uint32_t bits;
    cgjre_slot_kind kind;
} cgjre_vm_slot;

typedef enum {
    CGJRE_VM_OK = 0,
    CGJRE_VM_BAD_INPUT,
    CGJRE_VM_INVALID_CODE,
    CGJRE_VM_UNSUPPORTED,
    CGJRE_VM_DIVIDE_BY_ZERO,
    CGJRE_VM_LIMIT,
    CGJRE_VM_NOMEM
} cgjre_vm_status;

typedef struct {
    cgjre_vm_status status;
    int32_t value;
    uint32_t steps;
    uint32_t pc;
    uint8_t opcode;
} cgjre_vm_result;

/* M2 integer slice: static (I...)I methods without class initialization. */
cgjre_vm_result cgjre_vm_execute_int(const cgjre_classfile *file,
    uint16_t method_index, const int32_t *arguments, size_t argument_count,
    uint32_t step_limit);
const char *cgjre_vm_status_name(cgjre_vm_status status);

#endif
