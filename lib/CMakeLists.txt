find_package(FUSE3 REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include_directories(${FUSE3_INCLUDE_DIR})

PKG_CHECK_MODULES(TPMCTX REQUIRED tss2-mu tss2-sys gio-2.0 gio-unix-2.0)

add_definitions(${FUSE3_CFLAG_OTHER})

add_library(tpmctx tpm-context.c)
target_include_directories(tpmctx PRIVATE ${TPMCTX_INCLUDE_DIRS})
target_compile_options(tpmctx PRIVATE ${TPMCTX_CFLAGS_OTHER})
target_link_libraries(tpmctx ${TPMCTX_LIBRARIES} dl)

add_library(encfs encfs_helper.c create-context.c mount-context.c encfs-cb.c check-context.c)
target_link_libraries(encfs smcrypt tpmctx ${FUSE3_LIBRARIES})

add_subdirectory(crypto)
