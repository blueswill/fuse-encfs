find_package(FUSE3 REQUIRED)

add_definitions(-DFUSE_USE_VERSION=31)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include_directories(${FUSE3_INCLUDE_DIR})
add_library(encfs encfs.c context.c)
target_link_libraries(encfs smcrypt ${FUSE3_LIBRARIES})

add_subdirectory(crypto)
