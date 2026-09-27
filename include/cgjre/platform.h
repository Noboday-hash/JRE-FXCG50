#ifndef CGJRE_PLATFORM_H
#define CGJRE_PLATFORM_H

/* M0 boundary. VM services will extend this contract in later milestones. */
typedef struct cgjre_platform {
    void (*show_smoke)(void *context);
    int (*wait_for_exit)(void *context);
    void *context;
} cgjre_platform;

int cgjre_smoke_run(const cgjre_platform *platform);

#endif
