include_directories (${CMAKE_SOURCE_DIR}/src/lib)

link_directories (${CMAKE_SOURCE_DIR}/src/lib)
link_directories (${PC_LIBELEMENTARY_LIBRARY_DIRS})

add_executable(eolian-bin main.c)
set_target_properties(eolian-bin PROPERTIES OUTPUT_NAME eolian)

include_directories(
   ${EINA_INCLUDE_DIRS}
   )

target_link_libraries (eolian-bin
   eolian
)

