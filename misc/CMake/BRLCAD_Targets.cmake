#              B R L C A D _ T A R G E T S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2020 United States Government as represented by
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
###

# Need sophisticated option parsing
include(CMakeParseArguments)

# When defining targets, we need to know if we have a no-error flag
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
check_c_compiler_flag(-Wno-error NOERROR_FLAG_C)
check_cxx_compiler_flag(-Wno-error NOERROR_FLAG_CXX)


# For BRL-CAD targets, use CXX as the language if the user requests it
function(SET_LANG_CXX SRC_FILES)
  if(ENABLE_ALL_CXX_COMPILE)
    foreach(srcfile ${SRC_FILES})
      if(NOT "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}" MATCHES "src/other")
	if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
	  set_source_files_properties(${srcfile} PROPERTIES LANGUAGE CXX)
	endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
      endif(NOT "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}" MATCHES "src/other")
    endforeach(srcfile ${SRC_FILES})
  endif(ENABLE_ALL_CXX_COMPILE)
endfunction(SET_LANG_CXX SRC_FILES)

# BRL-CAD style checking with AStyle
function(VALIDATE_STYLE targetname srcslist)

  if(BRLCAD_STYLE_VALIDATE AND ASTYLE_EXECUTABLE AND NOT "${srcslist}" STREQUAL "")

    # Find out of any of the files need to be ignored
    set(fullpath_srcslist)
    foreach(srcfile ${srcslist})
      if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
	get_property(IGNORE_FILE SOURCE ${srcfile} PROPERTY EXTERNAL)
	if(IGNORE_FILE)
	  message("Note: skipping style validation on external file ${srcfile}")
	else(IGNORE_FILE)
	  set(fullpath_srcslist ${fullpath_srcslist} "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
	endif(IGNORE_FILE)
      endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
    endforeach(srcfile ${srcslist})

    # If we have a list that isn't empty, use it
    if(fullpath_srcslist)
      add_custom_command(
	TARGET ${targetname} PRE_LINK
	COMMAND "${ASTYLE_EXECUTABLE}" --report --options=${BRLCAD_SOURCE_DIR}/misc/astyle.opt ${fullpath_srcslist}
	COMMENT "Checking formatting of ${targetname} srcs"
	)
      if(TARGET astyle)
	add_dependencies(${targetname} astyle)
      endif(TARGET astyle)
    endif(fullpath_srcslist)

  endif(BRLCAD_STYLE_VALIDATE AND ASTYLE_EXECUTABLE AND NOT "${srcslist}" STREQUAL "")

endfunction(VALIDATE_STYLE)

# Determine if a given file is a C or C++ file
function(FILE_LANG sfile outvar)

  if("${srcfile}" MATCHES "<TARGET_OBJECTS:")
    set(${outvar} "" PARENT_SCOPE)
    return()
  endif("${srcfile}" MATCHES "<TARGET_OBJECTS:")

  # If we're building all CXX, assume CXX
  if(ENABLE_ALL_CXX_COMPILE)
    set(file_language CXX)
  endif(ENABLE_ALL_CXX_COMPILE)

  # Try LANGUAGE property
  get_property(file_language SOURCE ${srcfile} PROPERTY LANGUAGE)

  # If we still don't know, go with extension
  if(NOT file_language)

    get_filename_component(srcfile_ext ${srcfile} EXT)
    string(SUBSTRING "${srcfile_ext}" 1 -1 f_ext)
    while(NOT "${f_ext}" STREQUAL "")
      get_filename_component(f_ext ${f_ext} EXT)
      if(f_ext)
	set(srcfile_ext ${f_ext})
	string(SUBSTRING "${f_ext}" 1 -1 f_ext)
      endif(f_ext)
    endwhile(NOT "${f_ext}" STREQUAL "")

    # C++
    if(${srcfile_ext} STREQUAL ".cxx" OR ${srcfile_ext} STREQUAL ".cpp" OR ${srcfile_ext} STREQUAL ".cc" OR ${srcfile_ext} STREQUAL ".inl")
      set(file_language CXX)
    endif(${srcfile_ext} STREQUAL ".cxx" OR ${srcfile_ext} STREQUAL ".cpp" OR ${srcfile_ext} STREQUAL ".cc" OR ${srcfile_ext} STREQUAL ".inl")
    if(${srcfile_ext} STREQUAL ".hxx" OR ${srcfile_ext} STREQUAL ".hpp" OR ${srcfile_ext} STREQUAL ".hh")
      set(file_language CXX)
    endif(${srcfile_ext} STREQUAL ".hxx" OR ${srcfile_ext} STREQUAL ".hpp" OR ${srcfile_ext} STREQUAL ".hh")

    # C
    if(${srcfile_ext} STREQUAL ".c" OR ${srcfile_ext} STREQUAL ".h" OR ${srcfile_ext} STREQUAL ".m")
      set(file_language C)
    endif(${srcfile_ext} STREQUAL ".c" OR ${srcfile_ext} STREQUAL ".h" OR ${srcfile_ext} STREQUAL ".m")

    # If we can't figure it out, assume C...
    if(NOT file_language)
      message(WARNING "Can't determine the source language of ${sfile}, assuming C...")
      set(file_language C)
    endif(NOT file_language)

  endif(NOT file_language)

  set(${outvar} "${file_language}" PARENT_SCOPE)

endfunction(FILE_LANG)


# Assemble the targets and compilation flags needed by the target
function(GET_FLAGS_AND_DEFINITIONS targetname target_libs)

  cmake_parse_arguments(G "" "CFLAGS;CXXFLAGS;DEFINES" "" ${ARGN})

  #####################################################################
  # Compile flags - note that targets may have separate C and C++ flags

  if(G_CFLAGS OR G_CXXFLAGS)

    set(FLAG_LANGUAGES C CXX)
    foreach(slang ${FLAG_LANGUAGES})

      # If we've already got some flags assigned to this target, pull them in
      get_property(T_FLAGS GLOBAL PROPERTY ${targetname}_${lang}_FLAGS)

      # Get all the flags from all the associated libraries
      foreach(libitem ${target_libs})
	get_property(ITEM_FLAGS GLOBAL PROPERTY ${libitem}_${lang}_FLAGS)
	list(APPEND T_FLAGS ${${libitem}_${lang}_FLAGS})
      endforeach(libitem ${target_libs})

      # If we've got anything, scrub down to unique entries
      if(T_FLAGS)
	list(REMOVE_DUPLICATES T_FLAGS)
      endif(T_FLAGS)

      # Put the results back into the global target
      set_property(GLOBAL PROPERTY ${targetname}_${lang}_FLAGS "${T_FLAGS}")

      # Set the language specific flag variables
      if("${slang}" STREQUAL "C")
	set(T_C_FLAGS "${T_FLAGS}")
      endif("${slang}" STREQUAL "C")
      if("${slang}" STREQUAL "CXX")
	set(T_CXX_FLAGS "${T_FLAGS}")
      endif("${slang}" STREQUAL "CXX")

    endforeach(slang ${FLAG_LANGUAGES})

    if(G_CFLAGS)
      set(${G_CFLAGS} ${T_C_FLAGS} PARENT_SCOPE)
    endif(G_CFLAGS)
    if(G_CXXFLAGS)
      set(${G_CXXFLAGS} ${T_CXX_FLAGS} PARENT_SCOPE)
    endif(G_CXXFLAGS)

  endif(G_CFLAGS OR G_CXXFLAGS)

  #############################################################
  # Compilation definitions (common to all source files)

  if(G_DEFINES)

    get_property(T_DEFINES GLOBAL PROPERTY ${targetname}_DEFINES)
    foreach(libitem ${target_libs})
      get_property(${libitem}_DEFINES GLOBAL PROPERTY ${libitem}_DEFINES)
      list(APPEND T_DEFINES ${${libitem}_DEFINES})
    endforeach(libitem ${target_libs})

    # No duplicate definitions needed
    if(T_DEFINES)
      list(REMOVE_DUPLICATES T_DEFINES)
    endif(T_DEFINES)

    # Send the finalized list back to the parent
    set(${G_DEFINES} ${T_DEFINES} PARENT_SCOPE)

  endif(G_DEFINES)

endfunction(GET_FLAGS_AND_DEFINITIONS)

# Determine the language for a target

# For simplicity, always set compile definitions and compile flags on
# files rather than build targets (less logic, simplifies dealing with
# OBJECT libraries.)
function(SET_FLAGS_AND_DEFINITIONS srcslist)

  cmake_parse_arguments(S "NO_STRICT_CXX" "TARGET" "CFLAGS;CXXFLAGS;DEFINES" ${ARGN})

  foreach(srcfile ${srcslist})

    # Find existing definitions so we aren't adding duplicates
    get_property(E_DEFINES SOURCE ${srcfile} PROPERTY COMPILE_DEFINITIONS)
    set(W_DEFINES ${S_DEFINES})
    if(W_DEFINES)
      foreach(df ${E_DEFINES})
	list(REMOVE_ITEM W_DEFINES ${df})
      endforeach(df ${E_DEFINES})
    endif(W_DEFINES)

    # Defines apply across C/C++
    foreach(tdef ${W_DEFINES})
      set_property(SOURCE ${srcfile} APPEND PROPERTY COMPILE_DEFINITIONS "${tdef}")
    endforeach(tdef ${W_DEFINES})

    # C or C++?
    FILE_LANG("${srcfile}" file_language)

    # Handle C files
    if("${file_language}" STREQUAL "C")

      # Find existing flags so we aren't adding duplicates
      get_property(E_CFLAGS SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
      set(W_CFLAGS ${S_CFLAGS})
      if(W_CFLAGS)
	foreach(df ${E_CFLAGS})
	  list(REMOVE_ITEM W_CFLAGS ${df})
	endforeach(df ${E_CFLAGS})
      endif(W_CFLAGS)

      foreach(tflag ${W_CFLAGS})
	set_property(SOURCE ${srcfile} APPEND PROPERTY COMPILE_FLAGS "${tflag}")
      endforeach(tflag ${W_CFLAGS})

      # Handle inline definition for C files only
      if(NOT "${C_INLINE}" STREQUAL "inline")
	list(FIND E_DEFINES "inline=${C_INLINE}" HAVE_INLINE_DEF)
	if("${HAVE_INLINE_DEF}" EQUAL -1)
	  set_property(SOURCE ${srcfile} APPEND PROPERTY COMPILE_DEFINITIONS "inline=${C_INLINE}")
	endif("${HAVE_INLINE_DEF}" EQUAL -1)
      endif(NOT "${C_INLINE}" STREQUAL "inline")

      # Handle disabling of strict compilation if target requires that
      if(S_NO_STRICT AND NOERROR_FLAG AND BRLCAD_ENABLE_STRICT)
	set_property(SOURCE ${srcfile} APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
      endif(S_NO_STRICT AND NOERROR_FLAG AND BRLCAD_ENABLE_STRICT)

    endif("${file_language}" STREQUAL "C")

    # Handle C++ files
    if("${file_language}" STREQUAL "C++")

      # Find existing flags so we aren't adding duplicates
      get_property(E_CXXFLAGS SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
      set(W_CXXFLAGS ${S_CXXFLAGS})
      if(W_CXXFLAGS)
	foreach(df ${E_CXXFLAGS})
	  list(REMOVE_ITEM W_CXXFLAGS ${df})
	endforeach(df ${E_CXXFLAGS})
      endif(W_CXXFLAGS)

      foreach(tflag ${W_CXXFLAGS})
	set_property(SOURCE ${srcfile} APPEND PROPERTY COMPILE_FLAGS "${tflag}")
      endforeach(tflag ${W_CXXFLAGS})

      # Handle disabling of strict compilation if target requires that
      if( (S_NO_STRICT OR _S_NO_STRICT_CXX) AND NOERROR_FLAG_CXX AND BRLCAD_ENABLE_STRICT)
	set_property(SOURCE ${srcfile} APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
      endif( (S_NO_STRICT OR _S_NO_STRICT_CXX) AND NOERROR_FLAG_CXX AND BRLCAD_ENABLE_STRICT)

    endif("${file_language}" STREQUAL "C++")

  endforeach(srcfile ${srcslist})

endfunction(SET_FLAGS_AND_DEFINITIONS)

define_property(GLOBAL PROPERTY BRLCAD_EXEC_FILES BRIEF_DOCS "BRL-CAD binaries" FULL_DOCS "List of installed BRL-CAD binary programs")

#-----------------------------------------------------------------------------
# Core routines for adding executables and libraries to the build and
# install lists of CMake
function(BRLCAD_ADDEXEC execname srcslist libslist)

  cmake_parse_arguments(E "TEST;TEST_USESDATA;NO_INSTALL;NO_STRICT;NO_STRICT_CXX;GUI" "FOLDER" "" ${ARGN})

  # Go all C++ if the settings request it
  SET_LANG_CXX("${srcslist}")

  # Add the executable.  If the caller indicates this is a GUI type
  # executable, add the correct flag for Visual Studio building (where
  # it matters)
  if(E_GUI)
    add_executable(${execname} WIN32 ${srcslist})
  else(E_GUI)
    add_executable(${execname} ${srcslist})
  endif(E_GUI)

  # Set the standard build definitions for all BRL-CAD targets
  target_compile_definitions(${execname} PRIVATE BRLCADBUILD HAVE_CONFIG_H)

  # Let CMAKEFILES know what's going on
  CMAKEFILES(${srcslist})

  # If this is an installed program, note that
  if (NOT E_NO_INSTALL AND NOT E_TEST)
    set_property(GLOBAL APPEND PROPERTY BRLCAD_EXEC_FILES "${execname}")
  endif (NOT E_NO_INSTALL AND NOT E_TEST)

  # Check at compile time the standard BRL-CAD style rules
  VALIDATE_STYLE("${execname}" "${srcslist}")

  # Use the list of libraries to be linked into this target to
  # accumulate the necessary definitions and compilation flags.
  GET_FLAGS_AND_DEFINITIONS(${execname} "${libslist}" CFLAGS E_C_FLAGS CXXFLAGS E_CXX_FLAGS DEFINES E_DEFINES)

  # Having built up the necessary sets, apply them
  SET_FLAGS_AND_DEFINITIONS("${srcslist}" TARGET ${execname} CFLAGS "${E_C_FLAGS}" CXXFLAGS "${E_CXX_FLAGS}" DEFINES "${E_DEFINES}")

  # If we have libraries to link, link them.
  if(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
    target_link_libraries(${execname} ${libslist})
  endif(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")

  # In some situations (usually test executables) we want to be able
  # to force the executable to remain in the local compilation
  # directory regardless of the global CMAKE_RUNTIME_OUTPUT_DIRECTORY
  # setting.  The NO_INSTALL flag is used to denote such executables.
  # If an executable isn't to be installed or needs to be installed
  # somewhere other than the default location, the NO_INSTALL argument
  # bypasses the standard install command call.
  #
  # If we *are* installing, do so to the binary directory (BIN_DIR)
  if(E_NO_INSTALL OR E_TEST)
    # Unfortunately, we currently need Windows binaries in the same directories as their DLL libraries
    if(NOT WIN32 AND NOT E_TEST_USESDATA)
      if(NOT CMAKE_CONFIGURATION_TYPES)
	set_target_properties(${execname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
      else(NOT CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  set_target_properties(${execname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER} "${CMAKE_CURRENT_BINARY_DIR}/${CFG_TYPE}")
	endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      endif(NOT CMAKE_CONFIGURATION_TYPES)
    endif(NOT WIN32 AND NOT E_TEST_USESDATA)
  else(E_NO_INSTALL OR E_TEST)
    if (NOT E_TEST_USESDATA)
      install(TARGETS ${execname} DESTINATION ${BIN_DIR})
    endif (NOT E_TEST_USESDATA)
  endif(E_NO_INSTALL OR E_TEST)


  # Set the folder property (used in programs such as Visual Studio to
  # organize build targets.
  if(E_NO_INSTALL AND NOT E_FOLDER)
    set(SUBFOLDER "/Build Only")
  endif(E_NO_INSTALL AND NOT E_FOLDER)
  if(E_TEST AND NOT E_FOLDER AND NOT SUBFOLDER)
    set(SUBFOLDER "/Test Programs")
  endif(E_TEST AND NOT E_FOLDER AND NOT SUBFOLDER)
  if(E_TEST_USESDATA AND NOT E_FOLDER AND NOT SUBFOLDER)
    set(SUBFOLDER "/Test Programs")
  endif(E_TEST_USESDATA AND NOT E_FOLDER AND NOT SUBFOLDER)
  if(E_FOLDER)
    set(SUBFOLDER "/${E_FOLDER}")
  endif(E_FOLDER)
  set_target_properties(${execname} PROPERTIES FOLDER "BRL-CAD Executables${SUBFOLDER}")


endfunction(BRLCAD_ADDEXEC execname srcslist libslist)


#---------------------------------------------------------------------
# Library function handles both shared and static libs, so one
# "BRLCAD_ADDLIB" statement will cover both automatically
function(BRLCAD_ADDLIB libname srcslist libslist)

  cmake_parse_arguments(L "SHARED;STATIC;NO_INSTALL;NO_STRICT;NO_STRICT_CXX" "FOLDER" "SHARED_SRCS;STATIC_SRCS" ${ARGN})

  # The naming convention used for variables is the upper case of the
  # library name, without the lib prefix.
  string(REPLACE "lib" "" LOWERCORE "${libname}")
  string(TOUPPER ${LOWERCORE} UPPER_CORE)

  # Let CMAKEFILES know what's going on
  CMAKEFILES(${srcslist} ${L_SHARED_SRCS} ${L_STATIC_SRCS})

  # Go all C++ if the settings request it
  SET_LANG_CXX("${srcslist};${L_SHARED_SRCS};${L_STATIC_SRCS}")

  # Retrieve the build flags and definitions
  GET_FLAGS_AND_DEFINITIONS(${libname} "${libslist}"
    CFLAGS L_C_FLAGS
    CXXFLAGS L_CXX_FLAGS
    DEFINES L_DEFINES
    )

  # Local copy of srcslist in case manipulation is needed
  set(lsrcslist ${srcslist})

  # If we're going to have a specified subfolder, prepare the
  # appropriate string:
  if(L_FOLDER)
    set(SUBFOLDER "/${L_FOLDER}")
  endif(L_FOLDER)

  # If we need it, set up the OBJECT library build
  if(USE_OBJECT_LIBS)
    add_library(${libname}-obj OBJECT ${lsrcslist})
    if(${libname} MATCHES "^lib*")
      set_target_properties(${libname}-obj PROPERTIES PREFIX "")
    endif(${libname} MATCHES "^lib*")

    set(lsrcslist $<TARGET_OBJECTS:${libname}-obj>)
    set_target_properties(${libname}-obj PROPERTIES FOLDER "BRL-CAD OBJECT Libraries${SUBFOLDER}")

    # Set the standard build definitions for all BRL-CAD targets
    target_compile_definitions(${libname}-obj PRIVATE BRLCADBUILD HAVE_CONFIG_H)

    if(HIDE_INTERNAL_SYMBOLS)

      set_property(TARGET ${libname}-obj APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
    endif(HIDE_INTERNAL_SYMBOLS)

    # If the library depends on other targets in this build, not just system
    # libraries, make sure the object targets depends them as well.  In
    # principle this isn't always required - object compilation may be
    # independent of the dependencies needed at link time - but if compilation
    # DOES depends on those targets having first performed some action (like
    # staging a header in an expected location) NOT setting this dependency
    # explicitly will eventually cause build failures.
    #
    # Without setting the OBJECT dependencies, success in the above case would
    # depend on whether or not the high level build ordering happened to run
    # the required targets before performing this step.  That failure mode is
    # semi-random and intermittent (top level build ordering without explicit
    # dependencies varies depending on -j flag values) making it hard to debug.
    # Ask me how I know.
    if(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
      foreach(ll ${libslist})
	if (TARGET ${ll})
	  add_dependencies(${libname}-obj ${ll})
	endif (TARGET ${ll})
      endforeach(ll ${libslist})
    endif(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")

  endif(USE_OBJECT_LIBS)

  # Handle the shared library
  if(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))

    add_library(${libname} SHARED ${lsrcslist} ${L_SHARED_SRCS})
    if(${libname} MATCHES "^lib*")
      set_target_properties(${libname} PROPERTIES PREFIX "")
    endif(${libname} MATCHES "^lib*")

    # Set the standard build definitions for all BRL-CAD targets
    target_compile_definitions(${libname} PRIVATE BRLCADBUILD HAVE_CONFIG_H)

    if(HIDE_INTERNAL_SYMBOLS)
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
      set_property(TARGET ${libname} APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS  "${UPPER_CORE}_DLL_IMPORTS")
    endif(HIDE_INTERNAL_SYMBOLS)

  endif(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))

  if(L_STATIC OR (BUILD_STATIC_LIBS AND NOT L_SHARED))
    if(L_STATIC)
      set(libstatic ${libname})
    else(L_STATIC)
      set(libstatic ${libname}-static)
    endif(L_STATIC)
    add_library(${libstatic} STATIC ${lsrcslist} ${L_STATIC_SRCS})
    if(${libstatic} MATCHES "^lib*")
      set_target_properties(${libstatic} PROPERTIES PREFIX "")
    endif(${libstatic} MATCHES "^lib*")

    # Set the standard build definitions for all BRL-CAD targets
    target_compile_definitions(${libstatic} PRIVATE BRLCADBUILD HAVE_CONFIG_H)

    if(NOT MSVC)
      set_target_properties(${libstatic} PROPERTIES OUTPUT_NAME "${libname}")
    endif(NOT MSVC)
  endif(L_STATIC OR (BUILD_STATIC_LIBS AND NOT L_SHARED))

  # Make sure we don't end up with outputs named liblib...
  set(possible_targets ${libname} ${libname}-static)
  foreach(pt ${possible_targets})
    if(TARGET ${pt} AND ${pt} MATCHES "^lib*")
      set_target_properties(${pt} PROPERTIES PREFIX "")
    endif(TARGET ${pt} AND ${pt} MATCHES "^lib*")
  endforeach(pt ${possible_targets})

  # Now that we have both the sources lists and the build targets,
  # assign flags
  SET_FLAGS_AND_DEFINITIONS("${srcslist};${L_SHARED_SRCS};${L_STATIC_SRCS}"
    TARGET ${libname}
    CFLAGS "${L_C_FLAGS}"
    CXXFLAGS "${L_CXX_FLAGS}"
    DEFINES "${L_DEFINES}"
    )

  # Extra static lib specific work
  if(L_STATIC OR (BUILD_STATIC_LIBS AND NOT L_SHARED))
    # We need to make sure the target depends on any targets in the libslist
    foreach(ll ${libslist})
      if (TARGET ${ll})
	add_dependencies(${libstatic} ${ll})
      endif (TARGET ${ll})
    endforeach(ll ${libslist})
    set_target_properties(${libstatic} PROPERTIES FOLDER "BRL-CAD Static Libraries${SUBFOLDER}")
    VALIDATE_STYLE("${libstatic}" "${srcslist};${L_STATIC_SRCS}")
    if(NOT L_NO_INSTALL)
      install(TARGETS ${libstatic}
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})
    endif(NOT L_NO_INSTALL)
  endif(L_STATIC OR (BUILD_STATIC_LIBS AND NOT L_SHARED))

  # Extra shared lib specific work
  if(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))
    set_target_properties(${libname} PROPERTIES FOLDER "BRL-CAD Shared Libraries${SUBFOLDER}")
    VALIDATE_STYLE("${libname}" "${srcslist};${L_SHARED_SRCS}")
    # If we have libraries to link for a shared library, link them.
    if(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
      target_link_libraries(${libname} ${libslist})
    endif(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
    if(NOT L_NO_INSTALL)
      install(TARGETS ${libname}
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})
    endif(NOT L_NO_INSTALL)
  endif(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))

endfunction(BRLCAD_ADDLIB libname srcslist libslist)

#---------------------------------------------------------------------
# For situations when a local 3rd party library (say, zlib) has been
# chosen in preference to a system version of that library, it is
# important to ensure that the local header(s) get included before the
# system headers.  Normally this is handled by explicitly specifying
# the local include paths (which, being explicitly specified and
# non-standard, are checked prior to default system locations) but
# there are some situations (macports being a classic example) where
# *other* "non-standard" installed copies of libraries may exist and
# be found if those directories are included ahead of the desired
# local copy.  An observed case:
#
# 1.  macports is installed on OSX
#
# 2.  X11 is found in macports, X11 directories are set to
#     /usr/macports based paths
#
# 3.  These paths are mixed into the general include path lists for
#     some BRL-CAD libs.
#
# 4.  Because these paths are a) non-standard and b) contain zlib.h
#     they result in "system" versions of zlib present in macports
#     being found first even when the local zlib is enabled, if the
#     macports paths happen to appear in the include directory list
#     before the local zlib include paths.
#
# To mitigate this problem, BRL-CAD library include directories are
# sorted according to the following hierarchy (using gcc's
# left-to-right search order as a basis:
# http://gcc.gnu.org/onlinedocs/cpp/Search-Path.html):
#
# 1.  If CMAKE_CURRENT_BINARY_DIR or CMAKE_CURRENT_SOURCE_DIR are in
#     the include list, they come first.
#
# 2.  If BRLCAD_BINARY_DIR/include or BRLCAD_SOURCE_DIR/include are
#     present, they come second.
#
# 3.  For remaining paths, if the "root" path matches the
#     BRLCAD_SOURCE_DIR or BRLCAD_BINARY_DIR paths, they are appended.
#
# 4.  Any remaining paths are appended.
#
function(BRLCAD_SORT_INCLUDE_DIRS DIR_LIST)
  if(${DIR_LIST})
    set(ORDERED_ELEMENTS "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" "${BRLCAD_BINARY_DIR}/include" "${BRLCAD_SOURCE_DIR}/include")
    set(LAST_ELEMENTS "/usr/local/include" "/usr/include")

    set(NEW_DIR_LIST "")
    set(LAST_DIR_LIST "")

    foreach(element ${ORDERED_ELEMENTS})
      set(DEF_EXISTS "-1")
      list(FIND ${DIR_LIST} ${element} DEF_EXISTS)
      if(NOT "${DEF_EXISTS}" STREQUAL "-1")
	set(NEW_DIR_LIST ${NEW_DIR_LIST} ${element})
	list(REMOVE_ITEM ${DIR_LIST} ${element})
      endif(NOT "${DEF_EXISTS}" STREQUAL "-1")
    endforeach(element ${ORDERED_ELEMENTS})

    # paths in BRL-CAD build dir
    foreach(inc_path ${${DIR_LIST}})
      IS_SUBPATH("${BRLCAD_BINARY_DIR}" "${inc_path}" SUBPATH_TEST)
      if("${SUBPATH_TEST}" STREQUAL "1")
	set(NEW_DIR_LIST ${NEW_DIR_LIST} ${inc_path})
	list(REMOVE_ITEM ${DIR_LIST} ${inc_path})
      endif("${SUBPATH_TEST}" STREQUAL "1")
    endforeach(inc_path ${${DIR_LIST}})

    # paths in BRL-CAD source dir
    foreach(inc_path ${${DIR_LIST}})
      IS_SUBPATH("${BRLCAD_SOURCE_DIR}" "${inc_path}" SUBPATH_TEST)
      if("${SUBPATH_TEST}" STREQUAL "1")
	set(NEW_DIR_LIST ${NEW_DIR_LIST} ${inc_path})
	list(REMOVE_ITEM ${DIR_LIST} ${inc_path})
      endif("${SUBPATH_TEST}" STREQUAL "1")
    endforeach(inc_path ${${DIR_LIST}})

    # Pull out include paths that are definitely system paths (and
    # hence need to come after ours
    foreach(element ${LAST_ELEMENTS})
      set(DEF_EXISTS "-1")
      list(FIND ${DIR_LIST} ${element} DEF_EXISTS)
      if(NOT "${DEF_EXISTS}" STREQUAL "-1")
	set(LAST_DIR_LIST ${LAST_DIR_LIST} ${element})
	list(REMOVE_ITEM ${DIR_LIST} ${element})
      endif(NOT "${DEF_EXISTS}" STREQUAL "-1")
    endforeach(element ${LAST_ELEMENTS})

    # add anything that might be left
    set(NEW_DIR_LIST ${NEW_DIR_LIST} ${${DIR_LIST}} ${LAST_DIR_LIST})

    # remove any duplicates
    list(REMOVE_DUPLICATES NEW_DIR_LIST)

    # put the results into DIR_LIST
    set(${DIR_LIST} ${NEW_DIR_LIST} PARENT_SCOPE)
  endif(${DIR_LIST})
endfunction(BRLCAD_SORT_INCLUDE_DIRS)

#---------------------------------------------------------------------
# Wrapper to properly include directories for a BRL-CAD build.
# Handles the SYSTEM option to the include_directories command, as
# well as calling the sort function.
function(BRLCAD_INCLUDE_DIRS DIR_LIST)

  # TODO - We don't want parent directories values augmenting DIR_LIST
  # for subsequent targets - if we're calling this, we're taking full
  # control of all inclusions for all targets in this and subsequent
  # directories.  We should probably use the target level
  # INCLUDE_DIRECTORIES property and stop setting include_directories
  # on entire directories all together for maximal precision and
  # minimum surprises...
  #
  # set_property(DIRECTORY PROPERTY INCLUDE_DIRECTORIES "")

  set(INCLUDE_DIRS ${${DIR_LIST}})
  if(INCLUDE_DIRS)
    list(REMOVE_DUPLICATES INCLUDE_DIRS)
  endif(INCLUDE_DIRS)

  BRLCAD_SORT_INCLUDE_DIRS(INCLUDE_DIRS)

  foreach(inc_dir ${INCLUDE_DIRS})
    get_filename_component(abs_inc_dir ${inc_dir} ABSOLUTE)
    IS_SUBPATH("${BRLCAD_SOURCE_DIR}" "${abs_inc_dir}" IS_LOCAL)
    if (NOT IS_LOCAL)
      IS_SUBPATH("${BRLCAD_BINARY_DIR}" "${abs_inc_dir}" IS_LOCAL)
    endif (NOT IS_LOCAL)
    set(IS_SYSPATH 0)
    foreach(sp ${SYS_INCLUDE_PATTERNS})
      if("${inc_dir}" MATCHES "${sp}")
	set(IS_SYSPATH 1)
      endif("${inc_dir}" MATCHES "${sp}")
    endforeach(sp ${SYS_INCLUDE_PATTERNS})
    if(IS_SYSPATH OR NOT IS_LOCAL)
      if(IS_SYSPATH)
	include_directories(SYSTEM ${inc_dir})
      else(IS_SYSPATH)
	include_directories(AFTER SYSTEM ${inc_dir})
      endif(IS_SYSPATH)
    else(IS_SYSPATH OR NOT IS_LOCAL)
      include_directories(BEFORE ${inc_dir})
    endif(IS_SYSPATH OR NOT IS_LOCAL)
  endforeach(inc_dir ${ALL_INCLUDES})

endfunction(BRLCAD_INCLUDE_DIRS DIR_LIST)

#---------------------------------------------------------------------
# Wrapper to handle include directories specific to libraries.
# Removes duplicates and makes sure the <LIB>_INCLUDE_DIRS list is in
# the cache immediately, so it can be used by other libraries.  These
# lists are not intended as toplevel user settable options so mark as
# advanced.
function(BRLCAD_LIB_INCLUDE_DIRS libname DIR_LIST LOCAL_DIR_LIST)
  string(TOUPPER ${libname} LIB_UPPER)

  list(REMOVE_DUPLICATES ${DIR_LIST})
  set(${LIB_UPPER}_INCLUDE_DIRS ${${DIR_LIST}} CACHE STRING "Include directories for lib${libname}" FORCE)
  mark_as_advanced(${DIR_LIST})

  set(ALL_INCLUDES ${${DIR_LIST}} ${${LOCAL_DIR_LIST}})
  BRLCAD_INCLUDE_DIRS(ALL_INCLUDES)
endfunction(BRLCAD_LIB_INCLUDE_DIRS)


#---------------------------------------------------------------------
# Files needed by BRL-CAD need to be both installed and copied into
# the correct locations in the build directories to allow executables
# to run at build time.  Ideally, we would like any error messages
# returned when running from the build directory to direct back to the
# copies of files in the source tree, since those are the ones that
# should be edited.  On platforms that support symlinks, this is
# possible - build directory "copies" of files are symlinks to the
# source tree version.  On platforms without symlink support, we are
# forced to copy the files into place in the build directories.  In
# both cases we have a build target that is triggered if source files
# are edited in order to allow the build system to take further
# actions (for example, re-generating tclIndex and pkgIndex.tcl files
# when the source .tcl files change).
#
# Because BRLCAD_MANAGE_FILES defines custom commands and specifies
# the files it is to copy/symlink as dependencies, there is a
# potential for file names to conflict with build target names. (This
# has actually been observed with MSVC - our file INSTALL in the
# toplevel source directory conflicts with the MSVC target named
# INSTALL.) To avoid conflicts and make the dependencies of the custom
# commands robust we supply full file paths as dependencies to the
# file copying custom commands.

function(BRLCAD_MANAGE_FILES inputdata targetdir)

  if (NOT TARGET managed_files)
    add_custom_target(managed_files ALL)
    set_target_properties(managed_files PROPERTIES FOLDER "BRL-CAD File Copying")
  endif (NOT TARGET managed_files)

  string(RANDOM LENGTH 10 ALPHABET 0123456789 VAR_PREFIX)
  if(${ARGC} GREATER 2)
    CMAKE_PARSE_ARGUMENTS(${VAR_PREFIX} "EXEC" "FOLDER" "" ${ARGN})
  endif(${ARGC} GREATER 2)

  # Handle both a list of one or more files and variable holding a
  # list of files - find out what we've got.
  NORMALIZE_FILE_LIST("${inputdata}" RLIST datalist FPLIST fullpath_datalist TARGET targetname)

  # Identify the source files for CMake
  CMAKEFILES(${datalist})

  #-------------------------------------------------------------------
  # Some of the more advanced build system features in BRL-CAD's CMake
  # build need to know whether symlink support is present on the
  # current OS - go ahead and do this test up front, caching the
  # results.
  if(NOT DEFINED HAVE_SYMLINK)
    message("--- Checking operating system support for file symlinking")
    file(WRITE "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src" "testing for symlink ability")
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src" "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
    if(EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
      message("--- Checking operating system support for file symlinking - Supported")
      set(HAVE_SYMLINK 1 CACHE BOOL "Platform supports creation of symlinks" FORCE)
      mark_as_advanced(HAVE_SYMLINK)
      file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src" "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
    else(EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
      message("--- Checking operating system support for file symlinking - Unsupported")
      set(HAVE_SYMLINK 0 CACHE BOOL "Platform does not support creation of symlinks" FORCE)
      mark_as_advanced(HAVE_SYMLINK)
      file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src")
    endif(EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
  endif(NOT DEFINED HAVE_SYMLINK)

  # Now that the input data and target names are in order, define the
  # custom commands needed for build directory data copying on this
  # platform (per symlink test results.)

  if(HAVE_SYMLINK)

    # Make sure the target directory exists (symlinks need the target
    # directory already in place)
    if(NOT CMAKE_CONFIGURATION_TYPES)
      execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${targetdir}")
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${targetdir}")
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)

    # Using symlinks - in this case, the custom command doesn't
    # actually have to do the work every time the source file changes
    # - once established, the symlink will behave correctly.  That
    # being the case, we just go ahead and establish the symlinks in
    # the configure stage.
    foreach(filename ${fullpath_datalist})
      get_filename_component(ITEM_NAME ${filename} NAME)
      if(NOT CMAKE_CONFIGURATION_TYPES)
	execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filename} "${CMAKE_BINARY_DIR}/${targetdir}/${ITEM_NAME}")
      else(NOT CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filename} "${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${targetdir}/${ITEM_NAME}")
	endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      endif(NOT CMAKE_CONFIGURATION_TYPES)
    endforeach(filename ${fullpath_datalist})

    # check for and remove any dead symbolic links from a previous run
    file(GLOB listing LIST_DIRECTORIES false "${CMAKE_BINARY_DIR}/${targetdir}/*")
    foreach (filename ${listing})
      if (NOT EXISTS ${filename})
	message("Removing stale symbolic link ${filename}")
	execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${filename})
      endif (NOT EXISTS ${filename})
    endforeach (filename ${listing})

    # The custom command is still necessary - since it depends on the
    # original source files, this will be the trigger that tells other
    # commands depending on this data that they need to re-run when
    # one of the source files is changed.
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel"
      COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel"
      DEPENDS ${fullpath_datalist}
      )

  else(HAVE_SYMLINK)

    # Write out script for copying from source dir to build dir
    set(${targetname}_cmake_contents "set(FILES_TO_COPY \"${fullpath_datalist}\")\n")
    set(${targetname}_cmake_contents "${${targetname}_cmake_contents}foreach(filename \${FILES_TO_COPY})\n")
    if(NOT CMAKE_CONFIGURATION_TYPES)
      set(${targetname}_cmake_contents "${${targetname}_cmake_contents}  file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR}/${targetdir}\")\n")
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	set(${targetname}_cmake_contents "${${targetname}_cmake_contents}  file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${targetdir}\")\n")
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)
    set(${targetname}_cmake_contents "${${targetname}_cmake_contents}endforeach(filename \${CURRENT_FILE_LIST})\n")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake" "${${targetname}_cmake_contents}")

    # Define custom command for copying from src to bin.
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel"
      COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake"
      COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel"
      DEPENDS ${fullpath_datalist}
      )
  endif(HAVE_SYMLINK)

  # Define the target and add it to this directories list of data
  # targets
  add_custom_target(${targetname}_cp ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel")
  set_target_properties(${targetname}_cp PROPERTIES FOLDER "BRL-CAD File Copying")
  BRLCAD_ADD_DIR_LIST_ENTRY(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" ${targetname}_cp)

  # Because the target name for managed files is likely cryptic, we
  # add a dependency to the managed_files target so this poriton of
  # the logic can be referenced
  add_dependencies(managed_files ${targetname}_cp)

  # Set the FOLDER property.  If the target has supplied a folder, use
  # that as a subfolder
  if("${${VAR_PREFIX}_FOLDER}" STREQUAL "")
    set_target_properties(${targetname}_cp PROPERTIES FOLDER "BRL-CAD File Setup")
  else("${${VAR_PREFIX}_FOLDER}" STREQUAL "")
    set_target_properties(${targetname}_cp PROPERTIES FOLDER "BRL-CAD File Setup/${${VAR_PREFIX}_FOLDER}")
  endif("${${VAR_PREFIX}_FOLDER}" STREQUAL "")

  # Add outputs to the distclean rules - this is consistent regardless
  # of what type the output file is, symlink or copy.  Just need to
  # handle the single and multiconfig cases.
  foreach(filename ${fullpath_datalist})
    get_filename_component(ITEM_NAME "${filename}" NAME)
    if(NOT CMAKE_CONFIGURATION_TYPES)
      DISTCLEAN("${CMAKE_BINARY_DIR}/${targetdir}/${ITEM_NAME}")
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	DISTCLEAN("${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${targetdir}/${ITEM_NAME}")
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)
  endforeach(filename ${fullpath_datalist})

  # The installation rule relates only to the original source
  # directory copy, and so doesn't need to explicitly concern itself
  # with configurations.
  if(${VAR_PREFIX}_EXEC)
    install(PROGRAMS ${datalist} DESTINATION ${targetdir})
  else(${VAR_PREFIX}_EXEC)
    install(FILES ${datalist} DESTINATION ${targetdir})
  endif(${VAR_PREFIX}_EXEC)

endfunction(BRLCAD_MANAGE_FILES)

#-----------------------------------------------------------------------------
# Specific uses of the BRLCAD_MANAGED_FILES functionality - these
# cover most of the common cases in BRL-CAD.

function(BRLCAD_ADDDATA datalist targetdir)
  BRLCAD_MANAGE_FILES(${datalist} ${DATA_DIR}/${targetdir})
endfunction(BRLCAD_ADDDATA)

function(ADD_DOC doclist targetdir)
  set(doc_target_dir ${DOC_DIR}/${targetdir})
  BRLCAD_MANAGE_FILES(${doclist} ${doc_target_dir})
endfunction(ADD_DOC)

function(ADD_MAN_PAGES num inmanlist)
  set(man_target_dir ${MAN_DIR}/man${num})
  BRLCAD_MANAGE_FILES(${inmanlist} ${man_target_dir})
endfunction(ADD_MAN_PAGES)



#-----------------------------------------------------------------------------
# The default operational mode of Regression tests is to be executed
# by a parent CMake script, which captures the I/O from the test and
# stores it in an individual log file named after the test.  By
# default, a custom command and CTest add_test command are set up to
# run a configured script.  If TEST_SCRIPT is provided specifying a
# particular script file that is used, otherwise the convention of
# ${testname}.cmake.in in the current source directory is assumed to
# specify the input test script.
#
# Particularly when configuration dependent builds are in play, a test
# executable's location needs special handling to ensure the scripts
# run the correct version of a program.  The standard mechanism is to
# specify the CMake target name of the executable by supplying it via
# the EXEC option and then pass the output of
# $<TARGET_FILE:${${testname}_EXEC}> to the running CMake
# script. (Note that the script must in turn post-process this value
# to unquote it in case of special characters in pathnames.)
#
# To allow for more customized test execution, the option TEST_DEFINED
# may be passed to the function to instruct it to skip all setup for
# add_test and custom command definitions.  It is the callers
# responsibility to define an appropriately named test with add_test -
# BRLCAD_REGRESSION_TEST in this mode will then perform only the
# specific build target definition and subsequent steps for wiring the
# test into the higher level commands.
#
# Standard actions for all regression targets:
#
# 1.  A custom build target with the pattern regress-${testname} is
#     defined to allow for individual execution of the regression test
#     with "make ${testname}"
#
# 2.  A label is added identifying the test as a regression test so
#     the top level commands "make check" and "make regress" know this
#     particular tests is one of the tests they are supposed to
#     execute.
#
# 3.  Any dependencies in ${depends_list} are added as build
#     requirements to the regress and check targets.  This ensures
#     that (unlike "make test" and CTest itself) when those targets
#     are built the dependencies of the tests are built first.  (A
#     default CTest run prior to building will result in all tests
#     failing.)
#
# 4.  If the keyword "STAND_ALONE" is passed in, a ${testname} target
#     is defined but no other connections are made between that target
#     and the agglomeration targets.
#
# 5.  If a TIMEOUT argument is passed, a specific timeout tiem is set
#     on the test. Otherwise, a default is assigned to ensure no test
#     runs indefinitely.

function(BRLCAD_REGRESSION_TEST testname depends_list)

  cmake_parse_arguments(${testname} "TEST_DEFINED;STAND_ALONE" "TEST_SCRIPT;TIMEOUT;EXEC" "" ${ARGN})

  if (NOT ${testname}_TEST_DEFINED)

    # Test isn't yet defined - do the add_test setup
    if (${testname}_TEST_SCRIPT)
      configure_file("${${testname}_TEST_SCRIPT}" "${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake" @ONLY)
    else (${testname}_TEST_SCRIPT)
      configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${testname}.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake" @ONLY)
    endif (${testname}_TEST_SCRIPT)
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake")

    if (TARGET ${${testname}_EXEC})
      add_test(NAME ${testname} COMMAND "${CMAKE_COMMAND}" -DEXEC=$<TARGET_FILE:${${testname}_EXEC}> -P "${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake")
    else (TARGET ${${testname}_EXEC})
      add_test(NAME ${testname} COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake")
    endif (TARGET ${${testname}_EXEC})

  endif (NOT ${testname}_TEST_DEFINED)


  # Every regression test gets a build target
  if (CMAKE_CONFIGURATION_TYPES)
    add_custom_target(${testname}
      COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_CFG_INTDIR} -R^${testname}$ --output-on-failure
      VERBATIM
      )
  else (CMAKE_CONFIGURATION_TYPES)
    add_custom_target(${testname}
      COMMAND ${CMAKE_CTEST_COMMAND} -R ^${testname}$ --output-on-failure
      VERBATIM
      )
  endif (CMAKE_CONFIGURATION_TYPES)
  if (depends_list)
    add_dependencies(${testname} ${depends_list})
  endif (depends_list)

  # Make sure we at least get this into the regression test folder -
  # local subdirectories may override this if they have more specific
  # locations they want to use.
  if (${testname}_STAND_ALONE)
    set_target_properties(${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests")
  else (${testname}_STAND_ALONE)
    set_target_properties(${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests/regress")
  endif (${testname}_STAND_ALONE)

  # In Visual Studio, none of the regress build targets are added to
  # the default build.
  set_target_properties(${testname} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)

  # Group any test not excluded by the STAND_ALONE flag with the other
  # regression tests by assigning a standard label
  if (NOT ${testname}_STAND_ALONE)
    set_tests_properties(${testname} PROPERTIES LABELS "Regression")
  else (NOT ${testname}_STAND_ALONE)
    set_tests_properties(${testname} PROPERTIES LABELS "STAND_ALONE")
  endif (NOT ${testname}_STAND_ALONE)

  # Set up dependencies for both regress and check
  if (NOT "${depends_list}" STREQUAL "")
    add_dependencies(regress ${depends_list})
    add_dependencies(check   ${depends_list})
  endif (NOT "${depends_list}" STREQUAL "")

  # Set either the standard timeout or a specified custom timeout
  if (${testname}_TIMEOUT)
    set_tests_properties(${testname} PROPERTIES TIMEOUT ${${testname}_TIMEOUT})
  else (${testname}_TIMEOUT)
    set_tests_properties(${testname} PROPERTIES TIMEOUT 300) # FIXME: want <60
  endif (${testname}_TIMEOUT)

endfunction(BRLCAD_REGRESSION_TEST)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
