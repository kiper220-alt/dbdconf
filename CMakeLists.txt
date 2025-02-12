cmake_minimum_required(VERSION 3.20)
project(dbdconf)

add_subdirectory(libdbdconf)

add_executable(${CMAKE_PROJECT_NAME} ./src/main.c ./src/cli.c)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ./inc)
target_link_libraries(${CMAKE_PROJECT_NAME} lib${CMAKE_PROJECT_NAME})


