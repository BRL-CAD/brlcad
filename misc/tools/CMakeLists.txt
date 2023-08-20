function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# Convenience bundling of external third party build tools
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ext)
  add_subdirectory(ext)
endif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ext)

# The following are build or utility tools written by BRL-CAD developers
# specifically for use with our build and maintenance processes

# env2c - Scanner to generate a list of environment variables used by
# source code
add_subdirectory(env2c)
SetTargetFolder(env2c "Compilation Utilities")
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/env2c/Makefile)

# debug2c - Scanner to generate a list of debug variables used by
# source code
add_subdirectory(debug2c)
SetTargetFolder(debug2c "Compilation Utilities")
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/debug2c/Makefile)

# inactvhdrs - utility that tries to find local headers unused by other
# source code in a file hierarchy
add_subdirectory(inactvhdrs)
SetTargetFolder(debug2c "Maintenance Utilities")
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/inactvhdrs/Makefile)

CMAKEFILES(
  CMakeLists.txt
  README
  )

# Locac Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
