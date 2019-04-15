#include<udisks/udisks.h>
#include"encfs-monitor-grid.h"
#include"encfs-application.h"
#include"config.h"

struct _EncfsMonitorGrid {
    GtkGrid parent;
    GtkButton *refresh_button;
    GtkButton *unmount_button;
    GtkTreeView *monitor_list;

    GHashTable *lo_paths;
};

enum {
    COLUMN_NICK,
    COLUMN_LOOP_POINTER,
    COLUMN_DIR,
    N_COLUMN
};

G_DEFINE_TYPE(EncfsMonitorGrid, encfs_monitor_grid, GTK_TYPE_GRID);

static gboolean model_foreach_cb(GtkTreeModel *model, GtkTreePath *path,
                             GtkTreeIter *iter, EncfsMonitorGrid *self) {
    gchar *prefix;
    (void)path;
    gtk_tree_model_get(model, iter, COLUMN_DIR, &prefix, -1);
    g_autofree gchar *target = g_build_path(G_DIR_SEPARATOR_S, prefix, "target", NULL);
    g_autofree gpointer found = g_hash_table_lookup(self->lo_paths, target);
    gtk_list_store_set(GTK_LIST_STORE(model), iter, COLUMN_LOOP_POINTER, found, -1);
    return FALSE;
}

static void update_menu(EncfsMonitorGrid *self) {
    GtkTreeModel *model = gtk_tree_view_get_model(self->monitor_list);
    gtk_tree_model_foreach(model, (GtkTreeModelForeachFunc)model_foreach_cb, self);
}

static void get_all_loop_back(GHashTable *paths, UDisksClient *client, const gchar *prefix) {
    GDBusObjectManager *manager = udisks_client_get_object_manager(client);
    GList *objs = g_dbus_object_manager_get_objects(manager);
    GList *l;
    for (l = objs; l; l = l->next) {
        UDisksObject *obj = UDISKS_OBJECT(l->data);
        UDisksLoop *lo = udisks_object_peek_loop(obj);
        if (lo) {
            const gchar *backing_file = udisks_loop_get_backing_file(lo);
            if (g_str_has_prefix(backing_file, prefix)) {
                g_hash_table_insert(paths, g_strdup(backing_file), g_object_ref(lo));
            }
        }
    }
    g_list_free_full(objs, g_object_unref);
}

static void update_loop_paths(EncfsMonitorGrid *self, UDisksClient *client) {
    g_autofree gchar *prefix = g_build_path(G_DIR_SEPARATOR_S, g_get_user_cache_dir(), PROJECT_NAME, NULL);
    g_hash_table_remove_all(self->lo_paths);
    get_all_loop_back(self->lo_paths, client, prefix);
    g_idle_add(G_SOURCE_FUNC(update_menu), self);
}

static void update_path(EncfsMonitorGrid *self, const gchar *path) {
    g_autoptr(GError) err = NULL;
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(self->monitor_list));
    g_autoptr(GFile) f = g_file_new_for_path(path);
    g_autoptr(GFile) target = g_file_new_build_filename(path, "target", NULL);
    g_autoptr(GFile) attr = g_file_new_build_filename(path, "attributes", NULL);
    g_autoptr(GMount) mount = g_file_find_enclosing_mount(f, NULL, &err);
    if (err) {
        g_warning(err->message);
        return;
    }
    if (mount && g_file_query_exists(target, NULL) && g_file_query_exists(attr, NULL)) {
        g_autoptr(GBytes) bytes = g_file_load_bytes(attr, NULL, NULL, &err);
        if (err) {
            g_warning(err->message);
            return;
        }
        gconstpointer data = g_bytes_get_data(bytes, NULL);
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_NICK, data,
                           COLUMN_LOOP_POINTER, NULL,
                           COLUMN_DIR, path,
                           -1);
    }
}


static void update_monitor_list(EncfsMonitorGrid *self) {
    g_autofree gchar *cache = g_build_path(G_DIR_SEPARATOR_S, g_get_user_cache_dir(), PROJECT_NAME, NULL);
    g_autoptr(GError) err = NULL;
    g_autoptr(GDir) dir = g_dir_open(cache, 0, &err);
    GApplication *app = g_application_get_default();
    UDisksClient *client = encfs_application_get_client(ENCFS_APPLICATION(app));
    if (!dir) {
        g_warning(err->message);
        return;
    }
    const gchar *item;
    while ((item = g_dir_read_name(dir))) {
        g_autofree gchar *path = g_build_path(G_DIR_SEPARATOR_S, cache, item, NULL);
        update_path(self, path);
    }
    update_loop_paths(self, client);
}

static void unmount_cb(GObject *obj, GAsyncResult *res, gpointer userdata) {
    GtkTreeRowReference *ref = userdata;
    GtkTreeModel *model = gtk_tree_row_reference_get_model(ref);
    GtkTreePath *path = gtk_tree_row_reference_get_path(ref);
    GMount *mount = G_MOUNT(obj);
    g_autoptr(GError) err = NULL;
    if (!g_mount_unmount_with_operation_finish(mount, res, &err)) {
        g_warning(err->message);
        return;
    }
    g_autoptr(GFile) root = g_mount_get_root(mount);
    if (!g_file_delete(root, NULL, &err))
        g_warning(err->message);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path))
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
}

