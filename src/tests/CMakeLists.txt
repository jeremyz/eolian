#message(STATUS "CHECK:  ${CHECK_ENABLED} " )

if(CHECK_FOUND)

include_directories (${CMAKE_SOURCE_DIR}/src/lib)

link_directories (${CMAKE_SOURCE_DIR}/src/lib)

add_executable(suite suite.c)

include_directories(
   ${EINA_INCLUDE_DIRS}
   ${CHECK_INCLUDE_DIRS}
   )

target_link_libraries(suite
   eolian
   ${EINA_LIBRARIES}
   ${CHECK_LIBRARIES}
   rt
   pthread
   m
   )

add_test(name_of_test suite)
#add_dependencies(check suite)

### adding target "ttest", which runs "suite"
add_custom_target(ttest COMMAND ${CMAKE_BINARY_DIR}/src/tests/suite)
### adding target "ttest" depends on "suite"
add_dependencies(ttest suite)

#message(STATUS "TEST COMM:  ${CMAKE_CTEST_COMMAND}" )
#message(STATUS "TEST COMM:  ${CMAKE_BINARY_DIR}" )
#message(STATUS "TEST COMM:  ${CMAKE_CURRENT_BINARY_DIR}" )

endif (CHECK_FOUND)
