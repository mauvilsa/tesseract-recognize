cmake_minimum_required( VERSION 2.8.4 )
project( tesseract-recognize )
set( tool_EXE tesseract-recognize )
include( FindPkgConfig )
pkg_check_modules( lept REQUIRED lept )
pkg_check_modules( tesseract REQUIRED tesseract )
pkg_check_modules( opencv REQUIRED opencv>=2.4 )
pkg_check_modules( libxml REQUIRED libxml-2.0>=2.9 )

file( GLOB tool_SRC "*.cc" )
add_executable( ${tool_EXE} ${tool_SRC} )
set_property( TARGET ${tool_EXE} PROPERTY CXX_STANDARD 11 )

include_directories( SYSTEM ${tesseract_INCLUDEDIR} )

add_definitions( -D__PAGEXML_LEPT__ )

set( CMAKE_REQUIRED_INCLUDES "${CMAKE_REQUIRED_INCLUDES};${opencv_INCLUDE_DIRS}" )

string( REPLACE ";" " " CFLAGS_STR "-Wall -W ${lept_CFLAGS} ${tesseract_CFLAGS} ${opencv_CFLAGS} ${libxml_CFLAGS}" )
set_target_properties( ${tool_EXE} PROPERTIES COMPILE_FLAGS "${CFLAGS_STR}" )

#target_link_libraries( ${tool_EXE} ${lept_LDFLAGS} ${tesseract_LDFLAGS} ${opencv_LDFLAGS} ${libxml_LDFLAGS} -lOpenCL )
target_link_libraries( ${tool_EXE} ${lept_LDFLAGS} ${tesseract_LDFLAGS} ${opencv_LDFLAGS} ${libxml_LDFLAGS} )

install( TARGETS ${tool_EXE} DESTINATION bin )
add_custom_target( install-docker
  cp ${CMAKE_HOME_DIRECTORY}/dockerfiles/tesseract-recognize-docker ${CMAKE_INSTALL_PREFIX}/bin )

add_custom_target( docker-repo
  echo cd ${CMAKE_HOME_DIRECTORY}/dockerfiles
  COMMAND docker build -t mauvilsa/tesseract-recognize:latest ${CMAKE_HOME_DIRECTORY}/dockerfiles
  COMMAND docker tag mauvilsa/tesseract-recognize:latest mauvilsa/tesseract-recognize:$(docker run --rm -t mauvilsa/tesseract-recognize:latest bash -c "dpkg-query -W -f='${Version}' tesseract-ocr | sed 's|-.*||'") )

add_custom_target( docker-3.04
  cd ${CMAKE_HOME_DIRECTORY}/dockerfiles
  COMMAND docker build -t mauvilsa/tesseract-recognize:3.04 -f Dockerfile_3.04 . )

add_custom_target( realclean cd ${CMAKE_HOME_DIRECTORY} COMMAND rm -fr ${tool_EXE} ${tool_EXE}.exe ${tool_EXE}.dSYM CMakeFiles CMakeCache.txt cmake_install.cmake install_manifest.txt Makefile )
