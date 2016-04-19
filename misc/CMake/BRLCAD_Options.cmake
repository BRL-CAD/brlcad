#             B R L C A D _ O P T I O N S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2016 United States Government as represented by
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

#-----------------------------------------------------------------------------
# For "top-level" BRL-CAD options, some extra work is in order - descriptions and
# lists of aliases are supplied, and those are automatically addressed by this
# macro.  In this context, "aliases" are variables which may be defined on the
# CMake command line that are intended to set the value of th BRL-CAD option in
# question, but use some other name.  Aliases may be common typos, different
# nomenclature, or anything that the developer things should lead to a given
# option being set.  The documentation is auto-formatted into a text document
# describing all BRL-CAD options.
#
# We also generate the "translation lines" for converting between Autotools'
# configure style option syntax and CMake's variables, and append that to
# the generated configure.new file.

# Handle aliases for BRL-CAD options
macro(OPTION_ALIASES opt opt_ALIASES style)
  if(${opt_ALIASES})
    foreach(item ${${opt_ALIASES}})
      string(REPLACE "ENABLE_" "DISABLE_" inverse_item ${item})
      set(inverse_aliases ${inverse_aliases} ${inverse_item})
    endforeach(item ${${opt_ALIASES}})
    foreach(item ${${opt_ALIASES}})
      if(NOT "${item}" STREQUAL "${opt}")
	if(NOT "${${item}}" STREQUAL "")
	  set(${opt} ${${item}} CACHE STRING "setting ${opt} via alias ${item}" FORCE)
	  set(${item} "" CACHE STRING "set ${opt} via this variable" FORCE)
	  mark_as_advanced(${item})
	endif(NOT "${${item}}" STREQUAL "")
      endif(NOT "${item}" STREQUAL "${opt}")
    endforeach(item ${${opt_ALIASES}})
    # handle DISABLE forms of options
    foreach(item ${inverse_aliases})
      if(NOT "${item}" STREQUAL "${opt}")
	if(NOT "${${item}}" STREQUAL "")
	  set(invert_item ${${item}})
	  if("${${item}}" STREQUAL "ON")
	    set(invert_item "OFF")
	  ELSEif("${invert_item}" STREQUAL "OFF")
	    set(invert_item "ON")
	  endif("${${item}}" STREQUAL "ON")
	  set(${opt} ${invert_item} CACHE STRING "setting ${opt} via alias ${item}" FORCE)
	  set(${item} "" CACHE STRING "set ${opt} via this variable" FORCE)
	  mark_as_advanced(${item})
	endif(NOT "${${item}}" STREQUAL "")
      endif(NOT "${item}" STREQUAL "${opt}")
    endforeach(item ${inverse_aliases})
    foreach(item ${${opt_ALIASES}})
      string(TOLOWER ${item} alias_str)
      string(REPLACE "_" "-" alias_str "${alias_str}")
      string(REPLACE "enable" "disable" disable_str "${alias_str}")
      if("${style}" STREQUAL "ABS")
	file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "     --${alias_str})                options=\"$options -D${opt}=BUNDLED\";\n")
	file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "                                  shift;;\n")
	file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "     --${disable_str})                options=\"$options -D${opt}=SYSTEM\";\n")
	file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "                                  shift;;\n")
      endif("${style}" STREQUAL "ABS")
      if("${style}" STREQUAL "BOOL")
	file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "     --${alias_str})                options=\"$options -D${opt}=ON\";\n")
	file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "                                  shift;;\n")
	file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "     --${disable_str})                options=\"$options -D${opt}=OFF\";\n")
	file(APPEND "${CMAKE_BINARY_DIR}/configure.new" "                                  shift;;\n")
      endif("${style}" STREQUAL "BOOL")
    endforeach(item ${${opt_ALIASES}})
  endif(${opt_ALIASES})
endmacro(OPTION_ALIASES)

