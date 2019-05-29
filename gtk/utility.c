#include<gtk/gtk.h>
#include<udisks/udisks.h>
#include"encfs-application.h"

gchar *unfused_path(const gchar *path) {
    gchar *ret;
    GFile *file;
    gchar *uri;
    const gchar *home;

    file = g_file_new_for_path(path);
    uri = g_file_get_uri(file);
    if (g_str_has_prefix(uri, "file:"))
        ret = g_strdup(path);
    else
        ret = g_uri_unescape_string(uri, NULL);
    g_object_unref(file);
    g_free(uri);
    home = g_get_home_dir();
    if (g_str_has_prefix(ret, home)) {
        size_t home_len = strlen(home);
        if (home_len > 2) {
            if (home[home_len - 1] == '/')
                --home_len;
            if (ret[home_len] == '/') {
                gchar *tmp = ret;
                ret = g_strdup_printf("~/%s", ret + home_len + 1);
                g_free(tmp);
            }
        }
    }
    return ret;
}

int _decrypt(const gchar *in, size_t inlen, char **out, size_t *outlen, void *userdata) {
    const gchar *pass = userdata;
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    g_autoptr(GBytes) inbytes = g_bytes_new(in, inlen);
    g_autoptr(GBytes) outbytes = encfs_application_tpm_decrypt_file(app, pass, inbytes);
    if (outbytes != NULL) {
        gconstpointer data = g_bytes_get_data(outbytes, outlen);
        *out = g_malloc(*outlen);
        g_memmove(*out, data, *outlen);
        return 0;
    }
    return -1;
}

int _encrypt(const gchar *in, size_t inlen, char **out, size_t *outlen, void *userdata) {
    (void)userdata;
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    g_autoptr(GBytes) inbytes = g_bytes_new(in, inlen);
    g_autoptr(GBytes) outbytes = encfs_application_tpm_encrypt_file(app, NULL, NULL, NULL, NULL, inbytes);
    if (outbytes != NULL) {
        gconstpointer data = g_bytes_get_data(outbytes, outlen);
        *out = g_malloc(*outlen);
        g_memmove(*out, data, *outlen);
        return 0;
    }
    return -1;
}

gchar *_get_password(void) {
    g_autoptr(GtkBuilder) builder = gtk_builder_new_from_resource("/swhc/encfs/generate-rsa.ui");
    GObject *dialog = gtk_builder_get_object(builder, "generate-rsa-dialog");
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(GTK_WIDGET(dialog));
    gchar *ret = NULL;
    if (res == GTK_RESPONSE_ACCEPT) {
        GObject *entry = gtk_builder_get_object(builder, "objectpass");
        const gchar *pass = gtk_entry_get_text(GTK_ENTRY(entry));
        ret = g_strdup(pass);
    }
    return ret;
}
