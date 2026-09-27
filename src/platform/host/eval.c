#include <cgjre/classfile.h>
#include <cgjre/suite.h>
#include <cgjre/vm.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static int file_read_at(void *context, uint64_t offset,
    void *destination, size_t length)
{
    FILE *file = context;
    if(offset > LONG_MAX || fseek(file, (long)offset, SEEK_SET) != 0)
        return -1;
    return fread(destination, 1, length, file) == length ? 0 : -1;
}

static int parse_arguments(int count, char **strings, int32_t **out)
{
    *out = calloc(count ? (size_t)count : 1u, sizeof(**out));
    if(!*out) { fprintf(stderr, "out of memory\n"); return 0; }
    for(int i = 0; i < count; ++i) {
        char *end;
        long long parsed;
        errno = 0;
        parsed = strtoll(strings[i], &end, 10);
        if(errno || *end || end == strings[i] ||
           parsed < INT32_MIN || parsed > INT32_MAX) {
            fprintf(stderr, "invalid integer argument: %s\n", strings[i]);
            return 0;
        }
        (*out)[i] = (int32_t)parsed;
    }
    return 1;
}

static uint16_t find_method(const cgjre_classfile *file,
    const char *name, const char *descriptor)
{
    for(uint16_t i = 0; i < file->method_count; ++i) {
        char found_name[256], found_descriptor[256];
        if(cgjre_class_ascii(file, file->methods[i].name_index,
            found_name, sizeof(found_name)) == CGJRE_CLASS_OK &&
           cgjre_class_ascii(file, file->methods[i].descriptor_index,
            found_descriptor, sizeof(found_descriptor)) == CGJRE_CLASS_OK &&
           strcmp(found_name, name) == 0 &&
           strcmp(found_descriptor, descriptor) == 0) return i;
    }
    return UINT16_MAX;
}

static void print_vm_error(const char *label, const char *method,
    const char *descriptor, cgjre_vm_result result)
{
    const cgjre_classfile *file = result.active_class;
    char active_name[256], active_descriptor[256];
    const char *shown_name = method, *shown_descriptor = descriptor;
    if(file && result.method_index < file->method_count &&
       cgjre_class_ascii(file, file->methods[result.method_index].name_index,
           active_name, sizeof(active_name)) == CGJRE_CLASS_OK &&
       cgjre_class_ascii(file,
           file->methods[result.method_index].descriptor_index,
           active_descriptor, sizeof(active_descriptor)) == CGJRE_CLASS_OK) {
        shown_name = active_name;
        shown_descriptor = active_descriptor;
    }
    fprintf(stderr, "%s:%s%s pc=%u opcode=0x%02x: %s", label,
        shown_name, shown_descriptor, result.pc, result.opcode,
        cgjre_vm_status_name(result.status));
    if(file && result.cp_index && result.cp_index < file->cp_count &&
       (file->cp[result.cp_index].tag == 9 ||
        file->cp[result.cp_index].tag == 10)) {
        const uint8_t *reference = file->bytes +
            file->cp[result.cp_index].offset;
        uint16_t class_index = read_be16(reference);
        uint16_t name_type_index = read_be16(reference + 2);
        const uint8_t *name_type = file->bytes +
            file->cp[name_type_index].offset;
        const uint8_t *class_entry = file->bytes +
            file->cp[class_index].offset;
        char target_name[256], target_descriptor[256], owner[256];
        if(cgjre_class_ascii(file, read_be16(name_type), target_name,
           sizeof(target_name)) == CGJRE_CLASS_OK &&
           cgjre_class_ascii(file, read_be16(name_type + 2),
           target_descriptor, sizeof(target_descriptor)) == CGJRE_CLASS_OK)
            fprintf(stderr, " target=%s%s", target_name,
                target_descriptor);
        if(cgjre_class_ascii(file, read_be16(class_entry), owner,
           sizeof(owner)) == CGJRE_CLASS_OK)
            fprintf(stderr, " class=%s", owner);
    }
    else if(file && result.cp_index && result.cp_index < file->cp_count &&
            file->cp[result.cp_index].tag == 7) {
        const uint8_t *class_entry = file->bytes +
            file->cp[result.cp_index].offset;
        char owner[256];
        if(cgjre_class_ascii(file, read_be16(class_entry), owner,
           sizeof(owner)) == CGJRE_CLASS_OK)
            fprintf(stderr, " class=%s", owner);
    }
    fputc('\n', stderr);
}

static int run_method(const char *label, const cgjre_classfile *file,
    const char *method, const char *descriptor, int argument_count,
    char **arguments, cgjre_vm_resolve_class resolver, void *context)
{
    int32_t *values = NULL;
    int exit_status = 1;
    if(argument_count < 0 || !parse_arguments(argument_count,
       arguments, &values)) goto done;
    uint16_t selected = find_method(file, method, descriptor);
    if(selected == UINT16_MAX) {
        fprintf(stderr, "%s: missing member %s%s\n", label, method,
            descriptor);
        goto done;
    }
    cgjre_vm_result result = cgjre_vm_execute_int_resolved(file, selected,
        values, (size_t)argument_count, 100000u, resolver, context);
    if(result.status != CGJRE_VM_OK) {
        print_vm_error(label, method, descriptor, result);
        goto done;
    }
    printf("result=%d steps=%u\n", result.value, result.steps);
    exit_status = 0;
done:
    free(values);
    return exit_status;
}

