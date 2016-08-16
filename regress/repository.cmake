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

cmake_policy(SET CMP0007 NEW)
if ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)
  cmake_policy(SET CMP0054 NEW)
endif ("${CMAKE_VERSION}" VERSION_GREATER 3.0.9)


# Wrapper for pre-3.6 CMake - once we can require
# 3.6 as a minimum, replace these with
#
# list(FILTER working_list ${mode} REGEX "${pattern}")
#
# when we can require 3.6.
macro(list_FILTER working_list mode matchmode pattern)
  set(remove_list)
  set(listpos 0)
  foreach(line ${${working_list}})
    if("${mode}" STREQUAL "EXCLUDE")
      if("${line}" MATCHES "${pattern}")
	list(APPEND remove_list ${listpos})
      endif("${line}" MATCHES "${pattern}")
    else("${mode}" STREQUAL "EXCLUDE")
      if(NOT "${line}" MATCHES "${pattern}")
	list(APPEND remove_list ${listpos})
      endif(NOT "${line}" MATCHES "${pattern}")
    endif("${mode}" STREQUAL "EXCLUDE")
    math(EXPR listpos "${listpos} + 1")
  endforeach(line ${${working_list}})
  if(remove_list)
    list(REMOVE_AT ${working_list} ${remove_list})
  endif(remove_list)
endmacro(list_FILTER)


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
    list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX "^${REL_BUILD}/.*")
  endif(NOT "${SOURCE_DIR}" STREQUAL "${BUILD_DIR}")
endif(DEFINED BUILD_DIR)

# We don't want any .svn files
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*[.]svn.*")

# Skip 3rd party files
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*src/other.*")
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/tools.*")
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/svn2git.*")
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*/shapelib/.*")
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/macosx/openUp.c")

# Documentation files
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*doc/parsers/templates/.*")

# Test srcs
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/CMake/compat/.*")

# Misc. test and demo files
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/Bullet_Box_Chain_Demo.cpp")
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/opencl-raytracer-tests/.*")

# CMake find modules - often these are convenience inclusions of a 3rd
# party module not yet incorporated into CMake, and given the number of
# times they end up needing to contend with platform specific irregularities
# it doesn't make much sense to police them.
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/CMake/Find.*")

# We aren't interested in temporary files, log files, make
# files, or cache files
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*~")
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*[.]swp")
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*[.]log")
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*Makefile.*")
list_FILTER(FILE_SYSTEM_FILES EXCLUDE REGEX ".*cache$")

# Now, build up lists of files we *do* want consider for
# the various categories

# Source files
macro(define_src_files)
  if(NOT SRCFILES)
    set(SRCFILES ${FILE_SYSTEM_FILES})
    list_FILTER(SRCFILES INCLUDE REGEX ".*[.]c$|.*[.]cpp$|.*[.]cxx$|.*[.]cc$|.*[.]y$|.*[.]yy$|.*[.]l$")
  endif(NOT SRCFILES)
endmacro(define_src_files)

# Include files
macro(define_inc_files)
  if(NOT INCFILES)
    set(INCFILES ${FILE_SYSTEM_FILES})
    list_FILTER(INCFILES INCLUDE REGEX ".*[.]h$|.*[.]hpp$|.*[.]hxx$")
  endif(NOT INCFILES)
endmacro(define_inc_files)

