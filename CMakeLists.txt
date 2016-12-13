
cmake_minimum_required(VERSION 3.1)
project(libuct)

option(libuct_build_tests "Build libuct's own tests" OFF)

set(CMAKE_CXX_STANDARD 11)

################################
# Find thread
################################
find_package(Threads REQUIRED)

##################################
# libgoboard
##################################
if (NOT TARGET goboard)
    add_subdirectory(vendor/libgoboard)
    set(libgoboard_SOURCE_DIR vendor/libgoboard)
    set(libgoboard_LIBS goboard)
    include_directories(${libgoboard_INCLUDE_DIR})
endif()

##################################
# libgo-common
##################################
if (NOT TARGET go-common)
    add_subdirectory(vendor/libgo-common)
endif()
set(libgo-common_SOURCE_DIR vendor/libgo-common)
set(libgo_LIBS go-common)
include_directories(${libgo-common_INCLUDE_DIR})

##################################
# libfastrollout
##################################
if (NOT TARGET fastrollout)
    add_subdirectory(vendor/libfastrollout)
    set(libfastrollout_SOURCE_DIR vendor/libfastrollout)
    set(libfastrollout_LIBS fastrollout)
    include_directories(${libfastrollout_INCLUDE_DIR})
endif()

##################################
# uct
##################################
include_directories(src/)
add_library(uct STATIC src/uct/uct.cpp src/uct/detail/tree.hpp src/uct/uct.hpp)
target_link_libraries(uct ${libgo_LIBS} ${libgoboard_LIBS} ${libfastrollout_LIBS})
set(libuct_INCLUDE_DIR ${libgoboard_INCLUDE_DIR} ${libgo-common_INCLUDE_DIR} ${libfastrollout_INCLUDE_DIR} ${libuct_SOURCE_DIR}/src PARENT_SCOPE)

#################################
# tests
################################
if (libuct_build_tests)
    enable_testing()
    ################################
    # GTest
    ################################
    ADD_SUBDIRECTORY (vendor/googletest/googletest)
    include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
    ###############################
    # board-test
    ###############################
    add_executable(uct-test src/uct_test.cpp)
    target_link_libraries(uct-test uct gtest gtest_main)
    add_test(uct_test uct-test)
endif()
