set(GTOOLS_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${PKG_INCLUDE_DIRS}
  ${LINENOISE_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES GTOOLS_INCLUDE_DIRS)
BRLCAD_INCLUDE_DIRS(GTOOLS_INCLUDE_DIRS)

BRLCAD_ADDDATA(gtransfer.c sample_applications)

BRLCAD_ADDEXEC(ganalyze ganalyze.cpp "libanalyze;librt;libbu;libpkg;${LINENOISE_LIBRARIES}" NO_INSTALL)
set_property(TARGET ganalyze APPEND PROPERTY COMPILE_DEFINITIONS "LINENOISE_DLL_IMPORTS")

BRLCAD_ADDEXEC(gchecker gchecker.cpp "libged;librt;libbu")
BRLCAD_ADDEXEC(gex "gex.cpp" "libbu;librt")
BRLCAD_ADDEXEC(glint glint.cpp "librt;${M_LIBRARY}")
BRLCAD_ADDEXEC(gqa gqa.c "libged;librt")

BRLCAD_ADDEXEC(gsh gsh.cpp  "libged;libdm;libbu;${LINENOISE_LIBRARIES}")
set_property(TARGET gsh APPEND PROPERTY COMPILE_DEFINITIONS "LINENOISE_DLL_IMPORTS")
add_dependencies(gsh dm_plugins)

BRLCAD_ADDEXEC(gtransfer gtransfer.c "librt;libpkg" NO_INSTALL)

add_subdirectory(beset)
add_subdirectory(gdiff)

add_subdirectory(tests)

CMAKEFILES(
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
