#ifndef CGJRE_VM_H
#define CGJRE_VM_H

#include <cgjre/classfile.h>
#include <stddef.h>
#include <stdint.h>

#define CGJRE_VM_MAX_FRAMES 32u
#define CGJRE_VM_MAX_SLOTS 4096u
#define CGJRE_VM_MAX_HANDLES 256u
#define CGJRE_VM_MAX_CLASS_DEPTH 16u
#define CGJRE_VM_MAX_OBJECT_FIELDS 4096u
#define CGJRE_VM_MAX_ARRAY_LENGTH 16384u
#define CGJRE_VM_ARRAY_BUDGET_BYTES (256u * 1024u)

typedef enum {
    CGJRE_SLOT_EMPTY = 0,
    CGJRE_SLOT_INT,
    CGJRE_SLOT_REFERENCE,
    CGJRE_SLOT_UNINITIALIZED_REFERENCE,
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
    CGJRE_VM_MISSING_MEMBER,
    CGJRE_VM_MISSING_FIELD,
    CGJRE_VM_MISSING_CLASS,
    CGJRE_VM_CLASS_LOAD_ERROR,
    CGJRE_VM_DIVIDE_BY_ZERO,
    CGJRE_VM_NULL_REFERENCE,
    CGJRE_VM_ARRAY_BOUNDS,
    CGJRE_VM_ARRAY_STORE,
    CGJRE_VM_BAD_CAST,
    CGJRE_VM_NEGATIVE_ARRAY_SIZE,
    CGJRE_VM_UNINITIALIZED_OBJECT,
    CGJRE_VM_LIMIT,
    CGJRE_VM_NOMEM
} cgjre_vm_status;

typedef struct {
    cgjre_vm_status status;
    int32_t value;
    uint32_t steps;
    uint32_t pc;
    uint8_t opcode;
    uint16_t method_index;
    uint16_t cp_index;
    const cgjre_classfile *active_class; /* Borrowed until its repository closes. */
} cgjre_vm_result;

typedef cgjre_vm_status (*cgjre_vm_resolve_class)(void *context,
    const char *internal_name, const cgjre_classfile **out);

/* M2 integer slice: static (I...)I methods without class initialization. */
cgjre_vm_result cgjre_vm_execute_int(const cgjre_classfile *file,
    uint16_t method_index, const int32_t *arguments, size_t argument_count,
    uint32_t step_limit);
cgjre_vm_result cgjre_vm_execute_int_resolved(const cgjre_classfile *file,
    uint16_t method_index, const int32_t *arguments, size_t argument_count,
    uint32_t step_limit, cgjre_vm_resolve_class resolver, void *context);
const char *cgjre_vm_status_name(cgjre_vm_status status);

#endif
