# Minimum required version of CMake
cmake_minimum_required(VERSION 2.8.8)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
  CMAKE_POLICY(SET CMP0007 OLD)
  CMAKE_POLICY(SET CMP0017 OLD)
  if("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0020 OLD)
  endif("${CMAKE_VERSION}" VERSION_GREATER 2.99)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
endif(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(OpenSceneGraph)

set(OPENSCENEGRAPH_MAJOR_VERSION 3)
set(OPENSCENEGRAPH_MINOR_VERSION 2)
set(OPENSCENEGRAPH_PATCH_VERSION 0)
set(OPENSCENEGRAPH_SOVERSION 100)

# set to 0 when not a release candidate, non zero means that any generated
# svn tags will be treated as release candidates of given number
set(OPENSCENEGRAPH_RELEASE_CANDIDATE 0)

set(OPENSCENEGRAPH_VERSION ${OPENSCENEGRAPH_MAJOR_VERSION}.${OPENSCENEGRAPH_MINOR_VERSION}.${OPENSCENEGRAPH_PATCH_VERSION})

set(OSG_PLUGINS osgPlugins-${OPENSCENEGRAPH_VERSION})

set(OSG_PLUGIN_PREFIX "")

if(CYGWIN)
  set(OSG_PLUGIN_PREFIX "cygwin_")
endif()

if(MINGW)
  set(OSG_PLUGIN_PREFIX "mingw_")
endif()

add_definitions(-g)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-as-needed")

# We want to build SONAMES shared libraries
set(OPENSCENEGRAPH_SONAMES TRUE)
set(OPENTHREADS_SONAMES TRUE)

# We have some custom .cmake scripts not in the official distribution.
# Maybe this can be used override existing behavior if needed?
set(CMAKE_MODULE_PATH "${OpenSceneGraph_SOURCE_DIR}/CMakeModules;${CMAKE_MODULE_PATH}")


# Okay, here's the problem: On some platforms, linking against OpenThreads
# is not enough and explicit linking to the underlying thread library
# is also required (e.g. FreeBSD). But OpenThreads may be built with different
# backends (Pthreads, Sproc, Windows) so we don't know what the underlying
# thread library is because some platforms support multiple backends (e.g.
# IRIX supports Sproc and Pthreads). Linking all libraries won't work
# because the libraries may be incompatible.
# So the current solution is to attempt best guess linking and exempt certain
# cases. With IRIX, we're going to hope explicit linking to the underlying
# library is not necessary. We currently don't case for pthreads on Windows
# which might be an issue on things like Cygwin. This may need to be fixed.
if(NOT ANDROID)
  find_package(Threads)
endif()
if(CMAKE_SYSTEM MATCHES IRIX)
  # Erase CMAKE_THREAD_LIBS_INIT and hope it works
  set(CMAKE_THREAD_LIBS_INIT "" CACHE INTERNAL "")
endif()

if(NOT ANDROID)
  if(APPLE)
    # Determine the canonical name of the selected Platform SDK
    execute_process(COMMAND "/usr/bin/sw_vers" "-productVersion"
      OUTPUT_VARIABLE OSG_OSX_SDK_NAME
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(SUBSTRING "${OSG_OSX_SDK_NAME}" 0 4 OSG_OSX_SDK_NAME)
    set(OSG_OSX_SDK_NAME "macosx${OSG_OSX_SDK_NAME}")

    find_library(CARBON_LIBRARY Carbon)
    find_library(COCOA_LIBRARY Cocoa)

    # Apple OS X: Find OpenGL and AGL
    find_package(OpenGL)
    find_library(AGL_LIBRARY AGL)

    option(OSG_COMPILE_FRAMEWORKS "compile frameworks instead of dylibs (experimental)" OFF)
    set(OSG_COMPILE_FRAMEWORKS_INSTALL_NAME_DIR "@executable_path/../Frameworks" CACHE STRING "install name dir for compiled frameworks")
  else()
    # Non-Apple: Find OpenGL
    find_package(OpenGL)
  endif()
endif()

if(UNIX AND NOT ANDROID)
  # Not sure what this will do on Cygwin and Msys
  # Also, remember OS X X11 is a user installed option so it may not exist.
  find_package(X11)
  # Some Unicies need explicit linkage to the Math library or the build fails.
  find_library(MATH_LIBRARY m)

  find_library(DL_LIBRARY dl)
  if(NOT DL_LIBRARY)
    set(DL_LIBRARY "") # change from NOTFOUND to empty when passed to linker
  endif()

  if( CMAKE_SYSTEM MATCHES "Linux" )
    find_library( RT_LIBRARY rt )
  endif( CMAKE_SYSTEM MATCHES "Linux" )

endif()

include_directories(
  ${OpenSceneGraph_BINARY_DIR}/include
  ${OpenSceneGraph_SOURCE_DIR}/include
  ${OPENGL_INCLUDE_DIR}
  )

if(WIN32 AND NOT ANDROID)

  if(MSVC)
    # This option is to enable the /MP switch for Visual Studio 2005 and above compilers
    option(WIN32_USE_MP "Set to ON to build OpenSceneGraph with the /MP option (Visual Studio 2005 and above)." OFF)
    MARK_AS_ADVANCED(WIN32_USE_MP)
    if(WIN32_USE_MP)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    endif(WIN32_USE_MP)

    # turn off various warnings
    # foreach(warning 4244 4251 4267 4275 4290 4786 4305 4996)
    #     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd${warning}")
    # endforeach(warning)

    # This option is to enable the /DYNAMICBASE switch
    # It is used to workaround a bug in Windows 7 when linking in release, which results in corrupt
    # binaries. See this page for details: http://www.wintellect.com/CS/blogs/jrobbins/archive/2009/01/24/the-case-of-the-corrupt-pe-binaries.aspx
    option(WIN32_USE_DYNAMICBASE "Set to ON to build OpenSceneGraph with the /DYNAMICBASE option to work around a bug when linking release executables on Windows 7." OFF)
    MARK_AS_ADVANCED(WIN32_USE_DYNAMICBASE)
    if(WIN32_USE_DYNAMICBASE)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DYNAMICBASE")
    endif(WIN32_USE_DYNAMICBASE)

    # More MSVC specific compilation flags
    ADD_DEFINITIONS(-D_SCL_SECURE_NO_WARNINGS)
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)

    option(MSVC_DISABLE_CHECKED_ITERATORS "Set to ON to disable Visual C++ checked iterators. If you do this you must ensure that every other project in your solution and all dependencies are compiled with _SECURE_SCL=0." OFF)
    MARK_AS_ADVANCED(MSVC_DISABLE_CHECKED_ITERATORS)
    if(MSVC_DISABLE_CHECKED_ITERATORS)
      ADD_DEFINITIONS(-D_SECURE_SCL=0)
    endif(MSVC_DISABLE_CHECKED_ITERATORS)

    option(MSVC_USE_DEFAULT_STACK_SIZE "Set to ON to use the default Visual C++ stack size. CMake forces a high stack size by default, which can cause problems for applications with large number of threads." OFF)
    MARK_AS_ADVANCED(MSVC_USE_DEFAULT_STACK_SIZE)
    if(MSVC_USE_DEFAULT_STACK_SIZE)
      STRING(REGEX REPLACE "/STACK:[0-9]+" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
      STRING(REGEX REPLACE "/STACK:[0-9]+" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
      STRING(REGEX REPLACE "/STACK:[0-9]+" "" CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}")
    endif(MSVC_USE_DEFAULT_STACK_SIZE)
    get_filename_component( CMAKE_MAKE_PROGRAM_NAME ${CMAKE_MAKE_PROGRAM} NAME)
    IF (CMAKE_MAKE_PROGRAM_NAME STREQUAL "VCExpress.exe")
      option(MSVC_BUILD_USE_SOLUTION_FOLDERS "Enable project grouping in VS - VCExpress detected, not supported in VCExpress )" OFF)
    ELSE()
      option(MSVC_BUILD_USE_SOLUTION_FOLDERS "Enable project grouping in VS" ON)
    endif()
    SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ${MSVC_BUILD_USE_SOLUTION_FOLDERS})
  endif()

  #needed for net plugin
  set(OSG_SOCKET_LIBS wsock32)
  # Both Cygwin and Msys need -DNOMINMAX ???
  if(UNIX)
    ADD_DEFINITIONS(-DNOMINMAX)
  endif()

  ########################################################################################################
  # the following options are MSVC specific,
  # the first OSG_MSVC_VERSIONED_DLL activate a custom build-time layout that should allow to run examples and application
  # fron bin folder without requiring installation step.
  # it also prepend "osg${OPENSCENEGRAPH_SOVERSION}-" to only .dll files, leaving .lib files untouched in lib
  # it also use a hack to get rid of Debug and Release folder in MSVC projects
  # all the .dll and .pdb are in bin and all the .lib and .exp are in lib
  #
  # the second option disable incremental linking in debug build , that is enabled by default by CMake
  ##########################################################################################################

  if(MSVC)
    if(${CMAKE_MAJOR_VERSION} EQUAL 2 AND ${CMAKE_MINOR_VERSION} EQUAL 4 AND ${CMAKE_PATCH_VERSION} LESS 7)
      MESSAGE("Warning:  disabling versioned options 2.4.6 exibits inconsintencies in .pdb naming, at least under MSVC, suggested upgrading at least to 2.4.7")
      set(OSG_MSVC_VERSIONED_DLL OFF)
      set(OSG_MSVC_DEBUG_INCREMENTAL_LINK ON)
    ELSE()
      option(OSG_MSVC_VERSIONED_DLL "Set to ON to build OpenSceneGraph with versioned dll names" ON)
      MARK_AS_ADVANCED(OSG_MSVC_VERSIONED_DLL)
      option(OSG_MSVC_DEBUG_INCREMENTAL_LINK "Set to OFF to build OpenSceneGraph without incremental linking in debug (release is off by default)" ON)
      MARK_AS_ADVANCED(OSG_MSVC_DEBUG_INCREMENTAL_LINK)
      if(NOT OSG_MSVC_DEBUG_INCREMENTAL_LINK)
	set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO")
	set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO")
	set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO")
      endif(NOT OSG_MSVC_DEBUG_INCREMENTAL_LINK)
    endif()
  endif(MSVC)
endif(WIN32 AND NOT ANDROID)

option(OSG_NOTIFY_DISABLED "Set to ON to build OpenSceneGraph with the notify() disabled." OFF)

option(OSG_USE_FLOAT_MATRIX "Set to ON to build OpenSceneGraph with float Matrix instead of double." OFF)
MARK_AS_ADVANCED(OSG_USE_FLOAT_MATRIX)

option(OSG_USE_FLOAT_PLANE "Set to ON to build OpenSceneGraph with float Plane instead of double." OFF)
MARK_AS_ADVANCED(OSG_USE_FLOAT_PLANE)

option(OSG_USE_FLOAT_BOUNDINGSPHERE "Set to ON to build OpenSceneGraph with float BoundingSphere instead of double." ON)
MARK_AS_ADVANCED(OSG_USE_FLOAT_BOUNDINGSPHERE)

option(OSG_USE_FLOAT_BOUNDINGBOX "Set to ON to build OpenSceneGraph with float BoundingBox instead of double." ON)
MARK_AS_ADVANCED(OSG_USE_FLOAT_BOUNDINGBOX)

if(WIN32)
  option(OSG_USE_UTF8_FILENAME "Set to ON to use a UTF8 locale for filenames instead of the default locale." OFF)
  MARK_AS_ADVANCED(OSG_USE_UTF8_FILENAME)
endif()

option(OSG_DISABLE_MSVC_WARNINGS "Set to OFF to not disable MSVC warnings generated by OSG headers." ON)
MARK_AS_ADVANCED(OSG_DISABLE_MSVC_WARNINGS)

option(OSG_USE_REF_PTR_IMPLICIT_OUTPUT_CONVERSION "Set to ON to use the ref_ptr<> T* operator() output conversion. " ON)

option(OSG_GL1_AVAILABLE "Set to OFF to disable use of OpenGL 1.x functions library." ON)
option(OSG_GL2_AVAILABLE "Set to OFF to disable use of OpenGL 2.x functions library." ON)
option(OSG_GL3_AVAILABLE "Set to OFF to disable use of OpenGL 3.x functions library." OFF)
option(OSG_GLES1_AVAILABLE "Set to OFF to disable use of OpenGL ES 1.x functions library." OFF)
option(OSG_GLES2_AVAILABLE "Set to OFF to disable use of OpenGL ES 2.x functions library." OFF)
option(OSG_GL_LIBRARY_STATIC "Set to ON to statically link with OpenGL/GLES library." OFF)

set(OPENGL_egl_LIBRARY CACHE STRING "Set the OpenGL egl library.")

# set(OSG_GL_DISPLAYLISTS_AVAILABLE ${OSG_GL1_AVAILABLE})
# set(OSG_GL_MATRICES_AVAILABLE ${OSG_GL1_AVAILABLE})
# set(OSG_GL_VERTEX_FUNCS_AVAILABLE ${OSG_GL1_AVAILABLE})
# set(OSG_GL_VERTEX_ARRAY_FUNCS_AVAILABLE ${OSG_GL1_AVAILABLE})
# set(OSG_GL_FIXED_FUNCTION_AVAILABLE ${OSG_GL1_AVAILABLE})

option(OSG_GL_DISPLAYLISTS_AVAILABLE "Set to OFF to disable use of OpenGL display lists." ${OSG_GL1_AVAILABLE})
option(OSG_GL_MATRICES_AVAILABLE "Set to OFF to disable use of OpenGL built-in matrices." ${OSG_GL1_AVAILABLE})
option(OSG_GL_VERTEX_FUNCS_AVAILABLE "Set to OFF to disable use of OpenGL vertex functions such as glVertex/glColor etc." ${OSG_GL1_AVAILABLE})
option(OSG_GL_VERTEX_ARRAY_FUNCS_AVAILABLE "Set to OFF to disable use of OpenGL vertex functions such as glVertexPointer/glColorPointer etc." ${OSG_GL1_AVAILABLE})
option(OSG_GL_FIXED_FUNCTION_AVAILABLE "Set to OFF to disable use of OpenGL fixed function pipeline." ${OSG_GL1_AVAILABLE})

option(OSG_CPP_EXCEPTIONS_AVAILABLE "Set to OFF to disable compile of OSG components that use C++ exceptions." ON)

################################################################################
# Set Config file

set(OPENSCENEGRAPH_CONFIG_HEADER "${PROJECT_BINARY_DIR}/include/osg/Config")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/osg/Config.in"
  "${OPENSCENEGRAPH_CONFIG_HEADER}")

set(OPENSCENEGRAPH_VERSION_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/include/osg/Version")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/osg/Version.in"
  "${OPENSCENEGRAPH_VERSION_HEADER}")

# INSTALL_FILES(/include/osg/ FILES "${OPENSCENEGRAPH_CONFIG_HEADER}")


################################################################################
# Set Version Info resource file

if(MSVC)
  set(OPENSCENEGRAPH_VERSIONINFO_RC "${PROJECT_BINARY_DIR}/PlatformSpecifics/Windows/OpenSceneGraphVersionInfo.rc")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/PlatformSpecifics/Windows/OpenSceneGraphVersionInfo.rc.in"
    "${OPENSCENEGRAPH_VERSIONINFO_RC}")
endif()

################################################################################
# 3rd Party Dependency Stuff
if(WIN32 AND NOT ANDROID)
  include(Find3rdPartyDependencies)
endif()

find_package(FreeType)
find_package(ZLIB)
find_package(DirectInput)
if(NOT WIN32)
  find_package(Asio)
endif()
find_package(ZeroConf)

option(OSG_USE_QT "Enable to use Qt (build Qt-dependent libraries, plugins and examples)" ON)

if(OSG_USE_QT)
  # To select a specific version of QT define DESIRED_QT_VERSION
  # via cmake -DDESIRED_QT_VERSION=5
  # QUIET option disables messages if the package cannot be found.

  if(DESIRED_QT_VERSION)
    if(DESIRED_QT_VERSION MATCHES 5)
      find_package(Qt5Widgets)
    else()
      find_package(Qt4)
    endif()
  else()
    find_package(Qt5Widgets QUIET)
    if( NOT Qt5Widgets_FOUND )
      find_package(Qt4)
    endif()
  endif()

  #If we have found Qt5, let's try to top off by getting the webkit as well
  if( Qt5Widgets_FOUND )
    find_package(Qt5WebKitWidgets)
  endif()
endif()

# Platform specific:
# (We can approach this one of two ways. We can try to FIND everything
# and simply check if we found the packages before actually building
# or we can hardcode the cases. The advantage of the former is that
# packages that are installed on platforms that don't require them
# will still get built (presuming no compatibility issues). But this
# also means modules that are redundant may get built. For example,
# OS X doesn't need GIF, JPEG, PNG, TIFF, etc because it uses QuickTime.
# Also, it will clutter the CMake menu with "NOT_FOUND".
# The downside to the latter is that it is harder to build those
# potentially redundant modules.)

# Image readers/writers depend on 3rd party libraries except for OS X which
# can use Quicktime.
if(NOT APPLE)
  find_package(JPEG)
  find_package(PNG)
  find_package(TIFF)
  # QuickTime is required for OS X, but optional for Windows.
  if(WIN32)
    find_package(QuickTime)
  endif()

ELSE()
  find_package(TIFF)
  find_package(QuickTime)
  find_package(QTKit)
  find_package(CoreVideo)
  find_package(CoreMedia)
  find_package(QuartzCore)
  find_package(AVFoundation)
endif()

################################################################################
# Create bin and lib directories if required

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin ${CMAKE_BINARY_DIR}/lib ${CMAKE_BINARY_DIR}/lib/${OSG_PLUGINS})