# Build files
macro(define_bld_files)
  if(NOT BLDFILES)
    set(BLDFILES ${FILE_SYSTEM_FILES})
    list_FILTER(BLDFILES INCLUDE REGEX ".*[.]cmake$|.*CMakeLists.txt$|.*[.]cmake.in$")
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
  list_FILTER(PUBLIC_HEADERS INCLUDE REGEX ".*include/.*")

  # Exclude a few that have known issues...
  list_FILTER(PUBLIC_HEADERS EXCLUDE REGEX ".*include/brep.h")
  list_FILTER(PUBLIC_HEADERS EXCLUDE REGEX ".*include/bu/cmd.h")
  list_FILTER(PUBLIC_HEADERS EXCLUDE REGEX ".*include/fb.h")
  list_FILTER(PUBLIC_HEADERS EXCLUDE REGEX ".*include/fb/fb_osgl.h")
  list_FILTER(PUBLIC_HEADERS EXCLUDE REGEX ".*include/fb/fb_wgl.h")

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
    list_FILTER(COMMON_H_FILES EXCLUDE REGEX ".*${ef}")
  endforeach(ef ${EXEMPT_FILES})
  # Skip files in CMake directory
  list_FILTER(COMMON_H_FILES EXCLUDE REGEX ".*misc/CMake/.*")

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
      list_FILTER(FUNC_SRCS EXCLUDE REGEX ".*/${fname}")
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

function(sources_platform_symbol_usage_test inputtype expected)

  message("Searching ${inputtype} files for platform symbols...")

  # Build the source and include file test set
  if("${inputtype}" STREQUAL "source")
    define_src_files()
    set(ACTIVE_SRC_FILES ${SRCFILES})
  endif("${inputtype}" STREQUAL "source")
  if("${inputtype}" STREQUAL "header")
    define_inc_files()
    set(ACTIVE_SRC_FILES ${INCFILES})
  endif("${inputtype}" STREQUAL "header")
  set(EXEMPT_FILES pstdint.h uce-dirent.h)
  foreach(ef ${EXEMPT_FILES})
    list_FILTER(ACTIVE_SRC_FILES EXCLUDE REGEX ".*${ef}")
  endforeach(ef ${EXEMPT_FILES})

  # Check all files
  foreach(cfile ${ACTIVE_SRC_FILES})
    execute_process(COMMAND ${UNIFDEF_EXEC} -P ${cfile} WORKING_DIRECTORY ${SOURCE_DIR} OUTPUT_VARIABLE FILE_REPORT)
    if(FILE_REPORT)
      set(report "${report}\n${FILE_REPORT}")
    endif(FILE_REPORT)
  endforeach(cfile ${ACTIVE_SRC_FILES})

  # If we found anything, we have some reporting to do
  if(report)

    # unifdef will report each line for each symbol on that line - we only want
    # one report per line, so convert to list form and process
    string(REPLACE "\n" ";" report_list "${report}")
    list(SORT report_list)
    list(REMOVE_DUPLICATES report_list)
    list(LENGTH report_list symbol_cnt)

    # Back to string format
    string(REPLACE ";" "\n" report "${report_list}")

    # Final report
    message("\nFIXME: Found ${symbol_cnt} instances Operating System specific symbol usage in the ${inputtype} files:\n")
    message("${report}\n")

    # Based on symbol count, report appropriately
    if("${symbol_cnt}" GREATER "${expected}")
      message("Error: expected ${expected} symbols, found ${symbol_cnt}\n")
      # Let top level know about failure
      math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
      set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)
    else("${symbol_cnt}" GREATER "${expected}")
      if("${symbol_cnt}" LESS "${expected}")
	message("Note: expected ${expected} symbols, found ${symbol_cnt} - need to update expected total.")
      endif("${symbol_cnt}" LESS "${expected}")
      message("-> ${inputtype} platform symbol check succeeded")
    endif("${symbol_cnt}" GREATER "${expected}")

  else(report)

    message("-> ${inputtype} platform symbol check succeeded")

  endif(report)

endfunction(sources_platform_symbol_usage_test)


