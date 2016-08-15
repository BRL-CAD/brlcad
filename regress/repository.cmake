#!/bin/sh
#                R E P O S I T O R Y . C M A K E
# BRL-CAD
#
# Copyright (c) 2008-2016 United States Government as represented by
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
#
# Basic checks of the repository sources to make sure maintenance
# burden code clean-up problems don't creep in.
#
###

if ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
  CMAKE_POLICY(SET CMP0054 NEW)
endif ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)

# Set this if any test fails
set(REPO_CHECK_FAILED 0)

if(NOT DEFINED SOURCE_DIR)
  set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif(NOT DEFINED SOURCE_DIR)

if(NOT EXISTS ${SOURCE_DIR}/include/common.h)
  message(WARNING "Unable to find include/common.h in ${SOURCE_DIR}, this scan is almost certainly not testing the full BRL-CAD source tree")
endif(NOT EXISTS ${SOURCE_DIR}/include/common.h)


# List of lexer files, which frequently need to be ignored
set(LEXERS
  schema.h
  csg_parser.c
  csg_scanner.h
  obj_libgcv_grammar.cpp
  obj_obj-g_grammar.cpp
  obj_grammar.c
  obj_scanner.h
  obj_parser.h
  obj_rules.l
  obj_util.h
  obj_grammar.cpp
  obj_rules.cpp
  points_scan.c
  script.c
  )

# To start, recurse once to get the full list
file(GLOB_RECURSE FILE_SYSTEM_FILES
  LIST_DIRECTORIES false
  RELATIVE ${SOURCE_DIR}
  "${SOURCE_DIR}/*")

# Before we build our source, include and build file lists we do some
# preliminary filtering to remove files we don't want in any of those
# categories

# If a build directory is defined and is distinct from the source directory,
# filter out anything in that directory.
if(DEFINED BUILD_DIR)
  if(NOT "${SOURCE_DIR}" STREQUAL "${BUILD_DIR}")
    string(REPLACE "${SOURCE_DIR}/" "" REL_BUILD "${BUILD_DIR}")
    list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX "^${REL_BUILD}/.*")
  endif(NOT "${SOURCE_DIR}" STREQUAL "${BUILD_DIR}")
endif(DEFINED BUILD_DIR)

# We don't want any .svn files
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*[.]svn.*")

# Skip 3rd party files
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*src/other.*")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/tools.*")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/svn2git.*")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*/shapelib/.*")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/macosx/openUp.c")

# Documentation files
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*doc/parsers/templates/.*")

# Test srcs
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/CMake/compat/.*")

# Misc. test and demo files
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/Bullet_Box_Chain_Demo.cpp")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/opencl-raytracer-tests/.*")

# CMake find modules - often these are convenience inclusions of a 3rd
# party module not yet incorporated into CMake, and given the number of
# times they end up needing to contend with platform specific irregularities
# it doesn't make much sense to police them.
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/CMake/Find.*")

# We aren't interested in temporary files, log files, make
# files, or cache files
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*~")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*[.]swp")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*[.]log")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*Makefile.*")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*cache$")

# Now, build up lists of files we *do* want consider for
# the various categories

# Source files
macro(define_src_files)
  if(NOT SRCFILES)
    set(SRCFILES ${FILE_SYSTEM_FILES})
    list(FILTER SRCFILES INCLUDE REGEX ".*[.]c$|.*[.]cpp$|.*[.]cxx$|.*[.]cc$|.*[.]y$|.*[.]yy$|.*[.]l$")
  endif(NOT SRCFILES)
endmacro(define_src_files)

# Include files
macro(define_inc_files)
  if(NOT INCFILES)
    set(INCFILES ${FILE_SYSTEM_FILES})
    list(FILTER INCFILES INCLUDE REGEX ".*[.]h$|.*[.]hpp$|.*[.]hxx$")
  endif(NOT INCFILES)
endmacro(define_inc_files)

# Build files
macro(define_bld_files)
  if(NOT BLDFILES)
    set(BLDFILES ${FILE_SYSTEM_FILES})
    list(FILTER BLDFILES INCLUDE REGEX ".*[.]cmake$|.*CMakeLists.txt$|.*[.]cmake.in$")
  endif(NOT BLDFILES)
endmacro(define_bld_files)

