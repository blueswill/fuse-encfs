#include"encfs-window.h"
#include"encfs-mount-grid.h"
#include"encfs-create-grid.h"

struct _EncfsWindow {
    GtkApplicationWindow parent;
    GtkMenuButton *usb_sel_button;
    GtkMenuButton *mode_menu_button;
};

G_DEFINE_TYPE(EncfsWindow, encfs_window, GTK_TYPE_APPLICATION_WINDOW);

static void encfs_window_class_init(EncfsWindowClass *klass) {
    g_type_ensure(ENCFS_TYPE_MOUNT_GRID);
    g_type_ensure(ENCFS_TYPE_CREATE_GRID);
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/swhc/encfs/encfs-window.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, usb_sel_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, mode_menu_button);
}

static void encfs_window_init(EncfsWindow *win) {
    gtk_widget_init_template(GTK_WIDGET(win));
}

GtkWidget *encfs_window_new(EncfsApplication *app) {
    return g_object_new(ENCFS_TYPE_GTK_WINDOW,
            "application", app,
            NULL);
}
