#include <cgjre/classfile.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *bytes;
    size_t position, end;
} reader;

static int take(reader *r, size_t n, size_t *offset)
{
    if(n > r->end - r->position) return 0;
    *offset = r->position;
    r->position += n;
    return 1;
}

static int u1(reader *r, uint8_t *value)
{
    size_t at;
    if(!take(r, 1, &at)) return 0;
    *value = r->bytes[at];
    return 1;
}

static int u2(reader *r, uint16_t *value)
{
    size_t at;
    if(!take(r, 2, &at)) return 0;
    *value = ((uint16_t)r->bytes[at] << 8) | r->bytes[at + 1];
    return 1;
}

static int u4(reader *r, uint32_t *value)
{
    size_t at;
    if(!take(r, 4, &at)) return 0;
    *value = ((uint32_t)r->bytes[at] << 24) |
        ((uint32_t)r->bytes[at + 1] << 16) |
        ((uint32_t)r->bytes[at + 2] << 8) | r->bytes[at + 3];
    return 1;
}

static int mutf8_next(const uint8_t *bytes, size_t length,
    size_t *position, uint16_t *unit)
{
    unsigned a, b, c;
    if(*position >= length) return 0;
    a = bytes[(*position)++];
    if(a > 0 && a < 0x80) { *unit = (uint16_t)a; return 1; }
    if(a >= 0xc0 && a <= 0xdf && *position < length) {
        b = bytes[(*position)++];
        if((b & 0xc0) != 0x80 || (a == 0xc0 && b != 0x80) || a == 0xc1)
            return 0;
        *unit = (uint16_t)(((a & 31u) << 6) | (b & 63u));
        return 1;
    }
    if(a >= 0xe0 && a <= 0xef && length - *position >= 2) {
        b = bytes[(*position)++]; c = bytes[(*position)++];
        if((b & 0xc0) != 0x80 || (c & 0xc0) != 0x80 ||
           (a == 0xe0 && b < 0xa0)) return 0;
        *unit = (uint16_t)(((a & 15u) << 12) | ((b & 63u) << 6) | (c & 63u));
        return 1;
    }
    return 0;
}

static int cp_is(const cgjre_classfile *file, uint16_t index, uint8_t tag)
{
    return index > 0 && index < file->cp_count && file->cp[index].tag == tag;
}

static uint16_t be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static int utf8_equals(const cgjre_classfile *file, uint16_t index,
    const char *text)
{
    const cgjre_cp_entry *entry;
    size_t length = strlen(text);
    if(!cp_is(file, index, 1)) return 0;
    entry = &file->cp[index];
    return entry->length == length &&
        memcmp(file->bytes + entry->offset, text, length) == 0;
}

cgjre_class_status cgjre_class_utf16(const cgjre_classfile *file,
    uint16_t index, uint16_t *destination, size_t capacity, size_t *units)
{
    const cgjre_cp_entry *entry;
    size_t cursor = 0, count = 0;
    if(!file || !units || !cp_is(file, index, 1)) return CGJRE_CLASS_FORMAT;
    entry = &file->cp[index];
    while(cursor < entry->length) {
        uint16_t unit;
        if(!mutf8_next(file->bytes + entry->offset, entry->length,
            &cursor, &unit)) return CGJRE_CLASS_FORMAT;
        if(destination && count < capacity) destination[count] = unit;
        ++count;
    }
    *units = count;
    return destination && capacity < count ? CGJRE_CLASS_LIMIT : CGJRE_CLASS_OK;
}

cgjre_class_status cgjre_class_ascii(const cgjre_classfile *file,
    uint16_t index, char *destination, size_t capacity)
{
    const cgjre_cp_entry *entry;
    if(!file || !destination || !cp_is(file, index, 1))
        return CGJRE_CLASS_FORMAT;
    entry = &file->cp[index];
    if((size_t)entry->length + 1 > capacity) return CGJRE_CLASS_LIMIT;
    for(size_t i = 0; i < entry->length; ++i) {
        uint8_t c = file->bytes[entry->offset + i];
        if(c == 0 || c > 0x7f) return CGJRE_CLASS_UNSUPPORTED;
        destination[i] = (char)c;
    }
    destination[entry->length] = 0;
    return CGJRE_CLASS_OK;
}

