# - Find UUID
# Find the native UUID includes and library
# This module defines
#  UUID_INCLUDE_DIR, where to find jpeglib.h, etc.
#  UUID_LIBRARIES, the libraries needed to use UUID.
#  UUID_FOUND, If false, do not try to use UUID.
# also defined, but not for general use are
#  UUID_LIBRARY, where to find the UUID library.
#
#  Copyright (c) 2006-2011 Mathieu Malaterre <mathieu.malaterre@gmail.com> All
#  rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#  this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright notice,
#  this list of conditions and the following disclaimer in the documentation
#  and/or other materials provided with the distribution.
#
#  3. Neither the name of the copyright holder nor the names of its
#  contributors may be used to endorse or promote products derived from this
#  software without specific prior written permission.

#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.

# On MacOSX we have:
# $ nm -g /usr/lib/libSystem.dylib | grep uuid_generate
# 000b3aeb T _uuid_generate
# 0003e67e T _uuid_generate_random
# 000b37a1 T _uuid_generate_time
if(APPLE)
  set(UUID_LIBRARY_VAR System)
else()
  # Linux type:
  set(UUID_LIBRARY_VAR uuid)
endif()

find_library(UUID_LIBRARY
  NAMES ${UUID_LIBRARY_VAR}
  PATHS /lib /usr/lib /usr/local/lib
  )

# Must be *after* the lib itself
set(CMAKE_FIND_FRAMEWORK_SAVE ${CMAKE_FIND_FRAMEWORK})
set(CMAKE_FIND_FRAMEWORK NEVER)

find_path(UUID_INCLUDE_DIR uuid/uuid.h
  /usr/local/include
  /usr/include
  )

if (UUID_LIBRARY AND UUID_INCLUDE_DIR)
  set(UUID_LIBRARIES ${UUID_LIBRARY})
  set(UUID_FOUND "YES")
else ()
  set(UUID_FOUND "NO")
endif ()


if (UUID_FOUND)
  if (NOT UUID_FIND_QUIETLY)
    message(STATUS "Found UUID: ${UUID_LIBRARIES}")
  endif ()
else ()
  if (UUID_FIND_REQUIRED)
    message( "library: ${UUID_LIBRARY}" )
    message( "include: ${UUID_INCLUDE_DIR}" )
    message(FATAL_ERROR "Could not find UUID library")
  endif ()
endif ()

# Deprecated declarations.
#set (NATIVE_UUID_INCLUDE_PATH ${UUID_INCLUDE_DIR} )
#get_filename_component (NATIVE_UUID_LIB_PATH ${UUID_LIBRARY} PATH)

mark_as_advanced(
  UUID_LIBRARY
  UUID_INCLUDE_DIR
  )
set(CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_SAVE})

# We've done the work - be quiet if called again
set(UUID_FIND_QUIETLY ON CACHE INTERNAL "Quitet FindUUID messages")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

