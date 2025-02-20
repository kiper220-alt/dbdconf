cmake_minimum_required(VERSION 3.20)
project(dbdconf LANGUAGES C)

SET(CMAKE_SKIP_BUILD_RPATH  FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

add_subdirectory(libsvdb)

add_executable(${CMAKE_PROJECT_NAME} ./src/main.c ./src/cli.c)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ./inc)
target_link_libraries(${CMAKE_PROJECT_NAME} libsvdb)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION bin
)
