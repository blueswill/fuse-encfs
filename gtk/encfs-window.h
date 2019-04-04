#ifndef ENCFS_WINDOW_H
#define ENCFS_WINDOW_H

#include"encfs-application.h"
#include<gtk/gtk.h>
#include<udisks/udisks.h>

G_BEGIN_DECLS

#define ENCFS_TYPE_GTK_WINDOW (encfs_window_get_type())

G_DECLARE_FINAL_TYPE(EncfsWindow, encfs_window, ENCFS, WINDOW, GtkApplicationWindow);

EncfsWindow *encfs_window_new(EncfsApplication *app, UDisksClient *client);

G_END_DECLS

#endif
