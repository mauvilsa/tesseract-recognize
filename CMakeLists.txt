cmake_minimum_required( VERSION 2.8.4 )
project( tesseract-recognize )
set( tool_EXE tesseract-recognize )
include( FindPkgConfig )
set( ENV{PKG_CONFIG_PATH} "." )
pkg_check_modules( lept REQUIRED lept )
pkg_check_modules( tesseract REQUIRED tesseract )

file( GLOB tool_SRC "*.cc" )
add_executable( ${tool_EXE} ${tool_SRC} )
include_directories( ${tesseract_INCLUDEDIR} )
if( EXISTS "${CMAKE_HOME_DIRECTORY}/tesseract-ocr" )
  include_directories( tesseract-ocr/ccstruct tesseract-ocr/ccutil tesseract-ocr/cutil tesseract-ocr/image tesseract-ocr/viewer )
  add_definitions( -D__TESSERACT_SOURCE__ )
  message( "-- Found tesseract source" )
endif()

#set_target_properties( ${tool_EXE} PROPERTIES COMPILE_FLAGS "-Wall -W -Wno-unused-function" )
##set_source_files_properties(foo.cpp PROPERTIES COMPILE_FLAGS -Wno-effc++)

target_link_libraries( ${tool_EXE} ${lept_LDFLAGS} ${tesseract_LDFLAGS} )

install( TARGETS ${tool_EXE} DESTINATION bin )

add_custom_target( realclean cd ${CMAKE_HOME_DIRECTORY} COMMAND rm -fr ${tool_EXE} ${tool_EXE}.exe ${tool_EXE}.dSYM CMakeFiles CMakeCache.txt cmake_install.cmake install_manifest.txt Makefile )
