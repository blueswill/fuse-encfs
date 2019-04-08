cmake_minimum_required(VERSION 3.14)

project(fuse-encfs C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

find_package(PkgConfig REQUIRED)

PKG_CHECK_MODULES(GLIB REQUIRED glib-2.0)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(MIRACL_DIRECTORY ${PROJECT_SOURCE_DIR}/third_party/MIRACL)

add_definitions(${GLIB_CFLAGS_OTHER})
include_directories(include ${GLIB_INCLUDE_DIRS})
link_libraries(${GLIB_LIBRARIES})

add_subdirectory(lib)
add_subdirectory(cli)
add_subdirectory(gtk)
