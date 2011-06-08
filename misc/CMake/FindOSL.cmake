# ================================================
# Find OSL Dependences
# ================================================
include(util_macros)
include(boost)
include(FindOpenEXR)
include(FindTBB)
include(FindOIIO)

# ================================================
# Find OSL
# ================================================

# Find OSL library and its dependencies
FIND_LIBRARY(OSLEXEC_LIBRARY 
	NAMES liboslexec.dylib
	PATHS /Users/kunigami/dev/osl/dist/macosx/lib)
FIND_LIBRARY(OSLCOMP_LIBRARY 
	NAMES liboslcomp.dylib
	PATHS /Users/kunigami/dev/osl/dist/macosx/lib)
FIND_LIBRARY(OSLQUERY_LIBRARY 
	NAMES liboslquery.dylib
	PATHS /Users/kunigami/dev/osl/dist/macosx/lib)

FIND_PATH (OSL_INCLUDES
	NAMES oslexec.h
	PATHS /Users/kunigami/dev/osl/dist/macosx/include/OSL)

# == Dependences ==


if (OSL_LIBRARY AND OSL_INCLUDES)
   message("Found OSL")
   message("OSL LIBRARY = ${OSL_LIBRARY}")
   message("OSL INCLUDES = ${OSL_INCLUDES}")
endif()




