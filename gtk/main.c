#include"encfs-application.h"
#include"sm9.h"

int main(int argc, char **argv) {
    sm9_init();
    g_autoptr(EncfsApplication) app;
    g_set_application_name("USB encfs");
    app = encfs_application_new();
    g_application_set_default(G_APPLICATION(app));
    int ret = g_application_run(G_APPLICATION(app), argc, argv);
    sm9_release();
    return ret;
}
