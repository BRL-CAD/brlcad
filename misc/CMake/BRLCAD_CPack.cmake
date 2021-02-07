#              B R L C A D _ C P A C K . C M A K E
# BRL-CAD
#
# Copyright (c) 2020-2021 United States Government as represented by
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
configure_file("${BRLCAD_CMAKE_DIR}/source_archive_setup.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/source_archive_setup.cmake" @ONLY)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/CMakeTmp/source_archive_setup.cmake")

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
  if (NOT UMASK_OK)
    message(" ")
    message(WARNING "umask is set to ${umask_curr} and RPM package building is enabled - this is not a 'standard' umask setting for BRL-CAD RPM packages.  Double check that these umask permissions will have the desired results when installed - RPM packages can impact permissions on system directories such as /usr\nIf the umask settings need to be changed, it is recommended that the build directory be cleared and cmake re-run after the umask setting has been changed.")
    if(SLEEP_EXEC)
      execute_process(COMMAND ${SLEEP_EXEC} 5)
    endif(SLEEP_EXEC)
  endif (NOT UMASK_OK)

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
      set(CPACK_RPM_PACKAGE_NAME "brlcad_${BRLCAD_VERSION_MAJOR}_${BRLCAD_VERSION_MINOR}_${BRLCAD_VERSION_PATCH}_${BRLCAD_VERSION_AMEND}")
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
  find_package(NSIS)
  if (NSIS_FOUND)
    set(CPACK_GENERATOR ${CPACK_GENERATOR} NSIS)
    set(CPACK_NSIS_PACKAGE_NAME "BRL-CAD")
    set(CPACK_NSIS_INSTALL_DIRECTORY "BRL-CAD ${BRLCAD_VERSION}")
    set(CPACK_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
    set(CPACK_DATA_DIR "${DATA_DIR}")
    set(CPACK_DOC_DIR "${DOC_DIR}")
    # There is a bug in NSIS that does not handle full unix paths properly. Make
    # sure there is at least one set of four (4) backslashes.
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\brlcad.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/misc/nsis\\\\uninstall.ico")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_DISPLAY_NAME "BRL-CAD")
    set(CPACK_NSIS_MODIFY_PATH ON)
  endif (NSIS_FOUND)
  find_package(Wix)
  if (Wix_FOUND)
    # User report that this version of the installer also supports
    # non-graphical installation with the /passive option.
    #
    # Note: for WiX, start menu shortcuts and desktop icons are handled with
    # properties set on targets.  (At the moment, this is not true for NSIS -
    # it uses entries in the misc/CMake/NSIS.template.in file.)
    #
    # If we need to get fancier about this, look at the following:
    # https://github.com/Kitware/CMake/blob/master/CMakeCPackOptions.cmake.in#L216
    # https://github.com/Kitware/CMake/tree/master/Utilities/Release/WiX
    set(CPACK_GENERATOR ${CPACK_GENERATOR} WIX)
    set(CPACK_WIX_LICENSE_RTF "${CMAKE_SOURCE_DIR}/misc/wix/License.rtf")
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/misc/wix/brlcad_product.ico")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "BRL-CAD ${BRLCAD_VERSION}")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/misc/wix/brlcad_banner.bmp")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/misc/wix/brlcad_dialog.bmp")
  endif (Wix_FOUND)
  if (NOT CPACK_GENERATOR)
    # If nothing else, make a zip file
    set(CPACK_GENERATOR ZIP)
  endif (NOT CPACK_GENERATOR)
  if(CMAKE_CL_64)
    set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_win64")
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "BRL-CAD ${BRLCAD_VERSION} win64")
    # Use the setting from http://public.kitware.com/pipermail/cmake/2013-June/055000.html to
    # provide the correct default 64 bit directory with older CMake versions
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
  else(CMAKE_CL_64)
    set(CPACK_PACKAGE_FILE_NAME "BRL-CAD_${BRLCAD_VERSION}_win32")
    set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "BRL-CAD ${BRLCAD_VERSION} win32")
    set(CPACK_NSIS_PACKAGE_NAME "BRL-CAD (32 Bit)")
  endif(CMAKE_CL_64)
endif(WIN32)

set(CPACK_SOURCE_GENERATOR TGZ TBZ2 ZIP)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "brlcad-${BRLCAD_VERSION}")
set(CPACK_SOURCE_IGNORE_FILES "\\\\.svn/")

configure_file("${BRLCAD_CMAKE_DIR}/BRLCAD_CPackOptions.cmake.in" "${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake" @ONLY)
DISTCLEAN("${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/BRLCAD_CPackOptions.cmake")

include(CPack)

DISTCLEAN("${CMAKE_BINARY_DIR}/CPackConfig.cmake")
DISTCLEAN("${CMAKE_BINARY_DIR}/CPackSourceConfig.cmake")
DISTCLEAN("${CMAKE_BINARY_DIR}/DartConfiguration.tcl")


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
