# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

cmake_minimum_required(VERSION 3.18)

set(tklitehtml_sources src/init.cpp src/widget.cpp src/widget.h)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(tklitehtml VERSION 0.1.0 LANGUAGES C CXX)
  include(CTest)
  include(GNUInstallDirs)
  find_package(litehtml CONFIG REQUIRED)
  find_package(TCL 8.6 REQUIRED)
  set(TK_PLATFORM_INCLUDE_PATH "" CACHE PATH
    "Platform headers required by the selected Tk installation")
  if(NOT TK_LIBRARY OR NOT TCL_STUB_LIBRARY OR NOT TK_STUB_LIBRARY)
    message(FATAL_ERROR "Tcl and Tk 8.6 with stub libraries are required")
  endif()
  add_library(tklitehtml SHARED ${tklitehtml_sources})
  target_compile_definitions(tklitehtml PRIVATE USE_TCL_STUBS USE_TK_STUBS)
  target_link_libraries(tklitehtml PRIVATE ${TCL_STUB_LIBRARY} ${TK_STUB_LIBRARY})
  set_target_properties(tklitehtml PROPERTIES PREFIX "")
else()
  if(BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK AND TK_LIBRARY)
    list(TRANSFORM tklitehtml_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/"
      OUTPUT_VARIABLE embedded_sources)
    set(TKLITEHTML_SOURCES ${embedded_sources} PARENT_SCOPE)
    set(TKLITEHTML_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
  endif()
endif()

if(TARGET tklitehtml OR (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK AND TK_LIBRARY))
  # litehtml does not export a package version. Probe the API so both
  # v0.10 and newer installations work, including after a dependency update.
  function(tklitehtml_check_api)
    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    if(TARGET LITEHTML::LITEHTML)
      set(CMAKE_REQUIRED_LIBRARIES LITEHTML::LITEHTML)
    else()
      set(CMAKE_REQUIRED_LIBRARIES litehtml)
    endif()
    unset(TKLITEHTML_REDRAW_CALLBACK CACHE)
    check_cxx_source_compiles("
#include <litehtml.h>
void mouse_leave(litehtml::document& document)
{
    document.on_mouse_leave([](const litehtml::position&) {});
}
" TKLITEHTML_REDRAW_CALLBACK)
    configure_file(src/litehtml_compat.h.in litehtml_compat.h)
    cmake_pop_check_state()
  endfunction()
  tklitehtml_check_api()

  # Embed the helpers so the built-in and standalone packages initialize
  # identically, without needing to locate a separate Tcl script at runtime.
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS library/tklitehtml.tcl)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/library/tklitehtml.tcl" TKLITEHTML_LIBRARY_SCRIPT)
  set(TKLITEHTML_SCRIPT_CHUNK_SIZE 12000)
  string(ASCII 34 TKLITEHTML_DOUBLE_QUOTE)
  string(LENGTH "${TKLITEHTML_LIBRARY_SCRIPT}" TKLITEHTML_SCRIPT_LENGTH)
  set(TKLITEHTML_LIBRARY_SCRIPT_LITERALS "")
  set(TKLITEHTML_SCRIPT_OFFSET 0)
  while(TKLITEHTML_SCRIPT_OFFSET LESS TKLITEHTML_SCRIPT_LENGTH)
    string(SUBSTRING "${TKLITEHTML_LIBRARY_SCRIPT}" ${TKLITEHTML_SCRIPT_OFFSET}
      ${TKLITEHTML_SCRIPT_CHUNK_SIZE} TKLITEHTML_SCRIPT_CHUNK)
    string(APPEND TKLITEHTML_LIBRARY_SCRIPT_LITERALS
      "R${TKLITEHTML_DOUBLE_QUOTE}tklitehtml(${TKLITEHTML_SCRIPT_CHUNK})tklitehtml${TKLITEHTML_DOUBLE_QUOTE}\n")
    math(EXPR TKLITEHTML_SCRIPT_OFFSET
      "${TKLITEHTML_SCRIPT_OFFSET} + ${TKLITEHTML_SCRIPT_CHUNK_SIZE}")
  endwhile()
  configure_file(src/library_script.h.in library_script.h @ONLY)
  if(COMMAND distclean)
    distclean(library_script.h litehtml_compat.h)
  endif()
endif()

if(TARGET tklitehtml)
  target_compile_features(tklitehtml PRIVATE cxx_std_17)
  target_include_directories(tklitehtml PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} ${TCL_INCLUDE_PATH} ${TK_INCLUDE_PATH}
    ${TK_PLATFORM_INCLUDE_PATH} ${BRLCAD_X11_INCLUDE_DIRS})
  if(MSVC)
    target_compile_options(tklitehtml PRIVATE /W4)
  else()
    target_compile_options(tklitehtml PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  target_link_libraries(tklitehtml PRIVATE litehtml)
  set_target_properties(tklitehtml PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_EXTENSIONS OFF
    UNITY_BUILD OFF
  )
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl"
    CONTENT "package ifneeded tklitehtml ${PROJECT_VERSION} [list load [file join \$dir $<TARGET_FILE_NAME:tklitehtml>] Tklitehtml]\n")
  install(TARGETS tklitehtml
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/tklitehtml${PROJECT_VERSION}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBDIR}/tklitehtml${PROJECT_VERSION}")
  install(FILES LICENSE "${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/tklitehtml${PROJECT_VERSION}")
  if(BUILD_TESTING)
    foreach(suite all performance)
      add_test(NAME tklitehtml-${suite} COMMAND ${TCL_TCLSH}
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/${suite}.tcl"
        -packagepath "${CMAKE_CURRENT_BINARY_DIR}")
      set_tests_properties(tklitehtml-${suite} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
    endforeach()
    set_tests_properties(tklitehtml-performance PROPERTIES LABELS Performance)
  endif()
else()
  cmakefiles(${tklitehtml_sources}
    CMakeLists.txt LICENSE README.md library/tklitehtml.tcl src/library_script.h.in src/litehtml_compat.h.in
    tests/all.tcl tests/performance.tcl tests/embedded.tcl)
endif()
