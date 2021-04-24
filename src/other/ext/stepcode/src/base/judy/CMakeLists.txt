
cmake_minimum_required(VERSION 3.6.3)
project( JudyTemplates )

if( NOT DEFINED CMAKE_BUILD_TYPE )
  # set( CMAKE_BUILD_TYPE "RelWithDebInfo" ) #optimize, but include debug info
  set( CMAKE_BUILD_TYPE "Release" )
endif( NOT DEFINED CMAKE_BUILD_TYPE )

SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib )
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )

if( NOT DEFINED JUDY_OPTIMIZE_FLAGS )
  if( CMAKE_COMPILER_IS_GNUCC )
    set( JUDY_OPTIMIZE_FLAGS "-march=native" )

    #test for LTO; this uses an internal variable so it may break
    if( DEFINED CMAKE_C_COMPILER_VERSION )
      if( ${CMAKE_C_COMPILER_VERSION} VERSION_GREATER 4.7.0 )
	set( JUDY_OPTIMIZE_FLAGS "${JUDY_OPTIMIZE_FLAGS} -flto" )
	message( "  --  GCC version: ${CMAKE_C_COMPILER_VERSION}. Enabling link-time optimization." )
      endif( ${CMAKE_C_COMPILER_VERSION} VERSION_GREATER 4.7.0 )
    endif( DEFINED CMAKE_C_COMPILER_VERSION )
    #elseif( MSVC )
    #  set( JUDY_OPTIMIZE_FLAGS "..." ) # <--- set MSVC flags here <---
  else()
    message( "Unrecognized compiler - no optimization flags set. Edit CMakeLists.txt or set JUDY_OPTIMIZE_FLAGS." )
    set( JUDY_OPTIMIZE_FLAGS "" )
  endif( CMAKE_COMPILER_IS_GNUCC )
endif( NOT DEFINED JUDY_OPTIMIZE_FLAGS )
add_definitions( ${JUDY_OPTIMIZE_FLAGS} )

set( JUDYS_SOURCES src/judy.c src/judy.h )

if( CMAKE_COMPILER_IS_GNUCC )
  add_definitions( -pedantic -W -Wall -Wundef -Wfloat-equal -Wshadow -Winline -Wno-long-long )
endif( CMAKE_COMPILER_IS_GNUCC )

add_library( judy_lib STATIC ${JUDYS_SOURCES} )

include_directories( src )

if( ENABLE_TESTING )
  include( CTest )
  include_directories( test )
  enable_testing()

  add_executable( pennysort test/pennySort.c test/sort.c ${JUDYS_SOURCES} )
  add_executable( hexsort test/hexSort.c test/sort.c ${JUDYS_SOURCES} )
  set_target_properties( pennysort hexsort PROPERTIES COMPILE_FLAGS "-DSTANDALONE" )


  add_executable( judyLtest test/judyLtest.cc )
  target_link_libraries( judyLtest judy_lib )
  add_test( judyLtest judyLtest )

  add_executable( judyL2test test/judyL2test.cc )
  target_link_libraries( judyL2test judy_lib )
  add_test( judyL2test judyL2test )

  add_executable( judyStest test/judyStest.cc )
  target_link_libraries( judyStest judy_lib )
  add_test( judyStest judyStest )

  add_executable( judyS2test test/judyS2test.cc )
  target_link_libraries( judyS2test judy_lib )
  add_test( judyS2test judyS2test )

endif( ENABLE_TESTING )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

