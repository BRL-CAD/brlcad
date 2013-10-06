#            B R L C A D _ S U M M A R Y . C M A K E
# BRL-CAD
#
# Copyright (c) 2012-2013 United States Government as represented by
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
# This file contains the CMake routines that summarize the results
# of the BRL-CAD configure process.

# Beginning line of summary
message("\n")
if(CMAKE_BUILD_TYPE)
  message("------ BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} - ${CMAKE_BUILD_TYPE} Build  ------")
else(CMAKE_BUILD_TYPE)
  message("------------------- BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} ----------------------")
endif(CMAKE_BUILD_TYPE)
message("\n")

###################################################
#                                                 #
#            Installation Directories             #
#                                                 #
###################################################

# Labels
set(CMAKE_INSTALL_PREFIX_LABEL "Prefix")
set(BIN_DIR_LABEL "Binaries")
set(LIB_DIR_LABEL "Libraries")
set(MAN_DIR_LABEL "Manual pages")
set(DATA_DIR_LABEL "Data resources")
set(PATH_LABELS CMAKE_INSTALL_PREFIX BIN_DIR LIB_DIR MAN_DIR DATA_DIR)

# Initialize length var
set(PATH_LABEL_LENGTH 0)

# Find longest label string for installation directories
foreach(path_label ${PATH_LABELS})
  string(LENGTH "${${path_label}_LABEL}" CURRENT_LENGTH)
  if (${CURRENT_LENGTH} GREATER ${PATH_LABEL_LENGTH})
    set(PATH_LABEL_LENGTH ${CURRENT_LENGTH})
  endif (${CURRENT_LENGTH} GREATER ${PATH_LABEL_LENGTH})
endforeach(path_label ${PATH_LABELS})

