include(${CMAKE_SOURCE_DIR}/cmake/GResources.cmake)

PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)
PKG_CHECK_MODULES(UDISKS2 REQUIRED udisks2)

include_directories(${GTK3_INCLUDE_DIRS} ${UDISKS2_INCLUDE_DIRS})
link_directories(${GTK3_LINK_DIRS} ${UDISKS2_LINK_DIRS})

add_definitions(${GTK3_CFLAGS_OTHER} ${UDISKS2_CFLAGS_OTHER})

GLIB_COMPILE_RESOURCES(GLIB_RESOURCE_OUTPUT SOURCE ui/encfs.gresource.xml)

add_executable(encfs-gtk
    main.c
    encfs-application.c
    encfs-create-grid.c
    encfs-mount-grid.c
    encfs-monitor-grid.c
    encfs-window.c
    utility.c
    ${GLIB_RESOURCE_OUTPUT}
    )

target_link_libraries(encfs-gtk encfs ${GTK3_LIBRARIES} ${UDISKS2_LIBRARIES})
