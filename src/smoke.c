#include <cgjre/platform.h>

int cgjre_smoke_run(const cgjre_platform *platform)
{
    if(!platform || !platform->show_smoke || !platform->wait_for_exit)
        return 1;
    platform->show_smoke(platform->context);
    return platform->wait_for_exit(platform->context);
}
