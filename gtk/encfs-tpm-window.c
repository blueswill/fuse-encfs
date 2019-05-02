#include"encfs-tpm-window.h"
#include"tpm-context.h"
#include"encfs-application.h"

static void on_take_ownership_active(GSimpleAction *, GVariant *, gpointer);

struct _EncfsTpmWindow {
    GtkApplicationWindow parent;
    GtkMenuButton *tpm_menu_button;
    GtkEntry *primary_key, *owner_key;
    GtkLabel *priv_label, *pub_label;

    gchar *priv, *pub;
    /* active when Ok button clicked */
    gboolean active;
};

G_DEFINE_TYPE(EncfsTpmWindow, encfs_tpm_window, GTK_TYPE_APPLICATION_WINDOW);

static const GActionEntry actions[] = {
    {"take-ownership", on_take_ownership_active, NULL, NULL, NULL, 0 }
};

static GSList *_get_file(GtkWindow *window, const gchar *hint,
                        GtkFileChooserAction action, gboolean multiple) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(hint,
                                                    window, action,
                                                    "Cancel",
                                                    GTK_RESPONSE_CANCEL,
                                                    "Ok",
                                                    GTK_RESPONSE_ACCEPT,
                                                    NULL);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), multiple);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    GSList *ret = NULL;
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *c = GTK_FILE_CHOOSER(dialog);
        ret = gtk_file_chooser_get_filenames(c);
    }
    gtk_widget_destroy(dialog);
    return ret;
}

static void _show_dialog(GtkWindow *win, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    g_autofree gchar *str = g_strdup_vprintf(format, ap);
    va_end(ap);
    GtkWidget *dialog = gtk_message_dialog_new(win,
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s", str);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void on_ok_button_clicked(EncfsTpmWindow *self) {
    /* TODO: check all fields are satisfied */
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    if (!encfs_application_tpm_load_rsa(app,
                                        gtk_entry_get_text(self->owner_key),
                                        gtk_entry_get_text(self->primary_key),
                                        self->priv, self->pub)) {
        _show_dialog(GTK_WINDOW(self), "TPM load RSA key error");
    }
    else {
        self->active = TRUE;
        gtk_window_close(GTK_WINDOW(self));
    }
}

static void _encrypt_file(const gchar *path, EncfsTpmWindow *self) {
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    g_autoptr(GFile) file = g_file_new_for_path(path);
    g_autoptr(GError) err = NULL;
    const gchar *primary = gtk_entry_get_text(self->primary_key);
    const gchar *owner = gtk_entry_get_text(self->owner_key);
    g_autoptr(GBytes) content = NULL;
    if (g_file_query_file_type(file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) != G_FILE_TYPE_REGULAR) {
        g_warning("%s is not a regular file", path);
        return;
    }
    if (!(content = g_file_load_bytes(file, NULL, NULL, &err))) {
        g_warning(err->message);
        return;
    }
    g_autoptr(GBytes) out = encfs_application_tpm_encrypt_file(app,
                                                               self->priv, self->pub,
                                                               owner, primary,
                                                               content);
    if (!out)
        return;
    gconstpointer data;
    gsize size;
    data = g_bytes_get_data(out, &size);
    if (data) {
        g_file_replace_contents(file, data, size, NULL, TRUE,
                                G_FILE_CREATE_REPLACE_DESTINATION, NULL, NULL, &err);
        if (err)
            g_warning(err->message);
    }
}

static void on_encrypt_files_button_clicked(EncfsTpmWindow *self) {
    GSList *list = _get_file(GTK_WINDOW(self), "Select files",
                             GTK_FILE_CHOOSER_ACTION_OPEN, TRUE);
    g_slist_foreach(list, (GFunc)_encrypt_file, self);
    g_slist_free_full(list, g_free);
}

static void on_generate_rsa_button_clicked(EncfsTpmWindow *self) {
    g_autoptr(GtkBuilder) builder = gtk_builder_new_from_resource("/swhc/encfs/generate-rsa.ui");
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    GObject *dialog = gtk_builder_get_object(builder, "generate-rsa-dialog");
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        GObject *entry = gtk_builder_get_object(builder, "objectpass");
        const gchar *pass = gtk_entry_get_text(GTK_ENTRY(entry));
        GSList *list = _get_file(GTK_WINDOW(self), "File to create",
                                 GTK_FILE_CHOOSER_ACTION_SAVE, FALSE);
        if (list) {
            gchar *prefix = g_strdup(list->data);
            g_slist_free_full(list, g_free);
            if (!encfs_application_tpm_create_rsa(app,
                                                  gtk_entry_get_text(self->owner_key),
                                                  gtk_entry_get_text(self->primary_key),
                                                  pass,
                                                  prefix))
                _show_dialog(GTK_WINDOW(dialog), "create RSA error");
        }
    }
    gtk_window_close(GTK_WINDOW(dialog));
}

