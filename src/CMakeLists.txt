include(FindOpenSSL)
find_package(FUSE3 REQUIRED)
include_directories(../include)

if(!OPENSSL_FOUND)
    message(FATAL_ERROR "cannot find OpenSSL")
endif()

add_definitions(-DFUSE_USE_VERSION=31)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include_directories(${OPENSSL_INCLUDE_DIR} ${FUSE3_INCLUDE_DIR})
add_executable(encfs main.c encfs.c context.c)
target_link_libraries(encfs ${OPENSSL_LIBRARIES} ${FUSE3_LIBRARIES})
