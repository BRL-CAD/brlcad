# CMakeLists.txt

# Copyright (C) 2018 Cosmin Truta
# Copyright (C) 2007,2009-2018 Glenn Randers-Pehrson
# Written by Christian Ehrlicher, 2007
# Revised by Roger Lowman, 2009-2010
# Revised by Clifford Yapp, 2011-2012,2017
# Revised by Roger Leigh, 2016
# Revised by Andreas Franek, 2016
# Revised by Sam Serrels, 2017
# Revised by Vadim Barkov, 2017
# Revised by Vicky Pfau, 2018
# Revised by Cameron Cawley, 2018
# Revised by Cosmin Truta, 2018
# Revised by Kyle Bentley, 2018

# This code is released under the libpng license.
# For conditions of distribution and use, see the disclaimer
# and license in png.h

cmake_minimum_required(VERSION 3.12)

# install(CODE) uses generator expressions - requires 3.14 or newer.
# We want it to be on, but until we can bump our minimum requirement
# set it to OLD to make sure we don't break anything.
if (POLICY CMP0087)
  cmake_policy(SET CMP0087 OLD)
endif (POLICY CMP0087)

project(libpng C)
enable_testing()

set(PNGLIB_MAJOR 1)
set(PNGLIB_MINOR 6)
set(PNGLIB_RELEASE 37)
set(PNGLIB_NAME libpng${PNGLIB_MAJOR}${PNGLIB_MINOR})
set(PNGLIB_VERSION ${PNGLIB_MAJOR}.${PNGLIB_MINOR}.${PNGLIB_RELEASE})

# needed packages
if (Z_PREFIX)
  add_definitions(-DZ_PREFIX)
endif (Z_PREFIX)
if (Z_PREFIX_STR)
  add_definitions(-DZ_PREFIX_STR=${Z_PREFIX_STR})
endif (Z_PREFIX_STR)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})

if(UNIX AND NOT APPLE AND NOT BEOS AND NOT HAIKU)
  find_library(M_LIBRARY m)
else()
  # libm is not needed and/or not available
  set(M_LIBRARY "")
endif()

# COMMAND LINE OPTIONS
option(PNG_SHARED "Build shared lib" ON)
option(PNG_STATIC "Build static lib" ${BUILD_STATIC_LIBS})
option(PNG_TESTS  "Build libpng tests" ON)

# Many more configuration options could be added here
option(PNG_FRAMEWORK "Build OS X framework" OFF)
option(PNG_DEBUG "Build with debug output" OFF)
option(PNGARG        "Disable ANSI-C prototypes" OFF)

set(PNG_PREFIX "" CACHE STRING "Prefix to add to the API function names")
set(DFA_XTRA "" CACHE FILEPATH "File containing extra configuration settings")

# SET LIBNAME
if (NOT DEFINED PNG_LIB_NAME)
  set(PNG_LIB_NAME png${PNGLIB_MAJOR}${PNGLIB_MINOR})
endif (NOT DEFINED PNG_LIB_NAME)

# to distinguish between debug and release lib
#set(CMAKE_DEBUG_POSTFIX "d")

include(CheckCSourceCompiles)
option(ld-version-script "Enable linker version script" ON)
if(ld-version-script AND NOT APPLE)
  # Check if LD supports linker scripts.
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/conftest.map" "VERS_1 {
        global: sym;
        local: *;
};

VERS_2 {
        global: sym2;
                main;
} VERS_1;
")
  set(CMAKE_REQUIRED_FLAGS_SAVE ${CMAKE_REQUIRED_FLAGS})
  set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS} "-Wl,--version-script='${CMAKE_CURRENT_BINARY_DIR}/conftest.map'")
  check_c_source_compiles("void sym(void) {}
void sym2(void) {}
int main(void) {return 0;}
" HAVE_LD_VERSION_SCRIPT)
  if(NOT HAVE_LD_VERSION_SCRIPT)
    set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVE} "-Wl,-M -Wl,${CMAKE_CURRENT_BINARY_DIR}/conftest.map")
  check_c_source_compiles("void sym(void) {}
void sym2(void) {}
int main(void) {return 0;}
" HAVE_SOLARIS_LD_VERSION_SCRIPT)
  endif()
  set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVE})
  file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/conftest.map")
