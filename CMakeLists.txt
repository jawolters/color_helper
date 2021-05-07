cmake_minimum_required(VERSION 3.0)
project(color_helper C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(XEXT REQUIRED xext)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

include_directories(${X11_INCLUDE_DIR})
link_directories(${X11_LIBRARIES})

include_directories(${X11_Xext_INCLUDE_PATH})
link_directories(${X11_Xext_LIB})

find_library(XEXT_LIBRARY Xext)

add_definitions(${GTK3_CFLAGS_OTHER})
add_definitions(-DMAX_COLORS=5000)

file(GLOB_RECURSE DATA RELATIVE ${CMAKE_SOURCE_DIR} "res/*.txt")
find_program(RESOURCE_COMPILER xxd)
file(GLOB COMPILED_RESOURCES ${DATA})
foreach(INPUT_FILE ${COMPILED_RESOURCES})
    get_filename_component(INPUT_FILE_SHORT ${INPUT_FILE} NAME)
    set(OUTPUT_FILE ${INPUT_FILE_SHORT}.hex)
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/res/
        COMMAND ${RESOURCE_COMPILER} -i ${INPUT_FILE_SHORT} ${CMAKE_BINARY_DIR}/${OUTPUT_FILE}    )
    list( APPEND COMPILED_RESOURCES ${OUTPUT_FILE} )
endforeach()
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB SRCS src/*.c)
add_executable(${CMAKE_PROJECT_NAME} ${SRCS} ${COMPILED_RESOURCES})
target_link_libraries(${CMAKE_PROJECT_NAME} ${GTK3_LIBRARIES} ${X11_LIBRARIES} ${XEXT_LIBRARY} m)

install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
