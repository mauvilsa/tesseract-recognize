cmake_minimum_required( VERSION 2.8.12 )
project( tesseract-recognize )
set( tool_EXE tesseract-recognize )
list( APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules" )
include( FindPackageHandleStandardArgs )
#find_package( LibMagic )
find_package( Ghostscript )
find_package( PkgConfig )
pkg_check_modules( lept REQUIRED lept )
pkg_check_modules( tesseract REQUIRED tesseract )
pkg_check_modules( libxml REQUIRED libxml-2.0>=2.9 )
pkg_check_modules( libxslt REQUIRED libxslt )

file( GLOB tool_SRC "*.cc" )
add_executable( ${tool_EXE} ${tool_SRC} )
set_property( TARGET ${tool_EXE} PROPERTY CXX_STANDARD 11 )

include_directories( SYSTEM ${tesseract_INCLUDEDIR} )

add_definitions( -D__PAGEXML_LEPT__ )
#add_definitions( -D__PAGEXML_MAGICK__ )
add_definitions( -D__PAGEXML_GS__ )  # TODO: pdf support is broken, gsRenderPdfPageToPng generates empty png
add_definitions( -D__PAGEXML_SLIM__ )

set( CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES};${GHOSTSCRIPT_INCLUDES}" )

string( REPLACE ";" " " CFLAGS_STR "-Wall -W ${lept_CFLAGS} ${tesseract_CFLAGS} ${Magick_CFLAGS} ${libxml_CFLAGS} ${libxslt_CFLAGS}" )
set_target_properties( ${tool_EXE} PROPERTIES COMPILE_FLAGS "${CFLAGS_STR}" )

include_directories( SYSTEM ${Magick_INCLUDEDIR} ) # To suppress system header warnings

#target_link_libraries( ${tool_EXE} ${lept_LDFLAGS} ${tesseract_LDFLAGS} ${libxml_LDFLAGS} -lOpenCL )
target_link_libraries( ${tool_EXE} ${lept_LDFLAGS} ${tesseract_LDFLAGS} ${Magick_LDFLAGS} ${GHOSTSCRIPT_LIBRARIES} ${libxml_LDFLAGS} ${libxslt_LDFLAGS} )

install( TARGETS ${tool_EXE} DESTINATION bin )
add_custom_target( install-docker
  cp ${CMAKE_HOME_DIRECTORY}/tesseract-recognize-docker ${CMAKE_HOME_DIRECTORY}/tesseract_recognize_api.py ${CMAKE_INSTALL_PREFIX}/bin )

add_custom_target( realclean cd ${CMAKE_HOME_DIRECTORY} COMMAND rm -fr ${tool_EXE} ${tool_EXE}.exe ${tool_EXE}.dSYM CMakeFiles CMakeCache.txt cmake_install.cmake install_manifest.txt Makefile )
