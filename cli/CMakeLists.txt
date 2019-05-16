add_definitions(-DFUSE_USE_VERSION=31)

PKG_CHECK_MODULES(JSON_GLIB REQUIRED json-glib-1.0)
link_libraries(encfs)

add_executable(encfs-mount mount.c helper.c)
target_link_libraries(encfs-mount encfs)

add_executable(encfs-create create.c helper.c)
target_link_libraries(encfs-create encfs)

add_executable(encfs-check check.c helper.c)
target_compile_options(encfs-check PRIVATE ${JSON_GLIB_CFLAGS_OTHER})
target_include_directories(encfs-check PRIVATE ${JSON_GLIB_INCLUDE_DIRS})
target_link_libraries(encfs-check PRIVATE ${JSON_GLIB_LIBRARIES})

install(TARGETS encfs-mount encfs-create encfs-check
    RUNTIME DESTINATION ${INSTALL_BIN_DIR})
