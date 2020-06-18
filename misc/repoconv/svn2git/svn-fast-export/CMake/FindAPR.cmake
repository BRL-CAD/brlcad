# - Try to find APR
#
# From https://git.fedorahosted.org/cgit/pki.git/plain/cmake/Modules/FindAPR.cmake
#
# Once done this will define
#
#  APR_FOUND - system has APR
#  APR_INCLUDE_DIRS - the APR include directory
#  APR_LIBRARIES - Link these to use APR
#  APR_DEFINITIONS - Compiler switches required for using APR
#
#  Copyright (c) 2010 Andreas Schneider <asn@redhat.com>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#  1. Redistributions of source code must retain the copyright
#     notice, this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
#  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
#  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
#  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
#  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if (APR_LIBRARIES AND APR_INCLUDE_DIRS)
  # in cache already
  set(APR_FOUND TRUE)
else (APR_LIBRARIES AND APR_INCLUDE_DIRS)
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(_APR apr-1)
  endif (PKG_CONFIG_FOUND)

  find_path(APR_INCLUDE_DIRS
    NAMES
      apr.h
    PATHS
      ${_APR_INCLUDEDIR}
      ${APR_INSTALL_PATH}/include
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
    PATH_SUFFIXES
      apr-1
      apr-1.0
  )

  find_library(APR-1_LIBRARY
    NAMES
      apr-1
    PATHS
      ${APR_INSTALL_PATH}/lib
      ${_APR_LIBDIR}
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )

  if (APR-1_LIBRARY)
    set(APR_LIBRARIES
        ${APR_LIBRARIES}
        ${APR-1_LIBRARY}
    )
  endif (APR-1_LIBRARY)

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(APR DEFAULT_MSG APR_LIBRARIES APR_INCLUDE_DIRS)

  # show the APR_INCLUDE_DIRS and APR_LIBRARIES variables only in the advanced view
  mark_as_advanced(APR_INCLUDE_DIRS APR_LIBRARIES)

endif (APR_LIBRARIES AND APR_INCLUDE_DIRS)
