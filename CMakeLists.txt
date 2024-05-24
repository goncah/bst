set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_minimum_required(VERSION 3.28.3)
project(bst VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Werror")
set(CMAKE_C_FLAGS_RELEASE "-O1 -Wall -Werror") # remove default -O3 which slows down LRWL version
set(CPACK_PACKAGE_CONTACT "hfdsgoncalves@gmail.com")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

add_subdirectory(src)

add_executable(bst src/main.c)
target_link_libraries(bst pthread bst_st bst_mt_cgl bst_mt_fgl)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    install(TARGETS bst_common DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY src/bst_common/include/ DESTINATION include/bst_common)

    install(TARGETS bst_st DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY src/bst_st/include/ DESTINATION include/bst_t)

    install(TARGETS bst_mt_cgl DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY src/bst_mt_cgl/include/ DESTINATION include/bst_mt_cgl)

    install(TARGETS bst_mt_fgl DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(DIRECTORY src/bst_mt_fgl/include/ DESTINATION include/bst_mt_fgl)

    include(CPack)
endif ()