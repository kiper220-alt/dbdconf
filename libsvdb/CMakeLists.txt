cmake_minimum_required(VERSION 3.20)
project(libsvdb LANGUAGES C VERSION 0.0.1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO gio-2.0 REQUIRED)
link_directories(${GIO_LIBRARY_DIRS})

include(${CMAKE_CURRENT_LIST_DIR}/src/src.cmake)
include(CMakePackageConfigHelpers)

string(REPLACE "lib" "" LIBSVDB_OUTPUT_NAME ${PROJECT_NAME})

add_library(${PROJECT_NAME} SHARED ${LIBSVDB_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 0)
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${CMAKE_CURRENT_LIST_DIR}/include/svdb.h")
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${GIO_INCLUDE_DIRS})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${LIBSVDB_OUTPUT_NAME})
target_link_libraries(${PROJECT_NAME} ${GIO_LIBRARIES})

# GIR
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.gir
        COMMAND g-ir-scanner
        --namespace=svdb
        --nsversion=0.0
        --accept-unprefixed
        --include=GObject-2.0
        --library=svdb
        -L ${CMAKE_CURRENT_BINARY_DIR}/
        --warn-all
        --c-include="${CMAKE_CURRENT_LIST_DIR}/include/svdb.h" ${CMAKE_CURRENT_LIST_DIR}/src/svdb.c
        --output=${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.gir
        -I"${CMAKE_CURRENT_LIST_DIR}/include/"
        "${CMAKE_CURRENT_LIST_DIR}/include/svdb.h"
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.typelib
        COMMAND g-ir-compiler
        ${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.gir
        -o ${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.typelib
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.gir
)

add_custom_target(libsvdb_gir ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.gir)
add_custom_target(libsvdb_typelib ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.typelib)

install(TARGETS ${PROJECT_NAME}
        EXPORT svdb-config.cmake
        PUBLIC_HEADER
        LIBRARY)
export(TARGETS ${PROJECT_NAME} FILE svdb-targets.cmake)
install(EXPORT svdb-config.cmake FILE svdb-targets.cmake DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/svdb")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/svdb-config-version.cmake" VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES "cmake/svdb-config.cmake" "${CMAKE_CURRENT_BINARY_DIR}/svdb-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/svdb"
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/svdb.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/svdb.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/svdb.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.gir DESTINATION ${CMAKE_INSTALL_DATADIR}/gir-1.0)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/svdb-0.0.typelib DESTINATION ${CMAKE_INSTALL_DATADIR}/girepository-1.0)

if(NOT TARGET uninstall)
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY)

    add_custom_target(uninstall
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

include(test/test.cmake)
