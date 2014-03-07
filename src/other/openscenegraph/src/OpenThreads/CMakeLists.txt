# Minimum required version of CMake
cmake_minimum_required(VERSION 2.8.4)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
  CMAKE_POLICY(SET CMP0007 OLD)
  CMAKE_POLICY(SET CMP0017 OLD)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
endif(COMMAND CMAKE_POLICY)

# set CMake project name
project(OPENTHREADS)

set(OPENTHREADS_CMAKE_DIR "${OPENTHREADS_SOURCE_DIR}/CMake")

# This is mainly for Windows declspec, but other platforms know
# what to do with it.
add_definitions(-DOPENTHREADS_EXPORTS)

set(OPENTHREADS_MAJOR_VERSION 3)
set(OPENTHREADS_MINOR_VERSION 2)
set(OPENTHREADS_PATCH_VERSION 0)
set(OPENTHREADS_SOVERSION 13)

set(OPENTHREADS_VERSION ${OPENTHREADS_MAJOR_VERSION}.${OPENTHREADS_MINOR_VERSION}.${OPENTHREADS_PATCH_VERSION})

if(NOT ANDROID)
  include(${OPENTHREADS_CMAKE_DIR}/CheckAtomicOps.cmake)
endif()

# User Options
option(DYNAMIC_OPENTHREADS "Set to ON to build OpenThreads for dynamic linking.  Use OFF for static." ON)
if(DYNAMIC_OPENTHREADS)
  set(OPENTHREADS_USER_DEFINED_DYNAMIC_OR_STATIC "SHARED")
else()
  set(OPENTHREADS_USER_DEFINED_DYNAMIC_OR_STATIC "STATIC")
  set(OT_LIBRARY_STATIC 1)
endif()

################################################################################
# Set Config file

set(OPENTHREADS_CONFIG_HEADER "${OPENTHREADS_BINARY_DIR}/include/OpenThreads/Config")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/common/Config.in"
  "${OPENTHREADS_CONFIG_HEADER}")

set(OPENTHREADS_VERSION_HEADER "${OPENTHREADS_SOURCE_DIR}/include/OpenThreads/Version")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/common/Version.in"
  "${OPENTHREADS_VERSION_HEADER}")


################################################################################
# Set Version Info resource file

if(MSVC)
  set(OPENTHREADS_VERSIONINFO_RC "${OPENTHREADS_BINARY_DIR}/PlatformSpecifics/Windows/OpenThreadsVersionInfo.rc")
  configure_file("${OPENTHREADS_SOURCE_DIR}/PlatformSpecifics/Windows/OpenThreadsVersionInfo.rc.in"
    "${OPENTHREADS_VERSIONINFO_RC}")
endif()

################################################################################

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

set(HEADER_PATH include/OpenThreads)
set(OpenThreads_PUBLIC_HEADERS
  ${HEADER_PATH}/Atomic
  ${HEADER_PATH}/Barrier
  ${HEADER_PATH}/Block
  ${HEADER_PATH}/Condition
  ${HEADER_PATH}/Exports
  ${HEADER_PATH}/Mutex
  ${HEADER_PATH}/ReadWriteMutex
  ${HEADER_PATH}/ReentrantMutex
  ${HEADER_PATH}/ScopedLock
  ${HEADER_PATH}/Thread
  ${HEADER_PATH}/Version
  ${OPENTHREADS_CONFIG_HEADER}
  )

if(NOT ANDROID)
  # Use our modified version of FindThreads.cmake which has Sproc hacks.
  find_package(Threads)
endif()
# Do we have sproc?
if(CMAKE_SYSTEM MATCHES IRIX)
  if(CMAKE_USE_SPROC_INIT)
    # In this case, only Sproc exists, so no option.
    set(OPENTHREADS_USE_SPROC_INSTEAD_OF_PTHREADS 1)
  else()
    if(CMAKE_HAVE_SPROC_H)
      OPTION(OPENTHREADS_USE_SPROC_INSTEAD_OF_PTHREADS "Set to ON to build OpenThreads against sproc instead of pthreads" OFF)
    endif()
  endif()
endif(CMAKE_SYSTEM MATCHES IRIX)

# Add support for using Qt threads
if(QT4_FOUND OR Qt5Widgets_FOUND)
  OPTION(BUILD_OPENTHREADS_WITH_QT "Build OpenThreads with Qt threading support." OFF)
endif()

# Maybe we should be using the FindThreads.cmake module?
if(ANDROID)
  add_subdirectory(pthreads)
elseif((QT4_FOUND OR Qt5Widgets_FOUND) AND BUILD_OPENTHREADS_WITH_QT)
  add_subdirectory(qt)
elseif(WIN32)
  # So I think Cygwin wants to use pthreads
  if(CYGWIN)
    add_subdirectory(pthreads)
  else()
    # Everybody else including Msys should probably go here
    add_subdirectory(win32)
    # examples)
  endif()
elseif(UNIX)
  if(OPENTHREADS_USE_SPROC_INSTEAD_OF_PTHREADS)
    add_subdirectory(sproc)
  else()
    add_subdirectory(pthreads)
  endif()
else()
  message("Sorry, OpenThreads may not support your platform")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
