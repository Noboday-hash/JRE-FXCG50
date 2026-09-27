#include <cgjre/classfile.h>
#include <cgjre/vm.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cgjre_host_eval_class(const char *path, const char *method,
    const char *descriptor, int argument_count, char **arguments)
{
    FILE *input = NULL;
    uint8_t *bytes = NULL;
    int32_t *values = NULL;
    cgjre_classfile file;
    cgjre_class_status parse_status;
    uint16_t selected = UINT16_MAX;
    long length;
    int exit_status = 1;
    memset(&file, 0, sizeof(file));
    if(argument_count < 0) return 2;
    input = fopen(path, "rb");
    if(!input) { perror(path); goto done; }
    if(fseek(input, 0, SEEK_END) != 0 || (length = ftell(input)) < 0 ||
       (unsigned long)length > CGJRE_CLASS_MAX_BYTES ||
       fseek(input, 0, SEEK_SET) != 0) {
        fprintf(stderr, "%s: invalid class file size\n", path);
        goto done;
    }
    bytes = malloc((size_t)length ? (size_t)length : 1u);
    values = calloc(argument_count ? (size_t)argument_count : 1u,
        sizeof(*values));
    if(!bytes || !values) { fprintf(stderr, "out of memory\n"); goto done; }
    if(fread(bytes, 1, (size_t)length, input) != (size_t)length) {
        fprintf(stderr, "%s: short read\n", path);
        goto done;
    }
    for(int i = 0; i < argument_count; ++i) {
        char *end;
        long long parsed;
        errno = 0;
        parsed = strtoll(arguments[i], &end, 10);
        if(errno || *end || end == arguments[i] ||
           parsed < INT32_MIN || parsed > INT32_MAX) {
            fprintf(stderr, "invalid integer argument: %s\n", arguments[i]);
            goto done;
        }
        values[i] = (int32_t)parsed;
    }
    parse_status = cgjre_classfile_parse(&file, bytes, (size_t)length);
    if(parse_status != CGJRE_CLASS_OK) {
        fprintf(stderr, "%s: %s\n", path,
            cgjre_class_status_name(parse_status));
        goto done;
    }
    for(uint16_t i = 0; i < file.method_count; ++i) {
        char found_name[256], found_descriptor[256];
        if(cgjre_class_ascii(&file, file.methods[i].name_index,
            found_name, sizeof(found_name)) == CGJRE_CLASS_OK &&
           cgjre_class_ascii(&file, file.methods[i].descriptor_index,
            found_descriptor, sizeof(found_descriptor)) == CGJRE_CLASS_OK &&
           strcmp(found_name, method) == 0 &&
           strcmp(found_descriptor, descriptor) == 0) {
            selected = i;
            break;
        }
    }
    if(selected == UINT16_MAX) {
        fprintf(stderr, "%s: missing member %s%s\n", path, method,
            descriptor);
        goto done;
    }
    cgjre_vm_result result = cgjre_vm_execute_int(&file, selected, values,
        (size_t)argument_count, 100000u);
    if(result.status != CGJRE_VM_OK) {
        fprintf(stderr, "%s:%s%s pc=%u opcode=0x%02x: %s\n", path,
            method, descriptor, result.pc, result.opcode,
            cgjre_vm_status_name(result.status));
        goto done;
    }
    printf("result=%d steps=%u\n", result.value, result.steps);
    exit_status = 0;
done:
    cgjre_classfile_close(&file);
    free(values);
    free(bytes);
    if(input) fclose(input);
    return exit_status;
}
