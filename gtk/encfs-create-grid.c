#include<gtk/gtk.h>
#include<udisks/udisks.h>
#include<fcntl.h>
#include<errno.h>
#include<gio/gunixfdlist.h>
#include"encfs-create-grid.h"
#include"sm9.h"
#include"create-context.h"
#include"encfs_helper.h"
#include"config.h"

struct _EncfsCreateGrid {
    GtkGrid parent;
    GtkTreeView *id_list;
    GtkToolButton *list_add, *list_remove;
    GtkButton *create_button;

    GtkListStore *id_list_store;

    UDisksObject *target;
    gchar *master_file, *iddir;
};

enum {
    PROP_0,
    PROP_TARGET,
    PROP_MASTER,
    PROP_IDDIR
};

enum {
    COLUMN_STRING,
    N_COLUMNS
};

struct read_file_struct {
    GFileInputStream *in;
    EncfsCreateGrid *grid;
};

struct create_device_struct {
    int blkfd;
    EncfsCreateGrid *grid;
};

G_DEFINE_TYPE(EncfsCreateGrid, encfs_create_grid, GTK_TYPE_GRID);

static gboolean _id_list_foreach(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
                             gpointer userdata) {
    (void)path;
    GList **list = userdata;
    GValue val = G_VALUE_INIT;
    gtk_tree_model_get_value(model, iter, COLUMN_STRING, &val);
    *list = g_list_append(*list, g_value_dup_string(&val));
    g_value_unset(&val);
    return FALSE;
}

static gchar **id_list_get_ids(GtkTreeModel *model) {
    GList *list = NULL, *l;
    gtk_tree_model_foreach(model, _id_list_foreach, &list);
    guint size = g_list_length(list);
    gchar **ids = g_new(gchar*, size + 1);
    int i = 0;
    for (l = list; l; l = l->next)
        ids[i++] = l->data;
    ids[i] = NULL;
    return ids;
}

static gboolean _create(gpointer userdata) {
    struct create_device_struct *st = userdata;
    EncfsCreateGrid *self = st->grid;
    int iddir = open(self->iddir, O_RDONLY);
    gchar *masterfile = g_strdup(self->master_file);
    GtkWindow *win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self)));
    if (!g_str_has_suffix(masterfile, MASTER_KEY_PAIR_SUFFIX)) {
        g_free(masterfile);
        masterfile = g_strdup_printf("%s"MASTER_KEY_PAIR_SUFFIX, self->master_file);
    }
    if (iddir < 0) {
        GtkWidget *dialog = gtk_message_dialog_new(win,
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "Error open directory %s: %s", self->iddir, g_strerror(errno));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        goto end;
    }
    int masterfd = open(masterfile, O_RDWR | O_TRUNC | O_CREAT,
                        S_IRUSR | S_IWUSR | S_IRGRP);
    if (masterfd < 0) {
        GtkWidget *dialog = gtk_message_dialog_new(win,
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "Error open master file %s: %s", masterfile, g_strerror(errno));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        goto end;
    }
    struct master_key_pair *pair = master_key_pair_read_file(masterfd);
    int regenerate = 0;
    if (!pair) {
        regenerate = 1;
        pair = generate_master_key_pair(TYPE_ENCRYPT);
        master_key_pair_write_file(pair, masterfd);
    }
    close(masterfd);
    struct create_context *args = create_context_new(st->blkfd, iddir,
                                                     pair, regenerate);
    if (!args) {
        GtkWidget *dialog = gtk_message_dialog_new(win,
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "fail to get create args");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        goto end;
    }
    gchar **ids = id_list_get_ids(GTK_TREE_MODEL(self->id_list_store));
    if (create_context_create(args, (void *)ids) < 0) {
        GtkWidget *dialog = gtk_message_dialog_new(win,
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "fail to create device");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    g_strfreev(ids);
    create_context_free(args);
end:
    if (iddir >= 0)
        close(iddir);
    if (masterfd >= 0)
        close(masterfd);
    g_free(masterfile);
    return G_SOURCE_REMOVE;
}

static gboolean check_create_satisfied(EncfsCreateGrid *self) {
    GtkTreeIter iter;
    gboolean notempty = FALSE;
    if (self->id_list_store)
        notempty = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(self->id_list_store), &iter);
    gboolean ret = (self->target && self->master_file && self->iddir && notempty);
    gtk_widget_set_sensitive(GTK_WIDGET(self->create_button), ret);
    return ret;
}