static int field_type(const uint8_t *p, size_t length, size_t *cursor,
    int allow_void)
{
    size_t dimensions = 0;
    while(*cursor < length && p[*cursor] == '[') {
        ++*cursor;
        if(++dimensions > 255) return 0;
    }
    if(*cursor == length) return 0;
    uint8_t c = p[(*cursor)++];
    if(strchr("BCDFIJSZ", c)) return 1;
    if(c == 'V') return allow_void && dimensions == 0;
    if(c != 'L') return 0;
    size_t start = *cursor;
    while(*cursor < length && p[*cursor] != ';') {
        c = p[(*cursor)++];
        if(c == '.' || c == '[' || c == '(' || c == ')' || c == 0)
            return 0;
    }
    if(*cursor == length || *cursor == start) return 0;
    ++*cursor;
    return 1;
}

static int valid_descriptor(const cgjre_classfile *file,
    uint16_t index, int method)
{
    const cgjre_cp_entry *entry;
    const uint8_t *p;
    size_t length, cursor = 0;
    if(!cp_is(file, index, 1)) return 0;
    entry = &file->cp[index];
    p = file->bytes + entry->offset;
    length = entry->length;
    if(method) {
        if(!length || p[cursor++] != '(') return 0;
        while(cursor < length && p[cursor] != ')')
            if(!field_type(p, length, &cursor, 0)) return 0;
        if(cursor == length) return 0;
        ++cursor;
        if(!field_type(p, length, &cursor, 1)) return 0;
    }
    else if(!field_type(p, length, &cursor, 0)) return 0;
    return cursor == length;
}

static cgjre_class_status attributes(reader *r, cgjre_classfile *file,
    cgjre_class_member *member, int kind)
{
    uint16_t count;
    int seen_code = 0;
    if(!u2(r, &count)) return CGJRE_CLASS_FORMAT;
    for(uint16_t i = 0; i < count; ++i) {
        uint16_t name;
        uint32_t length;
        size_t start;
        reader sub;
        if(!u2(r, &name) || !u4(r, &length) ||
           !cp_is(file, name, 1) || !take(r, length, &start))
            return CGJRE_CLASS_FORMAT;
        sub = (reader){r->bytes, start, start + length};
        if(kind == 2 && utf8_equals(file, name, "Code")) {
            uint32_t code_length;
            uint16_t exception_count, nested;
            size_t code_start;
            int seen_lines = 0;
            if(seen_code++ || !u2(&sub, &member->max_stack) ||
               !u2(&sub, &member->max_locals) ||
               !u4(&sub, &code_length) || code_length == 0 ||
               code_length > 65535u || !take(&sub, code_length, &code_start) ||
               !u2(&sub, &exception_count)) return CGJRE_CLASS_FORMAT;
            member->code_offset = (uint32_t)code_start;
            member->code_length = code_length;
            member->exception_count = exception_count;
            for(uint16_t e = 0; e < exception_count; ++e) {
                uint16_t begin, end, handler, catch_type;
                if(!u2(&sub, &begin) || !u2(&sub, &end) ||
                   !u2(&sub, &handler) || !u2(&sub, &catch_type) ||
                   begin >= end || end > code_length ||
                   handler >= code_length ||
                   (catch_type && !cp_is(file, catch_type, 7)))
                    return CGJRE_CLASS_FORMAT;
            }
            if(!u2(&sub, &nested)) return CGJRE_CLASS_FORMAT;
            for(uint16_t n = 0; n < nested; ++n) {
                uint16_t nested_name;
                uint32_t nested_length;
                size_t nested_start;
                if(!u2(&sub, &nested_name) || !u4(&sub, &nested_length) ||
                   !cp_is(file, nested_name, 1) ||
                   !take(&sub, nested_length, &nested_start))
                    return CGJRE_CLASS_FORMAT;
                if(utf8_equals(file, nested_name, "LineNumberTable")) {
                    reader lines = {r->bytes, nested_start,
                        nested_start + nested_length};
                    uint16_t line_count;
                    if(seen_lines++ || !u2(&lines, &line_count) ||
                       nested_length != 2u + 4u * line_count)
                        return CGJRE_CLASS_FORMAT;
                    member->line_count = line_count;
                    for(uint16_t line = 0; line < line_count; ++line) {
                        uint16_t pc, number;
                        if(!u2(&lines, &pc) || !u2(&lines, &number) ||
                           pc >= code_length) return CGJRE_CLASS_FORMAT;
                        if(line == 0) member->first_line = number;
                    }
                }
                /* CLDC StackMap is bounded and skipped; execution checks remain required. */
            }
            if(sub.position != sub.end) return CGJRE_CLASS_FORMAT;
        }
        else if(kind == 1 && utf8_equals(file, name, "ConstantValue")) {
            uint16_t value;
            if(member->constant_value_index || length != 2 ||
               !u2(&sub, &value) || value == 0 ||
               value >= file->cp_count ||
               (file->cp[value].tag != 3 && file->cp[value].tag != 4 &&
                file->cp[value].tag != 5 && file->cp[value].tag != 6 &&
                file->cp[value].tag != 8)) return CGJRE_CLASS_FORMAT;
            member->constant_value_index = value;
        }
        else if(kind == 0 && utf8_equals(file, name, "SourceFile")) {
            uint16_t source;
            if(file->source_file_index || length != 2 ||
               !u2(&sub, &source) || !cp_is(file, source, 1))
                return CGJRE_CLASS_FORMAT;
            file->source_file_index = source;
        }
    }
    if(kind == 2 && !!(member->access_flags & 0x0500u) == !!seen_code)
        return CGJRE_CLASS_FORMAT;
    return CGJRE_CLASS_OK;
}

