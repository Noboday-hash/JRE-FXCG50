#include <cgjre/archive.h>
#include <cgjre/manifest.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

static int file_read_at(void *context, uint64_t offset, void *destination,
    size_t length)
{
    FILE *file = context;
    if(offset > LONG_MAX || fseek(file, (long)offset, SEEK_SET) != 0)
        return -1;
    return fread(destination, 1, length, file) == length ? 0 : -1;
}

int cgjre_host_inspect(const char *path)
{
    FILE *file = fopen(path, "rb");
    cgjre_zip zip;
    cgjre_zip_source source;
    cgjre_zip_status status;
    long size;
    if(!file) { perror(path); return 1; }
    if(fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0) {
        perror(path); fclose(file); return 1;
    }
    source = (cgjre_zip_source){file_read_at, file, (uint64_t)size};
    status = cgjre_zip_open(&zip, source);
    if(status != CGJRE_ZIP_OK) {
        fprintf(stderr, "%s: %s\n", path, cgjre_zip_status_name(status));
        fclose(file); return 1;
    }
    printf("ZIP entries: %u\n", zip.count);
    for(uint16_t i = 0; i < zip.count; ++i) {
        char name[CGJRE_ZIP_MAX_NAME + 1];
        status = cgjre_zip_name(&zip, i, name);
        if(status != CGJRE_ZIP_OK) break;
        printf("%s method=%u compressed=%u inflated=%u\n", name,
            zip.entries[i].method, zip.entries[i].compressed_size,
            zip.entries[i].inflated_size);
    }
    if(status == CGJRE_ZIP_OK) {
        uint16_t manifest;
        if(cgjre_zip_find(&zip, "META-INF/MANIFEST.MF", &manifest) == CGJRE_ZIP_OK) {
            uint8_t *bytes;
            size_t length;
            status = cgjre_zip_extract(&zip, manifest, &bytes, &length);
            if(status == CGJRE_ZIP_OK) {
                cgjre_manifest manifest;
                if(cgjre_manifest_parse(&manifest, bytes, length) != 0) {
                    fprintf(stderr, "manifest: malformed manifest\n");
                    status = CGJRE_ZIP_FORMAT;
                }
                else {
                    printf("Manifest bytes: %zu\n", length);
                    for(size_t i = 0; i < manifest.count; ++i)
                        printf("%s: %s\n", manifest.attributes[i].name,
                            manifest.attributes[i].value);
                }
                free(bytes);
            }
            else fprintf(stderr, "manifest: %s\n", cgjre_zip_status_name(status));
        }
    }
    cgjre_zip_close(&zip);
    fclose(file);
    return status == CGJRE_ZIP_OK ? 0 : 1;
}
