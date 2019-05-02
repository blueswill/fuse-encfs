#ifndef ENCFS_TPM_WINDOW_H
#define ENCFS_TPM_WINDOW_H

#include<gtk/gtk.h>

G_BEGIN_DECLS

#define ENCFS_TYPE_TPM_WINDOW (encfs_tpm_window_get_type())

G_DECLARE_FINAL_TYPE(EncfsTpmWindow, encfs_tpm_window, ENCFS, TPM_WINDOW, GtkApplicationWindow);

EncfsTpmWindow *encfs_tpm_window_new(GApplication *app);

gboolean encfs_tpm_window_get_active(EncfsTpmWindow *self);

G_END_DECLS

#endif /* ifndef ENCFS_TPM_WINDOW_H */
