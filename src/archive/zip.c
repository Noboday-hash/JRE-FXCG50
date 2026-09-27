#include <cgjre/archive.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int within(uint64_t offset, uint64_t length, uint64_t size)
{
    return offset <= size && length <= size - offset;
}

static int safe_name(const char *name, size_t length)
{
    size_t component = 0;
    if(!length || name[0] == '/') return 0;
    for(size_t i = 0; i <= length; ++i) {
        if(i == length || name[i] == '/') {
            size_t part = i - component;
            if(i == length && part == 0 && length > 1 &&
               name[length - 1] == '/') return 1;
            if(part == 0 || (part == 1 && name[component] == '.') ||
               (part == 2 && name[component] == '.' &&
                name[component + 1] == '.')) return 0;
            component = i + 1;
        }
        else if(name[i] == '\\' || name[i] == ':' || name[i] == 0)
            return 0;
    }
    return 1;
}

static cgjre_zip_status read_bytes(cgjre_zip_source source, uint64_t offset,
    void *destination, size_t length)
{
    if(!within(offset, length, source.size)) return CGJRE_ZIP_FORMAT;
    return source.read_at(source.context, offset, destination, length) == 0 ?
        CGJRE_ZIP_OK : CGJRE_ZIP_IO;
}

const char *cgjre_zip_status_name(cgjre_zip_status status)
{
    switch(status) {
    case CGJRE_ZIP_OK: return "ok";
    case CGJRE_ZIP_IO: return "I/O error";
    case CGJRE_ZIP_FORMAT: return "malformed ZIP";
    case CGJRE_ZIP_LIMIT: return "ZIP limit exceeded";
    case CGJRE_ZIP_UNSUPPORTED: return "unsupported ZIP feature";
    case CGJRE_ZIP_CRC: return "CRC mismatch";
    case CGJRE_ZIP_NOMEM: return "out of memory";
    }
    return "unknown ZIP error";
}

void cgjre_zip_close(cgjre_zip *zip)
{
    if(!zip) return;
    free(zip->entries);
    memset(zip, 0, sizeof(*zip));
}

cgjre_zip_status cgjre_zip_name(const cgjre_zip *zip, uint16_t index,
    char name[CGJRE_ZIP_MAX_NAME + 1])
{
    cgjre_zip_status status;
    if(!zip || !name || index >= zip->count) return CGJRE_ZIP_FORMAT;
    status = read_bytes(zip->source, zip->entries[index].name_offset,
        name, zip->entries[index].name_length);
    if(status != CGJRE_ZIP_OK) return status;
    name[zip->entries[index].name_length] = 0;
    return CGJRE_ZIP_OK;
}

cgjre_zip_status cgjre_zip_find(const cgjre_zip *zip, const char *name,
    uint16_t *index)
{
    char candidate[CGJRE_ZIP_MAX_NAME + 1];
    size_t length;
    if(!zip || !name || !index) return CGJRE_ZIP_FORMAT;
    length = strlen(name);
    if(length > CGJRE_ZIP_MAX_NAME) return CGJRE_ZIP_LIMIT;
    for(uint16_t i = 0; i < zip->count; ++i) {
        cgjre_zip_status status = cgjre_zip_name(zip, i, candidate);
        if(status != CGJRE_ZIP_OK) return status;
        if(strcmp(name, candidate) == 0) {
            *index = i;
            return CGJRE_ZIP_OK;
        }
    }
    return CGJRE_ZIP_FORMAT;
}