static cgjre_class_status members(reader *r, cgjre_classfile *file,
    cgjre_class_member **members_out, uint16_t *count_out, int method)
{
    uint16_t count;
    cgjre_class_member *members;
    if(!u2(r, &count)) return CGJRE_CLASS_FORMAT;
    if(count > CGJRE_CLASS_MAX_MEMBERS) return CGJRE_CLASS_LIMIT;
    members = calloc(count ? count : 1u, sizeof(*members));
    if(!members) return CGJRE_CLASS_NOMEM;
    *members_out = members;
    *count_out = count;
    for(uint16_t i = 0; i < count; ++i) {
        cgjre_class_member *m = &members[i];
        cgjre_class_status status;
        if(!u2(r, &m->access_flags) || !u2(r, &m->name_index) ||
           !u2(r, &m->descriptor_index) ||
           !cp_is(file, m->name_index, 1) ||
           !valid_descriptor(file, m->descriptor_index, method))
            return CGJRE_CLASS_FORMAT;
        status = attributes(r, file, m, method ? 2 : 1);
        if(status != CGJRE_CLASS_OK) return status;
    }
    return CGJRE_CLASS_OK;
}

void cgjre_classfile_close(cgjre_classfile *file)
{
    if(!file) return;
    free(file->cp);
    free(file->interfaces);
    free(file->fields);
    free(file->methods);
    memset(file, 0, sizeof(*file));
}

const char *cgjre_class_status_name(cgjre_class_status status)
{
    switch(status) {
    case CGJRE_CLASS_OK: return "ok";
    case CGJRE_CLASS_FORMAT: return "malformed class file";
    case CGJRE_CLASS_UNSUPPORTED: return "unsupported class format";
    case CGJRE_CLASS_LIMIT: return "class limit exceeded";
    case CGJRE_CLASS_NOMEM: return "out of memory";
    }
    return "unknown class error";
}