function(build_files_platform_symbol_usage_test symlist expected)

  # Build the build file test set
  define_bld_files()
  set(ACTIVE_BLD_FILES ${BLDFILES})
  set(EXEMPT_FILES autoheader.cmake repository.cmake BRLCAD_CMakeFiles.cmake)
  foreach(ef ${EXEMPT_FILES})
    list_FILTER(ACTIVE_BLD_FILES EXCLUDE REGEX ".*${ef}")
  endforeach(ef ${EXEMPT_FILES})

  string(REPLACE ";" "|" pcore "${${symlist}}")
  set(platform_regex "(${pcore})")

  # Check all files, but bookkeep separately for source and build files
  # CMake variables are case sensitive, so we only need to check for upper
  # case:  http://public.kitware.com/pipermail/cmake/2009-November/033080.html
  foreach(cfile ${ACTIVE_BLD_FILES})
    file(READ "${SOURCE_DIR}/${cfile}" FILE_SRC)
    if("${FILE_SRC}" MATCHES "${platform_regex}")
      set(regex "[# \t]*(if|IF).*[^A-Z](${pcore})_*([^A-Z]|\n|\r)")
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
	  if("${FILE_LINE}" MATCHES "${platform_regex}")
	    if("${FILE_LINE}" MATCHES "^${regex}")
	      string(FIND "${FILE_LINE}" "\n" POS)
	      string(SUBSTRING "${FILE_LINE}" 0 ${POS} TRIMMED_LINE)
	      list(APPEND BLD_INSTANCES "${cfile}:${cline}: ${TRIMMED_LINE}")
	    endif("${FILE_LINE}" MATCHES "^${regex}")
	  endif("${FILE_LINE}" MATCHES "${platform_regex}")
	  if(NOT "${working_file}" MATCHES "${platform_regex}")
	    set(HAVE_MATCH 0)
	  endif(NOT "${working_file}" MATCHES "${platform_regex}")
	  math(EXPR cline "${cline} + 1")
	endwhile(working_file AND HAVE_MATCH)
      endif("${FILE_SRC}" MATCHES "${regex}")
    endif("${FILE_SRC}" MATCHES "${platform_regex}")
  endforeach(cfile ${ACTIVE_BLD_FILES})

  set(total_cnt 0)
  set(msg)
  if(BLD_INSTANCES)
    list(LENGTH BLD_INSTANCES symb_len)
    list(SORT BLD_INSTANCES)
    set(msg "\nFIXME: Found ${symb_len} instances of platform symbol usage in the build files:\n")
    foreach(ln ${BLD_INSTANCES})
      set(msg "${msg}\n  ${ln}")
      math(EXPR total_cnt "${total_cnt} + 1")
    endforeach(ln ${BLD_INSTANCES})
  endif(BLD_INSTANCES)
  if(msg)
    message("${msg}")
  endif(msg)

  if(${total_cnt} GREATER ${expected})
    # Let top level know about failure
    math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
    set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)
    message("\nError: expected ${expected} instances, found ${total_cnt}")
  endif(${total_cnt} GREATER ${expected})
  if(${total_cnt} LESS ${expected})
    message("\nNote: expected ${expected} instances, found ${total_cnt} - update expected value.")
  endif(${total_cnt} LESS ${expected})

endfunction(build_files_platform_symbol_usage_test)


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

if(RUN_SRCS_PLATFORM_SYMBOLS OR RUN_ALL_TESTS)
  sources_platform_symbol_usage_test(header 0)
  sources_platform_symbol_usage_test(source 0)
endif(RUN_SRCS_PLATFORM_SYMBOLS OR RUN_ALL_TESTS)

set(BLD_PLATFORMS
  AIX
  APPLE
  CYGWIN
  DARWIN
  FREEBSD
  HAIKU
  HPUX
  LINUX
  MINGW
  MSDOS
  QNX
  SGI
  SOLARIS
  SUN
  SUNOS
  SVR4
  SYSV
  ULTRIX
  UNIX
  VMS
  WIN16
  WIN32
  WIN64
  WINE
  WINNT
  )
if(RUN_BLD_PLATFORM_SYMBOLS OR RUN_ALL_TESTS)
  build_files_platform_symbol_usage_test(BLD_PLATFORMS 0)
endif(RUN_BLD_PLATFORM_SYMBOLS OR RUN_ALL_TESTS)

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

