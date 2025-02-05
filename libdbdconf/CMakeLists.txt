cmake_minimum_required(VERSION 3.20)
project(libdbdconf LANGUAGES C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO gio-2.0 REQUIRED)
link_directories(${GIO_LIBRARY_DIRS})

include(${CMAKE_CURRENT_LIST_DIR}/src/src.cmake)

add_library(${PROJECT_NAME} SHARED ${LIBDBDCONF_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc ${GIO_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${GIO_LIBRARIES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
else()
    remove_definitions(-DDEBUG)
endif()

include(test/test.cmake)
