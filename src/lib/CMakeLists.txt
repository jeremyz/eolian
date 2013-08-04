LIST(APPEND EOLIAN_CC_SOURCES
   eo_parser.c lexer.c database.c)

include_directories(
   ${EINA_INCLUDE_DIRS}
   ${ECORE_FILE_INCLUDE_DIRS}
   .
)

add_library(eolian ${EOLIAN_CC_SOURCES})

target_link_libraries(eolian
   ${EINA_LIBRARIES}
   ${ECORE_FILE_LIBRARIES}
   #${EO_LIBRARIES}
   #${EVAS_LIBRARIES}
   #${ELEMENTARY_LIBRARIES}
   #${FFI_LIBRARIES}
   )

