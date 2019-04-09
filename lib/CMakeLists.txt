find_package(FUSE3 REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include_directories(${FUSE3_INCLUDE_DIR})

add_definitions(${FUSE3_CFLAG_OTHER})
add_library(encfs encfs_helper.c create-context.c mount-context.c encfs-cb.c)
target_link_libraries(encfs smcrypt ${FUSE3_LIBRARIES})

add_subdirectory(crypto)
