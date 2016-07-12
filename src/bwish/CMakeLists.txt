
set(BWISH_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${TERMIO_INCLUDE_DIRS}
  ${TCLCAD_INCLUDE_DIRS}
  ${TCL_INCLUDE_DIRS}
  ${TK_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES BWISH_INCLUDE_DIRS)
include_directories(${BWISH_INCLUDE_DIRS})

set(btclsh_libs libtclcad libbu ${TCL_LIBRARY} ${ITCL_LIBRARY})

if(TARGET libtermio)
  set(btclsh_libs ${btclsh_libs} libtermio)
endif(TARGET libtermio)

# Define the btclsh target
BRLCAD_ADDEXEC(btclsh "main.c;cmd.c;input.c" "${btclsh_libs}")

# If we have Tk enabled, we want bwish too
if(BRLCAD_ENABLE_TK)

  set(bwish_libs libtclcad libbu ${TCL_LIBRARY} ${ITCL_LIBRARY} ${TK_LIBRARY} ${ITK_LIBRARY})

  if(TARGET libtermio)
    set(bwish_libs ${bwish_libs} libtermio)
  endif(TARGET libtermio)

  if(CPP_DLL_DEFINES)
    set(bwish_libs ${bwish_libs} imm32.lib comctl32.lib )
  endif(CPP_DLL_DEFINES)

  BRLCAD_ADDEXEC(bwish "main.c;cmd.c;input.c" "${bwish_libs}" GUI)
  set_target_properties(bwish PROPERTIES COMPILE_DEFINITIONS "BWISH=1")
  set_target_properties(bwish PROPERTIES FOLDER "BRL-CAD Executables")

endif(BRLCAD_ENABLE_TK)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
