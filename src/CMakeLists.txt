add_library(bst_common SHARED bst_common.c)
target_include_directories(bst_common PUBLIC include)
set_target_properties(bst_common PROPERTIES VERSION ${PROJECT_VERSION})

add_subdirectory(bst_st)
add_subdirectory(bst_mt_cgl)
add_subdirectory(bst_mt_fgl)
add_subdirectory(bst_at)