static void on_unmount_button_clicked(GtkTreeView *monitor_list) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(monitor_list);
    GtkTreeModel *model;
    GList *selected = gtk_tree_selection_get_selected_rows(selection, &model);
    GList *refs = NULL, *l;
    for (l = selected; l; l = l->next)
        refs = g_list_prepend(refs, gtk_tree_row_reference_new(model, l->data));
    g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
    for (l = refs; l; l = l->next) {
        GtkTreePath *path = gtk_tree_row_reference_get_path(l->data);
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            UDisksLoop *lo = NULL;
            gchar *dir = NULL;
            gtk_tree_model_get(model, &iter,
                               COLUMN_LOOP_POINTER, &lo,
                               COLUMN_DIR, &dir,
                               -1);
            if (lo) {
                g_autoptr(GError) err = NULL;
                GVariantDict dict = G_VARIANT_DICT_INIT(NULL);
                g_variant_dict_insert(&dict, "auth.no_user_interaction", "b", FALSE);
                GVariant *options = g_variant_dict_end(&dict);
                if (!udisks_loop_call_delete_sync(lo, options, NULL, &err)) {
                    g_warning(err->message);
                    continue;
                }
            }
            g_autoptr(GError) err = NULL;
            g_autoptr(GFile) d = g_file_new_for_path(dir);
            g_autoptr(GMount) mount = g_file_find_enclosing_mount(d, NULL, &err);
            if (err) {
                g_warning(err->message);
                continue;
            }
            g_autoptr(GMountOperation) opers = g_mount_operation_new();
            g_mount_unmount_with_operation(mount, G_MOUNT_UNMOUNT_NONE,opers, NULL, unmount_cb, l->data);
        }
    }
}

static void on_refresh_button_clicked(EncfsMonitorGrid *self) {
    update_monitor_list(self);
}

static void on_monitor_list_row_activated(GtkTreeView *monitor_list, GtkTreePath *path,
                                          GtkTreeViewColumn *column, GtkButton *button) {
    gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);
}

static void encfs_monitor_grid_constructed(GObject *obj) {
    EncfsMonitorGrid *self = ENCFS_MONITOR_GRID(obj);
    GObjectClass *klass = G_OBJECT_CLASS(encfs_monitor_grid_parent_class);
    GtkTreeViewColumn *column;
    if (klass->constructed)
        klass->constructed(obj);
    g_autoptr(GtkListStore) store = gtk_list_store_new(N_COLUMN,
                                             G_TYPE_STRING,
                                             G_TYPE_POINTER,
                                             G_TYPE_STRING);
    gtk_tree_view_set_model(self->monitor_list, GTK_TREE_MODEL(store));
    column = gtk_tree_view_column_new_with_attributes("name", gtk_cell_renderer_text_new(),
                                                      "text", COLUMN_NICK,
                                                      NULL);
    gtk_tree_view_append_column(self->monitor_list, column);
    gtk_widget_set_sensitive(GTK_WIDGET(self->unmount_button), FALSE);
    g_idle_add(G_SOURCE_FUNC(update_monitor_list), self);
    GApplication *app = g_application_get_default();
    UDisksClient *client = encfs_application_get_client(ENCFS_APPLICATION(app));
    g_signal_connect_swapped(client, "changed", G_CALLBACK(update_loop_paths), self);
    g_signal_connect_swapped(ENCFS_APPLICATION(app), "mounted", G_CALLBACK(update_path), self);
}

static void encfs_monitor_grid_finalize(GObject *obj) {
    EncfsMonitorGrid *self = ENCFS_MONITOR_GRID(obj);
    GObjectClass *klass = G_OBJECT_CLASS(encfs_monitor_grid_parent_class);
    g_hash_table_unref(self->lo_paths);
    if (klass->finalize)
        klass->finalize(obj);
}

static void encfs_monitor_grid_class_init(EncfsMonitorGridClass *self_class) {
    GtkWidgetClass *parent = GTK_WIDGET_CLASS(self_class);
    GObjectClass *object_class = G_OBJECT_CLASS(self_class);
    gtk_widget_class_set_template_from_resource(parent, "/swhc/encfs/monitor-grid.ui");
    gtk_widget_class_bind_template_child(parent, EncfsMonitorGrid, refresh_button);
    gtk_widget_class_bind_template_child(parent, EncfsMonitorGrid, unmount_button);
    gtk_widget_class_bind_template_child(parent, EncfsMonitorGrid, monitor_list);
    gtk_widget_class_bind_template_callback(parent, on_unmount_button_clicked);
    gtk_widget_class_bind_template_callback(parent, on_refresh_button_clicked);
    gtk_widget_class_bind_template_callback(parent, on_monitor_list_row_activated);
    object_class->constructed = encfs_monitor_grid_constructed;
    object_class->finalize = encfs_monitor_grid_finalize;
}

static void encfs_monitor_grid_init(EncfsMonitorGrid *self) {
    gtk_widget_init_template(GTK_WIDGET(self));
    const gchar *cache_dir = g_get_user_cache_dir();
    g_autofree gchar *cache = g_build_path(G_DIR_SEPARATOR_S, cache_dir, PROJECT_NAME, NULL);
    g_autoptr(GFile) file = g_file_new_for_path(cache);
    self->lo_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
}
