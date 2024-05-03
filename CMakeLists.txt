set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_minimum_required(VERSION 3.22.1)
project(bst C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Werror -Og")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -Werror -Ofast")

message("#### CMAKE_C_FLAGS_DEBUG is ${CMAKE_C_FLAGS_DEBUG}")
message("#### CMAKE_C_FLAGS_RELEASE is ${CMAKE_C_FLAGS_RELEASE}")

add_library(bst_st SHARED bst_st.c)
target_compile_options(bst_st PRIVATE)

add_library(bst_mt_gmtx SHARED bst_mt_gmtx.c)
target_link_libraries(bst_mt_gmtx pthread)

add_library(bst_mt_lmtx SHARED bst_mt_lmtx.c)
target_link_libraries(bst_mt_lmtx pthread)

add_executable(bst main.c)
target_link_libraries(bst pthread bst_st bst_mt_gmtx bst_mt_lmtx)