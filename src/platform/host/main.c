#include <cgjre/platform.h>
#include <stdio.h>
#include <string.h>

int cgjre_host_inspect(const char *path);
int cgjre_host_eval_class(const char *path, const char *method,
    const char *descriptor, int argument_count, char **arguments);
int cgjre_host_eval_jar(const char *path, const char *class_name,
    const char *method, const char *descriptor, int argument_count,
    char **arguments);

static void show_smoke(void *context)
{
    (void)context;
    puts("CGJRE M0 host smoke: display, key, exit contract");
}

static int wait_for_exit(void *context)
{
    (void)context;
    puts("CGJRE M0 host smoke: simulated EXIT");
    return 0;
}

int main(int argc, char **argv)
{
    const cgjre_platform platform = {show_smoke, wait_for_exit, NULL};
    if(argc == 2 && strcmp(argv[1], "--smoke") == 0)
        return cgjre_smoke_run(&platform);
    if(argc == 3 && strcmp(argv[1], "--inspect") == 0)
        return cgjre_host_inspect(argv[2]);
    if(argc >= 5 && strcmp(argv[1], "--eval-class") == 0)
        return cgjre_host_eval_class(argv[2], argv[3], argv[4],
            argc - 5, argv + 5);
    if(argc >= 6 && strcmp(argv[1], "--eval-jar") == 0)
        return cgjre_host_eval_jar(argv[2], argv[3], argv[4], argv[5],
            argc - 6, argv + 6);
    fprintf(stderr, "Usage: %s --smoke | --inspect JAR | --eval-class CLASS METHOD DESCRIPTOR [INT...] | --eval-jar JAR CLASS METHOD DESCRIPTOR [INT...]\nMIDlet launch is not implemented.\n", argv[0]);
    return 2;
}
