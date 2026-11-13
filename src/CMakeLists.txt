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

# Keep registered command schemas and their manual pages synchronized.
# BRLCAD_ADDEXEC records schema-capable targets as they are declared; tests are
# created here after all source subdirectories and the argdoc target exist.
if(BUILD_TESTING AND TARGET argdoc AND NOT CMAKE_CROSSCOMPILING)
  get_property(bu_arg_targets GLOBAL PROPERTY BRLCAD_ARG_SCHEMA_TARGETS)
  list(REMOVE_DUPLICATES bu_arg_targets)
  list(SORT bu_arg_targets)
  foreach(arg_target ${bu_arg_targets})
    string(REPLACE "-" "_" arg_test_name ${arg_target})
    set(arg_man_page "${CMAKE_SOURCE_DIR}/doc/asciidoc/system/man1/${arg_target}.adoc")
    if(EXISTS "${arg_man_page}")
      brlcad_add_test(
        NAME argdoc_${arg_test_name}
        COMMAND
          "$<TARGET_FILE:argdoc>" --check
          --man-page "${arg_man_page}"
          -- "$<TARGET_FILE:${arg_target}>" --command-schema
      )
      set_tests_properties(argdoc_${arg_test_name} PROPERTIES TIMEOUT 30)
    else()
      brlcad_add_test(
        NAME argdoc_${arg_test_name}_schema
        COMMAND "$<TARGET_FILE:argdoc>" --render -- "$<TARGET_FILE:${arg_target}>" --command-schema
      )
      set_tests_properties(argdoc_${arg_test_name}_schema PROPERTIES TIMEOUT 30)
    endif()
  endforeach()

  if(TARGET gsh)
    brlcad_add_test(
      NAME argdoc_ged_commands
      COMMAND
        "$<TARGET_FILE:argdoc>"
        --audit-directory "${CMAKE_SOURCE_DIR}/doc/asciidoc/system/mann"
        --allow-undocumented "${CMAKE_SOURCE_DIR}/src/util/tests/argdoc_ged_undocumented.txt"
        -- "$<TARGET_FILE:gsh>" --command-schema all
    )
    set_tests_properties(argdoc_ged_commands PROPERTIES TIMEOUT 60)
  endif()
endif()

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
