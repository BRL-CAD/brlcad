###############################################################################
# CMake module to search for PROJ.4 library
#
# On success, the macro sets the following variables:
# PROJ4_FOUND       = if the library found
# PROJ4_VERSION     = version number of PROJ.4 found
# PROJ4_LIBRARIES   = full path to the library
# PROJ4_INCLUDE_DIR = where to find the library headers 
# Also defined, but not for general use are
# PROJ4_LIBRARY, where to find the PROJ.4 library.
#
# Copyright (c) 2009 Mateusz Loskot <mateusz@loskot.net>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
###############################################################################

# Try to use OSGeo4W installation
if(WIN32)
    set(PROJ4_OSGEO4W_HOME "C:/OSGeo4W") 

    if($ENV{OSGEO4W_HOME})
        set(PROJ4_OSGEO4W_HOME "$ENV{OSGEO4W_HOME}") 
    endif()
endif()

find_path(PROJ4_INCLUDE_DIR proj_api.h
    PATHS ${PROJ4_OSGEO4W_HOME}/include
    DOC "Path to PROJ.4 library include directory")

if (PROJ4_INCLUDE_DIR)
	# Extract version from proj_api.h (ex: 480)
	file(STRINGS ${PROJ4_INCLUDE_DIR}/proj_api.h
		PJ_VERSIONSTR
  		REGEX "#define[ ]+PJ_VERSION[ ]+[0-9]+")
	string(REGEX MATCH "[0-9]+" PJ_VERSIONSTR ${PJ_VERSIONSTR})	

	# TODO: Should be formatted as 4.8.0?
	set(PROJ4_VERSION ${PJ_VERSIONSTR})
endif()

set(PROJ4_NAMES ${PROJ4_NAMES} proj proj_i)
find_library(PROJ4_LIBRARY
    NAMES ${PROJ4_NAMES}
    PATHS ${PROJ4_OSGEO4W_HOME}/lib
    DOC "Path to PROJ.4 library file")

if(PROJ4_LIBRARY)
  set(PROJ4_LIBRARIES ${PROJ4_LIBRARY})
endif()

# Handle the QUIETLY and REQUIRED arguments and set PROJ4_FOUND to TRUE
# if all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PROJ4 DEFAULT_MSG
	PROJ4_LIBRARY
	PROJ4_INCLUDE_DIR)
