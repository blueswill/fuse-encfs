#include<gtk/gtk.h>
#include<udisks/udisks.h>

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
