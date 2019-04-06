#include"encfs-window.h"
#include"encfs-mount-grid.h"
#include"encfs-create-grid.h"
#include"encfs-mount-grid.h"

enum {
    PROP_0,
    PROP_APPLICATION,
    PROP_CLIENT
};

struct _EncfsWindow {
    GtkApplicationWindow parent;
    GtkMenuButton *usb_sel_button;
    GtkMenuButton *mode_menu_button;
    GtkMenuButton *action_menu_button;
    EncfsCreateGrid *create_grid;
    EncfsMountGrid *mount_grid;
    GtkLabel *usb_label;
    GtkStack *action_stack;
    GSettings *settings;
    UDisksClient *client;
    GMenu *usb_menu, *create_menu;
    GPtrArray *objects;
};

G_DEFINE_TYPE(EncfsWindow, encfs_window, GTK_TYPE_APPLICATION_WINDOW);

static void on_mode_activate(GSimpleAction *action,
                             GVariant *parameter,
                             gpointer userdata) {
    EncfsWindow *win = ENCFS_WINDOW(userdata);
    const gchar *mode = g_variant_get_string(parameter, NULL);
    g_simple_action_set_state(action, parameter);
    gtk_stack_set_visible_child_name(win->action_stack, mode);
    if (!g_strcmp0(mode, "create")) {
        gtk_widget_set_sensitive(GTK_WIDGET(win->action_menu_button), TRUE);
        gtk_menu_button_set_menu_model(win->action_menu_button,
                                       G_MENU_MODEL(win->create_menu));
    }
    else
        gtk_widget_set_sensitive(GTK_WIDGET(win->action_menu_button), FALSE);
    GtkPopover *pop = gtk_menu_button_get_popover(win->mode_menu_button);
    gtk_widget_hide(GTK_WIDGET(pop));
}

static void on_usb_sel_change(GSimpleAction *action,
                              GVariant *parameter,
                              gpointer userdata) {
    (void)action;
    EncfsWindow *win = ENCFS_WINDOW(userdata);
    GMenuModel *model = G_MENU_MODEL(win->usb_menu);
    int index = g_variant_get_int32(parameter);
    GVariant *label = g_menu_model_get_item_attribute_value(model, index, "label", G_VARIANT_TYPE_STRING);
    UDisksObject *obj = UDISKS_OBJECT(g_ptr_array_index(win->objects, index));
    GValue val = G_VALUE_INIT;
    g_value_init(&val, UDISKS_TYPE_OBJECT);
    gtk_label_set_text(win->usb_label, g_variant_get_string(label, NULL));
    g_value_set_object(&val, obj);
    g_object_set_property(G_OBJECT(win->create_grid), "target", &val);
    g_object_set_property(G_OBJECT(win->mount_grid), "target", &val);
    g_value_unset(&val);
}

