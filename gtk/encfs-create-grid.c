#include<gtk/gtk.h>
#include"encfs-create-grid.h"

struct _EncfsCreateGrid {
    GtkGrid parent;
    GtkFileChooserButton *master_file;
    GtkListBox *id_list;
    GtkToolButton *list_add, *list_remove;
    GtkButton *create_button;
};

G_DEFINE_TYPE(EncfsCreateGrid, encfs_create_grid, GTK_TYPE_GRID);

static void list_add_clicked_cb(EncfsCreateGrid *grid) {
    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "list_add button clicked");
}

static void list_remove_clicked_cb(EncfsCreateGrid *grid) {
    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "list_remove button clicked");
}

static void create_button_clicked_cb(EncfsCreateGrid *grid) {
    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "create button clicked");
}

static void encfs_create_grid_class_init(EncfsCreateGridClass *klass) {
    GtkWidgetClass *parent = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(parent, "/swhc/encfs/create-grid.ui");
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, master_file);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, id_list);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, list_add);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, list_remove);
    gtk_widget_class_bind_template_child(parent, EncfsCreateGrid, create_button);
    gtk_widget_class_bind_template_callback(parent, list_add_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, list_remove_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, create_button_clicked_cb);
}

static void encfs_create_grid_init(EncfsCreateGrid *grid) {
    gtk_widget_init_template(GTK_WIDGET(grid));
}
