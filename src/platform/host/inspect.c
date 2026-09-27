#include <cgjre/archive.h>
#include <cgjre/manifest.h>
#include <cgjre/classfile.h>
#include <cgjre/bytecode_scan.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

static uint16_t read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static void print_references(const cgjre_classfile *file)
{
    for(uint16_t i = 1; i < file->cp_count; ++i) {
        const cgjre_cp_entry *ref = &file->cp[i];
        if(ref->tag != 9 && ref->tag != 10 && ref->tag != 11) continue;
        const uint8_t *p = file->bytes + ref->offset;
        const cgjre_cp_entry *klass = &file->cp[read_be16(p)];
        const cgjre_cp_entry *name_type = &file->cp[read_be16(p + 2)];
        char owner[256], name[256], descriptor[256];
        if(cgjre_class_ascii(file,
               read_be16(file->bytes + klass->offset), owner,
               sizeof(owner)) != CGJRE_CLASS_OK ||
           cgjre_class_ascii(file,
               read_be16(file->bytes + name_type->offset), name,
               sizeof(name)) != CGJRE_CLASS_OK ||
           cgjre_class_ascii(file,
               read_be16(file->bytes + name_type->offset + 2), descriptor,
               sizeof(descriptor)) != CGJRE_CLASS_OK)
        {
            printf("    reference cp#%u name cannot be shown as bounded ASCII\n", i);
            continue;
        }
        printf("    reference %s.%s%s kind=%s\n", owner, name,
            descriptor, ref->tag == 9 ? "field" : "method");
        if(strncmp(owner, "javax/microedition/media/", 25) == 0 ||
           strncmp(owner, "javax/microedition/io/", 22) == 0 ||
           strncmp(owner, "java/net/", 9) == 0 ||
           strncmp(owner, "com/nokia/", 10) == 0 ||
           strncmp(owner, "com/siemens/", 12) == 0)
            printf("      likely incompatibility: API family is out of scope\n");
    }
}

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
        if(status != CGJRE_ZIP_OK) {
            fprintf(stderr, "entry %u: %s\n", i,
                cgjre_zip_status_name(status));
            break;
        }
        printf("%s method=%u compressed=%u inflated=%u\n", name,
            zip.entries[i].method, zip.entries[i].compressed_size,
            zip.entries[i].inflated_size);
        size_t name_length = strlen(name);
        if(name_length > 6 && strcmp(name + name_length - 6, ".class") == 0) {
            uint8_t *bytes;
            size_t length;
            cgjre_classfile file;
            cgjre_class_status class_status;
            status = cgjre_zip_extract(&zip, i, &bytes, &length);
            if(status != CGJRE_ZIP_OK) {
                fprintf(stderr, "%s: %s\n", name,
                    cgjre_zip_status_name(status));
                break;
            }
            class_status = cgjre_classfile_parse(&file, bytes, length);
            if(class_status == CGJRE_CLASS_OK) {
                uint32_t opcode_counts[256] = {0};
                printf("  class version=%u.%u fields=%u methods=%u\n",
                    file.major, file.minor, file.field_count, file.method_count);
                if(file.source_file_index) {
                    char source_name[256];
                    if(cgjre_class_ascii(&file, file.source_file_index,
                        source_name, sizeof(source_name)) == CGJRE_CLASS_OK)
                        printf("    source=%s\n", source_name);
                }
                for(uint16_t m = 0; m < file.method_count; ++m) {
                    char method_name[256], descriptor[256];
                    cgjre_class_member *member = &file.methods[m];
                    if(cgjre_class_ascii(&file, member->name_index,
                        method_name, sizeof(method_name)) == CGJRE_CLASS_OK &&
                       cgjre_class_ascii(&file, member->descriptor_index,
                        descriptor, sizeof(descriptor)) == CGJRE_CLASS_OK)
                        printf("    %s%s native=%s code=%u\n", method_name,
                            descriptor, (member->access_flags & 0x0100u) ?
                            "yes" : "no", member->code_length);
                    else
                        printf("    method #%u name or descriptor cannot be shown as bounded ASCII\n", m);
                    if(member->line_count)
                        printf("      line entries=%u first=%u\n",
                            member->line_count, member->first_line);
                    if(member->access_flags & 0x0100u)
                        printf("      likely incompatibility: application native method\n");
                    if(member->code_length) {
                        uint32_t method_counts[256];
                        if(cgjre_bytecode_scan(bytes + member->code_offset,
                           member->code_length, method_counts) != 0) {
                            fprintf(stderr, "%s: malformed bytecode\n", name);
                            status = CGJRE_ZIP_FORMAT;
                            break;
                        }
                        for(unsigned op = 0; op < 256; ++op)
                            opcode_counts[op] += method_counts[op];
                    }
                }
                if(status == CGJRE_ZIP_OK) {
                    for(unsigned op = 0; op < 256; ++op)
                        if(opcode_counts[op])
                            printf("    opcode 0x%02x count=%u%s\n", op,
                                opcode_counts[op],
                                op == 0xa8 || op == 0xa9 || op == 0xba ||
                                op == 0xc9 || op >= 0xca ?
                                " unsupported for baseline" : "");
                    print_references(&file);
                }
                cgjre_classfile_close(&file);
            }
            else {
                fprintf(stderr, "%s: %s\n", name,
                    cgjre_class_status_name(class_status));
                status = CGJRE_ZIP_FORMAT;
            }
            free(bytes);
            if(status != CGJRE_ZIP_OK) break;
        }
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
                    for(unsigned ordinal = 1; ordinal <= 99; ++ordinal) {
                        cgjre_midlet_decl midlet;
                        int found = cgjre_manifest_midlet(&manifest, ordinal,
                            &midlet);
                        if(found == 1) continue;
                        if(found < 0) {
                            fprintf(stderr, "manifest: malformed MIDlet-%u\n",
                                ordinal);
                            status = CGJRE_ZIP_FORMAT;
                            break;
                        }
                        printf("MIDlet %u class=%s name=%s icon=%s\n",
                            ordinal, midlet.class_name, midlet.name, midlet.icon);
                    }
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
