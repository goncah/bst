add_library(bst_st SHARED bst_st.c)
target_link_libraries(bst_st bst_common)
target_include_directories(bst_st PUBLIC include)
set_target_properties(bst_st PROPERTIES VERSION ${PROJECT_VERSION})