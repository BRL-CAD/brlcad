#              B R L C A D _ C P A C K . C M A K E
# BRL-CAD
#
# Copyright (c) 2020-2026 United States Government as represented by
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
configure_file(
  "${BRLCAD_CMAKE_DIR}/source_archive_setup.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/source_archive_setup.cmake"
  @ONLY
)
distclean("${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/source_archive_setup.cmake")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BRL-CAD - a powerful cross-platform open source solid modeling system")
set(CPACK_PACKAGE_VENDOR "BRL-CAD Development Team")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR ${BRLCAD_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${BRLCAD_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${BRLCAD_VERSION_PATCH})

# By default, we want debugging information
set(CPACK_STRIP_FILES FALSE)

# If we're not on Windows, set the install prefix and create
# TGZ and TBZ2 packages by default
if(NOT WIN32)
  set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
  set(CPACK_GENERATOR TGZ TBZ2)
endif(NOT WIN32)

# If we have RPMBUILD and it's not explicitly disabled, assume we're making an RPM.
find_program(RPMBUILD_EXEC rpmbuild)
mark_as_advanced(RPMBUILD_EXEC)
if(RPMBUILD_EXEC AND NOT CPACK_RPM_SKIP)
  # Build an RPM by default if we have the tool.  Out of the box, just
  # build a "vanilla" RPM.  For situations where we want more detail
  # and versioning of the RPM, set the variable CMAKE_RPM_VERSION
  # to the numerical "version" of the RPM: 1, 2, etc..

  # Since RPM packages present a particular problem with bad umask
  # settings and RPM package building is enabled, raise the issue again
  # with a longer wait time.
  if(NOT UMASK_OK)
    message(" ")
    message(
      WARNING
      "umask is set to ${umask_curr} and RPM package building is enabled - this is not a 'standard' umask setting for BRL-CAD RPM packages.  Double check that these umask permissions will have the desired results when installed - RPM packages can impact permissions on system directories such as /usr\nIf the umask settings need to be changed, it is recommended that the build directory be cleared and cmake re-run after the umask setting has been changed."
    )
    if(SLEEP_EXEC)
      execute_process(COMMAND ${SLEEP_EXEC} 5)
    endif(SLEEP_EXEC)
  endif(NOT UMASK_OK)

  set(CPACK_GENERATOR ${CPACK_GENERATOR} RPM)
  set(CPACK_RPM_PACKAGE_LICENSE "LGPL 2.1")
  set(CPACK_RPM_PACKAGE_GROUP "Applications/Engineering")
  set(CPACK_RPM_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")

  # We do NOT want to strip the binaries added to the RPM -
  # see https://cmake.org/Wiki/CMake:CPackPackageGenerators and
  # https://public.kitware.com/Bug/view.php?id=7435
  set(CPACK_RPM_SPEC_INSTALL_POST /bin/true)

  if(CPACK_RPM_VERSION)
    if(DEFINED BRLCAD_VERSION_AMEND)
      set(
        CPACK_RPM_PACKAGE_NAME
        "brlcad_${BRLCAD_VERSION_MAJOR}_${BRLCAD_VERSION_MINOR}_${BRLCAD_VERSION_PATCH}_${BRLCAD_VERSION_AMEND}"
      )
    else(DEFINED BRLCAD_VERSION_AMEND)
      set(CPACK_RPM_PACKAGE_NAME "brlcad_${BRLCAD_VERSION_MAJOR}_${BRLCAD_VERSION_MINOR}_${BRLCAD_VERSION_PATCH}")
    endif(DEFINED BRLCAD_VERSION_AMEND)

    # If we've got a Redhat release, include some info about the
    # specific release in the name.  Otherwise, just go generic.
    if(EXISTS /etc/redhat-release)
      file(READ /etc/redhat-release REDHAT_RELEASE)
      string(REGEX MATCH "[0-9]+" REDHAT_VERSION ${REDHAT_RELEASE})
      string(REGEX MATCH "Enterprise Linux" LINUX_DIST_TYPE ${REDHAT_RELEASE})
      if(LINUX_DIST_TYPE)
        set(LINUX_DIST_TYPE "el")
      else(LINUX_DIST_TYPE)
        set(LINUX_DIST_TYPE "rh")
      endif(LINUX_DIST_TYPE)
      set(CPACK_RPM_PACKAGE_RELEASE ${CPACK_RPM_VERSION}.${LINUX_DIST_TYPE}${REDHAT_VERSION})
    else(EXISTS /etc/redhat-release)
      set(CPACK_RPM_PACKAGE_RELEASE ${CPACK_RPM_VERSION})
    endif(EXISTS /etc/redhat-release)
  endif(CPACK_RPM_VERSION)
endif(RPMBUILD_EXEC AND NOT CPACK_RPM_SKIP)

if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${CMAKE_SYSTEM_NAME}_x86")
else(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
  set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}")
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
if(CPACK_RPM_PACKAGE_RELEASE)
  set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}_${CPACK_RPM_PACKAGE_RELEASE}")
endif(CPACK_RPM_PACKAGE_RELEASE)

if(WIN32)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$" OR
      CMAKE_VS_PLATFORM_NAME MATCHES "^(ARM64|arm64)$")
    set(_brlcad_package_cpu arm64)
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_brlcad_package_cpu x86_64)
  else()
    set(_brlcad_package_cpu x86)
  endif()
  # ZIP is not platform-specific, so identify Windows explicitly. EXE and MSI
  # identify Windows through their extensions and use the shorter HACKING form.
  set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_windows_${_brlcad_package_cpu}")
  set(BRLINST_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_${_brlcad_package_cpu}")
  unset(_brlcad_package_cpu)
  if(BRLINST_HOST_BINARY_DIR)
    # Create all three packages from one External generator staging tree. A
    # separate ZIP generator would repeat the complete install step.
    list(REMOVE_ITEM CPACK_GENERATOR ZIP)
    list(APPEND CPACK_GENERATOR External)
    set(CPACK_EXTERNAL_ENABLE_STAGING ON)
    set(BRLINST_CONFIG "${BRLINST_PRODUCT_CONFIG}")
    set(BRLINST_EXECUTABLE "")
    set(BRLINST_FORMATS exe msi)
    set(BRLINST_ZIP_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}")
    set(BRLI_FILTER_SHORTCUTS "${BRLINST_FILTER_SHORTCUTS}")
    set(_brlinst_package_script "${CMAKE_BINARY_DIR}/BrlinstPackage.cmake")
    configure_file("${CMAKE_SOURCE_DIR}/misc/tools/brlinst/cmake/Package.cmake.in"
      "${_brlinst_package_script}" @ONLY)
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${_brlinst_package_script}")
    distclean("${_brlinst_package_script}")
  endif()
  if(NOT CPACK_GENERATOR)
    # If nothing else, make a zip file
    set(CPACK_GENERATOR ZIP)
  endif(NOT CPACK_GENERATOR)
endif(WIN32)

set(CPACK_SOURCE_GENERATOR TGZ TBZ2 ZIP)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "brlcad-${BRLCAD_VERSION}")
set(CPACK_SOURCE_IGNORE_FILES "\\\\.git/")

configure_file("${BRLCAD_CMAKE_DIR}/BRLCAD_CPackOptions.cmake.in" "${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake" @ONLY)
distclean("${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake")

include(CPack)

distclean("${CMAKE_BINARY_DIR}/CPackConfig.cmake")
distclean("${CMAKE_BINARY_DIR}/CPackSourceConfig.cmake")
distclean("${CMAKE_BINARY_DIR}/DartConfiguration.tcl")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