# All source files
macro(define_allsrc_files)
  define_src_files()
  define_inc_files()
  set(ALLSRCFILES ${SRCFILES} ${INCFILES})
endmacro(define_allsrc_files)

# If we're running all tests, we need all the lists - handle it explicitly up front
if(RUN_ALL_TESTS)
  define_allsrc_files()
  define_bld_files()
endif(RUN_ALL_TESTS)

# Check if public headers are including private headers like bio.h
function(public_headers_test)

  set(HDR_PRINTED 0)
  message("running public header private header checks...")

  # Start with all include files
  define_inc_files()
  set(PUBLIC_HEADERS ${INCFILES})
  list(FILTER PUBLIC_HEADERS INCLUDE REGEX ".*include/.*")

  # Exclude a few that have known issues...
  list(FILTER PUBLIC_HEADERS EXCLUDE REGEX ".*include/brep.h")
  list(FILTER PUBLIC_HEADERS EXCLUDE REGEX ".*include/bu/cmd.h")
  list(FILTER PUBLIC_HEADERS EXCLUDE REGEX ".*include/fb.h")
  list(FILTER PUBLIC_HEADERS EXCLUDE REGEX ".*include/fb/fb_osgl.h")
  list(FILTER PUBLIC_HEADERS EXCLUDE REGEX ".*include/fb/fb_wgl.h")

  # The list of headers we want to check for
  set(PRIVATE_HEADERS bio.h bnetwork.h bsocket.h)
  foreach(pvhdr ${PRIVATE_HEADERS})
    set(${pvhdr}_check_succeeded 1)
  endforeach(pvhdr ${PRIVATE_HEADERS})

  foreach(puhdr ${PUBLIC_HEADERS})

    file(READ "${SOURCE_DIR}/${puhdr}" HDR_SRC)

    foreach(pvhdr ${PRIVATE_HEADERS})
      # If we found an improperly used header, report it
      if("${HDR_SRC}" MATCHES "${pvhdr}")
	if("${HDR_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${pvhdr}[\">]+")
	  if(NOT HDR_PRINTED)
	    message("\nPrivate header inclusion in public headers:")
	    set(HDR_PRINTED 1)
	    set(${pvhdr}_check_succeeded 0)
	  endif(NOT HDR_PRINTED)

	  # Since we know we have a problem, zero in on the exact line number(s) for reporting purposes
	  set(cline 1)
	  set(STOP_CHECK 0)
	  # We need to go with the while loop + substring approach because
	  # file(STRINGS ...) doesn't produce accurate line numbers and has issues
	  # with square brackets.  Make sure we always have a terminating newline
	  # so the string searches and while loop behave
	  set(working_file "${HDR_SRC}\n")
	  while(working_file AND NOT STOP_CHECK)
	    string(FIND "${working_file}" "\n" POS)
	    math(EXPR POS "${POS} + 1")
	    string(SUBSTRING "${working_file}" 0 ${POS} HDR_LINE)
	    string(SUBSTRING "${working_file}" ${POS} -1 working_file)
	    if("${HDR_LINE}" MATCHES "${pvhdr}")
	      if("${HDR_LINE}" MATCHES "[# ]+include[ ]+[\"<]+${pvhdr}[\">]+")
		message("    ${puhdr}:${cline}: ${pvhdr}")
		list(APPEND PVT_INC_INSTANCES "  ${puhdr}:${cline}: ${pvhdr}")
		set(STOP_CHECK 1)
	      endif("${HDR_LINE}" MATCHES "[# ]+include[ ]+[\"<]+${pvhdr}[\">]+")
	    endif("${HDR_LINE}" MATCHES "${pvhdr}")
	    math(EXPR cline "${cline} + 1")
	  endwhile(working_file AND NOT STOP_CHECK)

	  # We have a failure - let the global flag know
	  math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
	  set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)
	endif("${HDR_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${pvhdr}[\">]+")
      endif("${HDR_SRC}" MATCHES "${pvhdr}")
    endforeach(pvhdr ${PRIVATE_HEADERS})
      if(check_succeeded)
      endif(check_succeeded)

  endforeach(puhdr ${PUBLIC_HEADERS})

  if(HDR_PRINTED)
    message("")
  endif(HDR_PRINTED)

  foreach(pvhdr ${PRIVATE_HEADERS})
    if(${pvhdr}_check_succeeded)
      message("-> ${pvhdr} header check succeeded")
    endif(${pvhdr}_check_succeeded)
  endforeach(pvhdr ${PRIVATE_HEADERS})

