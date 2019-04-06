#include<gtk/gtk.h>
#include<udisks/udisks.h>
#include"encfs-create-grid.h"

struct _EncfsCreateGrid {
    GtkGrid parent;
    GtkListBox *id_list;
    GtkToolButton *list_add, *list_remove;
    GtkButton *create_button;

    UDisksObject *target;
    gchar *master_file, *iddir;
};

enum {
    PROP_0,
    PROP_TARGET,
    PROP_MASTER,
    PROP_IDDIR
};

struct read_file_struct {
    GFileInputStream *in;
    EncfsCreateGrid *grid;
};

G_DEFINE_TYPE(EncfsCreateGrid, encfs_create_grid, GTK_TYPE_GRID);

static gboolean check_create_satisfied(EncfsCreateGrid *self) {
    void *notempty = gtk_container_get_children(GTK_CONTAINER(self->id_list));
    gboolean ret = (self->target && self->master_file && self->iddir && notempty);
    gtk_widget_set_sensitive(GTK_WIDGET(self->create_button), ret);
    return ret;
}

static void read_file_struct_free(struct read_file_struct *f) {
    g_object_unref(f->in);
    g_free(f);
}

static void read_ids(gpointer userdata) {
    struct read_file_struct *fp = userdata;
    GDataInputStream *in = g_data_input_stream_new(G_INPUT_STREAM(fp->in));
    gchar *line;
    GError *err = NULL;
    while ((line = g_data_input_stream_read_line_utf8(in, NULL, NULL, &err))) {
        GtkWidget *label = gtk_label_new(line);
        gtk_list_box_insert(fp->grid->id_list, label, -1);
        gtk_widget_show(label);
    }
    if (err)
        g_warning("%s", err->message);
    g_object_unref(in);
    check_create_satisfied(fp->grid);
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
    GtkListBox *id_list = grid->id_list;
    GList *selected = gtk_list_box_get_selected_rows(id_list);
    GtkContainer *container = GTK_CONTAINER(id_list);
    GList *l;
    for (l = selected; l; l = l->next)
        gtk_container_remove(container, GTK_WIDGET(l->data));
    check_create_satisfied(grid);
}

static void create_button_clicked_cb(EncfsCreateGrid *self) {
    if (!check_create_satisfied(self))
        return;
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

    g_object_class_install_property(object_class,
                                    PROP_TARGET,
                                    g_param_spec_object("target",
                                                        "Target",
                                                        "The udisks object used",
                                                        UDISKS_TYPE_OBJECT,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_CONSTRUCT));
    g_object_class_install_property(object_class,
                                    PROP_MASTER,
                                    g_param_spec_string("master-file",
                                                        "Master file",
                                                        "The master file used",
                                                        "",
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_CONSTRUCT));
    g_object_class_install_property(object_class,
                                    PROP_IDDIR,
                                    g_param_spec_string("iddir",
                                                        "ID directory",
                                                        "directory to save id private key",
                                                        "",
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_CONSTRUCT));
}

static void encfs_create_grid_init(EncfsCreateGrid *grid) {
    gtk_widget_init_template(GTK_WIDGET(grid));
    check_create_satisfied(grid);
}
