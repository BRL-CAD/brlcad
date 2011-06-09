# ================================================
# Find OSL Dependences
# ================================================
include(util_macros)
include(FindOpenEXR)
include(FindTBB)
include(FindOIIO)

# ================================================
# Find OSL
# ================================================

# If 'OSL' not set, use the env variable of that name if available
if (NOT OSLHOME)
    if (NOT $ENV{OSLHOME} STREQUAL "")
        set (OSLHOME $ENV{OSLHOME})
    endif ()
endif ()

message("OSL_HOME = ${OSLHOME}")

# Find OSL library and its dependencies
FIND_LIBRARY(OSLEXEC_LIBRARY 
	NAMES oslexec
	PATHS ${OSLHOME}/lib)
FIND_LIBRARY(OSLCOMP_LIBRARY 
	NAMES oslcomp
	PATHS ${OSLHOME}/lib)
FIND_LIBRARY(OSLQUERY_LIBRARY 
	NAMES oslquery
	PATHS ${OSLHOME}/lib)

FIND_PATH (OSL_INCLUDES
	NAMES oslexec.h
	PATHS ${OSLHOME}/include/OSL)

if (OSLEXEC_LIBRARY AND OSLCOMP_LIBRARY AND OSLQUERY_LIBRARY AND OSL_INCLUDES)
   message("Found OSL")
   message("OSL EXEC = ${OSLEXEC_LIBRARY}")
   message("OSL COMP = ${OSLCOMP_LIBRARY}")
   message("OSL QUERY = ${OSLQUERY_LIBRARY}")
   message("OSL INCLUDES = ${OSL_INCLUDES}")
endif()




