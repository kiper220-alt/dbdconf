cmake_minimum_required(VERSION 3.20)
project(dbdconf LANGUAGES C)

add_subdirectory(libsvdb)

add_executable(${CMAKE_PROJECT_NAME} ./src/main.c ./src/cli.c)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ./inc)
target_link_libraries(${CMAKE_PROJECT_NAME} libsvdb)

install(TARGETS ${PROJECT_NAME} RUNTIME)
