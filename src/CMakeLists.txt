# Define high level features of BRL-CAD
DOXYGEN_FEATURE("Portability Library" DESCRIPTION "Portability layer that makes sure basic low level system functionality is present across all platforms via a uniform API.")
DOXYGEN_FEATURE("Solid Raytracing" DESCRIPTION "Ray/geometry intersection calculations that report not just first surface hits but in/out pairs of hits to support the characterization of closed geometric volumes.")

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
if (BRLCAD_COMPONENTS)

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

else (BRLCAD_COMPONENTS)

  # Add all directories
  foreach(od ${ordered_dirs})
    verbose_add_subdirectory(src ${od})
  endforeach(od ${ordered_dirs})

endif (BRLCAD_COMPONENTS)

# This corresponds roughly to the libbrlcad originally defined in BRL-CAD
# autotools build.
if(BRLCAD_ENABLE_BRLCAD_LIBRARY AND USE_OBJECT_LIBS)
  foreach(llib ${ordered_dirs})
    if(TARGET ${llib}-obj)
      set(libbrlcad_objs ${libbrlcad_objs} $<TARGET_OBJECTS:${llib}-obj>)
    endif(TARGET ${llib}-obj)
  endforeach(llib ${ordered_dirs})
  add_library(libbrlcad SHARED ${libbrlcad_objs})
  add_library(libbrlcad-static STATIC ${libbrlcad_objs})
endif(BRLCAD_ENABLE_BRLCAD_LIBRARY AND USE_OBJECT_LIBS)

CMAKEFILES(
  CMakeLists.txt
  README
  source_dirs.cmake
  libtermio/libtermio.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
