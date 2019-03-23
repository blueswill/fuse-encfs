set(MIRACL_DIRECTORY ${PROJECT_SOURCE_DIR}/third_party/MIRACL)
include_directories(${MIRACL_DIRECTORY}/include)
execute_process(
    WORKING_DIRECTORY ${MIRACL_DIRECTORY}/include
    COMMAND cp mirdef.h64 mirdef.h
    )
execute_process(
    WORKING_DIRECTORY ${MIRACL_DIRECTORY}/source
    COMMAND cp mrmuldv.g64 mrmuldv.c
    )
file(GLOB MIRACL_SRCS ${MIRACL_DIRECTORY}/source/mr*.c)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64 -O2")
add_library(miracl STATIC ${MIRACL_SRCS})
