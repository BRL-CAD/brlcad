function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# The following are build or utility tools written by BRL-CAD developers
# specifically for use with our build and maintenance processes

# yedit - console editor for Windows to be used as a fallback if
# there are no other console options present in the environment
add_subdirectory(yori)

# inactvhdrs - utility that tries to find local headers unused by other
# source code in a file hierarchy
if(BUILD_TESTING)
  add_subdirectory(inactvhdrs)
  settargetfolder(debug2c "Maintenance Utilities")
  distclean(${CMAKE_CURRENT_BINARY_DIR}/inactvhdrs/Makefile)
endif(BUILD_TESTING)

cmakefiles(
  CMakeLists.txt
  README
  inactvhdrs/CMakeLists.txt
  inactvhdrs/inactvhdrs.cpp
)

# Locac Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
