#ifndef CGJRE_ARCHIVE_H
#define CGJRE_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#define CGJRE_ZIP_MAX_ENTRIES 1024u
#define CGJRE_ZIP_MAX_NAME 255u
#define CGJRE_ZIP_MAX_CENTRAL_BYTES (1024u * 1024u)
#define CGJRE_ZIP_MAX_RESOURCE_BYTES (4u * 1024u * 1024u)

typedef enum {
    CGJRE_ZIP_OK = 0,
    CGJRE_ZIP_IO,
    CGJRE_ZIP_FORMAT,
    CGJRE_ZIP_LIMIT,
    CGJRE_ZIP_UNSUPPORTED,
    CGJRE_ZIP_CRC,
    CGJRE_ZIP_NOMEM
} cgjre_zip_status;

/* read_at must fill exactly length bytes and return zero on success. */
typedef int (*cgjre_zip_read_at)(void *context, uint64_t offset,
    void *destination, size_t length);

typedef struct {
    cgjre_zip_read_at read_at;
    void *context;
    uint64_t size;
} cgjre_zip_source;

typedef struct {
    uint32_t local_offset;
    uint32_t compressed_size;
    uint32_t inflated_size;
    uint32_t crc32;
    uint16_t flags;
    uint16_t method;
    uint16_t name_length;
    uint32_t name_offset;
} cgjre_zip_entry;

typedef struct {
    cgjre_zip_source source;
    cgjre_zip_entry *entries;
    uint16_t count;
} cgjre_zip;

const char *cgjre_zip_status_name(cgjre_zip_status status);
cgjre_zip_status cgjre_zip_open(cgjre_zip *zip, cgjre_zip_source source);
void cgjre_zip_close(cgjre_zip *zip);
cgjre_zip_status cgjre_zip_name(const cgjre_zip *zip, uint16_t index,
    char name[CGJRE_ZIP_MAX_NAME + 1]);
cgjre_zip_status cgjre_zip_find(const cgjre_zip *zip, const char *name,
    uint16_t *index);
/* Caller owns the returned buffer. Stored entries only in this slice. */
cgjre_zip_status cgjre_zip_extract(const cgjre_zip *zip, uint16_t index,
    uint8_t **bytes, size_t *length);

#endif
