find_package(FUSE3 REQUIRED)
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/crypto
    )

if(!FUSE3_FOUND)
    message(FATAL_ERROR "cannot find fuse3 library")
endif()

add_definitions(-DFUSE_USE_VERSION=31)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include_directories(${PROJECT_SOURCE_DIR}/crypto ${FUSE3_INCLUDE_DIR})
add_executable(encfs main.c encfs.c context.c)
target_link_libraries(encfs smcrypt ${FUSE3_LIBRARIES})
