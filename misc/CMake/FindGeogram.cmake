# Find Geogram
# ------------
#
# Find Geogram include dirs and libraries
#
# This module defines the following variables:
#
#   Geogram_FOUND        - True if geogram has been found.
#   Geogram::geogram     - Imported target for the main Geogram library.
#   Geogram::geogram_gfx - Imported target for Geogram graphics library.
#
# This module reads hints about the Geogram location from the following
# environment variables:
#
#   Geogram_INSTALL_PREFIX - Directory where Geogram is installed.
#
# Authors: Jeremie Dumas
#          Pierre Moulon
#          Bruno Levy

set (Geogram_SEARCH_PATHS
  "${Geogram_ROOT}"
  ${Geogram_INSTALL_PREFIX}
  "$ENV{Geogram_INSTALL_PREFIX}"
  "/usr/local/"
  "$ENV{PROGRAMFILES}/Geogram"
  "$ENV{PROGRAMW6432}/Geogram"
)

set (Geogram_SEARCH_PATHS_SYSTEM
  "/usr/lib"
  "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
)

find_path (Geogram_INCLUDE_DIR
                geogram/basic/common.h
                PATHS ${Geogram_SEARCH_PATHS}
                PATH_SUFFIXES include/geogram1
)

find_library (Geogram_LIBRARY
                NAMES geogram
                PATHS ${Geogram_SEARCH_PATHS}
                PATH_SUFFIXES lib
)

find_library (Geogram_GFX_LIBRARY
                NAMES geogram_gfx
                PATHS ${Geogram_SEARCH_PATHS}
                PATH_SUFFIXES lib
)

# This one we search in both Geogram search path and
# system search path since it may be already installed
# in the system
find_library (Geogram_GLFW3_LIBRARY
                NAMES glfw3 glfw geogram_glfw3 glfw3dll glfwdll
                PATHS ${Geogram_SEARCH_PATHS} ${Geogram_SEARCH_PATHS_SYSTEM}
                PATH_SUFFIXES lib
)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Geogram DEFAULT_MSG Geogram_LIBRARY Geogram_INCLUDE_DIR
)

# Create an imported target for Geogram 
If (Geogram_FOUND)
  
        set(Geogram_INSTALL_PREFIX ${Geogram_INCLUDE_DIR}/../..)
  
        if (NOT TARGET Geogram::geogram)
                add_library (Geogram::geogram UNKNOWN IMPORTED)

                # Interface include directory
                set_target_Properties(Geogram::geogram PROPERTIES
                  INTERFACE_INCLUDE_DIRECTORIES "${Geogram_INCLUDE_DIR}"
                )

                # Link to library file
                Set_Target_Properties(Geogram::geogram PROPERTIES
                  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
                  IMPORTED_LOCATION "${Geogram_LIBRARY}"
                )
        endif ()

        if (NOT TARGET Geogram::geogram_gfx)
                add_library (Geogram::geogram_gfx UNKNOWN IMPORTED)

                set_target_properties(Geogram::geogram_gfx PROPERTIES
                  INTERFACE_LINK_LIBRARIES ${Geogram_GLFW3_LIBRARY}
                )

                # Interface include directory
                set_target_properties(Geogram::geogram_gfx PROPERTIES
                  INTERFACE_INCLUDE_DIRECTORIES "${Geogram_INCLUDE_DIR}"
                )

                # Link to library file
                set_target_properties(Geogram::geogram_gfx PROPERTIES
                  IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
                  IMPORTED_LOCATION "${Geogram_GFX_LIBRARY}"
                )
                
        endif ()

        
endif ()

# Hide variables from the default CMake-Gui options
mark_as_advanced (Geogram_LIBRARY Geogram_GFX_LIBRARY Geogram_INCLUDE_DIR)