cgjre_class_status cgjre_classfile_parse(cgjre_classfile *out,
    const uint8_t *bytes, size_t length)
{
    reader r;
    uint32_t magic;
    cgjre_class_status status = CGJRE_CLASS_FORMAT;
    if(!out || !bytes) return status;
    memset(out, 0, sizeof(*out));
    if(length > CGJRE_CLASS_MAX_BYTES) return CGJRE_CLASS_LIMIT;
    out->bytes = bytes;
    out->length = length;
    r = (reader){bytes, 0, length};
    if(!u4(&r, &magic) || magic != 0xcafebabeu ||
       !u2(&r, &out->minor) || !u2(&r, &out->major)) goto fail;
    if(!((out->major == 45 && out->minor == 3) ||
         (out->major == 46 && out->minor == 0))) {
        status = CGJRE_CLASS_UNSUPPORTED; goto fail;
    }
    if(!u2(&r, &out->cp_count)) goto fail;
    if(out->cp_count == 0 || out->cp_count > CGJRE_CLASS_MAX_POOL) {
        status = CGJRE_CLASS_LIMIT; goto fail;
    }
    out->cp = calloc(out->cp_count, sizeof(*out->cp));
    if(!out->cp) { status = CGJRE_CLASS_NOMEM; goto fail; }
    for(uint16_t i = 1; i < out->cp_count; ++i) {
        cgjre_cp_entry *entry = &out->cp[i];
        uint8_t tag;
        size_t offset;
        if(!u1(&r, &tag)) goto fail;
        entry->tag = tag;
        entry->offset = (uint32_t)r.position;
        switch(tag) {
        case 1: {
            if(!u2(&r, &entry->length) ||
               !take(&r, entry->length, &offset)) goto fail;
            entry->offset = (uint32_t)offset;
            size_t pos = 0;
            while(pos < entry->length) {
                uint16_t unit;
                if(!mutf8_next(bytes + offset, entry->length, &pos, &unit))
                    goto fail;
            }
            break;
        }
        case 3: case 4:
            if(!take(&r, 4, &offset)) goto fail;
            break;
        case 5: case 6:
            if(!take(&r, 8, &offset) || i + 1 >= out->cp_count) goto fail;
            ++i;
            break;
        case 7: case 8:
            if(!take(&r, 2, &offset)) goto fail;
            break;
        case 9: case 10: case 11: case 12:
            if(!take(&r, 4, &offset)) goto fail;
            break;
        default:
            status = CGJRE_CLASS_UNSUPPORTED; goto fail;
        }
    }
    for(uint16_t i = 1; i < out->cp_count; ++i) {
        const cgjre_cp_entry *entry = &out->cp[i];
        const uint8_t *p = bytes + entry->offset;
        if((entry->tag == 7 || entry->tag == 8) &&
           !cp_is(out, be16(p), 1)) goto fail;
        if(entry->tag == 12 &&
           (!cp_is(out, be16(p), 1) || !cp_is(out, be16(p + 2), 1)))
            goto fail;
        if((entry->tag == 9 || entry->tag == 10 || entry->tag == 11) &&
           (!cp_is(out, be16(p), 7) || !cp_is(out, be16(p + 2), 12)))
            goto fail;
        if(entry->tag == 9 || entry->tag == 10 || entry->tag == 11) {
            const cgjre_cp_entry *name_type = &out->cp[be16(p + 2)];
            uint16_t descriptor = be16(bytes + name_type->offset + 2);
            if(!valid_descriptor(out, descriptor, entry->tag != 9)) goto fail;
        }
    }
    if(!u2(&r, &out->access_flags) || !u2(&r, &out->this_class) ||
       !u2(&r, &out->super_class) || !cp_is(out, out->this_class, 7) ||
       (out->super_class && !cp_is(out, out->super_class, 7)) ||
       !u2(&r, &out->interface_count)) goto fail;
    if(out->interface_count > CGJRE_CLASS_MAX_MEMBERS) {
        status = CGJRE_CLASS_LIMIT; goto fail;
    }
    out->interfaces = calloc(out->interface_count ? out->interface_count : 1u,
        sizeof(*out->interfaces));
    if(!out->interfaces) { status = CGJRE_CLASS_NOMEM; goto fail; }
    for(uint16_t i = 0; i < out->interface_count; ++i)
        if(!u2(&r, &out->interfaces[i]) ||
           !cp_is(out, out->interfaces[i], 7)) goto fail;
    status = members(&r, out, &out->fields, &out->field_count, 0);
    if(status != CGJRE_CLASS_OK) goto fail;
    status = members(&r, out, &out->methods, &out->method_count, 1);
    if(status != CGJRE_CLASS_OK) goto fail;
    status = attributes(&r, out, NULL, 0);
    if(status != CGJRE_CLASS_OK) goto fail;
    if(r.position != r.end) { status = CGJRE_CLASS_FORMAT; goto fail; }
    return CGJRE_CLASS_OK;
fail:
    cgjre_classfile_close(out);
    return status;
}