cgjre_zip_status cgjre_zip_open(cgjre_zip *zip, cgjre_zip_source source)
{
    uint8_t *tail = NULL;
    uint8_t header[46];
    uint64_t tail_start, tail_size, central_start, central_end, cursor;
    uint16_t count;
    cgjre_zip_status status = CGJRE_ZIP_FORMAT;
    int64_t eocd = -1;
    if(!zip || !source.read_at) return CGJRE_ZIP_FORMAT;
    memset(zip, 0, sizeof(*zip));
    zip->source = source;
    tail_size = source.size < 65557u ? source.size : 65557u;
    tail_start = source.size - tail_size;
    if(tail_size < 22u) return CGJRE_ZIP_FORMAT;
    tail = malloc((size_t)tail_size);
    if(!tail) return CGJRE_ZIP_NOMEM;
    status = read_bytes(source, tail_start, tail, (size_t)tail_size);
    if(status != CGJRE_ZIP_OK) goto fail;
    for(int64_t i = (int64_t)tail_size - 22; i >= 0; --i) {
        if(rd32(tail + i) == 0x06054b50u &&
           (uint64_t)i + 22u + rd16(tail + i + 20) == tail_size) {
            eocd = i;
            break;
        }
    }
    if(eocd < 0) { status = CGJRE_ZIP_FORMAT; goto fail; }
    const uint8_t *end = tail + eocd;
    if(rd16(end + 4) || rd16(end + 6) || rd16(end + 8) != rd16(end + 10)) {
        status = CGJRE_ZIP_UNSUPPORTED; goto fail;
    }
    count = rd16(end + 10);
    if(count == 0xffffu || rd32(end + 12) == 0xffffffffu ||
       rd32(end + 16) == 0xffffffffu) {
        status = CGJRE_ZIP_UNSUPPORTED; goto fail;
    }
    if(count > CGJRE_ZIP_MAX_ENTRIES ||
       rd32(end + 12) > CGJRE_ZIP_MAX_CENTRAL_BYTES) {
        status = CGJRE_ZIP_LIMIT; goto fail;
    }
    central_start = rd32(end + 16);
    central_end = central_start + rd32(end + 12);
    if(!within(central_start, rd32(end + 12), tail_start + (uint64_t)eocd) ||
       central_end > tail_start + (uint64_t)eocd) {
        status = CGJRE_ZIP_FORMAT; goto fail;
    }
    zip->entries = calloc(count ? count : 1u, sizeof(*zip->entries));
    if(!zip->entries) { status = CGJRE_ZIP_NOMEM; goto fail; }
    zip->count = count;
    cursor = central_start;
    for(uint16_t i = 0; i < count; ++i) {
        cgjre_zip_entry *entry = &zip->entries[i];
        uint16_t extra_length, comment_length;
        uint64_t record_length;
        uint8_t local[30];
        uint64_t data_start;
        char name[CGJRE_ZIP_MAX_NAME + 1];
        char local_name[CGJRE_ZIP_MAX_NAME + 1];
        if(!within(cursor, sizeof(header), central_end)) {
            status = CGJRE_ZIP_FORMAT; goto fail;
        }
        status = read_bytes(source, cursor, header, sizeof(header));
        if(status != CGJRE_ZIP_OK) goto fail;
        if(rd32(header) != 0x02014b50u) {
            status = CGJRE_ZIP_FORMAT; goto fail;
        }
        entry->flags = rd16(header + 8);
        entry->method = rd16(header + 10);
        entry->crc32 = rd32(header + 16);
        entry->compressed_size = rd32(header + 20);
        entry->inflated_size = rd32(header + 24);
        entry->name_length = rd16(header + 28);
        extra_length = rd16(header + 30);
        comment_length = rd16(header + 32);
        entry->local_offset = rd32(header + 42);
        record_length = 46u + entry->name_length + extra_length + comment_length;
        if(!within(cursor, record_length, central_end)) {
            status = CGJRE_ZIP_FORMAT; goto fail;
        }
        if(cursor + 46u > UINT32_MAX) {
            status = CGJRE_ZIP_UNSUPPORTED; goto fail;
        }
        if(entry->name_length == 0 || entry->name_length > CGJRE_ZIP_MAX_NAME ||
           entry->inflated_size > CGJRE_ZIP_MAX_RESOURCE_BYTES) {
            status = CGJRE_ZIP_LIMIT; goto fail;
        }
        if((entry->flags & 1u) || (entry->flags & 0x40u) ||
           rd16(header + 34) || entry->local_offset == 0xffffffffu ||
           entry->compressed_size == 0xffffffffu ||
           entry->inflated_size == 0xffffffffu) {
            status = CGJRE_ZIP_UNSUPPORTED; goto fail;
        }
        if(entry->method != 0 && entry->method != 8) {
            status = CGJRE_ZIP_UNSUPPORTED; goto fail;
        }
        if(rd16(header + 6) >= 45u) {
            status = CGJRE_ZIP_UNSUPPORTED; goto fail;
        }
        entry->name_offset = (uint32_t)(cursor + 46u);
        status = cgjre_zip_name(zip, i, name);
        if(status != CGJRE_ZIP_OK) goto fail;
        if(!safe_name(name, entry->name_length)) {
            status = CGJRE_ZIP_FORMAT; goto fail;
        }
        for(uint16_t j = 0; j < i; ++j) {
            char previous[CGJRE_ZIP_MAX_NAME + 1];
            status = cgjre_zip_name(zip, j, previous);
            if(status != CGJRE_ZIP_OK) goto fail;
            if(strcmp(name, previous) == 0) {
                status = CGJRE_ZIP_FORMAT; goto fail;
            }
        }
        status = read_bytes(source, entry->local_offset, local, sizeof(local));
        if(status != CGJRE_ZIP_OK) goto fail;
        if(rd32(local) != 0x04034b50u ||
           rd16(local + 6) != entry->flags ||
           rd16(local + 8) != entry->method ||
           rd16(local + 26) != entry->name_length) {
            status = CGJRE_ZIP_FORMAT; goto fail;
        }
        data_start = (uint64_t)entry->local_offset + 30u +
            entry->name_length + rd16(local + 28);
        if(!within(data_start, entry->compressed_size, central_start) ||
           !within((uint64_t)entry->local_offset + 30u,
                entry->name_length, central_start)) {
            status = CGJRE_ZIP_FORMAT; goto fail;
        }
        status = read_bytes(source, (uint64_t)entry->local_offset + 30u,
            local_name, entry->name_length);
        if(status != CGJRE_ZIP_OK) goto fail;
        local_name[entry->name_length] = 0;
        if(strcmp(local_name, name) != 0) {
            status = CGJRE_ZIP_FORMAT; goto fail;
        }
        cursor += record_length;
    }
    if(cursor != central_end) { status = CGJRE_ZIP_FORMAT; goto fail; }
    free(tail);
    return CGJRE_ZIP_OK;
fail:
    free(tail);
    cgjre_zip_close(zip);
    return status;
}

