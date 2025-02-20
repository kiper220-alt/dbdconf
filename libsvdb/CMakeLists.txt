cmake_minimum_required(VERSION 3.20)
project(libsvdb LANGUAGES C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO gio-2.0 REQUIRED)
link_directories(${GIO_LIBRARY_DIRS})

include(${CMAKE_CURRENT_LIST_DIR}/src/src.cmake)

string(REPLACE "lib" "" LIBSVDB_OUTPUT_NAME ${PROJECT_NAME})

add_library(${PROJECT_NAME} SHARED ${LIBSVDB_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/inc ${GIO_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${GIO_LIBRARIES})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${LIBSVDB_OUTPUT_NAME})

# GIR
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/svdb.gir
        COMMAND g-ir-scanner
            --namespace=svdb
            --nsversion=1.0
            --accept-unprefixed
            --include=GObject-2.0
            --library=svdb
            -L ${CMAKE_CURRENT_BINARY_DIR}/
            --warn-all
            --c-include="${CMAKE_CURRENT_LIST_DIR}/inc/svdb.h" ${CMAKE_CURRENT_LIST_DIR}/src/svdb.c
            --output=${CMAKE_CURRENT_BINARY_DIR}/svdb.gir
            -I"${CMAKE_CURRENT_LIST_DIR}/inc/"
            "${CMAKE_CURRENT_LIST_DIR}/inc/svdb.h"
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/svdb.typelib
        COMMAND g-ir-compiler
        ${CMAKE_CURRENT_BINARY_DIR}/svdb.gir
        -o ${CMAKE_CURRENT_BINARY_DIR}/svdb.typelib
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/svdb.gir
)

add_custom_target(libsvdb_gir ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/svdb.gir)
add_custom_target(libsvdb_typelib ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/svdb.typelib)

include(test/test.cmake)
