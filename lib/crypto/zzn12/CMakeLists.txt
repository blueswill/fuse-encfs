include_directories(${PROJECT_SOURCE_DIR}/lib/crypto)
add_library(zzn12 STATIC zzn12.cpp zzn12_wrapper.cpp)
target_link_libraries(zzn12 miracl)
