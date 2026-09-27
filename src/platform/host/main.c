#include <cgjre/platform.h>
#include <stdio.h>
#include <string.h>

int cgjre_host_inspect(const char *path);

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
    fprintf(stderr, "Usage: %s --smoke | --inspect JAR\nJAR execution is not implemented.\n", argv[0]);
    return 2;
}
