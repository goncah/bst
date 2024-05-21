add_library(bst_mt_grwl SHARED bst_mt_grwl.c)
target_link_libraries(bst_mt_grwl pthread)
target_include_directories(bst_mt_grwl PUBLIC src/include)
set_target_properties(bst_mt_grwl PROPERTIES VERSION ${PROJECT_VERSION})