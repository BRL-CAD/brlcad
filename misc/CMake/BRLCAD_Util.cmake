#               B R L C A D _ U T I L . C M A K E
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
# Pretty-printing macro that generates a box around a string and prints the
# resulting message.
macro(BOX_PRINT input_string border_string)
  string(LENGTH ${input_string} MESSAGE_LENGTH)
  string(LENGTH ${border_string} SEPARATOR_STRING_LENGTH)
  while(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
    set(SEPARATOR_STRING "${SEPARATOR_STRING}${border_string}")
    string(LENGTH ${SEPARATOR_STRING} SEPARATOR_STRING_LENGTH)
  endwhile(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
  message("${SEPARATOR_STRING}")
  message("${input_string}")
  message("${SEPARATOR_STRING}")
endmacro()


#-----------------------------------------------------------------------------
include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-Wno-error NOERROR_FLAG)

macro(CPP_WARNINGS srcslist)
  # We need to specify specific flags for c++ files - their warnings are
  # not usually used to hault the build, althogh BRLCAD_ENABLE_CXX_STRICT
  # can be set to on to achieve that
  if(BRLCAD_ENABLE_STRICT AND NOT BRLCAD_ENABLE_CXX_STRICT)
    foreach(srcfile ${${srcslist}})
      if(${srcfile} MATCHES "cpp$" OR ${srcfile} MATCHES "cc$")
	if(BRLCAD_ENABLE_COMPILER_WARNINGS)
	  if(NOERROR_FLAG)
	    get_property(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
	    set_source_files_properties(${srcfile} COMPILE_FLAGS "-Wno-error ${previous_flags}")
	  endif(NOERROR_FLAG)
	else(BRLCAD_ENABLE_COMPILER_WARNINGS)
	  get_property(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
	  set_source_files_properties(${srcfile} COMPILE_FLAGS "-w ${previous_flags}")
	endif(BRLCAD_ENABLE_COMPILER_WARNINGS)
      endif(${srcfile} MATCHES "cpp$" OR ${srcfile} MATCHES "cc$")
    endforeach(srcfile ${${srcslist}})
  endif(BRLCAD_ENABLE_STRICT AND NOT BRLCAD_ENABLE_CXX_STRICT)
endmacro(CPP_WARNINGS)

#-----------------------------------------------------------------------------
# Convenience macros for handling lists of defines at the directory and target
# levels - the latter is not needed for BRLCAD_* targets under normal 
# circumstances and is intended for cases where basic non-installed 
# add_executable calls are made.
macro(BRLCAD_ADD_DEF_LISTS)
  foreach(deflist ${ARGN})
    set(target_def_list ${target_def_list} ${${deflist}})
  endforeach(deflist ${ARGN})
  if(target_def_list)
    list(REMOVE_DUPLICATES target_def_list)
  endif(target_def_list)
  foreach(defitem ${${deflist}})
    add_definitions(${defitem})
  endforeach(defitem ${${deflist}})
endmacro(BRLCAD_ADD_DEF_LISTS)

macro(BRLCAD_TARGET_ADD_DEFS target)
  get_target_property(TARGET_EXISTING_DEFS ${target} COMPILE_DEFINITIONS)
  foreach(defitem ${ARGN})
    set(DEF_EXISTS "-1")
    if(TARGET_EXISTING_DEFS)
      list(FIND TARGET_EXISTING_DEFS ${defitem} DEF_EXISTS)
    endif(TARGET_EXISTING_DEFS)
    if("${DEF_EXISTS}" STREQUAL "-1")
      set_property(TARGET ${target} APPEND PROPERTY COMPILE_DEFINITIONS "${defitem}")
    endif("${DEF_EXISTS}" STREQUAL "-1")
  endforeach(defitem ${ARGN})
endmacro(BRLCAD_TARGET_ADD_DEFS)

macro(BRLCAD_TARGET_ADD_DEF_LISTS target)
  get_target_property(TARGET_EXISTING_DEFS ${target} COMPILE_DEFINITIONS)
  foreach(deflist ${ARGN})
    set(target_def_list ${target_def_list} ${${deflist}})
  endforeach(deflist ${ARGN})
  if(target_def_list)
    list(REMOVE_DUPLICATES target_def_list)
  endif(target_def_list)
  foreach(defitem ${target_def_list})
    set(DEF_EXISTS "-1")
    if(TARGET_EXISTING_DEFS)
      list(FIND TARGET_EXISTING_DEFS ${defitem} DEF_EXISTS)
    endif(TARGET_EXISTING_DEFS)
    if("${DEF_EXISTS}" STREQUAL "-1")
      set_property(TARGET ${target} APPEND PROPERTY COMPILE_DEFINITIONS "${defitem}")
    endif("${DEF_EXISTS}" STREQUAL "-1")
  endforeach(defitem ${${deflist}})
endmacro(BRLCAD_TARGET_ADD_DEF_LISTS)

#-----------------------------------------------------------------------------
# Core routines for adding executables and libraries to the build and
# install lists of CMake
macro(BRLCAD_ADDEXEC execname srcs libs)
  string(REGEX REPLACE " " ";" srcslist "${srcs}")
  string(REGEX REPLACE " " ";" libslist1 "${libs}")
  string(REGEX REPLACE "-framework;" "-framework " libslist "${libslist1}")

  add_executable(${execname} ${srcslist})
  target_link_libraries(${execname} ${libslist})

  install(TARGETS ${execname} DESTINATION ${BIN_DIR})

  foreach(libitem ${libslist})
    list(FIND BRLCAD_LIBS ${libitem} FOUNDIT)
    if(NOT ${FOUNDIT} STREQUAL "-1")
      string(REGEX REPLACE "lib" "" ITEMCORE "${libitem}")
      string(TOUPPER ${ITEMCORE} ITEM_UPPER_CORE)
      list(APPEND ${execname}_DEFINES ${${ITEM_UPPER_CORE}_DEFINES})
      if(${execname}_DEFINES)
	list(REMOVE_DUPLICATES ${execname}_DEFINES)
      endif(${execname}_DEFINES)
    endif(NOT ${FOUNDIT} STREQUAL "-1")
  endforeach(libitem ${libslist})

  foreach(lib_define ${${execname}_DEFINES})
    set_property(TARGET ${execname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
  endforeach(lib_define ${${execname}_DEFINES})

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
  if(${UPPER_CORE}_DEFINES)
    list(REMOVE_ITEM ${UPPER_CORE}_DEFINES ${UPPER_CORE}_DLL_IMPORTS)
  endif(${UPPER_CORE}_DEFINES)
  foreach(libitem ${libslist})
    list(FIND BRLCAD_LIBS ${libitem} FOUNDIT)
    if(NOT ${FOUNDIT} STREQUAL "-1")
      string(REGEX REPLACE "lib" "" ITEMCORE "${libitem}")
      string(TOUPPER ${ITEMCORE} ITEM_UPPER_CORE)
      list(APPEND ${UPPER_CORE}_DEFINES ${${ITEM_UPPER_CORE}_DEFINES})
    endif(NOT ${FOUNDIT} STREQUAL "-1")
  endforeach(libitem ${libslist})
  if(${UPPER_CORE}_DEFINES)
    list(REMOVE_DUPLICATES ${UPPER_CORE}_DEFINES)
  endif(${UPPER_CORE}_DEFINES)
  mark_as_advanced(${UPPER_CORE}_DEFINES)

  # Handle "shared" libraries (with MSVC, these would be dynamic libraries)
  if(BUILD_SHARED_LIBS)
    add_library(${libname} SHARED ${srcslist})

    if(CPP_DLL_DEFINES)
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
    endif(CPP_DLL_DEFINES)

    if(NOT ${libs} MATCHES "NONE")
      target_link_libraries(${libname} ${libslist})
    endif(NOT ${libs} MATCHES "NONE")

    install(TARGETS ${libname} DESTINATION ${LIB_DIR})

    foreach(lib_define ${${UPPER_CORE}_DEFINES})
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
    endforeach(lib_define ${${UPPER_CORE}_DEFINES})

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
  set(${UPPER_CORE}_DEFINES "${${UPPER_CORE}_DEFINES}" CACHE STRING "${UPPER_CORE} library required definitions" FORCE)

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

    install(TARGETS ${libname}-static DESTINATION ${LIB_DIR})

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
macro(BRLCAD_ADDDATA datalist targetdir)
  if(NOT WIN32)
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}")
      execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
    endif(NOT EXISTS "${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}")

    foreach(filename ${${datalist}})
      get_filename_component(ITEM_NAME ${filename} NAME)
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
      DISTCLEAN(${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
    endforeach(filename ${${datalist}})

    string(REGEX REPLACE "/" "_" targetprefix ${targetdir})

    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel
      COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel
      DEPENDS ${${datalist}}
      )
    ADD_CUSTOM_TARGET(${datalist}_cp ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel)
  else(NOT WIN32)
    file(COPY ${${datalist}} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
    string(REGEX REPLACE "/" "_" targetprefix ${targetdir})
    set(inputlist)

    foreach(filename ${${datalist}})
      set(inputlist ${inputlist} ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
    endforeach(filename ${${datalist}})

    set(${targetprefix}_cmake_contents "
		set(FILES_TO_COPY \"${inputlist}\")
		file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}\")
		")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.cmake "${${targetprefix}_cmake_contents}")
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.cmake
      COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel
      DEPENDS ${${datalist}}
      )
    ADD_CUSTOM_TARGET(${datalist}_cp ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${targetprefix}.sentinel)
  endif(NOT WIN32)

  foreach(filename ${${datalist}})
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${filename} DESTINATION ${DATA_DIR}/${targetdir})
  endforeach(filename ${${datalist}})
  CMAKEFILES(${${datalist}})
endmacro(BRLCAD_ADDDATA datalist targetdir)


#-----------------------------------------------------------------------------
macro(BRLCAD_ADDFILE filename targetdir)
  file(COPY ${filename} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
  get_filename_component(file_root ${filename} NAME)
  DISTCLEAN(${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${file_root})
  string(REGEX REPLACE "/" "_" targetprefix ${targetdir})

  if(BRLCAD_ENABLE_DATA_TARGETS)
    string(REGEX REPLACE "/" "_" filestring ${filename})
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename}
      COMMAND ${CMAKE_COMMAND} -E copy	${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
      )
    ADD_CUSTOM_TARGET(${targetprefix}_${filestring}_cp ALL DEPENDS ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename})
  endif(BRLCAD_ENABLE_DATA_TARGETS)

  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${filename} DESTINATION ${DATA_DIR}/${targetdir})

  file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${CMAKE_CURRENT_SOURCE_DIR}/${filename}\n")
endmacro(BRLCAD_ADDFILE datalist targetdir)


#-----------------------------------------------------------------------------
macro(ADD_MAN_PAGES num manlist)
  CMAKEFILES(${${manlist}})
  foreach(manpage ${${manlist}})
    configure_file(${manpage} ${CMAKE_BINARY_DIR}/${MAN_DIR}/man${num}/${manpage})
    install(FILES ${manpage} DESTINATION ${MAN_DIR}/man${num})
  endforeach(manpage ${${manlist}})
endmacro(ADD_MAN_PAGES num manlist)


#-----------------------------------------------------------------------------
macro(ADD_MAN_PAGE num manfile)
  CMAKEFILES(${manfile})
  file(APPEND ${CMAKE_BINARY_DIR}/cmakefiles.cmake "${manfile}\n")
  configure_file(${manfile} ${CMAKE_BINARY_DIR}/${MAN_DIR}/man${num}/${manfile})
  install(FILES ${manfile} DESTINATION ${MAN_DIR}/man${num})
endmacro(ADD_MAN_PAGE num manfile)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
