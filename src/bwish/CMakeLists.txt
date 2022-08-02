
if(TARGET libtclcad)
  set(BWISH_INCLUDE_DIRS
    ${BU_INCLUDE_DIRS}
    ${TERMIO_INCLUDE_DIRS}
    ${TCLCAD_INCLUDE_DIRS}
    ${TCL_INCLUDE_PATH}
    )
  if (TK_INCLUDE_PATH)
    set(BWISH_INCLUDE_DIRS ${BWISH_INCLUDE_DIRS} ${TK_INCLUDE_PATH})
  endif (TK_INCLUDE_PATH)
  list(REMOVE_DUPLICATES BWISH_INCLUDE_DIRS)
  BRLCAD_INCLUDE_DIRS(BWISH_INCLUDE_DIRS)

  set(btclsh_libs libtclcad libbu ${TCL_LIBRARY})

  if(TARGET libtermio)
    set(btclsh_libs ${btclsh_libs} libtermio)
  endif(TARGET libtermio)

  # Define the btclsh target
  BRLCAD_ADDEXEC(btclsh "main.c;cmd.c;input.c" "${btclsh_libs}")

  if (TARGET itcl_pkgIndex)
    add_dependencies(btclsh itcl_pkgIndex)
  endif (TARGET itcl_pkgIndex)

  # If we have Tk enabled, we want bwish too
  if(BRLCAD_ENABLE_TK)

    set(bwish_libs libtclcad libbu ${TCL_LIBRARY} ${TK_LIBRARY} ${IMM32_LIBRARY} ${COMCTL32_LIBRARY})

    if(TARGET libtermio)
      set(bwish_libs ${bwish_libs} libtermio)
    endif(TARGET libtermio)

    BRLCAD_ADDEXEC(bwish "main.c;cmd.c;input.c" "${bwish_libs}" GUI)
    set_property(TARGET bwish APPEND PROPERTY COMPILE_DEFINITIONS "BWISH=1")
    set_target_properties(bwish PROPERTIES FOLDER "BRL-CAD Executables")

    if (TARGET itcl_pkgIndex)
      add_dependencies(bwish itcl_pkgIndex)
    endif (TARGET itcl_pkgIndex)
    if (TARGET itk_pkgIndex)
      add_dependencies(bwish itk_pkgIndex)
    endif (TARGET itk_pkgIndex)

  endif(BRLCAD_ENABLE_TK)
endif(TARGET libtclcad)

CMAKEFILES(
  CMakeLists.txt
  cmd.c
  input.c
  main.c
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
