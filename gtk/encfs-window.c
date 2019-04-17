#include"encfs-window.h"
#include"encfs-mount-grid.h"
#include"encfs-create-grid.h"
#include"encfs-mount-grid.h"
#include"encfs-monitor-grid.h"
#include"utility.h"

enum {
    PROP_0,
    PROP_CLIENT,
    PROP_SELECTED_OBJECT,
    PROP_NUM
};

static const gchar *USB_LABEL = "No Device Selected";

struct _EncfsWindow {
    GtkApplicationWindow parent;
    GtkMenuButton *usb_sel_button;
    GtkMenuButton *mode_menu_button;
    EncfsCreateGrid *create_grid;
    EncfsMountGrid *mount_grid;
    GtkLabel *usb_label;
    GtkStack *action_stack;
    GSettings *settings;
    GMenu *usb_menu;
    GList *objects;
};

G_DEFINE_TYPE(EncfsWindow, encfs_window, GTK_TYPE_APPLICATION_WINDOW);

static gint _g_dbus_object_compare(GDBusObject *a, GDBusObject *b) {
    return g_strcmp0(g_dbus_object_get_object_path(a),
                     g_dbus_object_get_object_path(b));
}

static void update_current_selected(EncfsApplication *app, UDisksObject *obj) {
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_POINTER);
    g_value_set_pointer(&val, obj);
    g_object_set_property(G_OBJECT(app), "selected-object", &val);
    g_object_notify(G_OBJECT(app), "selected-object");
    g_value_unset(&val);
}

static void on_mode_activate(GSimpleAction *action,
                             GVariant *parameter,
                             gpointer userdata) {
    EncfsWindow *win = ENCFS_WINDOW(userdata);
    const gchar *mode = g_variant_get_string(parameter, NULL);
    g_simple_action_set_state(action, parameter);
    gtk_stack_set_visible_child_name(win->action_stack, mode);
    GtkPopover *pop = gtk_menu_button_get_popover(win->mode_menu_button);
    gtk_widget_hide(GTK_WIDGET(pop));
}

static void on_usb_sel_change(GSimpleAction *action,
                              GVariant *parameter,
                              gpointer userdata) {
    (void)action;
    EncfsWindow *win = ENCFS_WINDOW(userdata);
    GMenuModel *model = G_MENU_MODEL(win->usb_menu);
    GApplication *app = g_application_get_default();
    UDisksObject *selected = encfs_application_get_selected(ENCFS_APPLICATION(app));
    int index = g_variant_get_int32(parameter);
    GVariant *label = g_menu_model_get_item_attribute_value(model, index,
                                                            "label", G_VARIANT_TYPE_STRING);
    UDisksObject *obj = UDISKS_OBJECT(g_list_nth(win->objects, index)->data);
    if (!selected ||
        _g_dbus_object_compare(G_DBUS_OBJECT(obj),
                               G_DBUS_OBJECT(selected))) {
        update_current_selected(ENCFS_APPLICATION(app), obj);
        gtk_label_set_text(win->usb_label, g_variant_get_string(label, NULL));
    }
}

static const GActionEntry actions[] = {
    { "mode", on_mode_activate, "s", "'create'", NULL, 0 },
    { "change-usb", on_usb_sel_change, "i", NULL, NULL, 0 },
};

static void diff_sorted_devices(GList *list1, GList *list2, GCompareFunc compare,
                                GList **added, GList **removed) {
    gint order;
    *added = *removed = NULL;
    while (list1 && list2) {
        order = compare(list1->data, list2->data);
        if (order < 0) {
            *removed = g_list_prepend(*removed, list1->data);
            list1 = list1->next;
        }
        else if (order > 0) {
            *added = g_list_prepend(*added, list2->data);
            list2 = list2->next;
        }
        else {
            list1 = list1->next;
            list2 = list2->next;
        }
    }
    while (list1) {
        *removed = g_list_prepend(*removed, list1->data);
        list1 = list1->next;
    }
    while (list2) {
        *added = g_list_prepend(*added, list2->data);
        list2 = list2->next;
    }
}

static gboolean should_include_block(UDisksObject *object) {
    UDisksBlock *block;
    UDisksLoop *loop;
    UDisksPartition *partition;
    gboolean ret = FALSE;
    const gchar *device;
    guint64 size;

    block = udisks_object_peek_block(object);
    partition = udisks_object_peek_partition(object);
    loop = udisks_object_peek_loop(object);

    device = udisks_block_get_device(block);
    if (g_str_has_prefix(device, "/dev/ram")) {
        goto out;
    }
    size = udisks_block_get_size(block);
    if (!size && loop) {
        goto out;
    }
    if (partition) {
        goto out;
    }
    ret = TRUE;
out:
    return ret;
}