endif()

# Find symbol prefix.  Likely obsolete and unnecessary with recent
# toolchains (it's not done in many other projects).
function(symbol_prefix)
  set(SYMBOL_PREFIX)

  execute_process(COMMAND "${CMAKE_C_COMPILER}" "-E" "-"
                  INPUT_FILE /dev/null
                  OUTPUT_VARIABLE OUT
                  RESULT_VARIABLE STATUS)

  if(CPP_FAIL)
    message(WARNING "Failed to run the C preprocessor")
  endif()

  string(REPLACE "\n" ";" OUT "${OUT}")
  foreach(line ${OUT})
    string(REGEX MATCH "^PREFIX=" found_match "${line}")
    if(found_match)
      string(REGEX REPLACE "^PREFIX=(.*\)" "\\1" prefix "${line}")
      string(REGEX MATCH "__USER_LABEL_PREFIX__" found_match "${prefix}")
      if(found_match)
        string(REGEX REPLACE "(.*)__USER_LABEL_PREFIX__(.*)" "\\1\\2" prefix "${prefix}")
      endif()
      set(SYMBOL_PREFIX "${prefix}")
    endif()
  endforeach()
  set(SYMBOL_PREFIX "${SYMBOL_PREFIX}" PARENT_SCOPE)
endfunction()

if(UNIX)
  symbol_prefix()
endif()

find_program(AWK NAMES gawk awk)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(NOT AWK OR MSVC)
  # No awk available to generate sources; use pre-built pnglibconf.h
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/pnglibconf.h.prebuilt
                 ${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h)
  add_custom_target(genfiles) # Dummy