endfunction(public_headers_test)

# There is a pattern for redundant header tests - encode it in a function
function(redundant_headers_test phdr hdrlist)

  set(HDR_PRINTED 0)
  set(check_succeeded 1)
  set(PHDR_FILES)
  define_allsrc_files()
  message("running ${phdr} redundancy check...")

  # Start with all source files and find those that include ${phdr}
  foreach(cfile ${ALLSRCFILES})
    file(READ "${SOURCE_DIR}/${cfile}" FILE_SRC)
    if("${FILE_SRC}" MATCHES "${phdr}")
      if("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${phdr}[\">]+")
	set(PHDR_FILES ${PHDR_FILES} ${cfile})
      endif("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${phdr}[\">]+")
    endif("${FILE_SRC}" MATCHES "${phdr}")
  endforeach(cfile ${ALLSRCFILES})

  # Now we have our working file list - start looking for includes
  foreach(bfile ${PHDR_FILES})

    file(READ "${SOURCE_DIR}/${bfile}" HDR_SRC)

    # The caller told us which headers to look for - do so.
    foreach(shdr ${${hdrlist}})
      if("${HDR_SRC}" MATCHES "${shdr}")
	if("${HDR_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${shdr}[\">]+")
	  if(NOT HDR_PRINTED)
	    message("\nRedundant system header inclusion in file(s) with bio.h included:")
	    set(check_succeeded 0)
	    set(HDR_PRINTED 1)
	  endif(NOT HDR_PRINTED)

	  # Since we know we have a problem, zero in on the exact line number(s) for reporting purposes
	  set(cline 1)
	  set(STOP_CHECK 0)
	  # We need to go with the while loop + substring approach because
	  # file(STRINGS ...) doesn't produce accurate line numbers and has issues
	  # with square brackets.  Make sure we always have a terminating newline
	  # so the string searches and while loop behave
	  set(working_file "${HDR_SRC}\n")
	  while(working_file AND NOT STOP_CHECK)
	    string(FIND "${working_file}" "\n" POS)
	    math(EXPR POS "${POS} + 1")
	    string(SUBSTRING "${working_file}" 0 ${POS} HDR_LINE)
	    string(SUBSTRING "${working_file}" ${POS} -1 working_file)
	    if("${HDR_LINE}" MATCHES "${shdr}")
	      if("${HDR_LINE}" MATCHES "[# ]+include[ ]+[\"<]+${shdr}[\">]+")
		message("    ${bfile}:${cline}: ${shdr}")
		list(APPEND PVT_INC_INSTANCES "  ${bfile}:${cline}: ${shdr}")
		set(STOP_CHECK 1)
	      endif("${HDR_LINE}" MATCHES "[# ]+include[ ]+[\"<]+${shdr}[\">]+")
	    endif("${HDR_LINE}" MATCHES "${shdr}")
	    math(EXPR cline "${cline} + 1")
	  endwhile(working_file AND NOT STOP_CHECK)

	  # Flag failure to top level
	  math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
	  set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)

	endif("${HDR_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${shdr}[\">]+")
      endif("${HDR_SRC}" MATCHES "${shdr}")

    endforeach(shdr ${${hdrlist}})

  endforeach(bfile ${PHDR_FILES})

  if(HDR_PRINTED)
    message("")
  endif(HDR_PRINTED)

  if(check_succeeded)
    message("-> ${phdr} header check succeeded")
  endif(check_succeeded)

endfunction(redundant_headers_test phdr hdrlist)

#######################################################################

