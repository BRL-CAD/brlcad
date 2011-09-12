# - Try to find OpenGL
# Once done this will define
#  
#  OPENGL_FOUND            - system has OpenGL
#  OPENGL_XMESA_FOUND      - system has XMESA
#  OPENGL_GLU_FOUND        - system has GLU
#  OPENGL_INCLUDE_DIR_GL   - the GL include directory
#  OPENGL_INCLUDE_DIR_GLX  - the GLX include directory
#  OPENGL_LIBRARIES        - Link these to use OpenGL and GLU
#   
# If you want to use just GL you can use these values
#  OPENGL_gl_LIBRARY   - Path to OpenGL Library
#  OPENGL_glu_LIBRARY  - Path to GLU Library
#
# Define controlling option OPENGL_USE_AQUA if on Apple - 
# if this is not true, look for the X11 OpenGL.  Defaults
# to true.

#=============================================================================
# Copyright 2001-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distributed this file outside of CMake, substitute the full
#  License text for the above reference.)

IF (WIN32)
	IF (CYGWIN)

		FIND_PATH(OPENGL_INCLUDE_DIR_GL GL/gl.h )

		FIND_LIBRARY(OPENGL_gl_LIBRARY opengl32 )

		FIND_LIBRARY(OPENGL_glu_LIBRARY glu32 )

	ELSE (CYGWIN)

		IF(BORLAND)
			SET (OPENGL_gl_LIBRARY import32 CACHE STRING "OpenGL library for win32")
			SET (OPENGL_glu_LIBRARY import32 CACHE STRING "GLU library for win32")
		ELSE(BORLAND)
			SET (OPENGL_gl_LIBRARY opengl32 CACHE STRING "OpenGL library for win32")
			SET (OPENGL_glu_LIBRARY glu32 CACHE STRING "GLU library for win32")
		ENDIF(BORLAND)

	ENDIF (CYGWIN)

