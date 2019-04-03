#include"encfs-application.h"
#include"encfs-window.h"

struct _EncfsApplication {
    GtkApplication parent;
    GtkWidget *window;
};

G_DEFINE_TYPE(EncfsApplication, encfs_application, GTK_TYPE_APPLICATION);

static void encfs_application_startup(GApplication *app) {
    EncfsApplication *self = ENCFS_APPLICATION(app);
    G_APPLICATION_CLASS(encfs_application_parent_class)->startup(app);
    self->window = encfs_window_new(self);
}

static void encfs_application_activate(GApplication *app) {
    EncfsApplication *self = ENCFS_APPLICATION(app);
    gtk_widget_show(self->window);
    gtk_window_present(GTK_WINDOW(self->window));
}

static void encfs_application_class_init(EncfsApplicationClass *self) {
    GApplicationClass *app = G_APPLICATION_CLASS(self);
    app->activate = encfs_application_activate;
    app->startup = encfs_application_startup;
}

static void encfs_application_init(EncfsApplication *self) {
    (void)self;
}

EncfsApplication *encfs_application_new(void) {
    return g_object_new(ENCFS_TYPE_APPLICATION,
            "application-id", "swhc.encfs",
            NULL);
}
