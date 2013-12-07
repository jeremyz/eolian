find_program(RAGEL ragel)
#set(RAGEL false)

# add the command to generate the source code
if (RAGEL)
   add_custom_command (
      OUTPUT eo_lexer.c
      COMMAND ${RAGEL} -o eo_lexer.c ${CMAKE_CURRENT_SOURCE_DIR}/eo_lexer.rl
      COMMAND cp eo_lexer.c ${CMAKE_CURRENT_SOURCE_DIR}/eo_lexer_no_ragel.c
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/eo_lexer.rl # build when lexer.rl is newer
      )
else()
   add_custom_command (
      OUTPUT eo_lexer.c
      COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/eo_lexer_no_ragel.c eo_lexer.c
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/eo_lexer_no_ragel.c
      )
endif()

LIST(APPEND EOLIAN_CC_SOURCES
   eolian.c eo_lexer.c eo_parser.c eo_definitions.c database.c ch_parser.c)

include_directories(
   ${CMAKE_CURRENT_SOURCE_DIR}
   ${EINA_INCLUDE_DIRS}
   ${ECORE_FILE_INCLUDE_DIRS}
   )

add_definitions(-ggdb3)

add_library(eolian ${EOLIAN_CC_SOURCES})

target_link_libraries(eolian
   ${EINA_LIBRARIES}
   ${ECORE_FILE_LIBRARIES}
   )
