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
#include"utility.h"

struct _EncfsMountGrid {
    GtkGrid parent;
    GtkButton *mount_button;
    GtkFileChooserButton *priv_file;
};

G_DEFINE_TYPE(EncfsMountGrid, encfs_mount_grid, GTK_TYPE_GRID);

static UDisksObject *get_target(void) {
    GApplication *app = g_application_get_default();
    return encfs_application_get_selected(ENCFS_APPLICATION(app));
}

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

struct wait_fork_struct {
    pid_t pid;
    gchar *mount_point;
};

static gpointer wait_fork(gpointer userdata) {
    struct wait_fork_struct *st = userdata;
    int wstate;
    if (waitpid(st->pid, &wstate, WNOHANG) < 0)
        g_warning(g_strerror(errno));
    else if (WIFEXITED(wstate) && !WEXITSTATUS(wstate)) {
        GApplication *app = g_application_get_default();
        g_signal_emit_by_name(ENCFS_APPLICATION(app), "mounted", st->mount_point);
    }
    g_free(st->mount_point);
    g_free(st);
    return NULL;
}

static void do_mount_fork(struct mount_context *ctx, const char *mount_point,
                          int argc, ...) {
    pid_t pid = fork();
    if (pid < 0)
        g_warning("get sub process error: %s", g_strerror(errno));
    else if (!pid) {
        va_list ap;
        int ret;
        setsid();
        va_start(ap, argc);
        ret = mount_context_mountv(ctx, mount_point, argc, ap);
        va_end(ap);
        exit(ret);
    }
    else {
        struct wait_fork_struct *st = g_new(struct wait_fork_struct, 1);
        st->pid = pid;
        st->mount_point = g_strdup(mount_point);
        GThread *thread = g_thread_new(NULL, wait_fork, st);
        g_thread_unref(thread);
    }
}

static int do_mount(int blkfd, struct crypto *crypto, const gchar *target) {
    const gchar *cachedir = g_get_user_cache_dir();
    gchar *templ = g_strdup_printf("%s/%s/XXXXXX", cachedir, PROJECT_NAME);
    gchar *tempdir = g_mkdtemp(templ);
    uid_t uid = getuid();
    gid_t gid = getgid();
    gchar *options = g_strdup_printf("uid=%d,gid=%d,allow_other", uid, gid);
    if (!tempdir)
        return -1;
    struct mount_context *ctx = mount_context_new(blkfd, crypto, target);
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
    if (priv && get_target())
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
    GApplication *app = g_application_get_default();
    UDisksClient *client = UDISKS_CLIENT(encfs_application_get_client(ENCFS_APPLICATION(app)));
    UDisksObject *obj = get_target();
    if (!obj)
        return;
    UDisksBlock *blk = udisks_object_peek_block(obj);
    UDisksLoop *loop = udisks_object_peek_loop(obj);
    UDisksObjectInfo *info = udisks_client_get_object_info(client, obj);
    GtkWindow *win = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self)));
    struct crypto *crypto = NULL;
    g_autoptr(GError) err = NULL;
    GVariantDict dict;
    GVariant *out_index, *options;
    GUnixFDList *out_list;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "auth.no_user_interaction", "b", FALSE);
    g_variant_dict_insert(&dict, "flags", "i", O_EXCL);
    options = g_variant_dict_end(&dict);
    if (!udisks_block_call_open_device_sync(blk, "rw", options, NULL,
                                            &out_index, &out_list, NULL,&err)) {
        show_error_dialog(win, "%s", err->message);
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
            show_error_dialog(win, "%s", err->message);
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
    g_autofree gchar *pass = NULL;
    if ((pass = _get_password())) {
        if (!(crypto = crypto_read_file(cryptofd, _decrypt, pass))) {
            show_error_dialog(win, "read %s error", priv_file);
            goto end;
        }
        const gchar *name = udisks_object_info_get_name(info);
        const gchar *loop_backing_file = loop ? udisks_loop_get_backing_file(loop) : NULL;
        g_autofree gchar *target = NULL;
        if (loop_backing_file)
            target = unfused_path(loop_backing_file);
        else
            target = g_strdup(name);
        if (do_mount(blkfd, crypto, target) < 0)
            show_error_dialog(win, "mount error");
    }
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

static void on_usb_sel_changed(GtkWidget *widget, GParamSpec *spec,
                               gpointer userdata) {
    (void)widget;
    (void)spec;
    check_mount_satisfied(ENCFS_MOUNT_GRID(userdata));
}

static void encfs_mount_grid_constructed(GObject *object) {
    EncfsMountGrid *self = ENCFS_MOUNT_GRID(object);
    gtk_widget_set_sensitive(GTK_WIDGET(self->mount_button), FALSE);
    EncfsApplication *app = ENCFS_APPLICATION(g_application_get_default());
    g_signal_connect(app, "notify::selected-object",
                     G_CALLBACK(on_usb_sel_changed), self);
}

static void encfs_mount_grid_class_init(EncfsMountGridClass *klass) {
    GtkWidgetClass *parent = GTK_WIDGET_CLASS(klass);
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    gtk_widget_class_set_template_from_resource(parent, "/swhc/encfs/mount-grid.ui");
    gtk_widget_class_bind_template_child(parent, EncfsMountGrid, mount_button);
    gtk_widget_class_bind_template_child(parent, EncfsMountGrid, priv_file);
    gtk_widget_class_bind_template_callback(parent, mount_button_clicked_cb);
    gtk_widget_class_bind_template_callback(parent, priv_file_file_set_cb);
    object_class->constructed = encfs_mount_grid_constructed;
}

static void encfs_mount_grid_init(EncfsMountGrid *grid) {
    gtk_widget_init_template(GTK_WIDGET(grid));
}
