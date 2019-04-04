#include<udisks/udisks.h>
#include"encfs-application.h"
#include"encfs-window.h"

struct _EncfsApplication {
    GtkApplication parent;
    EncfsWindow *window;
    UDisksClient *client;
};

G_DEFINE_TYPE(EncfsApplication, encfs_application, GTK_TYPE_APPLICATION);

static void on_preferences_activate(GSimpleAction *action,
        GVariant *parameter,
        gpointer userdata) {
}

static const GActionEntry actions[] = {
    { "preferences", on_preferences_activate }
};

static void encfs_application_ensure_client(EncfsApplication *app) {
    GError *err;
    if (app->client)
        return;
    err = NULL;
    app->client = udisks_client_new_sync(NULL, &err);
    if (!app->client) {
        g_error("Error getting udisks client: %s", err->message);
        g_error_free(err);
    }
}

static void encfs_application_finalize(GObject *obj) {
    EncfsApplication *app = ENCFS_APPLICATION(obj);
    if (app->client)
        g_object_unref(app->client);
    G_OBJECT_CLASS(encfs_application_parent_class)->finalize(obj);
}

static void encfs_application_startup(GApplication *app) {
    EncfsApplication *self = ENCFS_APPLICATION(app);
    G_APPLICATION_CLASS(encfs_application_parent_class)->startup(app);
    g_action_map_add_action_entries(G_ACTION_MAP(app), actions, G_N_ELEMENTS(actions), app);
    encfs_application_ensure_client(self);
    self->window = encfs_window_new(self, self->client);
}

static void encfs_application_activate(GApplication *app) {
    EncfsApplication *self = ENCFS_APPLICATION(app);
    gtk_application_add_window(GTK_APPLICATION(app), GTK_WINDOW(self->window));
    gtk_widget_show(GTK_WIDGET(self->window));
    gtk_window_present(GTK_WINDOW(self->window));
}

static void encfs_application_class_init(EncfsApplicationClass *self) {
    GApplicationClass *app = G_APPLICATION_CLASS(self);
    app->activate = encfs_application_activate;
    app->startup = encfs_application_startup;
    G_OBJECT_CLASS(self)->finalize = encfs_application_finalize;
}

static void encfs_application_init(EncfsApplication *self) {
    (void)self;
}

EncfsApplication *encfs_application_new(void) {
    return g_object_new(ENCFS_TYPE_APPLICATION,
            "application-id", "swhc.encfs",
            NULL);
}
