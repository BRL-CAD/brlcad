# Minimal directories required for any BRL-CAD functionality

# globally consider anything in an 'ext' path a warning-free system header
set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} ext CACHE STRING "Bundled system include dirs" FORCE)

# The source directories and their dependencies are used both
# for the primary build logic here and by the parent build
# logic (in some modes) to control the enabling and disabling
# of third party dependencies, so we modularize the definitions
# to be reusable at multiple levels of the logic
include(${CMAKE_CURRENT_SOURCE_DIR}/source_dirs.cmake)

# Either we're only building some of the directories based on
# user provided lists, or we're building everything.
if(BRLCAD_COMPONENTS)
  # Use all provided components to build a build-ordered
  # list of directories to add
  set(active_dirs ${BRLCAD_COMPONENTS})
  foreach(wc ${BRLCAD_COMPONENTS})
    deps_expand(${wc} active_dirs)
  endforeach(wc ${BRLCAD_COMPONENTS})

  # Add the directories
  list(REVERSE active_dirs)
  foreach(ad ${active_dirs})
    verbose_add_subdirectory(src ${ad})
  endforeach(ad ${active_dirs})
else(BRLCAD_COMPONENTS)
  # Add all directories
  foreach(od ${ordered_dirs})
    verbose_add_subdirectory(src ${od})
  endforeach(od ${ordered_dirs})
endif(BRLCAD_COMPONENTS)

# Optional monolithic brlcad shared library.  Its contents are registered by
# BRLCAD_ADDLIB as library targets are defined, so additions, removals, and
# renames automatically follow the normal CMake build.
brlcad_add_aggregate_library()

cmakefiles(
  CMakeLists.txt
  README
  source_dirs.cmake
  libtermio/libtermio.h
  libtermio/linenoise.hpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
