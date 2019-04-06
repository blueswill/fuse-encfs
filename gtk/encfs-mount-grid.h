#ifndef ENCFS_MOUNT_GRID_H
#define ENCFS_MOUNT_GRID_H

#include<gtk/gtk.h>

G_BEGIN_DECLS

#define ENCFS_TYPE_MOUNT_GRID (encfs_mount_grid_get_type())

G_DECLARE_FINAL_TYPE(EncfsMountGrid, encfs_mount_grid, ENCFS, MOUNT_GRID, GtkGrid);

G_END_DECLS

#endif