static void on_select_master_clicked(GSimpleAction *action,
                                     GVariant *parameter,
                                     gpointer userdata) {
    (void)action;
    (void)parameter;
    EncfsWindow *win = ENCFS_WINDOW(userdata);
    GtkWidget *dialog =
        gtk_file_chooser_dialog_new("Select master key pair",
                                    GTK_WINDOW(win),
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
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        gchar *name = gtk_file_chooser_get_filename(chooser);
        GValue val = G_VALUE_INIT;
        g_value_init(&val, G_TYPE_STRING);
        g_value_set_string(&val, name);
        g_object_set_property(G_OBJECT(win->create_grid), "master-file", &val);
        g_value_unset(&val);
        g_free(name);
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_select_iddir_clicked(GSimpleAction *action,
                                    GVariant *parameter,
                                    gpointer userdata) {
    (void)action;
    (void)parameter;
    EncfsWindow *win = ENCFS_WINDOW(userdata);
    GtkWidget *dialog =
        gtk_file_chooser_dialog_new("Select ID directory",
                                    GTK_WINDOW(win),
                                    GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER |
                                    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                    "Cancel", GTK_RESPONSE_CANCEL,
                                    "Open", GTK_RESPONSE_ACCEPT,
                                    NULL);
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        gchar *name = gtk_file_chooser_get_filename(chooser);
        GValue val = G_VALUE_INIT;
        g_value_init(&val, G_TYPE_STRING);
        g_value_set_string(&val, name);
        g_object_set_property(G_OBJECT(win->create_grid), "iddir", &val);
        g_value_unset(&val);
        g_free(name);
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}

static const GActionEntry actions[] = {
    { "mode", on_mode_activate, "s", "'create'", NULL, 0 },
    { "change-usb", on_usb_sel_change, "i", NULL, NULL, 0 },
    { "select-master", on_select_master_clicked, NULL, NULL, NULL, 0 },
    { "select-iddir", on_select_iddir_clicked, NULL, NULL, NULL, 0 }
};

static UDisksClient *encfs_window_get_client(EncfsWindow *win) {
    g_return_val_if_fail(ENCFS_IS_WINDOW(win), NULL);
    return win->client;
}

static gint _g_dbus_object_compare(GDBusObject *a, GDBusObject *b) {
    return g_strcmp0(g_dbus_object_get_object_path(a),
                     g_dbus_object_get_object_path(b));
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

static gchar *unfused_path(const gchar *path) {
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

static void update_usb_menus(EncfsWindow *self) {
    GDBusObjectManager *object_manager;
    GList *objects, *blocks;
    GList *l;
    object_manager = udisks_client_get_object_manager(self->client);
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
    g_menu_remove_all(self->usb_menu);
    g_ptr_array_remove_range(self->objects, 0, self->objects->len);
    for (l = blocks; l; l = l->next) {
        UDisksObject *obj = UDISKS_OBJECT(l->data);
        UDisksBlock *block = udisks_object_peek_block(obj);
        UDisksLoop *loop = udisks_object_peek_loop(obj);
        UDisksObjectInfo *info = udisks_client_get_object_info(self->client, obj);
        guint64 size = udisks_block_get_size(block);
        gchar *size_str = udisks_client_get_size_for_display(self->client, size, FALSE, FALSE);
        const gchar *loop_backing_file = loop ? udisks_loop_get_backing_file(loop) : NULL;
        const gchar *name = udisks_object_info_get_name(info);
        gint menu_size;
        GVariant *var;
        gchar *action_str;
        gchar *id;
        if (loop_backing_file)
            id = unfused_path(loop_backing_file);
        else
            id = g_strdup_printf("%s-%s", size_str, name);
        menu_size = g_menu_model_get_n_items(G_MENU_MODEL(self->usb_menu));
        var = g_variant_new_int32(menu_size);
        action_str = g_action_print_detailed_name("win.change-usb", var);
        g_menu_append(self->usb_menu, id, action_str);
        g_ptr_array_add(self->objects, g_object_ref(obj));
        g_free(id);
        g_free(action_str);
        g_variant_unref(var);
        g_free(size_str);
    }
    gtk_menu_button_set_menu_model(self->usb_sel_button, G_MENU_MODEL(self->usb_menu));
    g_list_foreach(blocks, (GFunc)g_object_unref, NULL);
    g_list_free(blocks);
}

static void encfs_window_constructed(GObject *obj) {
    EncfsWindow *self = ENCFS_WINDOW(obj);
    G_OBJECT_CLASS(encfs_window_parent_class)->constructed(obj);
    self->settings = g_settings_new("swhc.encfs");
    GtkBuilder *builder = gtk_builder_new_from_resource("/swhc/encfs/menu.ui");
    GMenu *menu = G_MENU(gtk_builder_get_object(builder, "mode_menu"));
    self->create_menu = G_MENU(gtk_builder_get_object(builder, "create_menu"));
    g_object_ref(self->create_menu);
    /* default mode is create mode */
    gtk_menu_button_set_menu_model(self->action_menu_button, G_MENU_MODEL(self->create_menu));
    gtk_menu_button_set_menu_model(self->mode_menu_button, G_MENU_MODEL(menu));
    g_object_unref(builder);
    g_action_map_add_action_entries(G_ACTION_MAP(self), actions, G_N_ELEMENTS(actions), self);
    update_usb_menus(self);
    gtk_label_set_label(self->usb_label, "No Device Seletcted");
}

static void encfs_window_finalize(GObject *obj) {
    EncfsWindow *self = ENCFS_WINDOW(obj);
    if (self->client)
        g_object_unref(self->client);
    if (self->settings)
        g_object_unref(self->settings);
    if (self->create_menu)
        g_object_unref(self->create_menu);
    G_OBJECT_CLASS(encfs_window_parent_class)->finalize(obj);
}

static void encfs_window_get_property(GObject *obj,
                                      guint prop_id,
                                      GValue *value,
                                      GParamSpec *pspec) {
    EncfsWindow *self = ENCFS_WINDOW(obj);
    switch (prop_id) {
        case PROP_CLIENT:
            g_value_set_object(value, encfs_window_get_client(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void encfs_window_set_property(GObject *obj,
                                      guint prop_id,
                                      const GValue *value,
                                      GParamSpec *pspec) {
    EncfsWindow *self = ENCFS_WINDOW(obj);
    switch (prop_id) {
        case PROP_CLIENT:
            self->client = UDISKS_CLIENT(g_value_dup_object(value));
            g_signal_connect_swapped(self->client, "changed",
                                     (GCallback)update_usb_menus,
                                     self);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void encfs_window_class_init(EncfsWindowClass *klass) {
    g_type_ensure(ENCFS_TYPE_MOUNT_GRID);
    g_type_ensure(ENCFS_TYPE_CREATE_GRID);
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), "/swhc/encfs/encfs-window.ui");
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, usb_sel_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, mode_menu_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, action_stack);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, usb_label);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, action_menu_button);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, create_grid);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), EncfsWindow, mount_grid);
    GObjectClass *objclass = G_OBJECT_CLASS(klass);
    objclass->constructed = encfs_window_constructed;
    objclass->finalize = encfs_window_finalize;
    objclass->get_property = encfs_window_get_property;
    objclass->set_property = encfs_window_set_property;
    g_object_class_install_property(objclass,
                                    PROP_CLIENT,
                                    g_param_spec_object("client",
                                                        "Client",
                                                        "The client used by the window",
                                                        UDISKS_TYPE_CLIENT,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

static void encfs_window_init(EncfsWindow *win) {
    gtk_widget_init_template(GTK_WIDGET(win));
    win->usb_menu = g_menu_new();
    win->objects = g_ptr_array_new_with_free_func(g_object_unref);
}

EncfsWindow *encfs_window_new(EncfsApplication *app, UDisksClient *client) {
    return g_object_new(ENCFS_TYPE_GTK_WINDOW,
                        "application", app,
                        "client", client,
                        NULL);
}
