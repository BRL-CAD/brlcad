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

# Set this if any test fails
set(REPO_CHECK_FAILED 0)

if(NOT DEFINED SOURCE_DIR)
  set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif(NOT DEFINED SOURCE_DIR)

if(NOT EXISTS ${SOURCE_DIR}/include/common.h)
  message(FATAL_ERROR "Unable to find include/common.h in ${SOURCE_DIR}, aborting")
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

# Before we build our source, include and build file lists
# we do some preliminary filtering to remove files we don't
# want in any of those categories

# We don't want any .svn files
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*[.]svn.*")

# Skip 3rd party files
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*src/other.*")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/tools.*")
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/svn2git.*")

# Documentation files
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*doc/parsers/templates/.*")

# Test srcs
list(FILTER FILE_SYSTEM_FILES EXCLUDE REGEX ".*misc/CMake/compat/.*")

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
set(SRCFILES ${FILE_SYSTEM_FILES})
list(FILTER SRCFILES INCLUDE REGEX ".*[.]c$|.*[.]cpp$|.*[.]cxx$|.*[.]cc$|.*[.]h$|.*[.]hpp$|.*[.]hxx$|.*[.]y$|.*[.]yy$|.*[.]l$")

# Include files
# TODO - same initial set as src files?
set(INCFILES ${FILE_SYSTEM_FILES})
list(FILTER INCFILES INCLUDE REGEX ".*[.]c$|.*[.]cpp$|.*[.]cxx$|.*[.]cc$|.*[.]h$|.*[.]hpp$|.*[.]hxx$|.*[.]y$|.*[.]yy$|.*[.]l$")

# Build files
set(BLDFILES ${FILE_SYSTEM_FILES})
list(FILTER BLDFILES INCLUDE REGEX ".*[.]cmake$|.*CMakeLists.txt$|.*[.]cmake.in$")

# All source files
set(ALLSRCFILES ${SRCFILES} ${INCFILES})
list(REMOVE_DUPLICATES ALLSRCFILES)


# Check if public headers are including private headers like bio.h
function(public_headers_test)

  set(HDR_PRINTED 0)

  # Start with all include files
  set(PUBLIC_HEADERS ${INCFILES})
  list(FILTER PUBLIC_HEADERS INCLUDE REGEX ".*include/.*")

  # The list of headers we want to check for 
  set(PRIVATE_HEADERS bio.h bnetwork.h bsocket.h)

  foreach(puhdr ${PUBLIC_HEADERS})

    file(READ "${SOURCE_DIR}/${puhdr}" HDR_SRC)

    foreach(pvhdr ${PRIVATE_HEADERS})
      # If we found an improperly used header, report it
      if("${HDR_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${pvhdr}[\">]+")
	if(NOT HDR_PRINTED)
	  message("\nPrivate header inclusion in public headers:")
	  set(HDR_PRINTED 1)
	endif(NOT HDR_PRINTED)

	# Since we know we have a problem, zero in on the exact
	# line number for reporting purposes
	set(STOP_CHECK 0)
	set(CURR_LINE 1)
	file(STRINGS "${SOURCE_DIR}/${puhdr}" HDR_STRINGS)
	foreach(HDR_LINE ${HDR_STRINGS})
	  if("${HDR_LINE}" MATCHES "[# ]+include[ ]+[\"<]+${pvhdr}[\">]+")
	    message("  ${pvhdr} included in ${puhdr}, line ${CURR_LINE}")
	    set(STOP_CHECK 1)
	  endif("${HDR_LINE}" MATCHES "[# ]+include[ ]+[\"<]+${pvhdr}[\">]+")
	  math(EXPR CURR_LINE "${CURR_LINE} + 1")
	endforeach(HDR_LINE ${HDR_STRINGS})

	# We have a failure - let the global flag know
	math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
	set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)
      endif("${HDR_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${pvhdr}[\">]+")
    endforeach(pvhdr ${PRIVATE_HEADERS})

  endforeach(puhdr ${PUBLIC_HEADERS})

  if(HDR_PRINTED)
    message("")
  endif(HDR_PRINTED)

endfunction(public_headers_test)

# There is a pattern for redundant header tests - encode it in a function
function(redundant_headers_test phdr hdrlist)

  set(HDR_PRINTED 0)
  set(PHDR_FILES)

  # Start with all source files and find those that include ${phdr}
  foreach(cfile ${ALLSRCFILES})
    file(READ "${SOURCE_DIR}/${cfile}" FILE_SRC)
    if("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${phdr}[\">]+")
      set(PHDR_FILES ${PHDR_FILES} ${cfile})
    endif("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${phdr}[\">]+")
  endforeach(cfile ${ALLSRCFILES})

  # Now we have our working file list - start looking for includes
  foreach(bfile ${PHDR_FILES})

    file(READ "${SOURCE_DIR}/${bfile}" HDR_SRC)

    # The caller told us which headers to look for - do so.
    foreach(shdr ${${hdrlist}})
      if("${HDR_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${shdr}[\">]+")
	if(NOT HDR_PRINTED)
	  message("\nRedundant system header inclusion in file(s) with bio.h included:")
	  set(HDR_PRINTED 1)
	endif(NOT HDR_PRINTED)

	# Report with line number information
	set(STOP_CHECK 0)
	set(CURR_LINE 1)
	file(STRINGS "${SOURCE_DIR}/${bfile}" HDR_STRINGS)
	foreach(HDR_LINE ${HDR_STRINGS})
	  if("${HDR_LINE}" MATCHES "[# ]+include[ ]+[\"<]+${shdr}[\">]+")
	    message("  ${shdr} included in ${bfile}, line ${CURR_LINE}")
	    set(STOP_CHECK 1)
	  endif("${HDR_LINE}" MATCHES "[# ]+include[ ]+[\"<]+${shdr}[\">]+")
	  math(EXPR CURR_LINE "${CURR_LINE} + 1")
	endforeach(HDR_LINE ${HDR_STRINGS})

	# Flag failure to top level
	math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
	set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)

      endif("${HDR_SRC}" MATCHES "[# ]+include[ ]+[\"<]+${shdr}[\">]+")

    endforeach(shdr ${${hdrlist}})

  endforeach(bfile ${PHDR_FILES})

  if(HDR_PRINTED)
    message("")
  endif(HDR_PRINTED)
