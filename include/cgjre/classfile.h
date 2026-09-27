#ifndef CGJRE_CLASSFILE_H
#define CGJRE_CLASSFILE_H

#include <stddef.h>
#include <stdint.h>

#define CGJRE_CLASS_MAX_BYTES (1024u * 1024u)
#define CGJRE_CLASS_MAX_POOL 4096u
#define CGJRE_CLASS_MAX_MEMBERS 1024u

typedef enum {
    CGJRE_CLASS_OK = 0,
    CGJRE_CLASS_FORMAT,
    CGJRE_CLASS_UNSUPPORTED,
    CGJRE_CLASS_LIMIT,
    CGJRE_CLASS_NOMEM
} cgjre_class_status;

typedef struct {
    uint8_t tag;
    uint32_t offset;
    uint16_t length;
} cgjre_cp_entry;

typedef struct {
    uint16_t access_flags;
    uint16_t name_index;
    uint16_t descriptor_index;
    uint32_t code_offset;
    uint32_t code_length;
    uint16_t max_stack;
    uint16_t max_locals;
    uint16_t exception_count;
    uint16_t constant_value_index;
    uint16_t line_count;
    uint16_t first_line;
} cgjre_class_member;

/* All offsets point into bytes, which the caller must retain until close. */
typedef struct {
    const uint8_t *bytes;
    size_t length;
    uint16_t minor, major;
    uint16_t access_flags, this_class, super_class;
    uint16_t cp_count, field_count, method_count;
    cgjre_cp_entry *cp;
    cgjre_class_member *fields, *methods;
    uint16_t interface_count;
    uint16_t *interfaces;
    uint16_t source_file_index;
} cgjre_classfile;

cgjre_class_status cgjre_classfile_parse(cgjre_classfile *out,
    const uint8_t *bytes, size_t length);
void cgjre_classfile_close(cgjre_classfile *file);
const char *cgjre_class_status_name(cgjre_class_status status);
/* Decode modified UTF-8 to UTF-16 code units. Pass NULL to measure units. */
cgjre_class_status cgjre_class_utf16(const cgjre_classfile *file,
    uint16_t index, uint16_t *destination, size_t capacity, size_t *units);
/* Copies only ASCII class/member names; non-ASCII produces UNSUPPORTED. */
cgjre_class_status cgjre_class_ascii(const cgjre_classfile *file,
    uint16_t index, char *destination, size_t capacity);

#endif
