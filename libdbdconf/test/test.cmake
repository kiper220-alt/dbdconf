include(CTest)
enable_testing()

pkg_check_modules(GVDB gvdb REQUIRED)
link_directories(${GVDB_LIBRARY_DIRS})

macro(add_test_dbdconf test_exacutable sources)
    set(test_name_local "test.${test_exacutable}")
    add_executable(${test_exacutable} ${sources})
    target_link_libraries(${test_exacutable} libdbdconf)
    target_include_directories(${test_exacutable} PUBLIC ${GVDB_INCLUDE_DIRS})
    target_link_libraries(${test_exacutable} ${GVDB_LIBRARIES})
    set_target_properties(${test_exacutable} PROPERTIES OUTPUT_NAME ${test_name_local})
    add_test(NAME ${test_name_local} COMMAND ${test_exacutable})
endmacro(add_test_dbdconf)

include(${CMAKE_CURRENT_LIST_DIR}/auto/auto.cmake)
