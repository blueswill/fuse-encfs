include_directories(
    ${MIRACL_DIRECTORY}/include
    ${PROJECT_SOURCE_DIR}/crypto
    ${PROJECT_SOURCE_DIR}/include
    )

add_library(zzn12 STATIC zzn12.cpp zzn12_wrapper.cpp)
target_link_libraries(zzn12 miracl)