static void update_block(gpointer data, gpointer userdata) {
    EncfsWindow *self = ENCFS_WINDOW(userdata);
    UDisksObject *obj = UDISKS_OBJECT(data);
    UDisksBlock *block = udisks_object_peek_block(obj);
    UDisksLoop *loop = udisks_object_peek_loop(obj);
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    UDisksClient *client = encfs_application_get_client(app);
    g_autoptr(UDisksObjectInfo) info = udisks_client_get_object_info(client, obj);
    guint64 size = udisks_block_get_size(block);
    g_autofree gchar *size_str = udisks_client_get_size_for_display(client, size, FALSE, FALSE);
    const gchar *loop_backing_file = loop ? udisks_loop_get_backing_file(loop) : NULL;
    const gchar *name = udisks_object_info_get_name(info);
    gint menu_size;
    g_autoptr(GVariant) var = NULL;
    g_autofree gchar *action_str = NULL;
    g_autofree gchar *id = NULL;
    if (loop_backing_file)
        id = unfused_path(loop_backing_file);
    else
        id = g_strdup_printf("%s-%s", size_str, name);
    menu_size = g_menu_model_get_n_items(G_MENU_MODEL(self->usb_menu));
    var = g_variant_new_int32(menu_size);
    action_str = g_action_print_detailed_name("win.change-usb", var);
    g_menu_append(self->usb_menu, id, action_str);
    self->objects = g_list_prepend(self->objects, g_object_ref(data));
}

static void update_usb_menus(EncfsWindow *self) {
    GDBusObjectManager *object_manager;
    GList *objects, *blocks;
    GList *l, *added = NULL, *removed = NULL, *cur_selected;
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    UDisksClient *client = encfs_application_get_client(app);
    UDisksObject *selected = encfs_application_get_selected(app);
    object_manager = udisks_client_get_object_manager(client);
    objects = g_dbus_object_manager_get_objects(object_manager);
    blocks = NULL;
    for (l = objects; l; l = l->next) {
        UDisksObject *obj = UDISKS_OBJECT(l->data);
        UDisksBlock *blk = udisks_object_peek_block(obj);
        if (!blk)
            continue;
        if (should_include_block(obj))
            blocks = g_list_prepend(blocks, g_object_ref(obj));
    }
    blocks = g_list_sort(blocks, (GCompareFunc)_g_dbus_object_compare);
    if (self->objects) {
        diff_sorted_devices(self->objects, blocks,
                            (GCompareFunc)_g_dbus_object_compare,
                            &added, &removed);
    }
    g_menu_remove_all(self->usb_menu);
    if (!selected ||
        (cur_selected = g_list_find_custom(removed, selected,
                                           (GCompareFunc)_g_dbus_object_compare))) {
        gtk_label_set_text(self->usb_label, USB_LABEL);
        update_current_selected(app, NULL);
    }
    else {
        cur_selected = g_list_find_custom(blocks, selected,
                                          (GCompareFunc)_g_dbus_object_compare);
        g_warn_if_fail(cur_selected);
        update_current_selected(app, cur_selected->data);
    }
    g_list_free_full(self->objects, g_object_unref);
    g_list_free(added);
    g_list_free(removed);
    self->objects = NULL;
    g_list_foreach(blocks, (GFunc)update_block, self);
    self->objects = g_list_reverse(self->objects);
    gtk_menu_button_set_menu_model(self->usb_sel_button, G_MENU_MODEL(self->usb_menu));
    g_list_free_full(blocks, g_object_unref);
}

static void encfs_window_constructed(GObject *obj) {
    EncfsWindow *self = ENCFS_WINDOW(obj);
    G_OBJECT_CLASS(encfs_window_parent_class)->constructed(obj);
    self->settings = g_settings_new("swhc.encfs");
    GtkBuilder *builder = gtk_builder_new_from_resource("/swhc/encfs/menu.ui");
    GMenu *menu = G_MENU(gtk_builder_get_object(builder, "mode_menu"));
    /* default mode is create mode */
    gtk_menu_button_set_menu_model(self->mode_menu_button, G_MENU_MODEL(menu));
    g_object_unref(builder);
    g_action_map_add_action_entries(G_ACTION_MAP(self), actions, G_N_ELEMENTS(actions), self);
    update_usb_menus(self);
    gtk_label_set_label(self->usb_label, USB_LABEL);
}

static void encfs_window_finalize(GObject *obj) {
    EncfsWindow *self = ENCFS_WINDOW(obj);
    if (self->settings)
        g_object_unref(self->settings);
    g_list_free_full(self->objects, g_object_unref);
    G_OBJECT_CLASS(encfs_window_parent_class)->finalize(obj);
}

static void encfs_window_class_init(EncfsWindowClass *klass) {
    g_type_ensure(ENCFS_TYPE_MOUNT_GRID);
    g_type_ensure(ENCFS_TYPE_CREATE_GRID);
    g_type_ensure(ENCFS_TYPE_MONITOR_GRID);
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/swhc/encfs/encfs-window.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, usb_sel_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, mode_menu_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, action_stack);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, usb_label);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, create_grid);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, mount_grid);
    GObjectClass *objclass = G_OBJECT_CLASS(klass);
    objclass->constructed = encfs_window_constructed;
    objclass->finalize = encfs_window_finalize;
}

static void encfs_window_init(EncfsWindow *win) {
    gtk_widget_init_template(GTK_WIDGET(win));
    win->usb_menu = g_menu_new();
    win->objects = NULL;
}

EncfsWindow *encfs_window_new(EncfsApplication *app) {
    EncfsWindow *win = ENCFS_WINDOW(g_object_new(ENCFS_TYPE_GTK_WINDOW,
                                                 "application", app,
                                                 NULL));
    g_signal_connect_swapped(encfs_application_get_client(app), "changed",
                             (GCallback)update_usb_menus, win);
    return win;
}