static void read_file_struct_free(struct read_file_struct *f) {
    g_object_unref(f->in);
    g_free(f);
}

static void create_device_struct_free(struct create_device_struct *s) {
    close(s->blkfd);
    g_free(s);
}

static gboolean read_ids(gpointer userdata) {
    struct read_file_struct *fp = userdata;
    GDataInputStream *in = g_data_input_stream_new(G_INPUT_STREAM(fp->in));
    gchar *line;
    GError *err = NULL;
    GtkListStore *store = fp->grid->id_list_store;
    GtkTreeIter iter;
    while ((line = g_data_input_stream_read_line_utf8(in, NULL, NULL, &err))) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_STRING, line,
                           -1);
    }
    if (err)
        g_warning("%s", err->message);
    g_object_unref(in);
    check_create_satisfied(fp->grid);
    return G_SOURCE_REMOVE;
}

static gboolean read_id_from_file(EncfsCreateGrid *self, GFile *file) {
    GError *err = NULL;
    GFileInputStream *in = g_file_read(file, NULL, &err);
    if (!in) {
        GtkWindow *win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self)));
        GtkWidget *dialog = gtk_dialog_new_with_buttons("file read error",
                                                        win,
                                                        GTK_DIALOG_MODAL |
                                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        "OK", GTK_RESPONSE_OK,
                                                        "Select", GTK_RESPONSE_ACCEPT,
                                                        NULL);
        GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        GtkWidget *label = gtk_label_new(err->message);
        gtk_container_add(GTK_CONTAINER(content), label);
        gtk_widget_show(label);
        gint ret = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(GTK_WIDGET(dialog));
        return ret == GTK_RESPONSE_ACCEPT;
    }
    struct read_file_struct *r = g_new(struct read_file_struct, 1);
    r->grid = self;
    r->in = in;
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    G_SOURCE_FUNC(read_ids),
                    r,
                    (GDestroyNotify)read_file_struct_free);
    return FALSE;
}

static void list_add_clicked_cb(EncfsCreateGrid *grid) {
    GtkWidget *dialog =
        gtk_file_chooser_dialog_new("Select ID directory",
                                    GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(grid))),
                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                    "Cancel", GTK_RESPONSE_CANCEL,
                                    "Open", GTK_RESPONSE_ACCEPT,
                                    NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_mime_type(filter, "text/plain");
    gtk_file_filter_set_name(filter, "id list file");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    int ret = gtk_dialog_run(GTK_DIALOG(dialog));
    while (TRUE) {
        gtk_widget_hide(GTK_WIDGET(dialog));
        if (ret == GTK_RESPONSE_ACCEPT) {
            GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
            gboolean cont = read_id_from_file(grid, file);
            g_object_unref(file);
            if (!cont)
                break;
            gtk_widget_show(GTK_WIDGET(dialog));
        }
        else
            break;
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void list_remove_clicked_cb(EncfsCreateGrid *grid) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(grid->id_list);
    GList *selected = gtk_tree_selection_get_selected_rows(selection, NULL);
    GList *refs = NULL, *l;
    if (selected) {
        for (l = selected; l; l = l->next) {
            GtkTreePath *path = l->data;
            refs = g_list_append(refs, gtk_tree_row_reference_new(GTK_TREE_MODEL(grid->id_list_store), path));
        }
        g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
    }
    for (l = refs; l; l = l->next) {
        GtkTreePath *path = gtk_tree_row_reference_get_path(l->data);
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(grid->id_list_store), &iter, path))
            gtk_list_store_remove(grid->id_list_store, &iter);
    }
    g_list_free_full(refs, (GDestroyNotify)gtk_tree_row_reference_free);
    check_create_satisfied(grid);
}

