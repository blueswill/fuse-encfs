#include<gtk/gtk.h>
#include<udisks/udisks.h>
#include<gio/gunixfdlist.h>
#include<fcntl.h>
#include<errno.h>
#include<stdarg.h>
#include<sys/wait.h>
#include"encfs-mount-grid.h"
#include"encfs-application.h"
#include"encfs_helper.h"
#include"mount-context.h"
#include"config.h"

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

static gboolean mount_target(gpointer userdata) {
    gchar *tempdir = userdata;
    GApplication *app = g_application_get_default();
    GVariant *index = NULL;
    GUnixFDList *fd_list = NULL;
    int dirfd = open(tempdir, O_RDONLY);
    if (dirfd < 0) {
        g_warning("open %s error: %s", tempdir, g_strerror(errno));
        return FALSE;
    }
    int fd = openat(dirfd, "target", O_RDWR);
    if (fd < 0) {
        g_warning("open %s/target error: %s", tempdir, g_strerror(errno));
        goto end;
    }
    fd_list = g_unix_fd_list_new();
    GError *err = NULL;
    int ind = g_unix_fd_list_append(fd_list, fd, &err);
    if (err) {
        g_warning(err->message);
        g_error_free(err);
        goto end;
    }
    index = g_variant_new_handle(ind);
    gchar *out_device;
    if (!encfs_application_loop_setup(app, index, fd_list, &out_device, NULL, &err)) {
        g_warning(err->message);
        g_error_free(err);
        goto end;
    }
    g_info("mapped woth %s", out_device);
    g_free(out_device);
end:
    if (dirfd >= 0)
        close(dirfd);
    if (fd >= 0)
        close(fd);
    if (fd_list)
        g_object_unref(fd_list);
}

static void do_mount_fork(struct mount_context *ctx, const char *mount_point,
                          int argc, ...) {
    pid_t pid = fork();
    if (pid < 0)
        g_warning("get sub process error: %s", g_strerror(errno));
    else if (!pid) {
        setsid();
        va_list ap;
        va_start(ap, argc);
        mount_context_mountv(ctx, mount_point, argc, ap);
        va_end(ap);
        exit(EXIT_FAILURE);
    }
}

static int do_mount(int blkfd, struct crypto *crypto) {
    const gchar *cachedir = g_get_user_cache_dir();
    gchar *templ = g_strdup_printf("%s/%s/XXXXXX", cachedir, PROJECT_NAME);
    gchar *tempdir = g_mkdtemp(templ);
    uid_t uid = getuid();
    gid_t gid = getgid();
    gchar *options = g_strdup_printf("uid=%d,gid=%d,allow_other", uid, gid);
    if (!tempdir)
        return -1;
    struct mount_context *ctx = mount_context_new(blkfd, crypto);
    if (!ctx)
        return -1;
    g_info("mount point is %s", tempdir);
    do_mount_fork(ctx, tempdir, 2, "-o", options);
    g_free(options);
    g_timeout_add(500, mount_target, tempdir);
    mount_context_free(ctx);
    return 0;
}

static void check_mount_satisfied(EncfsMountGrid *self) {
    GFile *priv = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(self->priv_file));
    gboolean ret = FALSE;
    if (priv && self->target)
        ret = TRUE;
    if (priv)
        g_object_unref(priv);
    gtk_widget_set_sensitive(GTK_WIDGET(self->mount_button), ret);
}

static void show_error_dialog(GtkWindow *win, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    gchar *str = g_strdup_vprintf(format, ap);
    va_end(ap);
    GtkWidget *dialog = gtk_message_dialog_new(win,
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "%s", str);
    g_free(str);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void mount_button_clicked_cb(EncfsMountGrid *self) {
    int blkfd = -1, cryptofd = -1;
    gchar *priv_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(self->priv_file));
    UDisksBlock *blk = udisks_object_peek_block(self->target);
    GtkWindow *win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self)));
    struct crypto *crypto = NULL;
    GError *err = NULL;
    GVariantDict dict;
    GVariant *out_index, *options;
    GUnixFDList *out_list;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "auth.no_user_interaction", "b", FALSE);
    options = g_variant_dict_end(&dict);
    if (!udisks_block_call_open_device_sync(blk, "rw", options, NULL,
                                            &out_index, &out_list, NULL,&err)) {
        g_warning(err->message);
        g_error_free(err);
        goto end;
    }
    else {
        gint index = g_variant_get_handle(out_index);
        err = NULL;
        g_variant_unref(out_index);
        blkfd = g_unix_fd_list_get(out_list, index, &err);
        g_object_unref(out_list);
        if (blkfd == -1) {
            g_warning(err->message);
            g_error_free(err);
            goto end;
        }
    }
    if (!priv_file)
        return;
    cryptofd = open(priv_file, O_RDONLY);
    if (cryptofd < 0) {
        show_error_dialog(win, "Error open file %s: %s", priv_file, g_strerror(errno));
        goto end;
    }
    if (!(crypto = crypto_read_file(cryptofd))) {
        show_error_dialog(win, "read %s error", priv_file);
        goto end;
    }
    if (do_mount(blkfd, crypto) < 0)
        show_error_dialog(win, "mount error");
end:
    if (blkfd >= 0)
        close(blkfd);
    if (cryptofd >= 0)
        close(cryptofd);
    crypto_free(crypto);
    g_free(priv_file);
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