endfunction(redundant_headers_test phdr hdrlist)

function(common_h_order_test)
  set(HDR_PRINTED 0)

  # Build the test file set
  set(COMMON_H_FILES)
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
	  set(HDR_PRINTED 1)
	endif(NOT HDR_PRINTED)

	file(STRINGS "${SOURCE_DIR}/${cfile}" FILE_STRINGS)
	set(SYS_LINE 0)
	set(COMMON_LINE 0)
	set(CURR_LINE 0)
	foreach(FILE_LINE ${FILE_STRINGS})
	  # Note that we deliberately don't stop at the first instance of common.h
	  # since there have been occasional instances where common.h was included
	  # multiple times in the same file with the latter inclusion being after
	  # system headers.  COMMON_LINE should be the *last* line with common.h,
	  # in cases where it is not the only line.
	  math(EXPR CURR_LINE "${CURR_LINE} + 1")
	  if(NOT SYS_LINE AND "${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	    set(SYS_LINE ${CURR_LINE})
	  else(NOT SYS_LINE AND "${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	    if("${FILE_LINE}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+")
	      set(COMMON_LINE ${CURR_LINE})
	    endif("${FILE_LINE}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+")
	  endif(NOT SYS_LINE AND "${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	endforeach(FILE_LINE ${FILE_STRINGS})
	message("  ${cfile} common.h on line ${COMMON_LINE}, system header at line ${SYS_LINE}")

	# Let top level know about failure
	math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
	set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)

      endif("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<].*[# ]+include[ ]+[\"<]+common.h[\">]+")

    else("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+")

      # This time, we just need to know where the first system header is.
      file(STRINGS "${SOURCE_DIR}/${cfile}" FILE_STRINGS)
      set(SYS_LINE 0)
      set(CURR_LINE 0)
      foreach(FILE_LINE ${FILE_STRINGS})
	if(NOT SYS_LINE)
	  math(EXPR CURR_LINE "${CURR_LINE} + 1")
	  if("${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	    set(SYS_LINE ${CURR_LINE})
	  endif("${FILE_LINE}" MATCHES "[# ]+include[ ]+<")
	endif(NOT SYS_LINE)
      endforeach(FILE_LINE ${FILE_STRINGS})

      if(NOT HDR_PRINTED)
	message("\ncommon.h inclusion ordering problem(s):")
	set(HDR_PRINTED 1)
      endif(NOT HDR_PRINTED)
      message("  ${cfile} need common.h before system header on line ${SYS_LINE}")

      # Let top level know about failure
      math(EXPR REPO_CHECK_FAILED "${REPO_CHECK_FAILED} + 1")
      set(REPO_CHECK_FAILED ${REPO_CHECK_FAILED} PARENT_SCOPE)


    endif("${FILE_SRC}" MATCHES "[# ]+include[ ]+[\"<]+common.h[\">]+")
  endforeach(cfile ${COMMON_H_FILES})

  if(HDR_PRINTED)
    message("")
  endif(HDR_PRINTED)

endfunction(common_h_order_test)

# API usage

include(CMakeParseArguments)

function(api_usage_test func)

  set(HDR_PRINTED 0)
  set(PHDR_FILES)
  set(FUNC_SRCS ${ALLSRCFILES})

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
    if("${FILE_SRC}" MATCHES "[^a-zA-Z0-9_:]${func}[(]")
     if("${FILE_SRC}" MATCHES "[^a-zA-Z0-9_:]${func}[(]")
	message("file ${cfile} matches ${func}")
      endif("${FILE_SRC}" MATCHES "[^a-zA-Z0-9_:]${func}[(]")
    endif("${FILE_SRC}" MATCHES "[^a-zA-Z0-9_:]${func}[(]")
  endforeach(cfile ${FUNC_SRCS})

endfunction(api_usage_test func)


# Run tests

# TEST - public/private header inclusion 
public_headers_test()

# TEST: make sure bio.h isn't redundant with system headers
set(BIO_INC_HDRS stdio.h windows.h io.h unistd.h fcntl.h)
redundant_headers_test(bio.h BIO_INC_HDRS)

# TEST: make sure bnetwork.h isn't redundant with system headers
set(BNETWORK_INC_HDRS winsock2.h netinet/in.h netinet/tcp.h  arpa/inet.h)
redundant_headers_test(bnetwork.h BNETWORK_INC_HDRS)

# TEST - make sure common.h is always included first when included
common_h_order_test()

# TESTS - api usage
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


if(REPO_CHECK_FAILED)
  message(FATAL_ERROR "\nRepository check complete, errors found.")
else(REPO_CHECK_FAILED)
  message("\nRepository check complete, no errors found.")
endif(REPO_CHECK_FAILED)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

