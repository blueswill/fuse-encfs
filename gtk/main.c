#include"encfs-application.h"

int main(int argc, char **argv) {
    g_autoptr(EncfsApplication) app;
    g_set_application_name("USB encfs");
    app = encfs_application_new();
    g_application_set_default(G_APPLICATION(app));
    return g_application_run(G_APPLICATION(app), argc, argv);
}
