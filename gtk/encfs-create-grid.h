#ifndef ENCFS_CREATE_GRID_H
#define ENCFS_CREATE_GRID_H

#include<gtk/gtk.h>

G_BEGIN_DECLS

#define ENCFS_TYPE_CREATE_GRID (encfs_create_grid_get_type())

G_DECLARE_FINAL_TYPE(EncfsCreateGrid, encfs_create_grid, ENCFS, CREATE_GRID, GtkGrid);

G_END_DECLS

#endif
