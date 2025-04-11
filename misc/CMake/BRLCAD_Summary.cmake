#            B R L C A D _ S U M M A R Y . C M A K E
# BRL-CAD
#
# Copyright (c) 2012-2025 United States Government as represented by
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

# Given a list of compiler flags, do a line-wrapping printing of
# them
function(print_compiler_flags flag_type flags FLAGS_MAXLINE)
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
    if(NOT "${NEXT_FLAG}" STREQUAL "")
      string(LENGTH ${NEXT_FLAG} FLAG_LENGTH)
      math(EXPR NEW_LINE_LENGTH "${LINE_LENGTH} + ${FLAG_LENGTH} + 1")
      if(${LINE_LENGTH} EQUAL ${LABEL_LENGTH} OR ${NEW_LINE_LENGTH} LESS ${FLAGS_MAXLINE})
        set(LINE_STR "${LINE_STR} ${NEXT_FLAG}")
        list(REMOVE_AT ${flag_type}_LIST 0)
        list(LENGTH ${flag_type}_LIST FLAG_CNT)
      else(${LINE_LENGTH} EQUAL ${LABEL_LENGTH} OR ${NEW_LINE_LENGTH} LESS ${FLAGS_MAXLINE})
        message("${LINE_STR}")
        set(LINE_STR "")
      endif(${LINE_LENGTH} EQUAL ${LABEL_LENGTH} OR ${NEW_LINE_LENGTH} LESS ${FLAGS_MAXLINE})
    else(NOT "${NEXT_FLAG}" STREQUAL "")
      list(REMOVE_AT ${flag_type}_LIST 0)
      list(LENGTH ${flag_type}_LIST FLAG_CNT)
    endif(NOT "${NEXT_FLAG}" STREQUAL "")
  endwhile(${FLAG_CNT} GREATER 0)
  if(NOT "${LINE_STR}" STREQUAL "")
    message("${LINE_STR}")
  endif(NOT "${LINE_STR}" STREQUAL "")
endfunction()

