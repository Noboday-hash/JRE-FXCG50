#ifndef CGJRE_MANIFEST_H
#define CGJRE_MANIFEST_H

#include <stddef.h>
#include <stdint.h>

#define CGJRE_MANIFEST_MAX_BYTES 32768u
#define CGJRE_MANIFEST_MAX_ATTRIBUTES 64u
#define CGJRE_MANIFEST_MAX_NAME 70u
#define CGJRE_MANIFEST_MAX_VALUE 512u

typedef struct {
    char name[CGJRE_MANIFEST_MAX_NAME + 1];
    char value[CGJRE_MANIFEST_MAX_VALUE + 1];
} cgjre_manifest_attribute;

typedef struct {
    cgjre_manifest_attribute attributes[CGJRE_MANIFEST_MAX_ATTRIBUTES];
    size_t count;
} cgjre_manifest;

typedef struct {
    char name[129];
    char icon[256];
    char class_name[256];
} cgjre_midlet_decl;

/* Parses the main section. Named sections following its blank line are ignored. */
int cgjre_manifest_parse(cgjre_manifest *manifest, const uint8_t *bytes,
    size_t length);
const char *cgjre_manifest_get(const cgjre_manifest *manifest, const char *name);
/* 0: found, 1: absent, -1: malformed; ordinal starts at 1. */
int cgjre_manifest_midlet(const cgjre_manifest *manifest, unsigned ordinal,
    cgjre_midlet_decl *out);

#endif