function(common_h_order_test)

  set(check_succeeded 1)
  set(HDR_PRINTED 0)
  message("running common.h inclusion order check...")

  # Build the test file set
  set(COMMON_H_FILES)
  define_allsrc_files()
  foreach(cfile ${ALLSRCFILES})
    file(READ "${SOURCE_DIR}/${cfile}" FILE_SRC)
    if("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+|[# ]+include[ ]+<")
      set(COMMON_H_FILES ${COMMON_H_FILES} ${cfile})
    endif("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+|[# ]+include[ ]+<")
  endforeach(cfile ${ALLSRCFILES})
  set(EXEMPT_FILES bnetwork.h bio.h common.h config_win.h pstdint.h uce-dirent.h ttcp.c optionparser.h ${LEXERS})
  foreach(ef ${EXEMPT_FILES})
    list(FILTER COMMON_H_FILES EXCLUDE REGEX ".*${ef}")
  endforeach(ef ${EXEMPT_FILES})
  # Skip files in CMake directory
  list(FILTER COMMON_H_FILES EXCLUDE REGEX ".*misc/CMake/.*")

  # Have candidate files, start processing
  foreach(cfile ${COMMON_H_FILES})

    file(READ "${SOURCE_DIR}/${cfile}" FILE_SRC)

    # There are two cases - have common.h (ordering check) and no common.h (needed)
    if("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+")

      # Before we go line-by-line, try a pattern match
      if("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<].*[# ]+include[ ]+[\"<]+common.h[\">]+")

	if(NOT HDR_PRINTED)
	  message("\ncommon.h inclusion ordering problem(s):")
	  set(check_succeeded 0)
	  set(HDR_PRINTED 1)
	endif(NOT HDR_PRINTED)

	# Since we know we have a problem, zero in on the exact line number(s) for reporting purposes
	set(cline 1)
	set(SYS_LINE 0)
	set(COMMON_LINE 0)
	# We need to go with the while loop + substring approach because
	# file(STRINGS ...) doesn't produce accurate line numbers and has issues
	# with square brackets.  Make sure we always have a terminating newline
	# so the string searches and while loop behave
	set(working_file "${FILE_SRC}\n")
	while(working_file)
	  # Note that we deliberately don't stop at the first instance of common.h
	  # since there have been occasional instances where common.h was included
	  # multiple times in the same file with the latter inclusion being after
	  # system headers.  COMMON_LINE should be the *last* line with common.h,
	  # in cases where it is not the only line.
	  string(FIND "${working_file}" "\n" POS)
	  math(EXPR POS "${POS} + 1")
	  string(SUBSTRING "${working_file}" 0 ${POS} FILE_LINE)
	  string(SUBSTRING "${working_file}" ${POS} -1 working_file)
	  if(NOT SYS_LINE AND "${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	    set(SYS_LINE ${cline})
	  else(NOT SYS_LINE AND "${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	    if("${FILE_LINE}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+")
	      set(COMMON_LINE ${cline})
	    endif("${FILE_LINE}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+")
	  endif(NOT SYS_LINE AND "${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	  math(EXPR cline "${cline} + 1")
	endwhile(working_file)

	message("  ${cfile} common.h on line ${COMMON_LINE}, system header at line ${SYS_LINE}")

	# Let top level know about failure
	math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
	set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)

      endif("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<].*[# ]+include[ ]+[\"<]+common.h[\">]+")

    else("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+")

      # If a private header is included before the system header, we will
      # assume that header handles the common.h requirement.  Otherwise,
      # there's a problem.
      set(cline 1)
      set(STOP_CHECK 0)
      set(SYS_LINE 0)
      set(PRIV_HDR_FIRST 0)
      # We need to go with the while loop + substring approach because
      # file(STRINGS ...) doesn't produce accurate line numbers and has issues
      # with square brackets.  Make sure we always have a terminating newline
      # so the string searches and while loop behave
      set(working_file "${FILE_SRC}\n")
      while(working_file AND NOT STOP_CHECK)
	string(FIND "${working_file}" "\n" POS)
	math(EXPR POS "${POS} + 1")
	string(SUBSTRING "${working_file}" 0 ${POS} FILE_LINE)
	string(SUBSTRING "${working_file}" ${POS} -1 working_file)
	if(NOT SYS_LINE)
	  if("${FILE_LINE}" MATCHES "[# ]+include[ ]+\"")
	    set(PRIV_HDR_FIRST 1)
	    set(STOP_CHECK 1)
	  else("${FILE_LINE}" MATCHES "[# ]+include[ ]+\"")
	    if("${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	      set(SYS_LINE ${cline})
	      set(STOP_CHECK 1)
	    endif("${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	  endif("${FILE_LINE}" MATCHES "[# ]+include[ ]+\"")
	endif(NOT SYS_LINE)
	math(EXPR cline "${cline} + 1")
      endwhile(working_file AND NOT STOP_CHECK)

      if(NOT HDR_PRINTED AND NOT PRIV_HDR_FIRST)
	message("\ncommon.h inclusion ordering problem(s):")
	set(HDR_PRINTED 1)
      endif(NOT HDR_PRINTED AND NOT PRIV_HDR_FIRST)

      if(NOT PRIV_HDR_FIRST)
	message("  ${cfile} need common.h before system header on line ${SYS_LINE}")

	# Let top level know about failure
	math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
	set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)
      endif(NOT PRIV_HDR_FIRST)

    endif("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+")

  endforeach(cfile ${COMMON_H_FILES})

  if(HDR_PRINTED)
    message("")
  endif(HDR_PRINTED)

  if(check_succeeded)
    message("-> common.h check succeeded")
  endif(check_succeeded)

endfunction(common_h_order_test)

#######################################################################
# API usage

include(CMakeParseArguments)

function(api_usage_test func)

  set(HDR_PRINTED 0)
  set(PHDR_FILES)
  set(cnt 0)
  define_allsrc_files()
  set(FUNC_SRCS ${ALLSRCFILES})

  message("Searching for ${func}...")

  CMAKE_PARSE_ARGUMENTS(FUNC "" "" "EXEMPT" ${ARGN})

  if(FUNC_EXEMPT)
    foreach(fname ${FUNC_EXEMPT})
      list(FILTER FUNC_SRCS EXCLUDE REGEX ".*/${fname}")
    endforeach(fname ${FUNC_EXEMPT})
  endif(FUNC_EXEMPT)

  # Check all source files
  foreach(cfile ${FUNC_SRCS})
    file(READ "${SOURCE_DIR}/${cfile}" FILE_SRC)
    # Adapt C-comment regex from http://blog.ostermiller.org/find-comment for CMake
    # string(REGEX REPLACE "/[*]([^*]|[\r\n]|([*]+([^*/]|[\r\n])))*[*]+/" "" FILE_SRC ${FILE_SRC})
    if("${FILE_SRC}" MATCHES "${func}")
      if("${FILE_SRC}" MATCHES "[^a-zA-Z0-9_:]${func}[(]")

	# Since we know we have a problem, zero in on the exact line number(s) for reporting purposes
	set(cline 1)
	set(SYS_LINE 0)
	set(COMMON_LINE 0)
	# We need to go with the while loop + substring approach because
	# file(STRINGS ...) doesn't produce accurate line numbers and has issues
	# with square brackets.  Make sure we always have a terminating newline
	# so the string searches and while loop behave
	set(working_file "${FILE_SRC}\n")
	while(working_file)
	  string(FIND "${working_file}" "\n" POS)
	  math(EXPR POS "${POS} + 1")
	  string(SUBSTRING "${working_file}" 0 ${POS} FILE_LINE)
	  string(SUBSTRING "${working_file}" ${POS} -1 working_file)
	  if("${FILE_LINE}" MATCHES "${func}")
	    if("${FILE_LINE}" MATCHES "[^a-zA-Z0-9_:]${func}[(]")

	      if(NOT HDR_PRINTED)
		message("Found instance(s):")
		set(HDR_PRINTED 1)
	      endif(NOT HDR_PRINTED)
	      string(FIND "${FILE_LINE}" "\n" POS)
	      string(SUBSTRING "${FILE_LINE}" 0 ${POS} TRIMMED_LINE)
	      message("  ${cfile}:${cline}: ${TRIMMED_LINE}")
	      math(EXPR cnt "${cnt} + 1")

	      # Let top level know about failure
	      math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
	      set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)

	    endif("${FILE_LINE}" MATCHES "[^a-zA-Z0-9_:]${func}[(]")
	  endif("${FILE_LINE}" MATCHES "${func}")
	  math(EXPR cline "${cline} + 1")
	endwhile(working_file)

      endif("${FILE_SRC}" MATCHES "[^a-zA-Z0-9_:]${func}[(]")
    endif("${FILE_SRC}" MATCHES "${func}")
  endforeach(cfile ${FUNC_SRCS})

  math(EXPR api_uses_cnt "${api_uses_cnt} + ${cnt}")
  set(api_uses_cnt ${api_uses_cnt} PARENT_SCOPE)

endfunction(api_usage_test func)

##########################################################################
# Ideally, we don't want to be using WIN32, WIN64, etc. platform specific
# logic in our code - instead, we want to feature test and use those tests
# for conditional logic.

function(platform_symbol_usage_test symb stype expected)

  # Build the source and include file test set
  if("${stype}" STREQUAL "SRC")
    define_allsrc_files()
    set(ACTIVE_SRC_FILES ${ALLSRCFILES})
    set(EXEMPT_FILES pstdint.h uce-dirent.h shapefil.h shpopen.c)
    foreach(ef ${EXEMPT_FILES})
      list(FILTER ACTIVE_SRC_FILES EXCLUDE REGEX ".*${ef}")
    endforeach(ef ${EXEMPT_FILES})
  endif("${stype}" STREQUAL "SRC")

  # Build the build file test set
  if("${stype}" STREQUAL "BLD")
    define_bld_files()
    set(ACTIVE_BLD_FILES ${BLDFILES})
    set(EXEMPT_FILES autoheader.cmake repository.cmake BRLCAD_CMakeFiles.cmake)
    foreach(ef ${EXEMPT_FILES})
      list(FILTER ACTIVE_BLD_FILES EXCLUDE REGEX ".*${ef}")
    endforeach(ef ${EXEMPT_FILES})
  endif("${stype}" STREQUAL "BLD")

  # Check all files, but bookkeep separately for source and build files
  foreach(cfile ${ACTIVE_${stype}_FILES})
    file(READ "${SOURCE_DIR}/${cfile}" FILE_SRC)
    string(TOUPPER "${symb}" symbupper)
    if("${FILE_SRC}" MATCHES "${symb}" OR "${FILE_SRC}" MATCHES "${symbupper}")
      set(regex "[# \t]*(if|IF).*[^A-Z](${symb}|${symbupper})([^A-Z]|\n|\r)")
      if("${FILE_SRC}" MATCHES "${regex}")
	# Since we know we have a problem, zero in on the exact line number(s) for reporting purposes
	set(cline 1)
	# We need to go with the while loop + substring approach because
	# file(STRINGS ...) doesn't produce accurate line numbers and has issues
	# with square brackets.  Make sure we always have a terminating newline
	# so the string searches and while loop behave
	set(working_file "${FILE_SRC}\n")
	set(HAVE_MATCH 1)
	while(working_file AND HAVE_MATCH)
	  string(FIND "${working_file}" "\n" POS)
	  math(EXPR POS "${POS} + 1")
	  string(SUBSTRING "${working_file}" 0 ${POS} FILE_LINE)
	  string(SUBSTRING "${working_file}" ${POS} -1 working_file)
	  if("${FILE_LINE}" MATCHES "${symb}" OR "${FILE_LINE}" MATCHES "${symbupper}")
	    if("${FILE_LINE}" MATCHES "^${regex}")
	      string(FIND "${FILE_LINE}" "\n" POS)
	      string(SUBSTRING "${FILE_LINE}" 0 ${POS} TRIMMED_LINE)
	      list(APPEND ${symb}_${stype}_INSTANCES "${cfile}:${cline}: ${TRIMMED_LINE}")
	    endif("${FILE_LINE}" MATCHES "^${regex}")
	  endif("${FILE_LINE}" MATCHES "${symb}" OR "${FILE_LINE}" MATCHES "${symbupper}")
	  if(NOT "${working_file}" MATCHES "${symb}")
	    set(HAVE_MATCH 0)
	  endif(NOT "${working_file}" MATCHES "${symb}")
	  math(EXPR cline "${cline} + 1")
	endwhile(working_file AND HAVE_MATCH)
      endif("${FILE_SRC}" MATCHES "${regex}")
    endif("${FILE_SRC}" MATCHES "${symb}" OR "${FILE_SRC}" MATCHES "${symbupper}")
  endforeach(cfile ${ACTIVE_${stype}_FILES})

  set(total_cnt 0)
  set(total_${stype}_cnt ${total_${stype}_cnt})
  set(msg)
  if(${symb}_${stype}_INSTANCES)
    list(LENGTH ${symb}_${stype}_INSTANCES symb_len)
    if("${stype}" STREQUAL "SRC")
      set(msg "\nFIXME: Found ${symb_len} instances of ${symb} usage in the source files:\n")
    endif("${stype}" STREQUAL "SRC")
    if("${stype}" STREQUAL "BLD")
      set(msg "\nFIXME: Found ${symb_len} instances of ${symb} usage in the build files:\n")
    endif("${stype}" STREQUAL "BLD")
    foreach(ln ${${symb}_${stype}_INSTANCES})
      set(msg "${msg}\n  ${ln}")
      math(EXPR total_cnt "${total_cnt} + 1")
    endforeach(ln ${${symb}_${stype}_INSTANCES})
  endif(${symb}_${stype}_INSTANCES)
  if(msg)
    message("${msg}")
  endif(msg)
  math(EXPR total_${stype}_cnt "${total_${stype}_cnt} + ${total_cnt}")
  set(total_${stype}_cnt ${total_${stype}_cnt} PARENT_SCOPE)

  if(${total_cnt} GREATER ${expected})
    # Let top level know about failure
    math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
    set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)
    message("\nError: expected ${expected} instances of ${symb}, found ${total_cnt}")
  endif(${total_cnt} GREATER ${expected})
  if(${total_cnt} LESS ${expected})
    message("\nNote: expected ${expected} instances of ${symb}, found ${total_cnt} - update expected value.")
  endif(${total_cnt} LESS ${expected})

endfunction(platform_symbol_usage_test symb expected)


##############################################
# Run tests

# TODO - this should probably be per-header...
# TEST - public/private header inclusion
if(PUBLIC_PRIVATE_HEADER_TEST OR RUN_ALL_TESTS)
  public_headers_test()
endif(PUBLIC_PRIVATE_HEADER_TEST OR RUN_ALL_TESTS)

# TEST: make sure bio.h isn't redundant with system headers
set(BIO_INC_HDRS stdio.h windows.h io.h unistd.h fcntl.h)
if(BIO_REDUNDANT_HEADER_TEST OR RUN_ALL_TESTS)
  redundant_headers_test(bio.h BIO_INC_HDRS)
endif(BIO_REDUNDANT_HEADER_TEST OR RUN_ALL_TESTS)

# TEST: make sure bnetwork.h isn't redundant with system headers
set(BNETWORK_INC_HDRS winsock2.h netinet/in.h netinet/tcp.h  arpa/inet.h)
if(BNETWORK_REDUNDANT_HEADER_TEST OR RUN_ALL_TESTS)
  redundant_headers_test(bnetwork.h BNETWORK_INC_HDRS)
endif(BNETWORK_REDUNDANT_HEADER_TEST OR RUN_ALL_TESTS)

# TEST - make sure common.h is always included first when included
if(COMMON_H_ORDER_TEST OR RUN_ALL_TESTS)
  common_h_order_test()
endif(COMMON_H_ORDER_TEST OR RUN_ALL_TESTS)

# TESTS - api usage
if(API_USAGE_TEST)
  if(API_USAGE_EXEMPT)
    api_usage_test(${API_USAGE_TEST} EXEMPT ${API_USAGE_EXEMPT})
  else(API_USAGE_EXEMPT)
    api_usage_test(${API_USAGE_TEST})
  endif(API_USAGE_EXEMPT)
endif(API_USAGE_TEST)
if(RUN_ALL_TESTS)
  set(api_uses_cnt 0)
  api_usage_test(abort EXEMPT bomb.c log.h pkg.c)
  api_usage_test(dirname EXEMPT bu_dirname.c bu/path.h)
  api_usage_test(fgets)
  api_usage_test(getopt)
  api_usage_test(qsort)
  api_usage_test(remove EXEMPT safileio.c file.c)
  api_usage_test(rmdir)
  api_usage_test(strcasecmp EXEMPT str.c str.h)
  api_usage_test(strcat EXEMPT str.c)
  api_usage_test(strcmp EXEMPT str.c str.h pkg.c)
  api_usage_test(strcpy)
  api_usage_test(strdup EXEMPT str.c)
  api_usage_test(stricmp EXEMPT str.h)
  api_usage_test(strlcat EXEMPT str.c)
  api_usage_test(strlcpy EXEMPT str.c)
  api_usage_test(strncasecmp EXEMPT str.c str.h)
  api_usage_test(strncat EXEMPT str.c)
  api_usage_test(strncmp EXEMPT str.c str.h)
  api_usage_test(strncpy EXEMPT str.c vlc.c rt/db4.h cursor.c wfobj/obj_util.cpp pkg.c ttcp.c)
  api_usage_test(unlink)
  if(NOT api_uses_cnt)
    message("-> API usage check succeeded")
  else(NOT api_uses_cnt)
    message("-> found ${api_uses_cnt} instances of incorrect API usage.")
  endif(NOT api_uses_cnt)
endif(RUN_ALL_TESTS)

# Platform symbols
if(SYMBOL_USAGE_TEST)
  if(SYMBOL_USAGE_EXPECTED)
    platform_symbol_usage_test(${SYMBOL_USAGE_TEST} ${SYMBOL_USAGE_EXPECTED})
  else(SYMBOL_USAGE_EXPECTED)
    platform_symbol_usage_test(${SYMBOL_USAGE_TEST} 0)
  endif(SYMBOL_USAGE_EXPECTED)
endif(SYMBOL_USAGE_TEST)

set(SRC_PLATFORMS
  "AIX:0"
  "APPLE:0"
  "CYGWIN:0"
  "DARWIN:0"
  "FREEBSD:0"
  "HAIKU:0"
  "HPUX:0"
  "LINUX:0"
  "MINGW:0"
  "MSDOS:0"
  "QNX:0"
  "SGI:0"
  "SOLARIS:0"
  "SUN:0"
  "SUNOS:0"
  "SVR4:0"
  "SYSV:0"
  "ULTRIX:0"
  "UNIX:0"
  "VMS:0"
  "WIN16:0"
  "WIN32:0"
  "WIN64:0"
  "WINE:0"
  "WINNT:0"
  )
set(total_SRC_cnt 0)
foreach(ptfm ${SRC_PLATFORMS})
  string(REPLACE ":" ";" ptlist ${ptfm})
  list(GET ptlist 0 plat)
  list(GET ptlist 1 expect)
  if("${SYMBOL_USAGE_TEST_SYS}" STREQUAL "${plat}" OR RUN_ALL_TESTS)
    platform_symbol_usage_test(${plat} SRC ${expect})
  endif("${SYMBOL_USAGE_TEST_SYS}" STREQUAL "${plat}" OR RUN_ALL_TESTS)
endforeach(ptfm ${SRC_PLATFORMS})

if(RUN_ALL_TESTS)
  message("\nTotal symbol count (srcs): ${total_SRC_cnt}\n")
endif(RUN_ALL_TESTS)

set(BLD_PLATFORMS
  "AIX:0"
  "APPLE:0"
  "CYGWIN:0"
  "DARWIN:0"
  "FREEBSD:0"
  "HAIKU:0"
  "HPUX:0"
  "LINUX:0"
  "MINGW:0"
  "MSDOS:0"
  "QNX:0"
  "SGI:0"
  "SOLARIS:0"
  "SUN:0"
  "SUNOS:0"
  "SVR4:0"
  "SYSV:0"
  "ULTRIX:0"
  "UNIX:0"
  "VMS:0"
  "WIN16:0"
  "WIN32:0"
  "WIN64:0"
  "WINE:0"
  "WINNT:0"
  )
set(total_BLD_cnt 0)
foreach(ptfm ${BLD_PLATFORMS})
  string(REPLACE ":" ";" ptlist ${ptfm})
  list(GET ptlist 0 plat)
  list(GET ptlist 1 expect)
  if("${SYMBOL_USAGE_TEST_BLD}" STREQUAL "${plat}" OR RUN_ALL_TESTS)
    platform_symbol_usage_test(${plat} BLD ${expect})
  endif("${SYMBOL_USAGE_TEST_BLD}" STREQUAL "${plat}" OR RUN_ALL_TESTS)
endforeach(ptfm ${BLD_PLATFORMS})

if(RUN_ALL_TESTS)
  message("\nTotal symbol count (build files): ${total_BLD_cnt}\n")
endif(RUN_ALL_TESTS)

if(REPO_CHECK_FAILED)
  message(FATAL_ERROR "\ncheck complete, errors found.")
else(REPO_CHECK_FAILED)
  message("\ncheck complete, no errors found.")
endif(REPO_CHECK_FAILED)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

