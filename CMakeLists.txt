cmake_minimum_required(VERSION 2.8.1)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Eina REQUIRED)
find_package(Ecore REQUIRED COMPONENTS File)
find_package(Check REQUIRED)

add_subdirectory(src/lib)
add_subdirectory(src/bin)
add_subdirectory(src/tests)