static uint32_t crc32_bytes(const uint8_t *bytes, size_t length)
{
    uint32_t crc = 0xffffffffu;
    for(size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for(unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return ~crc;
}

cgjre_zip_status cgjre_zip_extract(const cgjre_zip *zip, uint16_t index,
    uint8_t **bytes, size_t *length)
{
    uint8_t header[30];
    uint8_t *result;
    uint64_t data_start;
    const cgjre_zip_entry *entry;
    cgjre_zip_status status;
    if(!zip || !bytes || !length || index >= zip->count)
        return CGJRE_ZIP_FORMAT;
    *bytes = NULL;
    *length = 0;
    entry = &zip->entries[index];
    if(entry->method == 8) return CGJRE_ZIP_UNSUPPORTED;
    if(entry->compressed_size != entry->inflated_size)
        return CGJRE_ZIP_FORMAT;
    status = read_bytes(zip->source, entry->local_offset, header, sizeof(header));
    if(status != CGJRE_ZIP_OK) return status;
    if(rd32(header) != 0x04034b50u || rd16(header + 8) != entry->method ||
       rd16(header + 6) != entry->flags || rd16(header + 26) != entry->name_length)
        return CGJRE_ZIP_FORMAT;
    data_start = (uint64_t)entry->local_offset + 30u +
        rd16(header + 26) + rd16(header + 28);
    if(!within(data_start, entry->compressed_size, zip->source.size))
        return CGJRE_ZIP_FORMAT;
    result = malloc((size_t)entry->inflated_size + 1u);
    if(!result) return CGJRE_ZIP_NOMEM;
    status = read_bytes(zip->source, data_start, result, entry->inflated_size);
    if(status != CGJRE_ZIP_OK) { free(result); return status; }
    if(crc32_bytes(result, entry->inflated_size) != entry->crc32) {
        free(result);
        return CGJRE_ZIP_CRC;
    }
    result[entry->inflated_size] = 0;
    *bytes = result;
    *length = entry->inflated_size;
    return CGJRE_ZIP_OK;
}
