#ifndef ENCFS_MONITOR_GRID_H
#define ENCFS_MONITOR_GRID_H

#include<gtk/gtk.h>

G_BEGIN_DECLS

#define ENCFS_TYPE_MONITOR_GRID (encfs_monitor_grid_get_type())

G_DECLARE_FINAL_TYPE(EncfsMonitorGrid, encfs_monitor_grid, ENCFS, MONITOR_GRID, GtkGrid);

G_END_DECLS

#endif
