#include<gtk/gtk.h>
#include<udisks/udisks.h>
#include<fcntl.h>
#include<errno.h>
#include<gio/gunixfdlist.h>
#include"encfs-application.h"
#include"encfs-window.h"
#include"encfs-create-grid.h"
#include"sm9.h"
#include"create-context.h"
#include"encfs_helper.h"
#include"config.h"
#include"utility.h"

struct _EncfsCreateGrid {
    GtkGrid parent;
    GtkTreeView *id_list;
    GtkToolButton *list_add, *list_remove;
    GtkButton *create_button,
              *master_sel_button,
              *id_dir_sel_button;
    GtkLabel *master_sel_label,
             *id_dir_sel_label;
    GtkWindow *win;

    GtkListStore *id_list_store;

    gchar *master_file, *iddir;
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

static void show_dialog(GtkWindow *win, gchar *format, ...) {
    va_list ap;
    va_start(ap, format);
    gchar *str = g_strdup_vprintf(format, ap);
    va_end(ap);
    GtkWidget *dialog = gtk_message_dialog_new(win,
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s", str);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(str);
    return;
}

static gboolean _create(gpointer userdata) {
    struct create_device_struct *st = userdata;
    EncfsCreateGrid *self = st->grid;
    int iddir = open(self->iddir, O_RDONLY);
    gchar *masterfile = g_strdup(self->master_file);
    if (!g_str_has_suffix(masterfile, MASTER_KEY_PAIR_SUFFIX)) {
        g_free(masterfile);
        masterfile = g_strdup_printf("%s"MASTER_KEY_PAIR_SUFFIX, self->master_file);
    }
    if (iddir < 0) {
        show_dialog(self->win,
                    "Error open directory %s: %s", self->iddir, g_strerror(errno));
        goto end;
    }
    int masterfd = open(masterfile, O_RDWR | O_CREAT,
                        S_IRUSR | S_IWUSR | S_IRGRP);
    if (masterfd < 0) {
        show_dialog(self->win,
                    "Error open master file %s: %s", masterfile, g_strerror(errno));
        goto end;
    }
    g_autofree gchar *pass = NULL;
    if ((pass = _get_password())) {
        struct master_key_pair *pair = master_key_pair_read_file(masterfd, _decrypt, (void *)pass);
        int regenerate = 0;
        if (!pair) {
            g_warning("master file regenerated");
            regenerate = 1;
            pair = generate_master_key_pair(TYPE_ENCRYPT);
            ftruncate(masterfd, 0);
            master_key_pair_write_file(pair, masterfd, _encrypt, NULL);
        }
        close(masterfd);
        struct create_context *args = create_context_new(st->blkfd, iddir,
                                                         pair, regenerate);
        if (!args) {
            show_dialog(self->win, "fail to get create context");
            goto end;
        }
        gchar **ids = id_list_get_ids(GTK_TREE_MODEL(self->id_list_store));
        if (create_context_create(args, (void *)ids) < 0)
            show_dialog(self->win, "fail to create device");
        g_strfreev(ids);
        create_context_free(args);
    }
end:
    if (iddir >= 0)
        close(iddir);
    if (masterfd >= 0)
        close(masterfd);
    g_free(masterfile);
    return G_SOURCE_REMOVE;
}

static UDisksObject *get_target(void) {
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    return encfs_application_get_selected(app);
}

static gboolean check_create_satisfied(EncfsCreateGrid *self) {
    GtkTreeIter iter;
    gboolean notempty = FALSE;
    if (self->id_list_store)
        notempty = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(self->id_list_store), &iter);
    gboolean ret = (UDISKS_IS_OBJECT(get_target()) && self->master_file && self->iddir && notempty);
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
        show_dialog(fp->grid->win, "%s", err->message);
    g_object_unref(in);
    check_create_satisfied(fp->grid);
    return G_SOURCE_REMOVE;
}

static gboolean read_id_from_file(EncfsCreateGrid *self, GFile *file) {
    GError *err = NULL;
    GFileInputStream *in = g_file_read(file, NULL, &err);
    if (!in) {
        GtkWidget *dialog = gtk_dialog_new_with_buttons("file read error",
                                                        self->win,
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
                                    grid->win,
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
    g_variant_dict_insert(&dict, "flags", "i", O_EXCL);
    gpointer data = get_target();
    if (!data)
        return;
    UDisksBlock *blk = udisks_object_peek_block(UDISKS_OBJECT(data));
    options = g_variant_dict_end(&dict);
    if (!udisks_block_call_open_device_sync(blk, "rw", options, NULL,
                                            &out_index, &out_list, NULL, &err)) {
        show_dialog(self->win, "%s", err->message);
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
            show_dialog(self->win, "%s", err->message);
            return;
        }
        struct create_device_struct *st = g_new(struct create_device_struct, 1);
        st->blkfd = handler;
        st->grid = self;
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, G_SOURCE_FUNC(_create),
                        st, (GDestroyNotify)create_device_struct_free);
    }
}

static void encfs_create_grid_finalize(GObject *obj) {
    EncfsCreateGrid *self = ENCFS_CREATE_GRID(obj);
    g_free(self->master_file);
    g_free(self->iddir);
}

static void master_sel_button_clicked_cb(EncfsCreateGrid *self) {
    GApplication *app = g_application_get_default();
    GtkWindow *win = encfs_application_get_window(ENCFS_APPLICATION(app));
    GtkWidget *dialog =
        gtk_file_chooser_dialog_new("Select master key pair",
                                    win,
                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                    "Cancel", GTK_RESPONSE_CANCEL,
                                    "Open", GTK_RESPONSE_ACCEPT,
                                    NULL);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.master");
    gtk_file_filter_set_name(filter, "master file");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        g_free(self->master_file);
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        self->master_file = gtk_file_chooser_get_filename(chooser);
        gtk_label_set_text(self->master_sel_label, self->master_file);
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
    check_create_satisfied(self);
}

static void id_dir_sel_button_clicked_cb(EncfsCreateGrid *self) {
    GApplication *app = g_application_get_default();
    GtkWindow *win = encfs_application_get_window(ENCFS_APPLICATION(app));
    GtkWidget *dialog =
        gtk_file_chooser_dialog_new("Select ID directory",
                                    win,
                                    GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER |
                                    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                    "Cancel", GTK_RESPONSE_CANCEL,
                                    "Open", GTK_RESPONSE_ACCEPT,
                                    NULL);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        g_free(self->iddir);
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        self->iddir = gtk_file_chooser_get_filename(chooser);
        gtk_label_set_text(self->id_dir_sel_label, self->iddir);
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_usb_sel_changed(GtkWidget *widget, GParamSpec *spec,
                               gpointer userdata) {
    (void)widget;
    (void)spec;
    check_create_satisfied(ENCFS_CREATE_GRID(userdata));
}

static void encfs_create_grid_constructed(GObject *object) {
    EncfsCreateGrid *self = ENCFS_CREATE_GRID(object);
    self->id_list_store = gtk_list_store_new(N_COLUMNS,
                                             G_TYPE_STRING);
    gtk_tree_view_set_model(self->id_list, GTK_TREE_MODEL(self->id_list_store));
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("id", renderer,
                                                      "text", COLUMN_STRING,
                                                      NULL);
    gtk_tree_view_append_column(self->id_list, column);
    GApplication *app = g_application_get_default();
    g_signal_connect(ENCFS_APPLICATION(app), "notify::selected-object",
                     G_CALLBACK(on_usb_sel_changed), self);
    gtk_widget_set_sensitive(GTK_WIDGET(self->create_button), FALSE);
}

static void encfs_create_grid_class_init(EncfsCreateGridClass *klass) {
    GtkWidgetClass *parent = GTK_WIDGET_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    gtk_widget_class_set_template_from_resource(parent, "/swhc/encfs/create-grid.ui");
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, id_list);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, list_add);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, list_remove);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, create_button);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, master_sel_button);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, id_dir_sel_button);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, master_sel_label);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, id_dir_sel_label);
    gtk_widget_class_bind_template_callback(parent, list_add_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, list_remove_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, create_button_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, master_sel_button_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, id_dir_sel_button_clicked_cb);
    object_class->finalize = encfs_create_grid_finalize;
    object_class->constructed = encfs_create_grid_constructed;
}

static void encfs_create_grid_init(EncfsCreateGrid *grid) {
    gtk_widget_init_template(GTK_WIDGET(grid));
}