ELSE (WIN32)

		# The first line below is to make sure that the proper headers
		# are used on a Linux machine with the NVidia drivers installed.
		# They replace Mesa with NVidia's own library but normally do not
		# install headers and that causes the linking to
		# fail since the compiler finds the Mesa headers but NVidia's library.
		# Make sure the NVIDIA directory comes BEFORE the others.
		#  - Atanas Georgiev <atanas@cs.columbia.edu>


		SET(OPENGL_INC_SEARCH_PATH
			/usr/share/doc/NVIDIA_GLX-1.0/include
			/usr/pkg/xorg/include
			/usr/X11/include
			/usr/X11R6/include
			/usr/X11R7/include
			/usr/include/X11
			/usr/local/include
			/usr/local/include/X11
			/usr/openwin/include
			/usr/openwin/share/include
			/opt/graphics/OpenGL/include
			/usr/include
			)
		# Handle HP-UX cases where we only want to find OpenGL in either hpux64
		# or hpux32 depending on if we're doing a 64 bit build.
		IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
			SET(HPUX_IA_OPENGL_LIB_PATH /opt/graphics/OpenGL/lib/hpux32/)
		ELSE(CMAKE_SIZEOF_VOID_P EQUAL 4)
			SET(HPUX_IA_OPENGL_LIB_PATH 
				/opt/graphics/OpenGL/lib/hpux64/
				/opt/graphics/OpenGL/lib/pa20_64)
		ENDIF(CMAKE_SIZEOF_VOID_P EQUAL 4)

		GET_PROPERTY(SEARCH_64BIT GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
		IF(SEARCH_64BIT)
			SET(64BIT_DIRS "/usr/lib64/X11;/usr/lib64")
		ELSE(SEARCH_64BIT)
			SET(32BIT_DIRS "/usr/lib/X11;/usr/lib")
		ENDIF(SEARCH_64BIT)

		SET(OPENGL_LIB_SEARCH_PATH
			/usr/X11/lib
			/usr/X11R6/lib
			/usr/X11R7/lib
			${64BIT_DIRS}
			${32BIT_DIRS}
			/usr/pkg/xorg/lib
			/usr/openwin/lib
			/opt/graphics/OpenGL/lib
			/usr/shlib
			${HPUX_IA_OPENGL_LIB_PATH}
			)

	IF (APPLE)
		OPTION(OPENGL_USE_AQUA "Require native OSX Framework version of OpenGL." ON)
	ENDIF(APPLE)

	IF(OPENGL_USE_AQUA)
		FIND_LIBRARY(OPENGL_gl_LIBRARY OpenGL DOC "OpenGL lib for OSX")
		FIND_LIBRARY(OPENGL_glu_LIBRARY AGL DOC "AGL lib for OSX")
		FIND_PATH(OPENGL_INCLUDE_DIR_GL OpenGL/gl.h DOC "Include for OpenGL on OSX")
	ELSE(OPENGL_USE_AQUA)
		# If we're on Apple and not using Aqua, we don't want frameworks
		SET(CMAKE_FIND_FRAMEWORK "NEVER")

		FIND_PATH(OPENGL_INCLUDE_DIR_GL GL/gl.h        ${OPENGL_INC_SEARCH_PATH})
		FIND_PATH(OPENGL_INCLUDE_DIR_GLX GL/glx.h      ${OPENGL_INC_SEARCH_PATH})
		FIND_PATH(OPENGL_xmesa_INCLUDE_DIR GL/xmesa.h  ${OPENGL_INC_SEARCH_PATH})
		FIND_LIBRARY(OPENGL_gl_LIBRARY NAMES GL MesaGL PATHS ${OPENGL_LIB_SEARCH_PATH})
		
		# On Unix OpenGL most certainly always requires X11.
		# Feel free to tighten up these conditions if you don't 
		# think this is always true.
		# It's not true on OSX.

		IF (OPENGL_gl_LIBRARY)
			IF(NOT X11_FOUND)
				INCLUDE(FindX11)
			ENDIF(NOT X11_FOUND)
			IF (X11_FOUND)
				SET (OPENGL_LIBRARIES ${X11_LIBRARIES})
			ENDIF (X11_FOUND)
		ENDIF (OPENGL_gl_LIBRARY)

		FIND_LIBRARY(OPENGL_glu_LIBRARY NAMES GLU MesaGLU PATHS ${OPENGL_LIB_SEARCH_PATH})
	ENDIF(OPENGL_USE_AQUA)

ENDIF (WIN32)

SET( OPENGL_FOUND "NO" )
IF(OPENGL_gl_LIBRARY)

	IF(OPENGL_xmesa_INCLUDE_DIR)
		SET( OPENGL_XMESA_FOUND "YES" )
	ELSE(OPENGL_xmesa_INCLUDE_DIR)
		SET( OPENGL_XMESA_FOUND "NO" )
	ENDIF(OPENGL_xmesa_INCLUDE_DIR)

	SET( OPENGL_LIBRARIES  ${OPENGL_gl_LIBRARY} ${OPENGL_LIBRARIES})
	IF(OPENGL_glu_LIBRARY)
		SET( OPENGL_GLU_FOUND "YES" )
		SET( OPENGL_LIBRARIES ${OPENGL_glu_LIBRARY} ${OPENGL_LIBRARIES} )
	ELSE(OPENGL_glu_LIBRARY)
		SET( OPENGL_GLU_FOUND "NO" )
	ENDIF(OPENGL_glu_LIBRARY)

	SET( OPENGL_FOUND "YES" )

	# This deprecated setting is for backward compatibility with CMake1.4

	SET (OPENGL_LIBRARY ${OPENGL_LIBRARIES})

ENDIF(OPENGL_gl_LIBRARY)

MARK_AS_ADVANCED(
	OPENGL_INCLUDE_DIR_GL
	OPENGL_INCLUDE_DIR_GLX
	OPENGL_xmesa_INCLUDE_DIR
	OPENGL_glu_LIBRARY
	OPENGL_gl_LIBRARY
	)