# Print each installation directory, adding white space
# as needed to the labels to align them properly
foreach(path_label ${PATH_LABELS})
  set(CURRENT_LABEL ${${path_label}_LABEL})
  string(LENGTH "${CURRENT_LABEL}" CURRENT_LENGTH)
  while(${PATH_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
    set(CURRENT_LABEL " ${CURRENT_LABEL}")
    string(LENGTH "${CURRENT_LABEL}" CURRENT_LENGTH)
  endwhile(${PATH_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
  if(path_label MATCHES "^CMAKE_INSTALL_PREFIX$")
    message("${CURRENT_LABEL}: ${${path_label}}")
  else(path_label MATCHES "^CMAKE_INSTALL_PREFIX$")
    message("${CURRENT_LABEL}: ${CMAKE_INSTALL_PREFIX}/${${path_label}}")
  endif(path_label MATCHES "^CMAKE_INSTALL_PREFIX$")
endforeach()
message(" ")


###################################################
#                                                 #
#                Compiler Flags                   #
#                                                 #
###################################################

set(ALL_FLAG_TYPES C CXX SHARED_LINKER)

# Labels
set(C_LABEL "CFLAGS")
set(CXX_LABEL "CXXFLAGS")
set(SHARED_LINKER_LABEL "LDFLAGS")
set(C_COMPILER_LABEL "CC")
set(CXX_COMPILER_LABEL "CXX")
set(ALL_FLAG_LABELS C_LABEL CXX_LABEL SHARED_LINKER_LABEL C_COMPILER_LABEL CXX_COMPILER_LABEL)

# Initialize length var
set(MAX_LABEL_LENGTH 0)

# Find longest label string for FLAGS
foreach(setting_label ${ALL_FLAG_LABELS})
  string(LENGTH "${${setting_label}}" CURRENT_LENGTH)
  if(${CURRENT_LENGTH} GREATER ${MAX_LABEL_LENGTH})
    set(MAX_LABEL_LENGTH ${CURRENT_LENGTH})
  endif(${CURRENT_LENGTH} GREATER ${MAX_LABEL_LENGTH})
endforeach(setting_label ${ALL_FLAG_LABELS})

# Add spaces to all labels to make their length uniform
foreach(setting_label ${ALL_FLAG_LABELS})
  string(LENGTH "${${setting_label}}" CURRENT_LENGTH)
  while(${MAX_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
    set(${setting_label} "${${setting_label}} ")
    string(LENGTH "${${setting_label}}" CURRENT_LENGTH)
  endwhile(${MAX_LABEL_LENGTH} GREATER ${CURRENT_LENGTH})
endforeach(setting_label ${ALL_FLAG_LABELS})

# Add the equals sign.
foreach(setting_label ${ALL_FLAG_LABELS})
  set(${setting_label} "${${setting_label}} =")
endforeach(setting_label ${ALL_FLAG_LABELS})

# If we're not using MSVC, go ahead and print the compilers
if(NOT MSVC)
  message("${C_COMPILER_LABEL} ${CMAKE_C_COMPILER}")
  message("${CXX_COMPILER_LABEL} ${CMAKE_CXX_COMPILER}")
endif(NOT MSVC)

list(GET ALL_FLAG_LABELS 0 LABEL_LENGTH_STR)
string(LENGTH "${LABEL_LENGTH_STR}" LABEL_LENGTH)

set(MAX_LINE_LENGTH 80)

function(print_flags flag_type flags FLAGS_MAXLINE)
  set(LINE_STR "${${flag_type}_LABEL}")
  string(REPLACE " " ";" ${flag_type}_LIST "${flags}")
  list(LENGTH ${flag_type}_LIST FLAG_CNT)
  while(${FLAG_CNT} GREATER 0)
    string(LENGTH "${LINE_STR}" LINE_LENGTH)
    if(${LINE_LENGTH} STREQUAL "0")
      while(${LINE_LENGTH} LESS ${LABEL_LENGTH})
	set(LINE_STR "${LINE_STR} ")
	string(LENGTH "${LINE_STR}" LINE_LENGTH)
      endwhile(${LINE_LENGTH} LESS ${LABEL_LENGTH})
      set(LINE_STR "${LINE_STR}   ")
    endif(${LINE_LENGTH} STREQUAL "0")
    list(GET ${flag_type}_LIST 0 NEXT_FLAG)
    string(LENGTH ${NEXT_FLAG} FLAG_LENGTH)
    math(EXPR NEW_LINE_LENGTH "${LINE_LENGTH} + ${FLAG_LENGTH} + 1")
    if(${NEW_LINE_LENGTH} LESS ${FLAGS_MAXLINE})
      set(LINE_STR "${LINE_STR} ${NEXT_FLAG}")
      list(REMOVE_AT ${flag_type}_LIST 0)
      list(LENGTH ${flag_type}_LIST FLAG_CNT)
    else(${NEW_LINE_LENGTH} LESS ${FLAGS_MAXLINE})
      message("${LINE_STR}")
      set(LINE_STR "")
    endif(${NEW_LINE_LENGTH} LESS ${FLAGS_MAXLINE})
  endwhile(${FLAG_CNT} GREATER 0)
  if(NOT "${LINE_STR}" STREQUAL "")
    message("${LINE_STR}")
  endif(NOT "${LINE_STR}" STREQUAL "")
endfunction()

if(CMAKE_CONFIGURATION_TYPES)
  message("Flags common to all build configurations:")
  foreach(flag_type ${ALL_FLAG_TYPES})
    print_flags(${flag_type} "${CMAKE_${flag_type}_FLAGS}" ${MAX_LINE_LENGTH})
  endforeach(flag_type ${ALL_FLAG_TYPES})
  message(" ")
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    set(HAVE_EXTRA_FLAGS 0)
    foreach(flag_type ${ALL_FLAG_TYPES})
      if(CMAKE_${flag_type}_FLAGS_${CFG_TYPE_UPPER})
	set(HAVE_EXTRA_FLAGS 1)
      endif(CMAKE_${flag_type}_FLAGS_${CFG_TYPE_UPPER})
    endforeach(flag_type ${ALL_FLAG_TYPES})
    if(HAVE_EXTRA_FLAGS)
      message("Additional Compilation flags used when building with configuration ${CFG_TYPE}:")
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      foreach(flag_type ${ALL_FLAG_TYPES})
	print_flags(${flag_type} "${CMAKE_${flag_type}_FLAGS_${CFG_TYPE_UPPER}}" ${MAX_LINE_LENGTH})
      endforeach(flag_type ${ALL_FLAG_TYPES})
      message(" ")
    endif(HAVE_EXTRA_FLAGS)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
else(CMAKE_CONFIGURATION_TYPES)
  if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
    foreach(flag_type ${ALL_FLAG_TYPES})
      print_flags(${flag_type} "${CMAKE_${flag_type}_FLAGS} ${CMAKE_${flag_type}_FLAGS_${BUILD_TYPE}}" ${MAX_LINE_LENGTH})
    endforeach(flag_type ${ALL_FLAG_TYPES})
    message(" ")
  else(CMAKE_BUILD_TYPE)
    foreach(flag_type ${ALL_FLAG_TYPES})
      print_flags(${flag_type} "${CMAKE_${flag_type}_FLAGS}" ${MAX_LINE_LENGTH})
    endforeach(flag_type ${ALL_FLAG_TYPES})
  endif(CMAKE_BUILD_TYPE)
endif(CMAKE_CONFIGURATION_TYPES)

###################################################
#                                                 #
#   Set up primary report item lists and labels   #
#                                                 #
###################################################

# Build options
set(BRLCAD_TCL_BUILD_LABEL "Compile Tcl ")
set(BRLCAD_TK_BUILD_LABEL "Compile Tk ")
set(BRLCAD_INCRTCL_BUILD_LABEL "Compile Itcl/Itk ")
set(BRLCAD_IWIDGETS_BUILD_LABEL "Compile Iwidgets ")
set(BRLCAD_TKHTML_BUILD_LABEL "Compile Tkhtml ")
set(BRLCAD_TKPNG_BUILD_LABEL "Compile tkpng ")
set(BRLCAD_TKTABLE_BUILD_LABEL "Compile Tktable ")
set(BRLCAD_PNG_BUILD_LABEL "Compile libpng ")
set(BRLCAD_REGEX_BUILD_LABEL "Compile libregex ")
set(BRLCAD_ZLIB_BUILD_LABEL "Compile zlib ")
set(BRLCAD_TERMLIB_BUILD_LABEL "Compile termlib ")
set(BRLCAD_UTAHRLE_BUILD_LABEL "Compile Utah Raster Toolkit ")
set(BRLCAD_OPENNURBS_BUILD_LABEL "Compile openNURBS ")
set(BRLCAD_SCL_BUILD_LABEL "Compile NIST STEP Class Libraries ")
set(BRLCAD_ENABLE_X11_LABEL "X11 support (optional) ")
set(BRLCAD_ENABLE_OPENGL_LABEL "OpenGL support (optional) ")
set(BRLCAD_ENABLE_QT_LABEL "Qt support (optional) ")
set(BRLCAD_ENABLE_RTSERVER_LABEL "librtserver JDK support (optional) ")
set(BRLCAD_ENABLE_RUNTIME_DEBUG_LABEL "Enable run-time debugging (optional) ")
set(BRLCAD_ARCH_BITSETTING_LABEL "Build 32/64-bit release ")
set(BRLCAD_OPTIMIZED_BUILD_LABEL "Build optimized release ")
set(BRLCAD_FLAGS_DEBUG_LABEL "Build debuggable release")
set(BRLCAD_ENABLE_PROFILING_LABEL "Build profile release ")
set(BRLCAD_ENABLE_SMP_LABEL "Build SMP-capable release ")
set(BUILD_STATIC_LIBS_LABEL "Build static libraries ")
set(BUILD_SHARED_LIBS_LABEL "Build dynamic libraries ")
set(BRLCAD_ENABLE_COMPILER_WARNINGS_LABEL "Print verbose compilation warnings ")
set(BRLCAD_ENABLE_VERBOSE_PROGRESS_LABEL "Print verbose compilation progress ")
set(BRLCAD_INSTALL_EXAMPLE_GEOMETRY_LABEL "Install example geometry models ")
set(BRLCAD_DOCBOOK_BUILD_LABEL "Generate extra docs ")
set(ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_LABEL "Build with strict ISO C compliance checking ")
set(ENABLE_POSIX_COMPLIANCE_LABEL "Build with strict POSIX compliance checking ")

# Make sets to use for iteration over all report items
set(BUILD_REPORT_ITEMS
    TCL TK INCRTCL IWIDGETS TKHTML TKPNG TKTABLE PNG REGEX ZLIB
    TERMLIB UTAHRLE OPENNURBS SCL)

set(FEATURE_REPORT_ITEMS
    BRLCAD_ENABLE_X11 BRLCAD_ENABLE_OPENGL BRLCAD_ENABLE_RTSERVER
    BRLCAD_ENABLE_RUNTIME_DEBUG BRLCAD_ENABLE_QT)

set(OTHER_REPORT_ITEMS
    BRLCAD_ARCH_BITSETTING BRLCAD_OPTIMIZED_BUILD
    BRLCAD_FLAGS_DEBUG BRLCAD_ENABLE_PROFILING
    BRLCAD_ENABLE_SMP BUILD_STATIC_LIBS BUILD_SHARED_LIBS
    BRLCAD_ENABLE_COMPILER_WARNINGS BRLCAD_ENABLE_VERBOSE_PROGRESS
    BRLCAD_INSTALL_EXAMPLE_GEOMETRY BRLCAD_DOCBOOK_BUILD
    ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE ENABLE_POSIX_COMPLIANCE
    )

# Construct list of all items
set(ALL_ITEMS)
foreach(item ${BUILD_REPORT_ITEMS})
  set(ALL_ITEMS ${ALL_ITEMS} BRLCAD_${item}_BUILD)
endforeach(item ${BUILD_REPORT_ITEMS})
set(ALL_ITEMS ${ALL_ITEMS} ${FEATURE_REPORT_ITEMS} ${OTHER_REPORT_ITEMS})

# Construct list of all labels
set(ALL_LABELS)
foreach(item ${ALL_ITEMS})
  set(ALL_LABELS ${ALL_LABELS} ${item}_LABEL)
endforeach(item ${ALL_ITEMS})

# Find the maximum label length
set(LABEL_LENGTH 0)
foreach(label ${ALL_LABELS})
  string(LENGTH "${${label}}" CURRENT_LENGTH)
  if(${CURRENT_LENGTH} GREATER ${LABEL_LENGTH})
    set(LABEL_LENGTH ${CURRENT_LENGTH})
  endif(${CURRENT_LENGTH} GREATER ${LABEL_LENGTH})
endforeach(label ${ALL_LABELS})

# Add necessary periods to each label to make a uniform
# label size
foreach(label ${ALL_LABELS})
  string(LENGTH "${${label}}" CURRENT_LENGTH)
  while(${CURRENT_LENGTH} LESS ${LABEL_LENGTH})
    set(${label} "${${label}}.")
    string(LENGTH "${${label}}" CURRENT_LENGTH)
  endwhile(${CURRENT_LENGTH} LESS ${LABEL_LENGTH})
endforeach(label ${ALL_LABELS})

# Add the final element to each label.
foreach(label ${ALL_LABELS})
  set(${label} "${${label}}..:")
endforeach(label ${ALL_LABELS})

###################################################
#                                                 #
#                  Third Party                    #
#                                                 #
###################################################
# The actual build state (as opposed to the AUTO/BUNDLED/SYSTEM setting)
# of the third party libraries is not present in the global cache and
# must be explicitly pulled from src/other
macro(GET_BUILD_STATE ITEM)
  get_directory_property(BRLCAD_${ITEM}_BUILD DIRECTORY src/other DEFINITION BRLCAD_${ITEM}_BUILD)
  get_directory_property(BRLCAD_${ITEM}_NOTFOUND DIRECTORY src/other DEFINITION BRLCAD_${ITEM}_NOTFOUND)
  if("${BRLCAD_${ITEM}_BUILD}" STREQUAL "OFF" AND BRLCAD_${ITEM}_NOTFOUND)
    set(BRLCAD_${ITEM}_BUILD "OFF!")
  endif("${BRLCAD_${ITEM}_BUILD}" STREQUAL "OFF" AND BRLCAD_${ITEM}_NOTFOUND)
endmacro(GET_BUILD_STATE)

# List of components to be reported on.
set(THIRD_PARTY_COMPONENT_LIST ${BUILD_REPORT_ITEMS})
# IncrTcl must be handled separately
list(REMOVE_ITEM THIRD_PARTY_COMPONENT_LIST "INCRTCL")

# Set state messages for standard components
foreach(item ${THIRD_PARTY_COMPONENT_LIST})
  GET_BUILD_STATE(${item})
endforeach(item ${THIRD_PARTY_COMPONENT_LIST})

# IncrTcl is both ITCL and ITK - handle the various possibilities here
get_directory_property(BRLCAD_ITCL_BUILD DIRECTORY src/other DEFINITION	BRLCAD_ITCL_BUILD)
get_directory_property(BRLCAD_ITK_BUILD DIRECTORY src/other DEFINITION BRLCAD_ITK_BUILD)
get_directory_property(BRLCAD_ITCL_NOTFOUND DIRECTORY src/other DEFINITION BRLCAD_ITCL_NOTFOUND)
get_directory_property(BRLCAD_ITK_NOTFOUND DIRECTORY src/other DEFINITION BRLCAD_ITK_NOTFOUND)
if(BRLCAD_ITCL_BUILD AND BRLCAD_ITK_BUILD)
  set(BRLCAD_INCRTCL_BUILD ON)
else(BRLCAD_ITCL_BUILD AND BRLCAD_ITK_BUILD)
  if(BRLCAD_ITCL_BUILD AND NOT BRLCAD_ITK_BUILD)
    set(BRLCAD_INCRTCL_BUILD "ON (Itcl only)")
  endif(BRLCAD_ITCL_BUILD AND NOT BRLCAD_ITK_BUILD)
  if(BRLCAD_ITK_BUILD AND NOT BRLCAD_ITCL_BUILD)
    set(BRLCAD_INCRTCL_BUILD "ON (Itk only)")
  endif(BRLCAD_ITK_BUILD AND NOT BRLCAD_ITCL_BUILD)
  if(NOT BRLCAD_ITCL_BUILD AND NOT BRLCAD_ITK_BUILD)
    if(BRLCAD_ITCL_NOTFOUND OR BRLCAD_ITK_NOTFOUND)
      set(BRLCAD_INCRTCL_BUILD "OFF!")
    else(BRLCAD_ITCL_NOTFOUND OR BRLCAD_ITK_NOTFOUND)
      set(BRLCAD_INCRTCL_BUILD "OFF")
    endif(BRLCAD_ITCL_NOTFOUND OR BRLCAD_ITK_NOTFOUND)
  endif(NOT BRLCAD_ITCL_BUILD AND NOT BRLCAD_ITK_BUILD)
endif(BRLCAD_ITCL_BUILD AND BRLCAD_ITK_BUILD)

foreach(item ${BUILD_REPORT_ITEMS})
  message("${BRLCAD_${item}_BUILD_LABEL} ${BRLCAD_${item}_BUILD}")
endforeach(item ${BUILD_REPORT_ITEMS})

message(" ")

###################################################
#                                                 #
#                    Features                     #
#                                                 #
###################################################

# Note when the word size is automatically set.
if(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
  set(BRLCAD_ARCH_BITSETTING "${CMAKE_WORD_SIZE} (Auto)")
else(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
  set(BRLCAD_ARCH_BITSETTING "${CMAKE_WORD_SIZE}")
endif(${BRLCAD_WORD_SIZE} MATCHES "AUTO")

foreach(item ${FEATURE_REPORT_ITEMS})
  message("${${item}_LABEL} ${${item}}")
endforeach(item ${BUILD_REPORT_ITEMS})

message(" ")

###################################################
#                                                 #
#            Other reportable items               #
#                                                 #
###################################################

# Flesh out the extradocs reporting with format information
set(DOCBOOK_FORMATS "")
if(BRLCAD_EXTRADOCS)
  if(BRLCAD_EXTRADOCS_HTML)
    set(DOCBOOK_FORMATS ${DOCBOOK_FORMATS} html)
  endif(BRLCAD_EXTRADOCS_HTML)
  if(BRLCAD_EXTRADOCS_MAN)
    set(DOCBOOK_FORMATS ${DOCBOOK_FORMATS} man)
  endif(BRLCAD_EXTRADOCS_MAN)
  if (BRLCAD_EXTRADOCS_PDF)
    set(DOCBOOK_FORMATS ${DOCBOOK_FORMATS} pdf)
  endif(BRLCAD_EXTRADOCS_PDF)
  if(DOCBOOK_FORMATS)
    string(REPLACE ";" "/" DOCBOOK_FORMATS "${DOCBOOK_FORMATS}")
    set(BRLCAD_DOCBOOK_BUILD "ON (${DOCBOOK_FORMATS})")
  else(DOCBOOK_FORMATS)
    set(BRLCAD_DOCBOOK_BUILD "ON (All formats disabled)")
  endif(DOCBOOK_FORMATS)
else (BRLCAD_EXTRADOCS)
  set(BRLCAD_DOCBOOK_BUILD "OFF")
endif (BRLCAD_EXTRADOCS)

# In multi-configuration builds, some features are
# set at build time instead of configure time.
set(MULTICONFIG_FEATURES "BRLCAD_OPTIMIZED_BUILD")
if(CMAKE_CONFIGURATION_TYPES)
  foreach(item ${MULTICONFIG_FEATURES})
    set(${item} "Build Configuration Dependent")
  endforeach(item ${MULTICONFIG_FEATURES})
endif(CMAKE_CONFIGURATION_TYPES)

foreach(item ${OTHER_REPORT_ITEMS})
  message("${${item}_LABEL} ${${item}}")
endforeach(item ${BUILD_REPORT_ITEMS})

message(" ")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
