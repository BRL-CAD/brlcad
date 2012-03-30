#              B R L C A D _ T A R G E T S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2012 United States Government as represented by
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

#-----------------------------------------------------------------------------
# Core routines for adding executables and libraries to the build and
# install lists of CMake
macro(BRLCAD_ADDEXEC execname srcs libs)
  # Basic setup
  string(REGEX REPLACE " " ";" srcslist "${srcs}")
  string(REGEX REPLACE " " ";" libslist1 "${libs}")
  string(REGEX REPLACE "-framework;" "-framework " libslist "${libslist1}")

  # Call standard CMake commands
  add_executable(${execname} ${srcslist})
  target_link_libraries(${execname} ${libslist})

  # Make sure we don't have a non-installed exec target.  If it is to
  # be installed, call the install function
  set(EXEC_INSTALL 1)
  foreach(extraarg ${ARGN})
    if(${extraarg} MATCHES "NO_INSTALL" AND BRLCAD_ENABLE_STRICT)
      set(EXEC_INSTALL 0)
    endif(${extraarg} MATCHES "NO_INSTALL" AND BRLCAD_ENABLE_STRICT)
  endforeach(extraarg ${ARGN})
  if(EXEC_INSTALL)
    install(TARGETS ${execname} DESTINATION ${BIN_DIR})
  endif(EXEC_INSTALL)

  # Take care of compile flags and definitions
  foreach(libitem ${libslist})
    list(FIND BRLCAD_LIBS ${libitem} FOUNDIT)
    if(NOT ${FOUNDIT} STREQUAL "-1")
      string(REGEX REPLACE "lib" "" ITEMCORE "${libitem}")
      string(TOUPPER ${ITEMCORE} ITEM_UPPER_CORE)
      get_property(${ITEM_UPPER_CORE}_DEFINES GLOBAL PROPERTY ${ITEM_UPPER_CORE}_DEFINES)
      list(APPEND ${execname}_DEFINES ${${ITEM_UPPER_CORE}_DEFINES})
      if(${execname}_DEFINES)
	list(REMOVE_DUPLICATES ${execname}_DEFINES)
      endif(${execname}_DEFINES)
      get_property(${ITEM_UPPER_CORE}_FLAGS GLOBAL PROPERTY ${ITEM_UPPER_CORE}_FLAGS)
      list(APPEND ${execname}_FLAGS ${${ITEM_UPPER_CORE}_FLAGS})
      if(${execname}_FLAGS)
	list(REMOVE_DUPLICATES ${execname}_FLAGS)
      endif(${execname}_FLAGS)
    endif(NOT ${FOUNDIT} STREQUAL "-1")
  endforeach(libitem ${libslist})
  foreach(lib_define ${${execname}_DEFINES})
    set_property(TARGET ${execname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
  endforeach(lib_define ${${execname}_DEFINES})
  foreach(lib_flag ${${execname}_FLAGS})
     set_property(TARGET ${execname} APPEND PROPERTY COMPILE_FLAGS "${lib_flag}")
  endforeach(lib_flag ${${execname}_FLAGS})

  # If this target is marked as incompatible with the strict flags, disable them
  foreach(extraarg ${ARGN})
    if(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
      if(NOERROR_FLAG)
	set_target_properties(${execname} PROPERTIES COMPILE_FLAGS "-Wno-error")
      endif(NOERROR_FLAG)
    endif(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
  endforeach(extraarg ${ARGN})

  CPP_WARNINGS(srcslist)
endmacro(BRLCAD_ADDEXEC execname srcs libs)


#-----------------------------------------------------------------------------
# Library macro handles both shared and static libs, so one "BRLCAD_ADDLIB"
# statement will cover both automatically
macro(BRLCAD_ADDLIB libname srcs libs)

  # Add ${libname} to the list of BRL-CAD libraries	
  list(APPEND BRLCAD_LIBS ${libname})
  list(REMOVE_DUPLICATES BRLCAD_LIBS)
  set(BRLCAD_LIBS "${BRLCAD_LIBS}" CACHE STRING "BRL-CAD libraries" FORCE)

  # Define convenience variables and scrub library list	
  string(REGEX REPLACE "lib" "" LOWERCORE "${libname}")
  string(TOUPPER ${LOWERCORE} UPPER_CORE)
  string(REGEX REPLACE " " ";" srcslist "${srcs}")
  string(REGEX REPLACE " " ";" libslist1 "${libs}")
  string(REGEX REPLACE "-framework;" "-framework " libslist "${libslist1}")

  # Collect the definitions needed by this library
  # Appending to the list to ensure that any non-template
  # definitions made in the CMakeLists.txt file are preserved.
  # In case of re-running cmake, make sure the DLL_IMPORTS define
  # for this specific library is removed, since for the actual target 
  # we need to export, not import.
  get_property(${UPPER_CORE}_DEFINES GLOBAL PROPERTY ${UPPER_CORE}_DEFINES)
  if(${UPPER_CORE}_DEFINES)
    list(REMOVE_ITEM ${UPPER_CORE}_DEFINES ${UPPER_CORE}_DLL_IMPORTS)
  endif(${UPPER_CORE}_DEFINES)
  foreach(libitem ${libslist})
    list(FIND BRLCAD_LIBS ${libitem} FOUNDIT)
    if(NOT ${FOUNDIT} STREQUAL "-1")
      string(REGEX REPLACE "lib" "" ITEMCORE "${libitem}")
      string(TOUPPER ${ITEMCORE} ITEM_UPPER_CORE)
      get_property(${ITEM_UPPER_CORE}_DEFINES GLOBAL PROPERTY ${ITEM_UPPER_CORE}_DEFINES)
      list(APPEND ${UPPER_CORE}_DEFINES ${${ITEM_UPPER_CORE}_DEFINES})
    endif(NOT ${FOUNDIT} STREQUAL "-1")
  endforeach(libitem ${libslist})
  if(${UPPER_CORE}_DEFINES)
    list(REMOVE_DUPLICATES ${UPPER_CORE}_DEFINES)
  endif(${UPPER_CORE}_DEFINES)
  mark_as_advanced(${UPPER_CORE}_DEFINES)
  # Need flags as well as definitions
  get_property(${UPPER_CORE}_FLAGS GLOBAL PROPERTY ${UPPER_CORE}_FLAGS)
  foreach(libitem ${libslist})
    list(FIND BRLCAD_LIBS ${libitem} FOUNDIT)
    if(NOT ${FOUNDIT} STREQUAL "-1")
      string(REGEX REPLACE "lib" "" ITEMCORE "${libitem}")
      string(TOUPPER ${ITEMCORE} ITEM_UPPER_CORE)
      get_property(${ITEM_UPPER_CORE}_FLAGS GLOBAL PROPERTY ${ITEM_UPPER_CORE}_FLAGS)
      list(APPEND ${UPPER_CORE}_FLAGS ${${ITEM_UPPER_CORE}_FLAGS})
    endif(NOT ${FOUNDIT} STREQUAL "-1")
  endforeach(libitem ${libslist})
  if(${UPPER_CORE}_FLAGS)
    list(REMOVE_DUPLICATES ${UPPER_CORE}_FLAGS)
  endif(${UPPER_CORE}_FLAGS)
  set_property(GLOBAL PROPERTY ${UPPER_CORE}_FLAGS "${${UPPER_CORE}_FLAGS}")
  mark_as_advanced(${UPPER_CORE}_FLAGS)


  # Handle "shared" libraries (with MSVC, these would be dynamic libraries)
  if(BUILD_SHARED_LIBS)
    add_library(${libname} SHARED ${srcslist})

    if(CPP_DLL_DEFINES)
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
    endif(CPP_DLL_DEFINES)

    if(NOT ${libs} MATCHES "NONE")
      target_link_libraries(${libname} ${libslist})
    endif(NOT ${libs} MATCHES "NONE")

    install(TARGETS ${libname} 
      RUNTIME DESTINATION ${BIN_DIR}
      LIBRARY DESTINATION ${LIB_DIR}
      ARCHIVE DESTINATION ${LIB_DIR})

    foreach(lib_define ${${UPPER_CORE}_DEFINES})
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
    endforeach(lib_define ${${UPPER_CORE}_DEFINES})

    foreach(lib_flag ${${UPPER_CORE}_FLAGS})
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_FLAGS "${lib_flag}")
    endforeach(lib_flag ${${UPPER_CORE}_FLAGS})

    foreach(extraarg ${ARGN})
      if(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
	if(NOERROR_FLAG)
	  set_property(TARGET ${libname} APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
	endif(NOERROR_FLAG)
      endif(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
    endforeach(extraarg ${ARGN})
  endif(BUILD_SHARED_LIBS)

  # For any BRL-CAD libraries using this library, define a variable 
  # with the needed definitions.
  if(CPP_DLL_DEFINES)
    list(APPEND ${UPPER_CORE}_DEFINES ${UPPER_CORE}_DLL_IMPORTS)
  endif(CPP_DLL_DEFINES)
  set_property(GLOBAL PROPERTY ${UPPER_CORE}_DEFINES "${${UPPER_CORE}_DEFINES}")

  # Handle static libraries (renaming requirements to both allow unique targets and
  # respect standard naming conventions.)
  if(BUILD_STATIC_LIBS)
    add_library(${libname}-static STATIC ${srcslist})

    if(NOT MSVC)
      if(NOT ${libs} MATCHES "NONE")
	target_link_libraries(${libname}-static ${libslist})
      endif(NOT ${libs} MATCHES "NONE")
      set_target_properties(${libname}-static PROPERTIES OUTPUT_NAME "${libname}")
    endif(NOT MSVC)

    install(TARGETS ${libname}-static
      RUNTIME DESTINATION ${BIN_DIR}
      LIBRARY DESTINATION ${LIB_DIR}
      ARCHIVE DESTINATION ${LIB_DIR})

    foreach(extraarg ${ARGN})
      if(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
	if(NOERROR_FLAG)
	  set_property(TARGET ${libname}-static APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
	endif(NOERROR_FLAG)
      endif(${extraarg} MATCHES "NOSTRICT" AND BRLCAD_ENABLE_STRICT)
    endforeach(extraarg ${ARGN})
  endif(BUILD_STATIC_LIBS)

  mark_as_advanced(BRLCAD_LIBS)
  CPP_WARNINGS(srcslist)
endmacro(BRLCAD_ADDLIB libname srcs libs)

#-----------------------------------------------------------------------------
# For situations when a local 3rd party library (say, zlib) has been chosen in 
# preference to a system version of that library, it is important to ensure 
# that the local header(s) get included before the system headers.  Normally 
# this is handled by explicitly specifying the local include paths (which, 
# being explicitly specified and non-standard, are checked prior to default 
# system locations) but there are some situations (macports being a classic 
# example) where *other* "non-standard" installed copies of libraries may 
# exist and be found if those directories are included ahead of the desired 
# local copy.  An observed case:
#
# 1.  macports is installed on OSX
# 2.  X11 is found in macports, X11 directories are set to /usr/macports based paths
# 3.  These paths are mixed into the general include path lists for some BRL-CAD libs.
# 4.  Because these paths are a) non-standard and b) contain zlib.h they result
#     in "system" versions of zlib present in macports being found first even when
#     the local zlib is enabled, if the macports paths happen to appear in the
#     include directory list before the local zlib include paths.
#
# To mitigate this problem, BRL-CAD library include directories are sorted
# according to the following hierarchy (using gcc's left-to-right search
# order as a basis: http://gcc.gnu.org/onlinedocs/cpp/Search-Path.html):
#
# 1.  If CMAKE_CURRENT_BINARY_DIR or CMAKE_CURRENT_SOURCE_DIR are in the
#     include list, they come first.
# 2.  If BRLCAD_BINARY_DIR/include or BRLCAD_SOURCE_DIR/include are present,
#     they come second.
# 3.  For remaining paths, if the "root" path matches the BRLCAD_SOURCE_DIR
#     or BRLCAD_BINARY_DIR paths, they are appended.
# 4.  Any remaining paths are appended.
macro(BRLCAD_SORT_INCLUDE_DIRS DIR_LIST)
  if(${DIR_LIST})
    set(ORDERED_ELEMENTS ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${BRLCAD_BINARY_DIR}/include ${BRLCAD_SOURCE_DIR}/include)
    set(NEW_DIR_LIST "")
    list(REMOVE_DUPLICATES ${DIR_LIST})
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
      if(${inc_path} MATCHES "^${BRLCAD_BINARY_DIR}")
	set(NEW_DIR_LIST ${NEW_DIR_LIST} ${inc_path})
	list(REMOVE_ITEM ${DIR_LIST} ${inc_path})
      endif(${inc_path} MATCHES "^${BRLCAD_BINARY_DIR}")
    endforeach(inc_path ${${DIR_LIST}})

    # paths in BRL-CAD source dir
    foreach(inc_path ${${DIR_LIST}})
      if(${inc_path} MATCHES "^${BRLCAD_SOURCE_DIR}")
	set(NEW_DIR_LIST ${NEW_DIR_LIST} ${inc_path})
	list(REMOVE_ITEM ${DIR_LIST} ${inc_path})
      endif(${inc_path} MATCHES "^${BRLCAD_SOURCE_DIR}")
    endforeach(inc_path ${${DIR_LIST}})

    # add anything that might be left
    set(NEW_DIR_LIST ${NEW_DIR_LIST} ${${DIR_LIST}})

    # put the results into DIR_LIST
    set(${DIR_LIST} ${NEW_DIR_LIST})
  endif(${DIR_LIST})
endmacro(BRLCAD_SORT_INCLUDE_DIRS)


#-----------------------------------------------------------------------------
# Wrapper to handle include directories specific to libraries.  Removes
# duplicates and makes sure the <LIB>_INCLUDE_DIRS list is in the cache
# immediately, so it can be used by other libraries.  These lists are not
# intended as toplevel user settable options so mark as advanced.
macro(BRLCAD_LIB_INCLUDE_DIRS libname DIR_LIST LOCAL_DIR_LIST)
  string(TOUPPER ${libname} LIB_UPPER)

  list(REMOVE_DUPLICATES ${DIR_LIST})
  set(${LIB_UPPER}_INCLUDE_DIRS ${${DIR_LIST}} CACHE STRING "Include directories for lib${libname}" FORCE)
  mark_as_advanced(${DIR_LIST})

  set(ALL_INCLUDES ${${DIR_LIST}} ${${LOCAL_DIR_LIST}})
  BRLCAD_SORT_INCLUDE_DIRS(ALL_INCLUDES)	
  include_directories(${ALL_INCLUDES})
endmacro(BRLCAD_LIB_INCLUDE_DIRS)


#-----------------------------------------------------------------------------
# We attempt here to strike a balance between competing needs.  Ideally, any error messages
# returned as a consequence of using data while running programs should point the developer
# back to the version controlled source code, not a copy in the build directory.  However,
# another design goal is to replicate the install tree layout in the build directory.  On
# platforms with symbolic links, we can do both by linking the source data files to their
# locations in the build directory.  On Windows, this is not possible and we have to fall
# back on an explicit file copy mechanism.  In both cases we have a build target that is
# triggered if source files are edited in order to allow the build system to take any further
# actions that may be needed (the current example is re-generating tclIndex and pkgIndex.tcl
# files when the source .tcl files change).

# In principle it may be possible to go even further and add rules to copy files in the build
# dir that are different from their source originals back into the source tree... need to
# think about complexity/robustness tradeoffs

# Because certain functions (for example, pkgIndex.tcl building) require that files
# handled by BRLCAD_ADDDATA already be in place when they run, global properties are
# used to track targets associated with each binary output directory.  Macros to ease
# adding to and retrieving from these lists are defined below.

macro(BRLCAD_ADDDATA inputdata targetdir)
  # Handle both a list of one or more files and variable holding a list of files - 
  # find out what we've got.
  set(havevarname 0)
  foreach(maybefilename ${inputdata})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${maybefilename})
      set(havevarname 1)
    endif(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${maybefilename})
  endforeach(maybefilename ${inputdata})
  if(NOT havevarname)
    set(FILELIST "${inputdata}")
    set(datalist FILELIST)
    BRLCAD_TARGET_NAME("${inputdata}" targetname)
  else(NOT havevarname)
    set(targetname "${inputdata}")
    set(datalist "${inputdata}")
  endif(NOT havevarname)

  # Now that the input data and target names are in order, define the custom
  # commands needed for build directory data copying on this platform (per
  # symlink test results.)

  if(HAVE_SYMLINK)

    # Make sure the target directory exists (symlinks need the target directory already in place)
    if(NOT CMAKE_CONFIGURATION_TYPES)
      execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir})
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)

    # Using symlinks - in this case, the custom command doesn't actually have to
    # do the work every time the source file changes - once established, the symlink
    # will behave correctly.  That being the case, we just go ahead and establish the
    # symlinks in the configure stage.
    foreach(filename ${${datalist}})
      get_filename_component(ITEM_NAME ${filename} NAME)
      if(NOT CMAKE_CONFIGURATION_TYPES)
	execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
      else(NOT CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
	endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      endif(NOT CMAKE_CONFIGURATION_TYPES)
    endforeach(filename ${${datalist}})

    # The custom command is still necessary - since it depends on the original source files, 
    # this will be the trigger that tells other commands depending on this data that 
    # they need to re-run one one of the source files is changed.
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel
      COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel
      DEPENDS ${${datalist}}
      )

  else(HAVE_SYMLINK)

    # Prepare full-path input list for scripts - the best we can do is to make build targets that ensure
    # the latest source direcotry file states are represented at build time.  Developers will have to
    # handle getting any changes they may make in the build directory copies of files back to the source
    # directories manually.
    set(inputlist)
    foreach(filename ${${datalist}})
      set(inputlist ${inputlist} ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
    endforeach(filename ${${datalist}})

    # Write out script for copying from source dir to build dir
    set(${targetname}_cmake_contents "set(FILES_TO_COPY \"${inputlist}\")\n")
    set(${targetname}_cmake_contents "${${targetname}_cmake_contents}foreach(filename \${FILES_TO_COPY})\n")
    if(NOT CMAKE_CONFIGURATION_TYPES)
      set(${targetname}_cmake_contents "${${targetname}_cmake_contents}  file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}\")\n")
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	set(${targetname}_cmake_contents "${${targetname}_cmake_contents}  file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}\")\n")
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)
    set(${targetname}_cmake_contents "${${targetname}_cmake_contents}endforeach(filename \${CURRENT_FILE_LIST})\n")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake "${${targetname}_cmake_contents}")

    # Define custom command for copying from src to bin.
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake
      COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel
      DEPENDS ${${datalist}}
      )
  endif(HAVE_SYMLINK)

  # Define the target and add it to this directories list of data targets
  add_custom_target(${targetname}_cp ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel)
  BRLCAD_ADD_DIR_LIST_ENTRY(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" ${targetname}_cp)

  # Add outputs to the distclean rules - this is consistent regardless of what type the output
  # file is, symlink or copy.  Just need to handle the single and multiconfig cases.
  foreach(filename ${${datalist}})
    get_filename_component(ITEM_NAME "${filename}" NAME)
    if(NOT CMAKE_CONFIGURATION_TYPES)
      DISTCLEAN(${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	DISTCLEAN(${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)
  endforeach(filename ${${datalist}})

  # The installation rule relates only to the original source directory copy, and so doesn't
  # need to explicitly concern itself with configurations.
  foreach(filename ${${datalist}})
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${filename} DESTINATION ${DATA_DIR}/${targetdir})
  endforeach(filename ${${datalist}})

  # Tell distcheck about these particular data files
  CMAKEFILES(${${datalist}})
endmacro(BRLCAD_ADDDATA datalist targetdir)

#-----------------------------------------------------------------------------
macro(ADD_MAN_PAGES num inmanlist)
  NORMALIZE_FILE_LIST(${inmanlist} manlist)
  CMAKEFILES(${${manlist}})
  foreach(manpage ${${manlist}})
    if (NOT CMAKE_CONFIGURATION_TYPES)
      configure_file(${manpage} ${CMAKE_BINARY_DIR}/${MAN_DIR}/man${num}/${manpage} COPYONLY)
    else (NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
        configure_file(${manpage} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${MAN_DIR}/man${num}/${manpage} COPYONLY)
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif (NOT CMAKE_CONFIGURATION_TYPES)
    install(FILES ${manpage} DESTINATION ${MAN_DIR}/man${num})
  endforeach(manpage ${${manlist}})
endmacro(ADD_MAN_PAGES num manlist)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