# Write documentation description for BRL-CAD options
macro(OPTION_DESCRIPTION opt opt_ALIASES opt_DESCRIPTION)
  file(APPEND "${CMAKE_BINARY_DIR}/OPTIONS" "\n--- ${opt} ---\n")
  file(APPEND "${CMAKE_BINARY_DIR}/OPTIONS" "${${opt_DESCRIPTION}}")

  set(ALIASES_LIST "\nAliases: ")
  foreach(item ${${opt_ALIASES}})
    set(ALIASES_LIST_TEST "${ALIASES_LIST}, ${item}")
    string(LENGTH ${ALIASES_LIST_TEST} LL)

    if(${LL} GREATER 80)
      file(APPEND "${CMAKE_BINARY_DIR}/OPTIONS" "${ALIASES_LIST}\n")
      set(ALIASES_LIST "          ${item}")
    else(${LL} GREATER 80)
      if(NOT ${ALIASES_LIST} MATCHES "\nAliases")
	set(ALIASES_LIST "${ALIASES_LIST}, ${item}")
      else(NOT ${ALIASES_LIST} MATCHES "\nAliases")
	if(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
	  set(ALIASES_LIST "${ALIASES_LIST}, ${item}")
	else(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
	  set(ALIASES_LIST "${ALIASES_LIST} ${item}")
	endif(NOT ${ALIASES_LIST} STREQUAL "\nAliases: ")
      endif(NOT ${ALIASES_LIST} MATCHES "\nAliases")
    endif(${LL} GREATER 80)
  endforeach(item ${${opt_ALIASES}})

  if(ALIASES_LIST)
    string(STRIP ALIASES_LIST ${ALIASES_LIST})
    if(ALIASES_LIST)
      file(APPEND "${CMAKE_BINARY_DIR}/OPTIONS" "${ALIASES_LIST}")
    endif(ALIASES_LIST)
  endif(ALIASES_LIST)

  file(APPEND "${CMAKE_BINARY_DIR}/OPTIONS" "\n\n")
endmacro(OPTION_DESCRIPTION)

# Standard option with BRL-CAD aliases and description
macro(BRLCAD_OPTION default opt opt_ALIASES opt_DESCRIPTION)
  if(NOT DEFINED ${opt} OR "${${opt}}" STREQUAL "")
    if("${default}" STREQUAL "ON" OR "${default}" STREQUAL "YES")
      set(${opt} ON CACHE BOOL "define ${opt}" FORCE)
    ELSEif("${default}" STREQUAL "OFF" OR "${default}" STREQUAL "NO")
      set(${opt} OFF CACHE BOOL "define ${opt}" FORCE)
    else("${default}" STREQUAL "ON" OR "${default}" STREQUAL "YES")
      set(${opt} ${default} CACHE STRING "define ${opt}" FORCE)
    endif("${default}" STREQUAL "ON" OR "${default}" STREQUAL "YES")
  endif(NOT DEFINED ${opt} OR "${${opt}}" STREQUAL "")

  OPTION_ALIASES("${opt}" "${opt_ALIASES}" "BOOL")
  OPTION_DESCRIPTION("${opt}" "${opt_ALIASES}" "${opt_DESCRIPTION}")
endmacro(BRLCAD_OPTION)


# Now we define the various options for BRL-CAD - ways to enable and
# disable features, select which parts of the system to build, etc.
# As much as possible, sane default options are either selected or
# detected.  Because documentation is autogenerated for BRL-CAD
# options, be sure to initialize the file.
set(CONFIG_OPT_STRING "CONFIGURATION OPTIONS\n---------------------\n")
file(WRITE "${CMAKE_BINARY_DIR}/OPTIONS" "${CONFIG_OPT_STRING}")

# The BRL-CAD CMake options system will also generate a configure script that
# emulates the command option style of GNU Autotool's configure.  Write the
# pre-defined header into the build-dir template to initialize the file.
file(REMOVE "${CMAKE_BINARY_DIR}/configure.new")
file(READ "${CMAKE_SOURCE_DIR}/misc/CMake/configure_prefix.sh" CONFIG_PREFIX)
file(WRITE "${CMAKE_BINARY_DIR}/configure.new.tmp" "${CONFIG_PREFIX}")
file(COPY "${CMAKE_BINARY_DIR}/configure.new.tmp" DESTINATION
  "${CMAKE_BINARY_DIR}/CMakeFiles" FILE_PERMISSIONS OWNER_READ OWNER_WRITE
  OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
file(REMOVE "${CMAKE_BINARY_DIR}/configure.new.tmp")
file(RENAME "${CMAKE_BINARY_DIR}/CMakeFiles/configure.new.tmp" "${CMAKE_BINARY_DIR}/configure.new")

# Turn off the brlcad.dll build.
# It's an expert's setting at the moment.
option(BRLCAD_ENABLE_BRLCAD_LIBRARY "Build the brlcad.dll" OFF)
mark_as_advanced(BRLCAD_ENABLE_BRLCAD_LIBRARY)

# Enable Aqua widgets on Mac OSX.  This impacts Tcl/Tk building and OpenGL
# building. Not currently working - needs work in at least Tk
# CMake logic (probably more), and the display manager/framebuffer codes are known to depend
# on either GLX or WGL specifically in their current forms.
option(BRLCAD_ENABLE_AQUA "Use Aqua instead of X11 whenever possible on OSX." OFF)
mark_as_advanced(BRLCAD_ENABLE_AQUA)
if(BRLCAD_ENABLE_AQUA)
  set(DISABLE_X11 1)
endif(BRLCAD_ENABLE_AQUA)

# test for X11 on all platforms since we don't know when/where we'll find it, unless
# we've indicated we *don't* want an X11 build
if(NOT DISABLE_X11)
  include("${CMAKE_SOURCE_DIR}/misc/CMake/FindX11.cmake")
endif(NOT DISABLE_X11)

# Set whether X11 is enabled or disabled by default
if(WIN32)
  # even if there is x11, we default to native
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
elseif(BRLCAD_ENABLE_AQUA)
  # aqua implies no X11
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
else(WIN32)
  # make everywhere else depend on whether we found a suitable X11
  if(X11_X11_LIB AND X11_Xext_LIB AND X11_Xi_LIB AND X11_Xlib_INCLUDE_PATH)
    option(BRLCAD_ENABLE_X11 "Use X11." ON)
  else(X11_X11_LIB AND X11_Xext_LIB AND X11_Xi_LIB AND X11_Xlib_INCLUDE_PATH)
    option(BRLCAD_ENABLE_X11 "Use X11." OFF)
  endif(X11_X11_LIB AND X11_Xext_LIB AND X11_Xi_LIB AND X11_Xlib_INCLUDE_PATH)
endif(WIN32)
mark_as_advanced(BRLCAD_ENABLE_X11)

# if X11 is enabled, make sure aqua is off
if(BRLCAD_ENABLE_X11)
  set(BRLCAD_ENABLE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
  set(OPENGL_USE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
endif(BRLCAD_ENABLE_X11)
mark_as_advanced(OPENGL_USE_AQUA)

# Install example BRL-CAD Geometry Files
option(BRLCAD_INSTALL_EXAMPLE_GEOMETRY "Install the example BRL-CAD geometry files." ON)


# Enable/disable features requiring the Tk toolkit - usually this should
# be on, as a lot of functionality in BRL-CAD depends on Tk
option(BRLCAD_ENABLE_TK "Enable features requiring the Tk toolkit" ON)
mark_as_advanced(BRLCAD_ENABLE_TK)
if(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
  set(BRLCAD_ENABLE_TK OFF)
endif(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
if(BRLCAD_ENABLE_X11)
  set(TK_X11_GRAPHICS ON CACHE STRING "Need X11 Tk" FORCE)
endif(BRLCAD_ENABLE_X11)

# Enable features requiring OPENGL
# Be smart about this - if we don't have X11 or Aqua and we're
# not on Windows, we're non-graphical and that means OpenGL is
# a no-go.  The Windows version would have to be some sort of
# option for the WIN32 graphics layer?  Should probably think
# about that... for now, on Win32 don't try OpenGL if Tk is
# off.  That'll hold until we get a non-Tk based GUI - then
# setting non-graphical on Windows will take more thought.
if(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
  set(OPENGL_FOUND OFF)
  set(BRLCAD_ENABLE_OPENGL OFF CACHE BOOL "Disabled - NOT BRLCAD_ENABLE_X11 and NOT BRLCAD_ENABLE_AQUA" FORCE)
else(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
  include("${CMAKE_SOURCE_DIR}/misc/CMake/FindGL.cmake")
endif(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)

set(BRLCAD_ENABLE_OPENGL_ALIASES
  ENABLE_OPENGL
  )
set(BRLCAD_ENABLE_OPENGL_DESCRIPTION "
Enable support for OpenGL based Display Managers in BRL-CAD.
Default depends on whether OpenGL is successfully detected -
if it is, default is to enable.
")
BRLCAD_OPTION(${OPENGL_FOUND} BRLCAD_ENABLE_OPENGL BRLCAD_ENABLE_OPENGL_ALIASES BRLCAD_ENABLE_OPENGL_DESCRIPTION)

# Enable RTGL.  Requires an enabled OpenGL.
option(BRLCAD_ENABLE_RTGL "Enable experimental RTGL code." OFF)
mark_as_advanced(BRLCAD_ENABLE_RTGL)
if(NOT BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_RTGL)
  message("RTGL requested, but OpenGL is not enabled - disabling")
  set(BRLCAD_ENABLE_RTGL OFF CACHE BOOL "Enable experimental RTGL code." FORCE)
endif(NOT BRLCAD_ENABLE_OPENGL AND BRLCAD_ENABLE_RTGL)
if(NOT BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_RTGL)
  message("RTGL currently works only with GLX, and X11 is not enabled - disabling")
  set(BRLCAD_ENABLE_RTGL OFF CACHE BOOL "Enable experimental RTGL code." FORCE)
endif(NOT BRLCAD_ENABLE_X11 AND BRLCAD_ENABLE_RTGL)
if(BRLCAD_ENABLE_AQUA)
  set(OPENGL_USE_AQUA ON CACHE STRING "Aqua enabled - use Aqua OpenGL" FORCE)
endif(BRLCAD_ENABLE_AQUA)

# Enable features requiring Qt
find_package(Qt5Widgets QUIET)
option(BRLCAD_ENABLE_QT "Enable features requiring Qt" OFF)
mark_as_advanced(BRLCAD_ENABLE_QT)
mark_as_advanced(Qt5Core_DIR)
mark_as_advanced(Qt5Gui_DIR)
if(NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
  message("QT interface requested, but QT5 is not found - disabling")
  set(BRLCAD_ENABLE_QT OFF)
endif(NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
mark_as_advanced(Qt5Widgets_DIR)

# Enable features requiring OpenSceneGraph
option(BRLCAD_ENABLE_OSG "Enable features requiring OpenSceneGraph" OFF)
mark_as_advanced(BRLCAD_ENABLE_OSG)
if(BRLCAD_ENABLE_OSG)
  if(APPLE AND NOT BRLCAD_ENABLE_AQUA)
    set(OSG_WINDOWING_SYSTEM "X11" CACHE STRING "Use X11" FORCE)
  endif(APPLE AND NOT BRLCAD_ENABLE_AQUA)
endif(BRLCAD_ENABLE_OSG)


#----------------------------------------------------------------------
# The following are fine-grained options for enabling/disabling compiler
# and source code definition settings.  Typically these are set to
# various configurations by the toplevel CMAKE_BUILD_TYPE setting, but
# can also be individually set.

# Enable/disable runtime debugging - these are protections for
# minimizing the possibility of corrupted data files.  Generally
# speaking these should be left on.
set(BRLCAD_ENABLE_RUNTIME_DEBUG_ALIASES
  ENABLE_RUNTIME_DEBUG
  ENABLE_RUN_TIME_DEBUG
  ENABLE_RUNTIME_DEBUGGING
  ENABLE_RUN_TIME_DEBUGGING)
set(BRLCAD_ENABLE_RUNTIME_DEBUG_DESCRIPTION "
Enables support for application and library debugging facilities.
Disabling the run-time debugging facilities can provide a significant
(10%-30%) performance boost at the expense of extensive error
checking (that in turn help prevent corruption of your data).
Default is \"ON\", and should only be disabled for read-only render
work where performance is critical.
")
BRLCAD_OPTION(ON BRLCAD_ENABLE_RUNTIME_DEBUG BRLCAD_ENABLE_RUNTIME_DEBUG_ALIASES BRLCAD_ENABLE_RUNTIME_DEBUG_DESCRIPTION)
mark_as_advanced(BRLCAD_ENABLE_RUNTIME_DEBUG)
if(NOT BRLCAD_ENABLE_RUNTIME_DEBUG)
  message("}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
  message("While disabling run-time debugging should increase")
  message("performance, it will likewise remove several")
  message("data-protection safeguards that are in place to")
  message("minimize the possibility of corrupted data files")
  message("in the inevitable event of a user encountering a bug.")
  message("You have been warned.  Proceed at your own risk.")
  message("{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{")
  CONFIG_H_APPEND(BRLCAD "/*Define to not do anything for macros that only bomb on a fatal error. */\n")
  CONFIG_H_APPEND(BRLCAD "#define NO_BOMBING_MACROS 1\n")
  CONFIG_H_APPEND(BRLCAD "/*Define to not perform magic number checking */\n")
  CONFIG_H_APPEND(BRLCAD "#define NO_MAGIC_CHECKING 1\n")
  CONFIG_H_APPEND(BRLCAD "/*Define to not provide run-time debug facilities via RTG.debug */\n")
  CONFIG_H_APPEND(BRLCAD "#define NO_DEBUG_CHECKING 1\n")
endif(NOT BRLCAD_ENABLE_RUNTIME_DEBUG)

# Enable debug flags during compilation - we always want to use these
# unless explicitly told not to.
set(BRLCAD_FLAGS_DEBUG_ALIASES
  ENABLE_DEBUG
  ENABLE_FLAGS_DEBUG
  ENABLE_DEBUG_FLAGS
  )
set(BRLCAD_FLAGS_DEBUG_DESCRIPTION "
Add compiler flags to aid in program debugging.  Defaults to ON.
")
BRLCAD_OPTION(ON BRLCAD_FLAGS_DEBUG BRLCAD_FLAGS_DEBUG_ALIASES BRLCAD_FLAGS_DEBUG_DESCRIPTION)

# A variety of debugging messages in the code key off of the DEBUG
# definition - set it according to whether we're using debug flags.
if(BRLCAD_FLAGS_DEBUG)
  CONFIG_H_APPEND(BRLCAD "#define DEBUG 1\n")
endif(BRLCAD_FLAGS_DEBUG)

# Build with compiler warning flags
set(BRLCAD_ENABLE_COMPILER_WARNINGS_ALIASES
  ENABLE_WARNINGS
  ENABLE_COMPILER_WARNINGS
  )
set(BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION "
Use extra compiler warning flags when compiling C/C++ code.  Defaults to ON.
")
BRLCAD_OPTION(ON BRLCAD_ENABLE_COMPILER_WARNINGS
  BRLCAD_ENABLE_COMPILER_WARNINGS_ALIASES
  BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION)
mark_as_advanced(BRLCAD_ENABLE_COMPILER_WARNINGS)

# Enable/disable strict compiler settings - these are used for building
# BRL-CAD by default, but not src/other code.  Always used for BRL-CAD
# code unless the NO_STRICT option is specified when defining a target
# with BRLCAD_ADDEXEC or BRLCAD_ADDLIB.  If only C++ files in a target
# are not compatible with strict, the NO_STRICT_CXX option can be used.
set(BRLCAD_ENABLE_STRICT_ALIASES
  ENABLE_STRICT
  ENABLE_STRICT_COMPILE
  ENABLE_STRICT_COMPILE_FLAGS)
set(BRLCAD_ENABLE_STRICT_DESCRIPTION "
Causes all compilation warnings for C code to be treated as errors.  This is now
the default for BRL-CAD source code, and developers should address issues
discovered by these flags whenever possible rather than disabling strict
mode.
")
BRLCAD_OPTION(ON BRLCAD_ENABLE_STRICT BRLCAD_ENABLE_STRICT_ALIASES BRLCAD_ENABLE_STRICT_DESCRIPTION)
if(BRLCAD_ENABLE_STRICT)
  mark_as_advanced(BRLCAD_ENABLE_STRICT)
  CONFIG_H_APPEND(BRLCAD "#define STRICT_FLAGS 1\n")
endif(BRLCAD_ENABLE_STRICT)


# Build with full compiler lines visible by default (won't need make
# VERBOSE=1) on command line
option(BRLCAD_ENABLE_VERBOSE_PROGRESS "verbose output" OFF)
mark_as_advanced(BRLCAD_ENABLE_VERBOSE_PROGRESS)
if(BRLCAD_ENABLE_VERBOSE_PROGRESS)
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif(BRLCAD_ENABLE_VERBOSE_PROGRESS)

# Build with profiling support
option(BRLCAD_ENABLE_PROFILING "Build with profiling support" OFF)
mark_as_advanced(BRLCAD_ENABLE_PROFILING)

#====== ALL CXX COMPILE ===================
# Build all C and C++ files with a C++ compiler
set(ENABLE_ALL_CXX_COMPILE_ALIASES "ENABLE_ALL_CXX")
set(ENABLE_ALL_CXX_COMPILE_DESCRIPTION "
Build all C and C++ files with a C++ compiler.  Defaults to OFF.

EXPERIMENTAL!
")
BRLCAD_OPTION(OFF ENABLE_ALL_CXX_COMPILE
  ENABLE_ALL_CXX_COMPILE_ALIASES
  ENABLE_ALL_CXX_COMPILE_DESCRIPTION)
mark_as_advanced(ENABLE_ALL_CXX_COMPILE)

# Build with coverage enabled
option(BRLCAD_ENABLE_COVERAGE "Build with coverage enabled" OFF)
mark_as_advanced(BRLCAD_ENABLE_COVERAGE)

#====== POSIX ===================
# Build with strict POSIX compliance checking
set(ENABLE_POSIX_COMPLIANCE_ALIASES "ENABLE_POSIX")
set(ENABLE_POSIX_COMPLIANCE_DESCRIPTION "
Build with strict POSIX compliance checking.  Defaults to OFF.

Causes compiler options to be set for strict compliance with the
minimum C and C++ standards acceptable according to current BRL-CAD
policy.  The current minimum C standard is ??.
There is no currently defined minimum C++ standard.
")
BRLCAD_OPTION(OFF ENABLE_POSIX_COMPLIANCE
  ENABLE_POSIX_COMPLIANCE_ALIASES
  ENABLE_POSIX_COMPLIANCE_DESCRIPTION)
mark_as_advanced(ENABLE_POSIX_COMPLIANCE)

#== ISO C ==
set(ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_ALIASES "STRICT_ISO_C")
set(ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_DESCRIPTION "
Build with strict ISO C compliance checking.  Defaults to OFF.

Causes C compiler options to be set for strict compliance with the
appropriate ISO C standard.
")
BRLCAD_OPTION(OFF ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE
  ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_ALIASES
  ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE_DESCRIPTION)
mark_as_advanced(ENABLE_STRICT_COMPILER_STANDARD_COMPLIANCE)

# Build with dtrace support
option(BRLCAD_ENABLE_DTRACE "Build with dtrace support" OFF)
mark_as_advanced(BRLCAD_ENABLE_DTRACE)
if(BRLCAD_ENABLE_DTRACE)
  BRLCAD_INCLUDE_FILE(sys/sdt.h HAVE_SYS_SDT_H)
  if(NOT HAVE_SYS_SDT_H)
    set(BRLCAD_ENABLE_DTRACE OFF)
  endif(NOT HAVE_SYS_SDT_H)
endif(BRLCAD_ENABLE_DTRACE)


if(BRLCAD_HEADERS_OLD_COMPAT)
  add_definitions(-DEXPOSE_FB_HEADER)
  add_definitions(-DEXPOSE_DM_HEADER)
endif(BRLCAD_HEADERS_OLD_COMPAT)


#----------------------------------------------------------------------
# There are extra documentation files available requiring DocBook
# They are quite useful in graphical interfaces, but also add considerably
# to the overall build time.  If necessary BRL-CAD provides its own
# xsltproc (see src/other/xmltools), so the html and man page
# outputs are always potentially available.  PDF output, on the other hand,
# needs Apache FOP.  FOP is not a candidate for bundling with BRL-CAD for
# a number of reasons, so we simply check to see if it is present and set
# the options accordingly.

# Do we have the environment variable set locally?
if(NOT "$ENV{APACHE_FOP}" STREQUAL "")
  set(APACHE_FOP "$ENV{APACHE_FOP}")
endif(NOT "$ENV{APACHE_FOP}" STREQUAL "")
if(NOT APACHE_FOP)
  find_program(APACHE_FOP fop DOC "path to the exec script for Apache FOP")
endif(NOT APACHE_FOP)
mark_as_advanced(APACHE_FOP)
# We care about the FOP version, unfortunately - find out what we have.
if(APACHE_FOP)
  execute_process(COMMAND ${APACHE_FOP} -v OUTPUT_VARIABLE APACHE_FOP_INFO ERROR_QUIET)
  string(REGEX REPLACE "FOP Version ([0-9\\.]*)" "\\1" APACHE_FOP_VERSION_REGEX "${APACHE_FOP_INFO}")
  if(APACHE_FOP_VERSION_REGEX)
    string(STRIP ${APACHE_FOP_VERSION_REGEX} APACHE_FOP_VERSION_REGEX)
  endif(APACHE_FOP_VERSION_REGEX)
  if(NOT "${APACHE_FOP_VERSION}" STREQUAL "${APACHE_FOP_VERSION_REGEX}")
    message("-- Found Apache FOP: version ${APACHE_FOP_VERSION_REGEX}")
    set(APACHE_FOP_VERSION ${APACHE_FOP_VERSION_REGEX} CACHE STRING "Apache FOP version" FORCE)
    mark_as_advanced(APACHE_FOP_VERSION)
  endif(NOT "${APACHE_FOP_VERSION}" STREQUAL "${APACHE_FOP_VERSION_REGEX}")
endif(APACHE_FOP)

# Toplevel variable that controls all DocBook based documentation.  Key it off
# of what target level is enabled.
if(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  set(EXTRADOCS_DEFAULT "ON")
else(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  set(EXTRADOCS_DEFAULT "OFF")
endif(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
set(BRLCAD_EXTRADOCS_ALIASES
  ENABLE_DOCS
  ENABLE_EXTRA_DOCS
  ENABLE_DOCBOOK
  )
set(BRLCAD_EXTRADOCS_DESCRIPTION "
The core option that enables and disables building of BRL-CAD's
DocBook based documentation (includes manuals and man pages for
commands, among other things).  Defaults to ON, but only HTML and MAN
formats are enabled by default - PDF must be enabled separately by use
of this option or one of its aliases.  Note that you may set
environment variable APACHE_FOP to point to your locally installed fop
executable file (which on Linux is usually a shell script with 0755
permissions).
")
BRLCAD_OPTION(${EXTRADOCS_DEFAULT} BRLCAD_EXTRADOCS BRLCAD_EXTRADOCS_ALIASES BRLCAD_EXTRADOCS_DESCRIPTION)

include(CMakeDependentOption)

# The HTML output is used in the graphical help browsers in MGED and Archer,
# as well as being the most likely candidate for external viewers. Turn this
# on unless explicitly instructed otherwise by the user or all extra
# documentation is disabled.
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_HTML "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_HTML)

CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_PHP "Build MAN page output from DocBook documentation" OFF "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_PHP)

CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_PPT "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_PPT)

# Normally, we'll turn on man page output by default, but there is
# no point in doing man page output for a Visual Studio build - the
# files aren't useful and it *seriously* increases the target build
# count/build time.  Conditionalize on the CMake MSVC variable NOT
# being set.
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_MAN "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS;NOT MSVC" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_MAN)

# Don't do PDF by default because it's pretty expensive, and hide the
# option unless the tools to do it are present.
set(BRLCAD_EXTRADOCS_PDF_DESCRIPTION "
Option that enables building of BRL-CAD's DocBook PDF-based documentation
(includes manuals and man pages for commands, among
other things.) Defaults to OFF.
Note that you may set environment variable APACHE_FOP
to point to your locally installed fop executable file (which on Linux is
usually a shell script with 0755 permissions).
")
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_PDF "Build PDF output from DocBook documentation" OFF "BRLCAD_EXTRADOCS;APACHE_FOP" OFF)

# Provide an option to enable/disable XML validation as part
# of the DocBook build - sort of a "strict flags" mode for DocBook.
# By default, this will be enabled when extra docs are built and
# the toplevel BRLCAD_ENABLE_STRICT setting is enabled.
# Unfortunately, Visual Studio 2010 seems to have issues when we
# enable validation on top of everything else... not clear why,
# unless build target counts >1800 are beyond MSVC's practical
# limit.  Until we either find a resolution or a way to reduce
# the target count on MSVC, disable validation there.
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_VALIDATE "Perform validation for DocBook documentation" ON "BRLCAD_EXTRADOCS;BRLCAD_ENABLE_STRICT" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_VALIDATE)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