function(BRLCAD_Summary)
  # By default, tailor the summary for an 80 column terminal
  set(MAX_LINE_LENGTH 80)

  ###################################################
  #                                                 #
  #               Print Summary Banner              #
  #                                                 #
  ###################################################

  message("\n")
  if(CMAKE_BUILD_TYPE)
    set(BRLCAD_SUMMARY_BANNER " BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} - ${CMAKE_BUILD_TYPE} Build ")
  else(CMAKE_BUILD_TYPE)
    set(BRLCAD_SUMMARY_BANNER " BRL-CAD Release ${BRLCAD_VERSION}, Build ${CONFIG_DATE} ")
  endif(CMAKE_BUILD_TYPE)

  # Standardize width of summary line
  math(EXPR BANNER_LINE_TRIGGER "${MAX_LINE_LENGTH} - 1")
  string(LENGTH "${BRLCAD_SUMMARY_BANNER}" CURRENT_LENGTH)
  while(${CURRENT_LENGTH} LESS ${BANNER_LINE_TRIGGER})
    set(BRLCAD_SUMMARY_BANNER "-${BRLCAD_SUMMARY_BANNER}-")
    string(LENGTH "${BRLCAD_SUMMARY_BANNER}" CURRENT_LENGTH)
  endwhile(${CURRENT_LENGTH} LESS ${BANNER_LINE_TRIGGER})

  set(BRLCAD_SUMMARY_BANNER "${BRLCAD_SUMMARY_BANNER}\n")

  message("${BRLCAD_SUMMARY_BANNER}")

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
  set(
    PATH_LABELS
    CMAKE_INSTALL_PREFIX
    BIN_DIR
    LIB_DIR
    MAN_DIR
    DATA_DIR
  )

  # Initialize length var
  set(PATH_LABEL_LENGTH 0)

  # Find longest label string for installation directories
  foreach(path_label ${PATH_LABELS})
    string(LENGTH "${${path_label}_LABEL}" CURRENT_LENGTH)
    if(${CURRENT_LENGTH} GREATER ${PATH_LABEL_LENGTH})
      set(PATH_LABEL_LENGTH ${CURRENT_LENGTH})
    endif(${CURRENT_LENGTH} GREATER ${PATH_LABEL_LENGTH})
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
  set(
    ALL_FLAG_LABELS
    C_LABEL
    CXX_LABEL
    SHARED_LINKER_LABEL
    C_COMPILER_LABEL
    CXX_COMPILER_LABEL
  )

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

  if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
    foreach(flag_type ${ALL_FLAG_TYPES})
      print_compiler_flags(${flag_type} "${CMAKE_${flag_type}_FLAGS} ${CMAKE_${flag_type}_FLAGS_${BUILD_TYPE}}" ${MAX_LINE_LENGTH})
    endforeach(flag_type ${ALL_FLAG_TYPES})
    message(" ")
  else(CMAKE_BUILD_TYPE)
    foreach(flag_type ${ALL_FLAG_TYPES})
      print_compiler_flags(${flag_type} "${CMAKE_${flag_type}_FLAGS}" ${MAX_LINE_LENGTH})
    endforeach(flag_type ${ALL_FLAG_TYPES})
  endif(CMAKE_BUILD_TYPE)

  # Spacer between flags and compilation status lists
  message(" ")

  ###################################################
  #                                                 #
  #            Bundled External Libraries           #
  #                                                 #
  ###################################################
  # In current build logic, we're not building the external libs as part of our
  # build anymore.  We have to check the library variables to see if we're
  # using the bundled versions.
  #
  # We define a number of routines to help with printing and managing the
  # dependencies

  set(BUNDLED_LABELS)
  set(BUNDLED_VARS)
  set(BUNDLED_REQUIRED)
  function(EXT_REPORT blabel bvar)
    cmake_parse_arguments(ER "" "REQUIRED_VARS" "" ${ARGN})
    set(BUNDLED_LABELS ${BUNDLED_LABELS} ${blabel} PARENT_SCOPE)
    set(BUNDLED_VARS ${BUNDLED_VARS} ${bvar} PARENT_SCOPE)
    if(ER_REQUIRED_VARS)
      set(BUNDLED_REQUIRED ${BUNDLED_REQUIRED} ${ER_REQUIRED_VARS} PARENT_SCOPE)
    else(ER_REQUIRED_VARS)
      set(BUNDLED_REQUIRED ${BUNDLED_REQUIRED} "VARS_NONE" PARENT_SCOPE)
    endif(ER_REQUIRED_VARS)
  endfunction(EXT_REPORT blabel bvar)

  ext_report("Asset Import Library" ASSIMP_STATUS REQUIRED_VARS "BRLCAD_ENABLE_ASSETIMPORT")
  ext_report("Eigen" EIGEN3_INCLUDE_DIR)
  ext_report("Geogram" GEOGRAM_STATUS)
  ext_report("Geospatial Data Abstraction Library" GDAL_STATUS REQUIRED_VARS "BRLCAD_ENABLE_GDAL")
  ext_report("Lightning Memory-Mapped Database" LMDB_STATUS)
  ext_report("Manifold" MANIFOLD_STATUS)
  ext_report("Netpbm" NETPBM_STATUS)
  ext_report("OpenCV" OPENCV_STATUS)
  ext_report("OpenMesh" OPENMESH_STATUS REQUIRED_VARS "BRLCAD_ENABLE_OPENMESH")
  ext_report("OpenNURBS" OPENNURBS_STATUS)
  ext_report("OSMesa" OSMESA_STATUS)
  ext_report("Portable Network Graphics" PNG_STATUS)

  # Because we may have a Qt5 from the system as well as a Qt6, we need to
  # flatten the two variables for reporting
  set(QtCore_Dir)
  if(Qt6Core_DIR)
    set(QtCore_DIR ${Qt6Core_DIR})
  endif(Qt6Core_DIR)
  if(Qt5Core_DIR)
    set(QtCore_DIR ${Qt5Core_DIR})
  endif(Qt5Core_DIR)

  ext_report("Qt" QtCore_DIR REQUIRED_VARS "BRLCAD_ENABLE_QT")
  ext_report("Regex Library" REGEX_STATUS)
  ext_report("STEPcode" STEPCODE_STATUS REQUIRED_VARS "BRLCAD_ENABLE_STEP")
  ext_report("Tcl" TCL_LIBRARY REQUIRED_VARS "BRLCAD_ENABLE_TCL")
  ext_report("Tk" TK_LIBRARY REQUIRED_VARS "BRLCAD_ENABLE_TCL")
  ext_report("UtahRLE" UTAHRLE_STATUS)
  ext_report("Zlib" ZLIB_LIBRARY)

  # Find the maximum label length
  set(LABEL_LENGTH 0)
  foreach(label ${BUNDLED_LABELS})
    string(LENGTH "${label}" CURRENT_LENGTH)
    if(${CURRENT_LENGTH} GREATER ${LABEL_LENGTH})
      set(LABEL_LENGTH ${CURRENT_LENGTH})
    endif(${CURRENT_LENGTH} GREATER ${LABEL_LENGTH})
  endforeach(label ${BUNDLED_LABELS})

  # Add necessary periods to each label to make a uniform
  # label size
  set(BUNDLED_LABELS_TMP)
  foreach(label ${BUNDLED_LABELS})
    string(LENGTH "${label}" CURRENT_LENGTH)
    while(${CURRENT_LENGTH} LESS ${LABEL_LENGTH})
      set(label "${label}.")
      string(LENGTH "${label}" CURRENT_LENGTH)
    endwhile(${CURRENT_LENGTH} LESS ${LABEL_LENGTH})
    set(label "${label}..:")
    set(BUNDLED_LABELS_TMP ${BUNDLED_LABELS_TMP} ${label})
  endforeach(label ${BUNDLED_LABELS})
  set(BUNDLED_LABELS ${BUNDLED_LABELS_TMP})

  # If we're referencing a local copy of the library,
  # it's getting bundled.  Otherwise we're using a
  # system version
  set(bindex 0)
  foreach(blabel ${BUNDLED_LABELS})
    list(GET BUNDLED_VARS ${bindex} LVAR)
    list(GET BUNDLED_REQUIRED ${bindex} RVARS)
    set(DO_REPORT TRUE)
    if(NOT "${RVARS}" STREQUAL "VARS_NONE")
      # If we've got some enable variables to check, make sure they're active
      # before reporting on this dependency
      foreach(rvar ${RVARS})
        if(NOT ${rvar})
          set(DO_REPORT FALSE)
        endif(NOT ${rvar})
      endforeach(rvar ${RVARS})
    endif(NOT "${RVARS}" STREQUAL "VARS_NONE")
    if(DO_REPORT)
      if(NOT ${LVAR} OR "${${LVAR}}" STREQUAL "NotFound")
        message("${blabel} NotFound")
        math(EXPR bindex "${bindex} + 1")
        continue()
      endif(NOT ${LVAR} OR "${${LVAR}}" STREQUAL "NotFound")
      if("${${LVAR}}" STREQUAL "Bundled")
        message("${blabel} Bundled")
        math(EXPR bindex "${bindex} + 1")
        continue()
      endif("${${LVAR}}" STREQUAL "Bundled")
      if("${${LVAR}}" STREQUAL "System")
        message("${blabel} System")
        math(EXPR bindex "${bindex} + 1")
        continue()
      endif("${${LVAR}}" STREQUAL "System")
      is_subpath("${CMAKE_BINARY_DIR}" "${${LVAR}}" LOCAL_TEST)
      if(LOCAL_TEST)
        message("${blabel} Bundled")
      else(LOCAL_TEST)
        # For header-only dependencies, we don't copy them into the build tree
        # - check for the NOINSTALL directory
        is_subpath("${BRLCAD_EXT_NOINSTALL_DIR}" "${${LVAR}}" EXT_TEST)
        if(EXT_TEST)
          message("${blabel} Bundled")
        else(EXT_TEST)
          message("${blabel} System")
        endif(EXT_TEST)
      endif(LOCAL_TEST)
    endif(DO_REPORT)
    math(EXPR bindex "${bindex} + 1")
  endforeach(blabel ${BUNDLED_LABELS})

  ###################################################
  #                                                 #
  #              Feature settings                   #
  #                                                 #
  ###################################################

  # Build options
  set(BRLCAD_ENABLE_X11_LABEL "X11 support (optional) ")
  set(BRLCAD_ENABLE_OPENGL_LABEL "OpenGL support (optional) ")
  set(BRLCAD_ENABLE_QT_LABEL "Qt support (optional) ")
  set(BRLCAD_ENABLE_RUNTIME_DEBUG_LABEL "Run-time debuggability (optional) ")
  set(BRLCAD_ARCH_BITSETTING_LABEL "Build 32/64-bit release ")
  set(BRLCAD_OPTIMIZED_LABEL "Build with optimization ")
  set(BRLCAD_DEBUGGING_LABEL "Build with debugging symbols ")
  set(BRLCAD_PROFILING_LABEL "Build with performance profiling ")
  set(BRLCAD_SMP_LABEL "Build SMP-capable release ")
  set(BUILD_STATIC_LIBS_LABEL "Build static libraries ")
  set(BUILD_SHARED_LIBS_LABEL "Build dynamic libraries ")
  set(BRLCAD_WARNINGS_LABEL "Print verbose compilation warnings ")
  set(BRLCAD_VERBOSE_LABEL "Print verbose compilation progress ")
  set(BRLCAD_INSTALL_EXAMPLE_GEOMETRY_LABEL "Install example geometry models ")
  set(BRLCAD_DOCBOOK_BUILD_LABEL "Generate extra docs ")
  set(ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_LABEL "Build with strict ISO C compliance checking ")
  set(ENABLE_POSIX_COMPLIANCE_LABEL "Build with strict POSIX compliance checking ")
  set(ENABLE_ALL_CXX_COMPILE_LABEL "Build all C and C++ files with a C++ compiler ")

  # Make sets to use for iteration over all report items
  set(BUILD_REPORT_ITEMS)

  set(FEATURE_REPORT_ITEMS BRLCAD_ENABLE_OPENGL BRLCAD_ENABLE_X11 BRLCAD_ENABLE_QT BRLCAD_ENABLE_RUNTIME_DEBUG)

  set(
    OTHER_REPORT_ITEMS
    BRLCAD_ARCH_BITSETTING
    BRLCAD_OPTIMIZED
    BUILD_STATIC_LIBS
    BUILD_SHARED_LIBS
    BRLCAD_INSTALL_EXAMPLE_GEOMETRY
    BRLCAD_DOCBOOK_BUILD
  )

  if(BRLCAD_SUMMARIZE_DEV_SETTINGS)
    set(
      OTHER_REPORT_ITEMS
      ${OTHER_REPORT_ITEMS}
      BRLCAD_DEBUGGING
      BRLCAD_SMP
      BRLCAD_PROFILING
      BRLCAD_WARNINGS
      BRLCAD_VERBOSE
      ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE
      ENABLE_POSIX_COMPLIANCE
      ENABLE_ALL_CXX_COMPILE
    )
  endif(BRLCAD_SUMMARIZE_DEV_SETTINGS)

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

  message(" ")

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
    if(BRLCAD_EXTRADOCS_PHP)
      set(DOCBOOK_FORMATS ${DOCBOOK_FORMATS} php)
    endif(BRLCAD_EXTRADOCS_PHP)
    if(BRLCAD_EXTRADOCS_PPT)
      set(DOCBOOK_FORMATS ${DOCBOOK_FORMATS} html)
    endif(BRLCAD_EXTRADOCS_PPT)
    if(BRLCAD_EXTRADOCS_MAN)
      set(DOCBOOK_FORMATS ${DOCBOOK_FORMATS} man)
    endif(BRLCAD_EXTRADOCS_MAN)
    if(BRLCAD_EXTRADOCS_PDF)
      set(DOCBOOK_FORMATS ${DOCBOOK_FORMATS} pdf)
    endif(BRLCAD_EXTRADOCS_PDF)
    if(DOCBOOK_FORMATS)
      list(REMOVE_DUPLICATES DOCBOOK_FORMATS)
      string(REPLACE ";" "/" DOCBOOK_FORMATS "${DOCBOOK_FORMATS}")
      set(BRLCAD_DOCBOOK_BUILD "ON (${DOCBOOK_FORMATS})")
    else(DOCBOOK_FORMATS)
      set(BRLCAD_DOCBOOK_BUILD "ON (All formats disabled)")
    endif(DOCBOOK_FORMATS)
  else(BRLCAD_EXTRADOCS)
    set(BRLCAD_DOCBOOK_BUILD "OFF")
  endif(BRLCAD_EXTRADOCS)

  foreach(item ${OTHER_REPORT_ITEMS})
    message("${${item}_LABEL} ${${item}}")
  endforeach(item ${BUILD_REPORT_ITEMS})

  message(" ")
endfunction(BRLCAD_Summary)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