static void encfs_tpm_window_constructed(GObject *obj) {
    if (G_OBJECT_CLASS(encfs_tpm_window_parent_class)->constructed)
        G_OBJECT_CLASS(encfs_tpm_window_parent_class)->constructed(obj);
    EncfsTpmWindow *self = ENCFS_TPM_WINDOW(obj);
    g_autoptr(GtkBuilder) builder = gtk_builder_new_from_resource("/swhc/encfs/menu.ui");
    GMenu *menu = G_MENU(gtk_builder_get_object(builder, "tpm_menu"));
    gtk_menu_button_set_menu_model(self->tpm_menu_button, G_MENU_MODEL(menu));
    g_action_map_add_action_entries(G_ACTION_MAP(self), actions, G_N_ELEMENTS(actions),
                                    self);
}

static void encfs_tpm_window_finalize(GObject *obj) {
    EncfsTpmWindow *self = ENCFS_TPM_WINDOW(obj);
    g_free(self->priv);
    g_free(self->pub);
    if (G_OBJECT_CLASS(encfs_tpm_window_parent_class)->finalize)
        G_OBJECT_CLASS(encfs_tpm_window_parent_class)->finalize(obj);
}

static void on_priv_sel_button_clicked(EncfsTpmWindow *self) {
    GSList *list = _get_file(GTK_WINDOW(self), "Open private key",
                             GTK_FILE_CHOOSER_ACTION_OPEN, FALSE);
    if (list)
        self->priv = g_strdup(list->data);
    g_slist_free_full(list, g_free);
    if (self->priv)
        gtk_label_set_text(self->priv_label, self->priv);
}

static void on_pub_sel_button_clicked(EncfsTpmWindow *self) {
    GSList *list = _get_file(GTK_WINDOW(self), "Open public key",
                             GTK_FILE_CHOOSER_ACTION_OPEN, FALSE);
    if (list)
        self->pub = g_strdup(list->data);
    g_slist_free_full(list, g_free);
    if (self->pub)
        gtk_label_set_text(self->pub_label, self->pub);
}

static void encfs_tpm_window_class_init(EncfsTpmWindowClass *self) {
    GtkWidgetClass *klass = GTK_WIDGET_CLASS(self);
    GObjectClass *objclass = G_OBJECT_CLASS(self);
    gtk_widget_class_set_template_from_resource(klass, "/swhc/encfs/tpm-window.ui");
    gtk_widget_class_bind_template_child(klass, EncfsTpmWindow, tpm_menu_button);
    gtk_widget_class_bind_template_child(klass, EncfsTpmWindow, primary_key);
    gtk_widget_class_bind_template_child(klass, EncfsTpmWindow, owner_key);
    gtk_widget_class_bind_template_child(klass, EncfsTpmWindow, priv_label);
    gtk_widget_class_bind_template_child(klass, EncfsTpmWindow, pub_label);
    gtk_widget_class_bind_template_callback(klass, on_ok_button_clicked);
    gtk_widget_class_bind_template_callback(klass, on_priv_sel_button_clicked);
    gtk_widget_class_bind_template_callback(klass, on_pub_sel_button_clicked);
    gtk_widget_class_bind_template_callback(klass, on_encrypt_files_button_clicked);
    gtk_widget_class_bind_template_callback(klass, on_generate_rsa_button_clicked);
    objclass->constructed = encfs_tpm_window_constructed;
    objclass->finalize = encfs_tpm_window_finalize;
}

static void encfs_tpm_window_init(EncfsTpmWindow *self) {
    gtk_widget_init_template(GTK_WIDGET(self));
}

EncfsTpmWindow *encfs_tpm_window_new(GApplication *app) {
    return ENCFS_TPM_WINDOW(g_object_new(ENCFS_TYPE_TPM_WINDOW,
                                         "application", app,
                                         NULL));
}

#define BUILDER_GET_BY_NAME(builder, name) \
    GObject *name = gtk_builder_get_object(builder, #name)

static gboolean _take_ownership(GtkBuilder *builder) {
    BUILDER_GET_BY_NAME(builder, old_owner_entry);
    BUILDER_GET_BY_NAME(builder, new_owner_entry);
    BUILDER_GET_BY_NAME(builder, old_lockout_entry);
    BUILDER_GET_BY_NAME(builder, new_lockout_entry);
    struct ownership_password old, new;
    ownership_password_init(&old, gtk_entry_get_text(GTK_ENTRY(old_owner_entry)),
                            NULL, gtk_entry_get_text(GTK_ENTRY(old_lockout_entry)));
    ownership_password_init(&new, gtk_entry_get_text(GTK_ENTRY(new_owner_entry)),
                            NULL, gtk_entry_get_text(GTK_ENTRY(new_lockout_entry)));
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    return encfs_application_tpm_take_ownership(app, &old, &new);
}

static void on_take_ownership_active(GSimpleAction *action, GVariant *parameter,
                                     gpointer userdata) {
    (void)action; (void)parameter;
    EncfsTpmWindow *self = ENCFS_TPM_WINDOW(userdata);
    g_autoptr(GtkBuilder) builder = gtk_builder_new_from_resource("/swhc/encfs/ownership-dialog.ui");
    GObject *dialog = gtk_builder_get_object(builder, "ownership-dialog");
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT && !_take_ownership(builder))
        _show_dialog(GTK_WINDOW(self), "TPM take ownership error");
    gtk_window_close(GTK_WINDOW(dialog));
}

gboolean encfs_tpm_window_get_active(EncfsTpmWindow *self) {
    return self->active;
}
