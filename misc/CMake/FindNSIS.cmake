#-------------------------------------------------------------------
# This file is based on the Wix module from the CMake build system
# for OGRE (Object-oriented Graphics Rendering Engine)
# For the latest info, see http://www.ogre3d.org/
#
# The contents of this file are placed in the public domain. Feel
# free to make use of it in any way you like.
#-------------------------------------------------------------------

# - Try to find Nullsoft Scriptable Install System (NSIS)
# You can help this by defining NSIS_HOME in the environment / CMake
# Once done, this will define
#
#  NSIS_FOUND - system has NSIS
#  NSIS_BINARY_DIR - location of the NSIS binaries

include(FindPkgMacros)

# Get path, convert backslashes as ${ENV_${var}}
getenv_path(NSIS_HOME)

# construct search paths
set(NSIS_PREFIX_PATH ${NSIS_HOME} ${ENV_NSIS_HOME}
	"C:/Program Files (x86)/NSIS"
)
find_path(NSIS_BINARY_DIR NAMES makensis.exe HINTS ${NSIS_PREFIX_PATH} PATH_SUFFIXES bin)

if(NSIS_BINARY_DIR)
	set (NSIS_FOUND TRUE)
endif()

mark_as_advanced(NSIS_BINARY_DIR NSIS_FOUND)
