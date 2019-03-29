set(MIRACL_DIRECTORY ${PROJECT_SOURCE_DIR}/third_party/MIRACL)
add_subdirectory(zzn12)
include_directories(${MIRACL_DIRECTORY}/include)
include_directories(${PROJECT_SOURCE_DIR}/include)
execute_process(
    WORKING_DIRECTORY ${MIRACL_DIRECTORY}/include
    COMMAND cp mirdef.h64 mirdef.h
    )
execute_process(
    WORKING_DIRECTORY ${MIRACL_DIRECTORY}/source
    COMMAND cp mrmuldv.g64 mrmuldv.c
    )
file(GLOB MIRACL_SRCS ${MIRACL_DIRECTORY}/source/mr*.c)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
add_library(miracl STATIC ${MIRACL_SRCS})

IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
    add_definitions(-DCHECK)
ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

add_library(smcrypt STATIC
    kgc.c
    parameter.c
    sm3.c
    sm4.c
    sm4_xts.c 
    sm9.c
    sm9_helper.c
    )

target_link_libraries(smcrypt miracl zzn12)
