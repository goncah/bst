add_library(bst_at SHARED bst_at.c)
target_link_libraries(bst_at bst_common)
target_include_directories(bst_at PUBLIC src/include)
set_target_properties(bst_at PROPERTIES VERSION ${PROJECT_VERSION})