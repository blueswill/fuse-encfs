add_definitions(-DFUSE_USE_VERSION=31)

add_executable(encfs-mount mount.c)
target_link_libraries(encfs-mount encfs)

add_executable(encfs-create create.c)
target_link_libraries(encfs-create encfs)