else()
  include(CMakeParseArguments)
  # Generate .chk from .out with awk
  # generate_chk(INPUT inputfile OUTPUT outputfile [DEPENDS dep1 [dep2...]])
  function(generate_chk)
    set(options)
    set(oneValueArgs INPUT OUTPUT)
    set(multiValueArgs DEPENDS)
    cmake_parse_arguments(_GC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT _GC_INPUT)
      message(FATAL_ERROR "generate_chk: Missing INPUT argument")
    endif()
    if(NOT _GC_OUTPUT)
      message(FATAL_ERROR "generate_chk: Missing OUTPUT argument")
    endif()

    add_custom_command(OUTPUT "${_GC_OUTPUT}"
                       COMMAND "${CMAKE_COMMAND}"
                               "-DINPUT=${_GC_INPUT}"
                               "-DOUTPUT=${_GC_OUTPUT}"
                               -P "${CMAKE_CURRENT_BINARY_DIR}/scripts/genchk.cmake"
                       DEPENDS "${_GC_INPUT}" ${_GC_DEPENDS}
                       WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  endfunction()

  # Generate .out from .c with awk
  # generate_out(INPUT inputfile OUTPUT outputfile [DEPENDS dep1 [dep2...]])
  function(generate_out)
    set(options)
    set(oneValueArgs INPUT OUTPUT)
    set(multiValueArgs DEPENDS)
    cmake_parse_arguments(_GO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT _GO_INPUT)
      message(FATAL_ERROR "generate_out: Missing INPUT argument")
    endif()
    if(NOT _GO_OUTPUT)
      message(FATAL_ERROR "generate_out: Missing OUTPUT argument")
    endif()

    add_custom_command(OUTPUT "${_GO_OUTPUT}"
                       COMMAND "${CMAKE_COMMAND}"
                               "-DINPUT=${_GO_INPUT}"
                               "-DOUTPUT=${_GO_OUTPUT}"
                               -P "${CMAKE_CURRENT_BINARY_DIR}/scripts/genout.cmake"
                       DEPENDS "${_GO_INPUT}" ${_GO_DEPENDS}
                       WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  endfunction()

  # Generate specific source file with awk
  # generate_source(OUTPUT outputfile [DEPENDS dep1 [dep2...]])
  function(generate_source)
    set(options)
    set(oneValueArgs OUTPUT)
    set(multiValueArgs DEPENDS)
    cmake_parse_arguments(_GSO "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(NOT _GSO_OUTPUT)
      message(FATAL_ERROR "generate_source: Missing OUTPUT argument")
    endif()

    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${_GSO_OUTPUT}"
                       COMMAND "${CMAKE_COMMAND}"
                               "-DOUTPUT=${_GSO_OUTPUT}"
                               -P "${CMAKE_CURRENT_BINARY_DIR}/scripts/gensrc.cmake"
                       DEPENDS ${_GSO_DEPENDS}
                       WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  endfunction()

  # Copy file
  function(generate_copy source destination)
    add_custom_command(OUTPUT "${destination}"
                       COMMAND "${CMAKE_COMMAND}" -E remove "${destination}"
                       COMMAND "${CMAKE_COMMAND}" -E copy "${source}"
                                                          "${destination}"
                       DEPENDS "${source}")
  endfunction()

  # Generate scripts/pnglibconf.h
  generate_source(OUTPUT "scripts/pnglibconf.c"
                  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pnglibconf.dfa"
                          "${CMAKE_CURRENT_SOURCE_DIR}/scripts/options.awk"
                          "${CMAKE_CURRENT_SOURCE_DIR}/pngconf.h")

  # Generate pnglibconf.c
  generate_source(OUTPUT "pnglibconf.c"
                  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pnglibconf.dfa"
                          "${CMAKE_CURRENT_SOURCE_DIR}/scripts/options.awk"
                          "${CMAKE_CURRENT_SOURCE_DIR}/pngconf.h")

  if(PNG_PREFIX)
    set(PNGLIBCONF_H_EXTRA_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/scripts/prefix.out"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/macro.lst")
    set(PNGPREFIX_H_EXTRA_DEPENDS
        "${CMAKE_CURRENT_BINARY_DIR}/scripts/intprefix.out")
  endif()

  generate_out(INPUT "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.c"
               OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.out")

  # Generate pnglibconf.h
  generate_source(OUTPUT "pnglibconf.h"
                  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.out"
                          ${PNGLIBCONF_H_EXTRA_DEPENDS})

  generate_out(INPUT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/intprefix.c"
               OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scripts/intprefix.out"
               DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h")

  generate_out(INPUT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/prefix.c"
               OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scripts/prefix.out"
               DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/png.h"
                       "${CMAKE_CURRENT_SOURCE_DIR}/pngconf.h"
                       "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.out")

  # Generate pngprefix.h
  generate_source(OUTPUT "pngprefix.h"
                  DEPENDS ${PNGPREFIX_H_EXTRA_DEPENDS})

  generate_out(INPUT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/sym.c"
               OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scripts/sym.out"
               DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h")

  generate_out(INPUT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/symbols.c"
               OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scripts/symbols.out"
               DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/png.h"
                       "${CMAKE_CURRENT_SOURCE_DIR}/pngconf.h"
                       "${CMAKE_CURRENT_SOURCE_DIR}/scripts/pnglibconf.h.prebuilt")

  generate_out(INPUT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vers.c"
               OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scripts/vers.out"
               DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/png.h"
                       "${CMAKE_CURRENT_SOURCE_DIR}/pngconf.h"
                       "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h")

  generate_chk(INPUT "${CMAKE_CURRENT_BINARY_DIR}/scripts/symbols.out"
               OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scripts/symbols.chk"
               DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/checksym.awk"
                       "${CMAKE_CURRENT_SOURCE_DIR}/scripts/symbols.def")

  add_custom_target(symbol-check DEPENDS
                    "${CMAKE_CURRENT_BINARY_DIR}/scripts/symbols.chk")

  generate_copy("${CMAKE_CURRENT_BINARY_DIR}/scripts/sym.out"
                "${CMAKE_CURRENT_BINARY_DIR}/libpng.sym")
  generate_copy("${CMAKE_CURRENT_BINARY_DIR}/scripts/vers.out"
                "${CMAKE_CURRENT_BINARY_DIR}/libpng.vers")

  add_custom_target(genvers DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libpng.vers")
  add_custom_target(gensym DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/libpng.sym")

  add_custom_target("genprebuilt"
                    COMMAND "${CMAKE_COMMAND}"
                            "-DOUTPUT=scripts/pnglibconf.h.prebuilt"
                            -P "${CMAKE_CURRENT_BINARY_DIR}/scripts/gensrc.cmake"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

  # A single target handles generation of all generated files.  If
  # they are depended upon separately by multiple targets, this
  # confuses parallel make (it would require a separate top-level
  # target for each file to track the dependencies properly).
  add_custom_target(genfiles DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/libpng.sym"
    "${CMAKE_CURRENT_BINARY_DIR}/libpng.vers"
    "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.c"
    "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h"
    "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.out"
    "${CMAKE_CURRENT_BINARY_DIR}/pngprefix.h"
    "${CMAKE_CURRENT_BINARY_DIR}/scripts/intprefix.out"
    "${CMAKE_CURRENT_BINARY_DIR}/scripts/pnglibconf.c"
    "${CMAKE_CURRENT_BINARY_DIR}/scripts/prefix.out"
    "${CMAKE_CURRENT_BINARY_DIR}/scripts/sym.out"
    "${CMAKE_CURRENT_BINARY_DIR}/scripts/symbols.chk"
    "${CMAKE_CURRENT_BINARY_DIR}/scripts/symbols.out"
    "${CMAKE_CURRENT_BINARY_DIR}/scripts/vers.out")
endif(NOT AWK OR MSVC)

# OUR SOURCES
set(libpng_public_hdrs
  png.h
  pngconf.h
  "${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h"
)
set(libpng_private_hdrs
  pngpriv.h
  pngdebug.h
  pnginfo.h
  pngstruct.h
)
if(AWK AND NOT MSVC)
  list(APPEND libpng_private_hdrs "${CMAKE_CURRENT_BINARY_DIR}/pngprefix.h")
endif()
set(libpng_sources
  ${libpng_public_hdrs}
  ${libpng_private_hdrs}
  png.c
  pngerror.c
  pngget.c
  pngmem.c
  pngpread.c
  pngread.c
  pngrio.c
  pngrtran.c
  pngrutil.c
  pngset.c
  pngtrans.c
  pngwio.c
  pngwrite.c
  pngwtran.c
  pngwutil.c
)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
endif()

if(PNG_DEBUG)
  add_definitions(-DPNG_DEBUG)
endif()

# NOW BUILD OUR TARGET
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${ZLIB_INCLUDE_DIRS})

unset(PNG_LIB_TARGETS)

if(PNG_SHARED)
  add_library(png SHARED ${libpng_sources})
  set(PNG_LIB_TARGETS png)
  set_target_properties(png PROPERTIES OUTPUT_NAME ${PNG_LIB_NAME})
  add_dependencies(png genfiles)
  target_link_libraries(png ${ZLIB_LIBRARIES} ${M_LIBRARY})

  if(UNIX AND AWK)
    if(HAVE_LD_VERSION_SCRIPT)
      set_target_properties(png PROPERTIES LINK_FLAGS
        "-Wl,--version-script='${CMAKE_CURRENT_BINARY_DIR}/libpng.vers'")
    elseif(HAVE_SOLARIS_LD_VERSION_SCRIPT)
      set_target_properties(png PROPERTIES LINK_FLAGS
        "-Wl,-M -Wl,'${CMAKE_CURRENT_BINARY_DIR}/libpng.vers'")
    endif()
  endif()
endif()

if(PNG_STATIC)
  # does not work without changing name
  set(PNG_LIB_NAME_STATIC png_static)
  add_library(png_static STATIC ${libpng_sources})
  add_dependencies(png_static genfiles)
  # MSVC doesn't use a different file extension for shared vs. static
  # libs.  We are able to change OUTPUT_NAME to remove the _static
  # for all other platforms.
  if(NOT MSVC)
    set_target_properties(png_static PROPERTIES
      OUTPUT_NAME "${PNG_LIB_NAME}"
      CLEAN_DIRECT_OUTPUT 1)
  else()
    set_target_properties(png_static PROPERTIES
      OUTPUT_NAME "${PNG_LIB_NAME}_static"
      CLEAN_DIRECT_OUTPUT 1)
  endif()
  list(APPEND PNG_LIB_TARGETS png_static)
  target_link_libraries(png_static ${ZLIB_LIBRARIES} ${M_LIBRARY})
endif()

if(PNG_FRAMEWORK)
  set(PNG_LIB_NAME_FRAMEWORK png_framework)
  add_library(png_framework SHARED ${libpng_sources})
  add_dependencies(png_framework genfiles)
  list(APPEND PNG_LIB_TARGETS png_framework)
  set_target_properties(png_framework PROPERTIES
    FRAMEWORK TRUE
    FRAMEWORK_VERSION ${PNGLIB_VERSION}
    MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${PNGLIB_MAJOR}.${PNGLIB_MINOR}
    MACOSX_FRAMEWORK_BUNDLE_VERSION ${PNGLIB_VERSION}
    MACOSX_FRAMEWORK_IDENTIFIER org.libpng.libpng
    XCODE_ATTRIBUTE_INSTALL_PATH "@rpath"
    PUBLIC_HEADER "${libpng_public_hdrs}"
    OUTPUT_NAME png)
  target_link_libraries(png_framework ${ZLIB_LIBRARIES} ${M_LIBRARY})
endif()

if(NOT PNG_LIB_TARGETS)
  message(SEND_ERROR
    "No library variant selected to build. "
    "Please enable at least one of the following options: "
    "PNG_STATIC, PNG_SHARED, PNG_FRAMEWORK")
endif()

if(PNG_SHARED AND WIN32)
  set_target_properties(png PROPERTIES DEFINE_SYMBOL PNG_BUILD_DLL)
endif()

function(png_add_test)
  set(options)
  set(oneValueArgs NAME COMMAND)
  set(multiValueArgs OPTIONS FILES)
  cmake_parse_arguments(_PAT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT _PAT_NAME)
    message(FATAL_ERROR "png_add_test: Missing NAME argument")
  endif()
  if(NOT _PAT_COMMAND)
    message(FATAL_ERROR "png_add_test: Missing COMMAND argument")
  endif()

  set(TEST_OPTIONS "${_PAT_OPTIONS}")
  set(TEST_FILES "${_PAT_FILES}")

  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scripts/test.cmake.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/tests/${_PAT_NAME}.cmake" @ONLY)
  add_test(NAME "${_PAT_NAME}"
           COMMAND "${CMAKE_COMMAND}"
           "-DLIBPNG=$<TARGET_FILE:png>"
           "-DTEST_COMMAND=$<TARGET_FILE:${_PAT_COMMAND}>"
           -P "${CMAKE_CURRENT_BINARY_DIR}/tests/${_PAT_NAME}.cmake")
endfunction()

# Set a variable with CMake code which:
# Creates a symlink from src to dest (if possible) or alternatively
# copies if different.
include(CMakeParseArguments)

function(create_symlink DEST_FILE)

  cmake_parse_arguments(S "" "FILE;TARGET" "" ${ARGN})

  if(NOT S_TARGET AND NOT S_FILE)
    message(FATAL_ERROR "create_symlink: Missing TARGET or FILE argument")
  endif()

  if(S_TARGET AND S_FILE)
    message(FATAL_ERROR "create_symlink: Both source file ${S_FILE} and build target ${S_TARGET} arguments are present; can only have one.")
  endif()

  if(S_FILE)
    # If we don't need to symlink something that's coming from a build target,
    # we can go ahead and symlink/copy at configure time.
    if(CMAKE_HOST_WIN32 AND NOT CYGWIN)
      execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${S_FILE} ${DEST_FILE}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    else()
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${S_FILE} ${DEST_FILE}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
  endif()

  if(S_TARGET)
    # We need to use generator expressions, which can be a bit tricky, so for
    # simplicity make the symlink a POST_BUILD step and use the TARGET
    # signature of add_custom_command.
    if(CMAKE_HOST_WIN32 AND NOT CYGWIN)
      add_custom_command(TARGET ${S_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_LINKER_FILE_NAME:${S_TARGET}> $<TARGET_LINKER_FILE_DIR:${S_TARGET}>/${DEST_FILE})
    else()
      add_custom_command(TARGET ${S_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E create_symlink $<TARGET_LINKER_FILE_NAME:${S_TARGET}> $<TARGET_LINKER_FILE_DIR:${S_TARGET}>/${DEST_FILE})
    endif()
  endif()

endfunction()

# Create source generation scripts.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/genchk.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/scripts/genchk.cmake @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/genout.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/scripts/genout.cmake @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/gensrc.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/scripts/gensrc.cmake @ONLY)

# libpng is a library so default to 'lib'
if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR lib)
endif()

# CREATE PKGCONFIG FILES
# We use the same files like ./configure, so we have to set its vars.
# Only do this on Windows for Cygwin - the files don't make much sense outside
# a UNIX look alike
if(NOT WIN32 OR CYGWIN OR MINGW)
  set(prefix      ${CMAKE_INSTALL_PREFIX})
  set(exec_prefix ${CMAKE_INSTALL_PREFIX})
  set(libdir      ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})
  set(includedir  ${CMAKE_INSTALL_PREFIX}/include)
  set(LIBS        "-lz -lm")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libpng.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PNGLIB_NAME}.pc @ONLY)
  create_symlink(libpng.pc FILE ${PNGLIB_NAME}.pc)

  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libpng-config.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PNGLIB_NAME}-config @ONLY)
  create_symlink(libpng-config FILE ${PNGLIB_NAME}-config)
endif()

# SET UP LINKS
if(PNG_SHARED)
  set_target_properties(png PROPERTIES
#   VERSION 16.${PNGLIB_RELEASE}.1.6.37
    VERSION 16.${PNGLIB_RELEASE}.0
    SOVERSION 16
    CLEAN_DIRECT_OUTPUT 1)
endif()

# INSTALL
if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
  install(TARGETS ${PNG_LIB_TARGETS}
          EXPORT libpng
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
          FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR})

  if(PNG_SHARED)
    # Create a symlink for libpng.dll.a => libpng16.dll.a on Cygwin
    if(CYGWIN OR MINGW)
      create_symlink(libpng${CMAKE_IMPORT_LIBRARY_SUFFIX} TARGET png)
      install(FILES $<TARGET_LINKER_FILE_DIR:png>/libpng${CMAKE_IMPORT_LIBRARY_SUFFIX}
              DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    if(NOT WIN32)
      create_symlink(libpng${CMAKE_SHARED_LIBRARY_SUFFIX} TARGET png)
      install(FILES $<TARGET_LINKER_FILE_DIR:png>/libpng${CMAKE_SHARED_LIBRARY_SUFFIX}
              DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
  endif()

  if(PNG_STATIC)
    if(NOT WIN32 OR CYGWIN OR MINGW)
      create_symlink(libpng${CMAKE_STATIC_LIBRARY_SUFFIX} TARGET png_static)
      install(FILES $<TARGET_LINKER_FILE_DIR:png_static>/libpng${CMAKE_STATIC_LIBRARY_SUFFIX}
              DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
  endif()
endif()

if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL)
  install(FILES ${libpng_public_hdrs} DESTINATION include)
  install(FILES ${libpng_public_hdrs} DESTINATION include/${PNGLIB_NAME})
endif()
if(NOT SKIP_INSTALL_EXECUTABLES AND NOT SKIP_INSTALL_ALL)
  if(NOT WIN32 OR CYGWIN OR MINGW)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/libpng-config DESTINATION bin)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${PNGLIB_NAME}-config DESTINATION bin)
  endif()
endif()

if(NOT SKIP_INSTALL_PROGRAMS AND NOT SKIP_INSTALL_ALL)
  install(TARGETS ${PNG_BIN_TARGETS}
          RUNTIME DESTINATION bin)
endif()

if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL)
  # Install man pages
  if(NOT PNG_MAN_DIR)
    set(PNG_MAN_DIR "share/man")
  endif()
  install(FILES libpng.3 libpngpf.3 DESTINATION ${PNG_MAN_DIR}/man3)
  install(FILES png.5 DESTINATION ${PNG_MAN_DIR}/man5)
  # Install pkg-config files
  if(NOT CMAKE_HOST_WIN32 OR CYGWIN OR MINGW)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libpng.pc
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/libpng-config
            DESTINATION bin)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PNGLIB_NAME}.pc
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${PNGLIB_NAME}-config
            DESTINATION bin)
  endif()
endif()

# Create an export file that CMake users can include() to import our targets.
if(NOT SKIP_INSTALL_EXPORT AND NOT SKIP_INSTALL_ALL)
  install(EXPORT libpng DESTINATION lib/libpng FILE lib${PNG_LIB_NAME}.cmake)
endif()

# what's with libpng-manual.txt and all the extra files?

# UNINSTALL
# do we need this?

# DIST
# do we need this?

# to create msvc import lib for mingw compiled shared lib
# pexports libpng.dll > libpng.def
# lib /def:libpng.def /machine:x86

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

