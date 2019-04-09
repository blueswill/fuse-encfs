#ifndef ENCFS_APPLICATION_H
#define ENCFS_APPLICATION_H

#include<gtk/gtk.h>

G_BEGIN_DECLS

#define ENCFS_TYPE_APPLICATION (encfs_application_get_type())

G_DECLARE_FINAL_TYPE(EncfsApplication, encfs_application, ENCFS, APPLICATION, GtkApplication);

EncfsApplication *encfs_application_new(void);

gboolean encfs_application_loop_setup(GApplication *app,
                                      GVariant *arg_fd, GUnixFDList *fd_list,
                                      gchar **out_resulting_device, GUnixFDList **out_fd_list,
                                      GError **error);

G_END_DECLS

#endif