################################################################################
# Installation stuff
include(${OpenSceneGraph_SOURCE_DIR}/CMakeModules/RPath_Setup.cmake)
include(${OpenSceneGraph_SOURCE_DIR}/CMakeModules/Path_Setup.cmake)

################################################################################
# User Options

# Dynamic vs Static Linking
option(DYNAMIC_OPENSCENEGRAPH "Set to ON to build OpenSceneGraph for dynamic linking.  Use OFF for static." ON)
if(DYNAMIC_OPENSCENEGRAPH)
  set(OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC "SHARED")
else()
  set(OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC "STATIC")
endif()


# OSG Core
add_subdirectory(src)

if(APPLE AND NOT ANDROID)

  # Set defaults for Universal Binaries. We want 32-bit Intel/PPC on 10.4
  # and 32/64-bit Intel/PPC on >= 10.5. Anything <= 10.3 doesn't support.

  # These are just defaults/recommendations, but how we want to build
  # out of the box. But the user needs to be able to change these options.
  # So we must only set the values the first time CMake is run, or we
  # will overwrite any changes the user sets.
  # FORCE is used because the options are not reflected in the UI otherwise.
  # Seems like a good place to add version specific compiler flags too.
  if(NOT OSG_CONFIG_HAS_BEEN_RUN_BEFORE)
    if(${OSG_OSX_SDK_NAME} STREQUAL "macosx10.8")
      set(OSG_DEFAULT_IMAGE_PLUGIN_FOR_OSX "imageio" CACHE STRING "Forced imageio default image plugin for OSX" FORCE)
      # 64 Bit Works, i386,ppc is not supported any more
      set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures for OSX" FORCE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.8 -fvisibility-inlines-hidden" CACHE STRING "Flags used by the compiler during all build types." FORCE)
    elseif(${OSG_OSX_SDK_NAME} STREQUAL "macosx10.7")
      set(OSG_DEFAULT_IMAGE_PLUGIN_FOR_OSX "imageio" CACHE STRING "Forced imageio default image plugin for OSX" FORCE)
      # 64 Bit Works, PPC is not supported any more
      set(CMAKE_OSX_ARCHITECTURES "i386;x86_64" CACHE STRING "Build architectures for OSX" FORCE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.7 -fvisibility-inlines-hidden" CACHE STRING "Flags used by the compiler during all build types." FORCE)
    elseif(${OSG_OSX_SDK_NAME} STREQUAL "macosx10.6" OR ${OSG_OSX_SDK_NAME} STREQUAL "macosx10.5")
      set(OSG_DEFAULT_IMAGE_PLUGIN_FOR_OSX "imageio" CACHE STRING "Forced imageio default image plugin for OSX" FORCE)
      # 64-bit compiles are not supported with Carbon.
      set(CMAKE_OSX_ARCHITECTURES "ppc;i386" CACHE STRING "Build architectures for OSX" FORCE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.5 -ftree-vectorize -fvisibility-inlines-hidden" CACHE STRING "Flags used by the compiler during all build types." FORCE)
    elseif(${OSG_OSX_SDK_NAME} STREQUAL "macosx10.4")
      set(OSG_DEFAULT_IMAGE_PLUGIN_FOR_OSX "quicktime" CACHE STRING "Forced imageio default image plugin for OSX" FORCE)
      set(CMAKE_OSX_ARCHITECTURES "ppc;i386" CACHE STRING "Build architectures for OSX" FORCE)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.4 -ftree-vectorize -fvisibility-inlines-hidden" CACHE STRING "Flags used by the compiler during all build types." FORCE)
    else()
      # No Universal Binary support
      # Should break down further to set the -mmacosx-version-min,
      # but the SDK detection is too unreliable here.
    endif()
  endif()

  option(OSG_BUILD_APPLICATION_BUNDLES "Enable the building of applications and examples as OSX Bundles" OFF)

endif()


# This needs to be run very last so other parts of the scripts can take
# advantage of this.
if(NOT OSG_CONFIG_HAS_BEEN_RUN_BEFORE)
  set(OSG_CONFIG_HAS_BEEN_RUN_BEFORE 1 CACHE INTERNAL "Flag to track whether this is the first time running CMake or if CMake has been configured before")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
