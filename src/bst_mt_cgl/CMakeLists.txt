add_library(bst_mt_cgl SHARED bst_mt_cgl.c)
target_link_libraries(bst_mt_cgl bst_common pthread)
target_include_directories(bst_mt_cgl PUBLIC src/include)
set_target_properties(bst_mt_cgl PROPERTIES VERSION ${PROJECT_VERSION})