static void create_button_clicked_cb(EncfsCreateGrid *self) {
    if (!check_create_satisfied(self))
        return;
    GVariant *out_index, *options;
    GUnixFDList *out_list;
    GError *err = NULL;
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "auth.no_user_interaction", "b", FALSE);
    UDisksBlock *blk = udisks_object_peek_block(self->target);
    options = g_variant_dict_end(&dict);
    if (!udisks_block_call_open_device_sync(blk, "rw", options, NULL,
                                            &out_index, &out_list, NULL, &err)) {
        g_warning(err->message);
        return;
    }
    else {
        gint index = g_variant_get_handle(out_index);
        gint handler;
        err = NULL;
        g_variant_unref(out_index);
        handler = g_unix_fd_list_get(out_list, index, &err);
        g_object_unref(out_list);
        if (handler == -1) {
            g_warning(err->message);
            return;
        }
        struct create_device_struct *st = g_new(struct create_device_struct, 1);
        st->blkfd = handler;
        st->grid = self;
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, G_SOURCE_FUNC(_create),
                        st, (GDestroyNotify)create_device_struct_free);
    }
}

static UDisksObject *encfs_create_grid_get_object(EncfsCreateGrid *grid) {
    g_return_val_if_fail(ENCFS_CREATE_IS_GRID(grid), NULL);
    return grid->target;
}

static gchar *encfs_create_grid_get_master_file(EncfsCreateGrid *grid) {
    g_return_val_if_fail(ENCFS_CREATE_IS_GRID(grid), NULL);
    return grid->master_file;
}

static gchar *encfs_create_grid_get_iddir(EncfsCreateGrid *grid) {
    g_return_val_if_fail(ENCFS_CREATE_IS_GRID(grid), NULL);
    return grid->iddir;
}

static void encfs_create_grid_get_property(GObject *obj,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec) {
    EncfsCreateGrid *grid = ENCFS_CREATE_GRID(obj);
    switch (prop_id) {
        case PROP_TARGET:
            g_value_set_object(value, encfs_create_grid_get_object(grid));
            break;
        case PROP_MASTER:
            g_value_set_string(value, encfs_create_grid_get_master_file(grid));
            break;
        case PROP_IDDIR:
            g_value_set_string(value, encfs_create_grid_get_iddir(grid));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void encfs_create_grid_set_property(GObject *obj,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec) {
    (void)pspec;
    EncfsCreateGrid *self = ENCFS_CREATE_GRID(obj);
    switch (prop_id) {
        case PROP_TARGET:
            self->target = UDISKS_OBJECT(g_value_dup_object(value));
            break;
        case PROP_MASTER:
            self->master_file = g_value_dup_string(value);
            break;
        case PROP_IDDIR:
            self->iddir = g_value_dup_string(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
    check_create_satisfied(self);
}

static void encfs_create_grid_finalize(GObject *obj) {
    EncfsCreateGrid *self = ENCFS_CREATE_GRID(obj);
    g_object_unref(self->target);
    g_free(self->master_file);
    g_free(self->iddir);
}

static void encfs_create_grid_class_init(EncfsCreateGridClass *klass) {
    GtkWidgetClass *parent = GTK_WIDGET_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    gtk_widget_class_set_template_from_resource(parent, "/swhc/encfs/create-grid.ui");
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, id_list);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, list_add);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, list_remove);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, create_button);
    gtk_widget_class_bind_template_callback(parent, list_add_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, list_remove_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, create_button_clicked_cb);
    object_class->get_property = encfs_create_grid_get_property;
    object_class->set_property = encfs_create_grid_set_property;
    object_class->finalize = encfs_create_grid_finalize;

    g_object_class_install_property(object_class,
                                    PROP_TARGET,
                                    g_param_spec_object("target",
                                                        "Target",
                                                        "The udisks object used",
                                                        UDISKS_TYPE_OBJECT,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_MASTER,
                                    g_param_spec_string("master-file",
                                                        "Master file",
                                                        "The master file used",
                                                        "",
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(object_class,
                                    PROP_IDDIR,
                                    g_param_spec_string("iddir",
                                                        "ID directory",
                                                        "directory to save id private key",
                                                        "",
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void encfs_create_grid_init(EncfsCreateGrid *grid) {
    gtk_widget_init_template(GTK_WIDGET(grid));
    grid->id_list_store = gtk_list_store_new(N_COLUMNS,
                                             G_TYPE_STRING);
    grid->iddir = NULL;
    grid->master_file = NULL;
    gtk_tree_view_set_model(grid->id_list, GTK_TREE_MODEL(grid->id_list_store));
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("id", renderer,
                                                      "text", COLUMN_STRING,
                                                      NULL);
    gtk_tree_view_append_column(grid->id_list, column);
    check_create_satisfied(grid);
}
