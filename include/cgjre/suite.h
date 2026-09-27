#ifndef CGJRE_SUITE_H
#define CGJRE_SUITE_H

#include <cgjre/archive.h>
#include <cgjre/classfile.h>
#include <stddef.h>
#include <stdint.h>

#define CGJRE_SUITE_MAX_CLASSES 128u

typedef enum {
    CGJRE_SUITE_OK = 0,
    CGJRE_SUITE_ARCHIVE,
    CGJRE_SUITE_MISSING_CLASS,
    CGJRE_SUITE_BAD_CLASS,
    CGJRE_SUITE_UNSUPPORTED,
    CGJRE_SUITE_LIMIT,
    CGJRE_SUITE_NOMEM,
    CGJRE_SUITE_BAD_NAME
} cgjre_suite_status;

typedef struct cgjre_loaded_class cgjre_loaded_class;

typedef struct {
    cgjre_zip zip;
    cgjre_loaded_class *loaded[CGJRE_SUITE_MAX_CLASSES];
    uint16_t loaded_count;
    cgjre_suite_status last_status;
    cgjre_zip_status last_zip_status;
    cgjre_class_status last_class_status;
} cgjre_suite;

cgjre_suite_status cgjre_suite_open(cgjre_suite *suite,
    cgjre_zip_source source);
cgjre_suite_status cgjre_suite_load_class(cgjre_suite *suite,
    const char *internal_name, const cgjre_classfile **out);
void cgjre_suite_close(cgjre_suite *suite);
const char *cgjre_suite_status_name(cgjre_suite_status status);

#endif
