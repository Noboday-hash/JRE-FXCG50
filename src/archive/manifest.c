#include <cgjre/manifest.h>
#include <string.h>

static unsigned char ascii_lower(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 'a' - 'A') : c;
}

static int name_equal(const char *a, const char *b)
{
    while(*a && *b) {
        if(ascii_lower((unsigned char)*a) != ascii_lower((unsigned char)*b))
            return 0;
        ++a; ++b;
    }
    return !*a && !*b;
}

const char *cgjre_manifest_get(const cgjre_manifest *manifest, const char *name)
{
    if(!manifest || !name) return NULL;
    for(size_t i = 0; i < manifest->count; ++i)
        if(name_equal(manifest->attributes[i].name, name))
            return manifest->attributes[i].value;
    return NULL;
}

int cgjre_manifest_parse(cgjre_manifest *manifest, const uint8_t *bytes,
    size_t length)
{
    size_t cursor = 0;
    if(!manifest || !bytes || length > CGJRE_MANIFEST_MAX_BYTES) return -1;
    memset(manifest, 0, sizeof(*manifest));
    while(cursor < length) {
        size_t start = cursor, line_length, value_length;
        cgjre_manifest_attribute *attribute;
        while(cursor < length && bytes[cursor] != '\r' && bytes[cursor] != '\n')
            ++cursor;
        line_length = cursor - start;
        if(cursor < length && bytes[cursor] == '\r') ++cursor;
        if(cursor < length && bytes[cursor] == '\n') ++cursor;
        if(line_length == 0) return 0;
        if(bytes[start] == ' ') {
            if(manifest->count == 0) return -1;
            attribute = &manifest->attributes[manifest->count - 1];
            value_length = strlen(attribute->value);
            if(value_length + line_length - 1 > CGJRE_MANIFEST_MAX_VALUE)
                return -1;
            memcpy(attribute->value + value_length, bytes + start + 1,
                line_length - 1);
            attribute->value[value_length + line_length - 1] = 0;
            continue;
        }
        if(manifest->count == CGJRE_MANIFEST_MAX_ATTRIBUTES) return -1;
        size_t colon = 0;
        while(colon < line_length && bytes[start + colon] != ':') ++colon;
        if(colon == 0 || colon > CGJRE_MANIFEST_MAX_NAME ||
           colon + 1 >= line_length || bytes[start + colon + 1] != ' ' ||
           line_length - colon - 2 > CGJRE_MANIFEST_MAX_VALUE)
            return -1;
        for(size_t i = 0; i < colon; ++i) {
            unsigned char c = bytes[start + i];
            if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '-' || c == '_')) return -1;
        }
        attribute = &manifest->attributes[manifest->count];
        memcpy(attribute->name, bytes + start, colon);
        attribute->name[colon] = 0;
        if(cgjre_manifest_get(manifest, attribute->name)) return -1;
        value_length = line_length - colon - 2;
        if(memchr(bytes + start + colon + 2, 0, value_length)) return -1;
        memcpy(attribute->value, bytes + start + colon + 2, value_length);
        attribute->value[value_length] = 0;
        ++manifest->count;
    }
    return 0;
}
