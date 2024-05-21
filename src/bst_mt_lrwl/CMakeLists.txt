add_library(bst_mt_lrwl SHARED bst_mt_lrwl.c)
target_link_libraries(bst_mt_lrwl pthread)
target_include_directories(bst_mt_lrwl PUBLIC include)
set_target_properties(bst_mt_lrwl PROPERTIES VERSION ${PROJECT_VERSION})