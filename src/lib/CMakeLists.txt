LIST(APPEND EOLIAN_CC_SOURCES
   eo_parser.c lexer.c database.c)
include_directories(
   ${EINA_INCLUDE_DIRS}
   ${ECORE_FILE_INCLUDE_DIRS}
)

add_library(eolian ${EOLIAN_CC_SOURCES})

target_link_libraries(eolian
   ${EINA_LIBRARIES}
   ${ECORE_FILE_LIBRARIES}
   eolianjson
   #${EO_LIBRARIES}
   #${EVAS_LIBRARIES}
   #${ELEMENTARY_LIBRARIES}
   #${FFI_LIBRARIES}
   m
   )

#some json parsing library
LIST(APPEND JSON_CC_SOURCES
   eina_json.c)
add_library(eolianjson ${JSON_CC_SOURCES})

target_link_libraries(eolianjson
   ${EINA_LIBRARIES}
   ${ECORE_FILE_LIBRARIES}
   m
   )
