add_library(bst_mt_fgl SHARED bst_mt_fgl.c)
target_link_libraries(bst_mt_fgl bst_common pthread)
target_include_directories(bst_mt_fgl PUBLIC include)
set_target_properties(bst_mt_fgl PROPERTIES VERSION ${PROJECT_VERSION})