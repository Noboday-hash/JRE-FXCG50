#include <cgjre/suite.h>
#include <stdlib.h>
#include <string.h>

struct cgjre_loaded_class {
    char name[CGJRE_ZIP_MAX_NAME + 1];
    uint8_t *bytes;
    cgjre_classfile classfile;
};

static cgjre_suite_status record(cgjre_suite *suite,
    cgjre_suite_status status)
{
    suite->last_status = status;
    return status;
}

static int valid_name(const char *name, size_t *length)
{
    size_t component = 0;
    size_t n = strlen(name);
    if(!n || n + 6u > CGJRE_ZIP_MAX_NAME || name[0] == '/' ||
       strncmp(name, "java/", 5) == 0 ||
       strncmp(name, "javax/", 6) == 0) return 0;
    for(size_t i = 0; i < n; ++i) {
        char c = name[i];
        if(c == '/') {
            if(i == component) return 0;
            component = i + 1;
        }
        else if(c == '\\' || c == ':' || c == '.') return 0;
    }
    if(component == n) return 0;
    *length = n;
    return 1;
}

static int matching_class_name(const cgjre_classfile *file,
    const char *requested, size_t length)
{
    uint16_t index = file->this_class;
    if(!index || index >= file->cp_count || file->cp[index].tag != 7)
        return 0;
    const uint8_t *class_ref = file->bytes + file->cp[index].offset;
    index = ((uint16_t)class_ref[0] << 8) | class_ref[1];
    if(!index || index >= file->cp_count || file->cp[index].tag != 1 ||
       file->cp[index].length != length) return 0;
    return memcmp(file->bytes + file->cp[index].offset,
        requested, length) == 0;
}

const char *cgjre_suite_status_name(cgjre_suite_status status)
{
    switch(status) {
    case CGJRE_SUITE_OK: return "ok";
    case CGJRE_SUITE_ARCHIVE: return "archive error";
    case CGJRE_SUITE_MISSING_CLASS: return "missing class";
    case CGJRE_SUITE_BAD_CLASS: return "class name or format mismatch";
    case CGJRE_SUITE_UNSUPPORTED: return "unsupported class format";
    case CGJRE_SUITE_LIMIT: return "class repository limit reached";
    case CGJRE_SUITE_NOMEM: return "out of memory";
    case CGJRE_SUITE_BAD_NAME: return "invalid or protected class name";
    }
    return "unknown suite error";
}

cgjre_suite_status cgjre_suite_open(cgjre_suite *suite,
    cgjre_zip_source source)
{
    if(!suite) return CGJRE_SUITE_BAD_NAME;
    memset(suite, 0, sizeof(*suite));
    suite->last_zip_status = cgjre_zip_open(&suite->zip, source);
    return record(suite, suite->last_zip_status == CGJRE_ZIP_OK ?
        CGJRE_SUITE_OK : CGJRE_SUITE_ARCHIVE);
}

void cgjre_suite_close(cgjre_suite *suite)
{
    if(!suite) return;
    for(uint16_t i = 0; i < suite->loaded_count; ++i) {
        cgjre_loaded_class *item = suite->loaded[i];
        cgjre_classfile_close(&item->classfile);
        free(item->bytes);
        free(item);
    }
    cgjre_zip_close(&suite->zip);
    memset(suite, 0, sizeof(*suite));
}

cgjre_suite_status cgjre_suite_load_class(cgjre_suite *suite,
    const char *internal_name, const cgjre_classfile **out)
{
    size_t length, inflated;
    uint16_t entry;
    char path[CGJRE_ZIP_MAX_NAME + 1];
    cgjre_loaded_class *item;
    if(!suite || !internal_name || !out)
        return CGJRE_SUITE_BAD_NAME;
    *out = NULL;
    suite->last_zip_status = CGJRE_ZIP_OK;
    suite->last_class_status = CGJRE_CLASS_OK;
    if(!valid_name(internal_name, &length))
        return record(suite, CGJRE_SUITE_BAD_NAME);
    for(uint16_t i = 0; i < suite->loaded_count; ++i)
        if(strcmp(suite->loaded[i]->name, internal_name) == 0) {
            *out = &suite->loaded[i]->classfile;
            return record(suite, CGJRE_SUITE_OK);
        }
    if(suite->loaded_count == CGJRE_SUITE_MAX_CLASSES)
        return record(suite, CGJRE_SUITE_LIMIT);
    memcpy(path, internal_name, length);
    memcpy(path + length, ".class", 7);
    suite->last_zip_status = cgjre_zip_find(&suite->zip, path, &entry);
    if(suite->last_zip_status != CGJRE_ZIP_OK)
        return record(suite, suite->last_zip_status == CGJRE_ZIP_FORMAT ?
            CGJRE_SUITE_MISSING_CLASS : CGJRE_SUITE_ARCHIVE);
    if(suite->zip.entries[entry].inflated_size > CGJRE_CLASS_MAX_BYTES)
        return record(suite, CGJRE_SUITE_LIMIT);
    item = calloc(1, sizeof(*item));
    if(!item) return record(suite, CGJRE_SUITE_NOMEM);
    memcpy(item->name, internal_name, length + 1);
    suite->last_zip_status = cgjre_zip_extract(&suite->zip, entry,
        &item->bytes, &inflated);
    if(suite->last_zip_status != CGJRE_ZIP_OK) {
        free(item);
        return record(suite, CGJRE_SUITE_ARCHIVE);
    }
    suite->last_class_status = cgjre_classfile_parse(&item->classfile,
        item->bytes, inflated);
    if(suite->last_class_status != CGJRE_CLASS_OK) {
        free(item->bytes);
        free(item);
        return record(suite,
            suite->last_class_status == CGJRE_CLASS_UNSUPPORTED ?
            CGJRE_SUITE_UNSUPPORTED : CGJRE_SUITE_BAD_CLASS);
    }
    if(!matching_class_name(&item->classfile, internal_name, length)) {
        cgjre_classfile_close(&item->classfile);
        free(item->bytes);
        free(item);
        return record(suite, CGJRE_SUITE_BAD_CLASS);
    }
    suite->loaded[suite->loaded_count++] = item;
    *out = &item->classfile;
    return record(suite, CGJRE_SUITE_OK);
}
