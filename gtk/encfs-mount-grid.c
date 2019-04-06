#include<gtk/gtk.h>
#include<udisks/udisks.h>
#include"encfs-mount-grid.h"

struct _EncfsMountGrid {
    GtkGrid parent;
    GtkButton *mount_button;
    GtkFileChooserButton *priv_file;
    
    UDisksObject *target;
};

enum {
    PROP_0,
    PROP_TARGET
};

G_DEFINE_TYPE(EncfsMountGrid, encfs_mount_grid, GTK_TYPE_GRID);

static void check_mount_satisfied(EncfsMountGrid *self) {
    GFile *priv = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(self->priv_file));
    gboolean ret = FALSE;
    if (priv && self->target)
        ret = TRUE;
    if (priv)
        g_object_unref(priv);
    gtk_widget_set_sensitive(GTK_WIDGET(self->mount_button), ret);
}

static void mount_button_clicked_cb(GtkFileChooserButton *fb) {
}

static void priv_file_file_set_cb(EncfsMountGrid *self) {
    check_mount_satisfied(self);
}

static UDisksObject *encfs_mount_grid_get_target(EncfsMountGrid *self) {
    g_return_val_if_fail(ENCFS_IS_MOUNT_GRID(self), NULL);
    return self->target;
}

static void encfs_mount_grid_get_property(GObject *obj,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec) {
    EncfsMountGrid *self = ENCFS_MOUNT_GRID(obj);
    switch (prop_id) {
        case PROP_TARGET:
            g_value_set_object(value, encfs_mount_grid_get_target(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void encfs_mount_grid_set_property(GObject *obj,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec) {
    EncfsMountGrid *self = ENCFS_MOUNT_GRID(obj);
    switch (prop_id) {
        case PROP_TARGET:
            self->target = UDISKS_OBJECT(g_value_dup_object(value));
            check_mount_satisfied(self);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void encfs_mount_grid_class_init(EncfsMountGridClass *klass) {
    GtkWidgetClass *parent = GTK_WIDGET_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    gtk_widget_class_set_template_from_resource(parent, "/swhc/encfs/mount-grid.ui");
    gtk_widget_class_bind_template_child(parent, EncfsMountGrid, mount_button);
    gtk_widget_class_bind_template_child(parent, EncfsMountGrid, priv_file);
    gtk_widget_class_bind_template_callback(parent, mount_button_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, priv_file_file_set_cb);
    object_class->get_property = encfs_mount_grid_get_property;
    object_class->set_property = encfs_mount_grid_set_property;
    g_object_class_install_property(object_class,
                                    PROP_TARGET,
                                    g_param_spec_object("target",
                                                        "Target device",
                                                        "target device to use",
                                                        UDISKS_TYPE_OBJECT,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void encfs_mount_grid_init(EncfsMountGrid *grid) {
    gtk_widget_init_template(GTK_WIDGET(grid));
}