int cgjre_host_eval_class(const char *path, const char *method,
    const char *descriptor, int argument_count, char **arguments)
{
    FILE *input = fopen(path, "rb");
    uint8_t *bytes = NULL;
    cgjre_classfile file = {0};
    long length;
    int exit_status = 1;
    if(!input) { perror(path); return 1; }
    if(fseek(input, 0, SEEK_END) != 0 || (length = ftell(input)) < 0 ||
       (unsigned long)length > CGJRE_CLASS_MAX_BYTES ||
       fseek(input, 0, SEEK_SET) != 0) {
        fprintf(stderr, "%s: invalid class file size\n", path);
        goto done;
    }
    bytes = malloc((size_t)length ? (size_t)length : 1u);
    if(!bytes) { fprintf(stderr, "out of memory\n"); goto done; }
    if(fread(bytes, 1, (size_t)length, input) != (size_t)length) {
        fprintf(stderr, "%s: short read\n", path);
        goto done;
    }
    cgjre_class_status status = cgjre_classfile_parse(&file, bytes,
        (size_t)length);
    if(status != CGJRE_CLASS_OK) {
        fprintf(stderr, "%s: %s\n", path,
            cgjre_class_status_name(status));
        goto done;
    }
    exit_status = run_method(path, &file, method, descriptor,
        argument_count, arguments, NULL, NULL);
done:
    cgjre_classfile_close(&file);
    free(bytes);
    fclose(input);
    return exit_status;
}

static cgjre_vm_status resolve_suite_class(void *context,
    const char *name, const cgjre_classfile **out)
{
    cgjre_suite_status status = cgjre_suite_load_class(context, name, out);
    switch(status) {
    case CGJRE_SUITE_OK: return CGJRE_VM_OK;
    case CGJRE_SUITE_MISSING_CLASS: return CGJRE_VM_MISSING_CLASS;
    case CGJRE_SUITE_LIMIT: return CGJRE_VM_LIMIT;
    case CGJRE_SUITE_NOMEM: return CGJRE_VM_NOMEM;
    case CGJRE_SUITE_BAD_NAME: case CGJRE_SUITE_UNSUPPORTED:
        return CGJRE_VM_UNSUPPORTED;
    case CGJRE_SUITE_ARCHIVE: case CGJRE_SUITE_BAD_CLASS:
        return CGJRE_VM_CLASS_LOAD_ERROR;
    }
    return CGJRE_VM_CLASS_LOAD_ERROR;
}

static void print_suite_detail(const cgjre_suite *suite)
{
    if(suite->last_status == CGJRE_SUITE_ARCHIVE)
        fprintf(stderr, "archive cause: %s\n",
            cgjre_zip_status_name(suite->last_zip_status));
    else if(suite->last_status == CGJRE_SUITE_UNSUPPORTED ||
            (suite->last_status == CGJRE_SUITE_BAD_CLASS &&
             suite->last_class_status != CGJRE_CLASS_OK))
        fprintf(stderr, "class cause: %s\n",
            cgjre_class_status_name(suite->last_class_status));
    else if(suite->last_status == CGJRE_SUITE_BAD_CLASS)
        fprintf(stderr, "class cause: declared name differs from JAR path\n");
}

int cgjre_host_eval_jar(const char *path, const char *class_name,
    const char *method, const char *descriptor, int argument_count,
    char **arguments)
{
    FILE *input = fopen(path, "rb");
    cgjre_suite suite;
    cgjre_zip_source source;
    const cgjre_classfile *file = NULL;
    cgjre_suite_status status;
    long length;
    int exit_status = 1;
    if(!input) { perror(path); return 1; }
    if(fseek(input, 0, SEEK_END) != 0 || (length = ftell(input)) < 0) {
        fprintf(stderr, "%s: invalid JAR size\n", path);
        fclose(input); return 1;
    }
    source = (cgjre_zip_source){file_read_at, input, (uint64_t)length};
    status = cgjre_suite_open(&suite, source);
    if(status != CGJRE_SUITE_OK) {
        fprintf(stderr, "%s: %s: %s\n", path,
            cgjre_suite_status_name(status),
            cgjre_zip_status_name(suite.last_zip_status));
        goto done;
    }
    status = cgjre_suite_load_class(&suite, class_name, &file);
    if(status != CGJRE_SUITE_OK) {
        fprintf(stderr, "%s:%s: %s\n", path, class_name,
            cgjre_suite_status_name(status));
        print_suite_detail(&suite);
        goto done;
    }
    exit_status = run_method(path, file, method, descriptor,
        argument_count, arguments, resolve_suite_class, &suite);
    if(exit_status != 0) print_suite_detail(&suite);
done:
    cgjre_suite_close(&suite);
    fclose(input);
    return exit_status;
}
