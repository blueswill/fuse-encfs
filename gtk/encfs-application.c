#include<udisks/udisks.h>
#include<fcntl.h>
#include<errno.h>
#include"encfs-application.h"
#include"encfs-window.h"
#include"config.h"

struct _EncfsApplication {
    GtkApplication parent;
    EncfsWindow *window;
    UDisksClient *client;
};

G_DEFINE_TYPE(EncfsApplication, encfs_application, GTK_TYPE_APPLICATION);

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
    encfs_application_ensure_client(self);
    self->window = encfs_window_new(self, self->client);
    g_application_set_default(app);
    gtk_application_add_window(GTK_APPLICATION(self), GTK_WINDOW(self->window));
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
    const gchar *cachedir = g_get_user_cache_dir();
    gchar *cache = g_strdup_printf("%s/"PROJECT_NAME, cachedir);
    if (g_mkdir_with_parents(cache,
                             S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0) {
        g_warning("create cache directory %s error: %s", cache, g_strerror(errno));
    }
    g_free(cache);
}

EncfsApplication *encfs_application_new(void) {
    return g_object_new(ENCFS_TYPE_APPLICATION,
            "application-id", "swhc.encfs",
            NULL);
}

gboolean encfs_application_loop_setup(GApplication *app,
                                      GVariant *arg_fd, GUnixFDList *fd_list,
                                      gchar **out_resulting_device, GUnixFDList **out_fd_list, GError **error) {
    EncfsApplication *self = ENCFS_APPLICATION(app);
    UDisksManager *manager = udisks_client_get_manager(self->client);
    GVariantDict dict;
    GVariant *options;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "auth.no_user_interaction", "b", FALSE);
    options = g_variant_dict_end(&dict);
    return udisks_manager_call_loop_setup_sync(manager, arg_fd, options,fd_list,
                                               out_resulting_device, out_fd_list, NULL, error);
}
