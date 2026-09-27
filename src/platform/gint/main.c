#include <cgjre/platform.h>
#include <gint/display.h>
#include <gint/keyboard.h>

static void show_smoke(void *context)
{
    (void)context;
    dclear(C_WHITE);
    dtext(8, 8, C_BLACK, "CGJRE M0 smoke test");
    dtext(8, 32, C_BLACK, "Press EXIT to return");
    dupdate();
}

static int wait_for_exit(void *context)
{
    (void)context;
    for(;;) {
        key_event_t event = getkey();
        if(event.key == KEY_EXIT)
            return 0;
    }
}

int main(void)
{
    const cgjre_platform platform = {show_smoke, wait_for_exit, 0};
    return cgjre_smoke_run(&platform);
}
