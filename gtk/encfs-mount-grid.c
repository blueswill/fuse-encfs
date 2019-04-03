#include<gtk/gtk.h>
#include"encfs-mount-grid.h"

struct _EncfsMountGrid {
    GtkGrid parent;
    GtkButton *mount_button;
};

G_DEFINE_TYPE(EncfsMountGrid, encfs_mount_grid, GTK_TYPE_GRID);

static void mount_button_clicked_cb(GtkFileChooserButton *fb) {
    g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "mount button clicked");
}

static void encfs_mount_grid_class_init(EncfsMountGridClass *klass) {
    GtkWidgetClass *parent = GTK_WIDGET_CLASS(klass);
    gtk_widget_class_set_template_from_resource(parent, "/swhc/encfs/mount-grid.ui");
    gtk_widget_class_bind_template_child(parent, EncfsMountGrid, mount_button);
    gtk_widget_class_bind_template_callback(parent, mount_button_clicked_cb);
}

static void encfs_mount_grid_init(EncfsMountGrid *grid) {
    gtk_widget_init_template(GTK_WIDGET(grid));
}
