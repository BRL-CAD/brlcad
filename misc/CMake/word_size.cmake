# Decide whether to do a 32 or a 64 bit build.
set(WORD_SIZE_LABEL "Compile as 32BIT or 64BIT?")
if(NOT BRLCAD_WORD_SIZE)
  set(BRLCAD_WORD_SIZE "AUTO" CACHE STRING WORD_SIZE_LABEL)
endif(NOT BRLCAD_WORD_SIZE)
set_property(CACHE BRLCAD_WORD_SIZE PROPERTY STRINGS AUTO 32BIT 64BIT)
string(TOUPPER "${BRLCAD_WORD_SIZE}" BRLCAD_WORD_SIZE_UPPER)
set(BRLCAD_WORD_SIZE "${BRLCAD_WORD_SIZE_UPPER}" CACHE STRING WORD_SIZE_LABEL FORCE)
if(NOT BRLCAD_WORD_SIZE MATCHES "AUTO" AND NOT BRLCAD_WORD_SIZE MATCHES "64BIT" AND NOT BRLCAD_WORD_SIZE MATCHES "32BIT")
  message(WARNING "Unknown value ${BRLCAD_WORD_SIZE} supplied for BRLCAD_WORD_SIZE - defaulting to AUTO")
  message(WARNING "Valid options are AUTO, 32BIT and 64BIT")
  set(BRLCAD_WORD_SIZE "AUTO" CACHE STRING WORD_SIZE_LABEL FORCE)
endif(NOT BRLCAD_WORD_SIZE MATCHES "AUTO" AND NOT BRLCAD_WORD_SIZE MATCHES "64BIT" AND NOT BRLCAD_WORD_SIZE MATCHES "32BIT")
# On Windows, we can't set word size at CMake configure time - the
# compiler chosen at the beginning dictates the result.  Mark as
# advanced in that situation.
if(MSVC)
  mark_as_advanced(BRLCAD_WORD_SIZE)
endif(MSVC)

# calculate the size of a pointer if we haven't already
CHECK_TYPE_SIZE("void *" CMAKE_SIZEOF_VOID_P)

# still not defined?
if(NOT CMAKE_SIZEOF_VOID_P)
  message(WARNING "CMAKE_SIZEOF_VOID_P is not defined - assuming 32 bit platform")
  set(CMAKE_SIZEOF_VOID_P 4)
endif(NOT CMAKE_SIZEOF_VOID_P)

if(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
  if(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
    set(CMAKE_WORD_SIZE "64BIT")
    set(BRLCAD_WORD_SIZE "64BIT (AUTO)" CACHE STRING WORD_SIZE_LABEL FORCE)
  else(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
    if(${CMAKE_SIZEOF_VOID_P} MATCHES "^4$")
      set(CMAKE_WORD_SIZE "32BIT")
      set(BRLCAD_WORD_SIZE "32BIT (AUTO)" CACHE STRING WORD_SIZE_LABEL FORCE)
    else(${CMAKE_SIZEOF_VOID_P} MATCHES "^4$")
      if(${CMAKE_SIZEOF_VOID_P} MATCHES "^2$")
	set(CMAKE_WORD_SIZE "16BIT")
	set(BRLCAD_WORD_SIZE "16BIT (AUTO)" CACHE STRING WORD_SIZE_LABEL FORCE)
      else(${CMAKE_SIZEOF_VOID_P} MATCHES "^2$")
	set(CMAKE_WORD_SIZE "8BIT")
	set(BRLCAD_WORD_SIZE "8BIT (AUTO)" CACHE STRING WORD_SIZE_LABEL FORCE)
      endif(${CMAKE_SIZEOF_VOID_P} MATCHES "^2$")
    endif(${CMAKE_SIZEOF_VOID_P} MATCHES "^4$")
  endif(${CMAKE_SIZEOF_VOID_P} MATCHES "^8$")
else(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
  set(CMAKE_WORD_SIZE "${BRLCAD_WORD_SIZE}")
endif(${BRLCAD_WORD_SIZE} MATCHES "AUTO")
CONFIG_H_APPEND(BRLCAD "#define SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P}\n")

# Enable/disable 64-bit build settings for MSVC, which is apparently
# determined at the CMake generator level - need to override other
# settings if the compiler disagrees with them.
if(MSVC)
  if(CMAKE_CL_64)
    if(NOT ${CMAKE_WORD_SIZE} MATCHES "64BIT")
      set(CMAKE_WORD_SIZE "64BIT")
      if(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
	message(WARNING "Selected MSVC compiler is 64BIT - setting word size to 64BIT.  To perform a 32BIT MSVC build, select the 32BIT MSVC CMake generator.")
	set(BRLCAD_WORD_SIZE "64BIT" CACHE STRING WORD_SIZE_LABEL FORCE)
      endif(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
    endif(NOT ${CMAKE_WORD_SIZE} MATCHES "64BIT")
    add_definitions("-D_WIN64")
  else(CMAKE_CL_64)
    if(NOT ${CMAKE_WORD_SIZE} MATCHES "32BIT")
      set(CMAKE_WORD_SIZE "32BIT")
      if(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
	message(WARNING "Selected MSVC compiler is 32BIT - setting word size to 32BIT.  To perform a 64BIT MSVC build, select the 64BIT MSVC CMake generator.")
	set(BRLCAD_WORD_SIZE "32BIT" CACHE STRING WORD_SIZE_LABEL FORCE)
      endif(NOT "${BRLCAD_WORD_SIZE}" MATCHES "AUTO")
    endif(NOT ${CMAKE_WORD_SIZE} MATCHES "32BIT")
  endif(CMAKE_CL_64)
endif(MSVC)

if (APPLE)
  if (${CMAKE_WORD_SIZE} MATCHES "32BIT")
    set(CMAKE_OSX_ARCHITECTURES "i386" CACHE STRING "Building for i386" FORCE)
  endif (${CMAKE_WORD_SIZE} MATCHES "32BIT")
endif (APPLE)

# Based on what we are doing, we may need to constrain our search paths
#
# NOTE: Ideally we would set a matching property for 32 bit paths
# on systems that default to 64 bit - as of 2.8.8 CMake doesn't yet
# support FIND_LIBRARY_USE_LIB32_PATHS.  There is a bug report on the
# topic here: http://www.cmake.org/Bug/view.php?id=11260
#
if(${CMAKE_WORD_SIZE} MATCHES "32BIT")
  set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS OFF)
else(${CMAKE_WORD_SIZE} MATCHES "32BIT")
  set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS ON)
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT")

# One of the problems with 32/64 building is we need to search anew
# for 64 bit libs after a 32 bit configure, or vice versa.
if(PREVIOUS_CONFIGURE_TYPE)
  if(NOT ${PREVIOUS_CONFIGURE_TYPE} MATCHES ${CMAKE_WORD_SIZE})
    include("${CMAKE_SOURCE_DIR}/misc/CMake/ResetCache.cmake")
    RESET_CACHE_file()
  endif(NOT ${PREVIOUS_CONFIGURE_TYPE} MATCHES ${CMAKE_WORD_SIZE})
endif(PREVIOUS_CONFIGURE_TYPE)

set(PREVIOUS_CONFIGURE_TYPE ${CMAKE_WORD_SIZE} CACHE STRING "Previous configuration word size" FORCE)
mark_as_advanced(PREVIOUS_CONFIGURE_TYPE)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

