#ifndef ENCFS_APPLICATION_H
#define ENCFS_APPLICATION_H

#include<gtk/gtk.h>

G_BEGIN_DECLS

#define ENCFS_TYPE_APPLICATION (encfs_application_get_type())

G_DECLARE_FINAL_TYPE(EncfsApplication, encfs_application, ENCFS, APPLICATION, GtkApplication);

EncfsApplication *encfs_application_new(void);

G_END_DECLS

#endif
