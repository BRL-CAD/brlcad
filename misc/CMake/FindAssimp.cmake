###
# - Find Assimp
# Find the native Assimp includes and library
#
#  ASSIMP_INCLUDE_DIR   - where to find include headers
#  ASSIMP_LIBRARIES     - List of libraries when using assimp.
#  assimp_FOUND         - True if assimp found.
#
###
# - helpful cmake flags when building
#
#  Assimp_ROOT          - path to assimp root if it is built outside of /usr
#  assimp_FIND_QUIETLY  - if =FALLSE; says if assimp is found in brlcad build
#  assimp_FIND_REQUIRED - if =TRUE; breaks brlcad build if paths dont find assimp
#
#=============================================================================


include (FindPackageHandleStandardArgs)

find_path (Assimp_INCLUDE_DIR 
            NAMES assimp/postprocess.h assimp/scene.h assimp/version.h assimp/config.h assimp/cimport.h
	    PATHS /usr/local/include
	    PATHS /usr/include/
            HINTS 
            ${Assimp_ROOT}
	    PATH_SUFFIXES
            include
	)

find_library (Assimp_LIBRARY
  	    NAMES assimp
            PATHS /usr/local/lib/
	    PATHS /usr/lib64/
	    PATHS /usr/lib/
	    HINTS
	    ${Assimp_ROOT}
	    PATH_SUFFIXES
            bin
  	    lib
	    lib64
	)

# Handle the QUIETLY and REQUIRED arguments and set assimp_FOUND.
find_package_handle_standard_args (Assimp DEFAULT_MSG
    Assimp_INCLUDE_DIR
    Assimp_LIBRARY
)

# Set the output variables.
if (assimp_FOUND)
    set (Assimp_INCLUDE_DIRS ${Assimp_INCLUDE_DIR})
    set (Assimp_LIBRARIES ${Assimp_LIBRARY})
else ()
    set (Assimp_INCLUDE_DIRS)
    set (Assimp_LIBRARIES)
endif ()

mark_as_advanced (
    Assimp_INCLUDE_DIR
    Assimp_LIBRARY
)
