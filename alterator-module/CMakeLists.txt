cmake_minimum_required(VERSION 3.20)
project(alterator_module_dbdconf LANGUAGES C VERSION 0.0.1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GIO gio-2.0 REQUIRED)
pkg_check_modules(POLKIT polkit-gobject-1 REQUIRED)
pkg_check_modules(LIBTOML libtoml REQUIRED)
link_directories(${GIO_LIBRARY_DIRS})

include(${CMAKE_CURRENT_LIST_DIR}/src/src.cmake)

add_library(${PROJECT_NAME} SHARED ${ALTERATOR_MODULE_DBDCONF_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "dbdconf")
target_include_directories(
        ${PROJECT_NAME}
        PUBLIC
        ${GIO_INCLUDE_DIRS}
        ${POLKIT_INCLUDE_DIRS}
        ${LIBTOML_INCLUDE_DIRS}
        include
)

target_link_directories(${PROJECT_NAME} PUBLIC ${GIO_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} #[[libsvdb]] #[[<- temporary]] ${GIO_LIBRARIES} ${LIBTOML_LIBRARIES})

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION ${LIBEXECDIR}/alterator/)