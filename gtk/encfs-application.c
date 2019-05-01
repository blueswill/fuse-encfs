#include<fcntl.h>
#include<errno.h>
#include"encfs-application.h"
#include"encfs-window.h"
#include"config.h"
#include"tpm-context.h"
#include"encfs-tpm-window.h"

enum tpm_load_state {
    TPM_INIT = 0,
    TPM_LOAD_PRIMARY,
    TPM_LOAD_RSA
};

struct _EncfsApplication {
    GtkApplication parent;
    EncfsWindow *window;
    UDisksClient *client;
    UDisksObject *selected;

    struct tpm_context *tpm_ctx;
    tpm_handle_t handle;
    enum tpm_load_state tpm_state;
};

enum {
    PROP_0,
    PROP_CLIENT,
    PROP_SELECTED_OBJECT,
    NUM_PROP
};

enum {
    SIGNAL_MOUNT,
    NUM_SIGNAL
};

static guint app_signals[NUM_SIGNAL];

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
    tpm_context_free(app->tpm_ctx);
    G_OBJECT_CLASS(encfs_application_parent_class)->finalize(obj);
}

static void on_tpm_window_unrealize(EncfsTpmWindow *win, EncfsApplication *app) {
}

static void encfs_application_startup(GApplication *app) {
    EncfsApplication *self = ENCFS_APPLICATION(app);
    G_APPLICATION_CLASS(encfs_application_parent_class)->startup(app);
    g_application_set_default(app);
    encfs_application_ensure_client(self);
    if (!(self->tpm_ctx = tpm_context_new()))
        g_application_quit(app);
}

static void encfs_application_activate(GApplication *app) {
    EncfsApplication *self = ENCFS_APPLICATION(app);
    EncfsTpmWindow *tpm_window = encfs_tpm_window_new(app);
    g_signal_connect(tpm_window, "unrealize",
                     G_CALLBACK(on_tpm_window_unrealize), self);
    gtk_widget_show(GTK_WIDGET(tpm_window));
    gtk_window_present(GTK_WINDOW(tpm_window));
}

UDisksClient *encfs_application_get_client(EncfsApplication *app) {
    g_return_val_if_fail(ENCFS_IS_APPLICATION(app), NULL);
    return app->client;
}

UDisksObject *encfs_application_get_selected(EncfsApplication *app) {
    g_return_val_if_fail(ENCFS_IS_APPLICATION(app), NULL);
    return app->selected;
}

static void encfs_application_get_property(GObject *obj,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec) {
    EncfsApplication *self = ENCFS_APPLICATION(obj);
    switch (prop_id) {
        case PROP_CLIENT:
            g_value_set_object(value, encfs_application_get_client(self));
            break;
        case PROP_SELECTED_OBJECT:
            g_value_set_object(value, encfs_application_get_selected(self));
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void encfs_application_set_property(GObject *obj,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec) {
    EncfsApplication *self = ENCFS_APPLICATION(obj);
    switch (prop_id) {
        case PROP_SELECTED_OBJECT:
            if (self->selected)
                g_object_unref(self->selected);
            self->selected = g_value_get_pointer(value);
            if (self->selected)
                g_object_ref(self->selected);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}


static void encfs_application_class_init(EncfsApplicationClass *self) {
    GApplicationClass *app = G_APPLICATION_CLASS(self);
    GObjectClass *objclass = G_OBJECT_CLASS(self);
    app->activate = encfs_application_activate;
    app->startup = encfs_application_startup;
    G_OBJECT_CLASS(self)->finalize = encfs_application_finalize;
    objclass->get_property = encfs_application_get_property;
    objclass->set_property = encfs_application_set_property;
    g_object_class_install_property(objclass, PROP_CLIENT,
                                    g_param_spec_object("client",
                                                        "Client",
                                                        "The client used by the window",
                                                        UDISKS_TYPE_CLIENT,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(objclass, PROP_SELECTED_OBJECT,
                                    g_param_spec_pointer("selected-object",
                                                        "Selected Object",
                                                        "selected object from USB menu",
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS));
    app_signals[SIGNAL_MOUNT] = g_signal_new("mounted", G_TYPE_FROM_CLASS(self),
                                             G_SIGNAL_RUN_LAST |G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                                             0, NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
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

GtkWindow *encfs_application_get_window(EncfsApplication *app) {
    return GTK_WINDOW(app->window);
}
