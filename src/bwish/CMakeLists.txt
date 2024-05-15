if(TARGET libtclcad)
  set(btclsh_libs libtclcad libbu ${TCL_LIBRARY})

  # Define the btclsh target
  BRLCAD_ADDEXEC(btclsh "main.c;cmd.c;input.c" "${btclsh_libs}")
  target_include_directories(btclsh BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../libtermio)

  if (TARGET itcl_pkgIndex)
    add_dependencies(btclsh itcl_pkgIndex)
  endif (TARGET itcl_pkgIndex)

  # If we have Tk enabled, we want bwish too
  if(BRLCAD_ENABLE_TK)

    set(bwish_libs libtclcad libbu ${TCL_LIBRARY} ${TK_LIBRARY} ${IMM32_LIBRARY} ${COMCTL32_LIBRARY})

    BRLCAD_ADDEXEC(bwish "main.c;cmd.c;input.c" "${bwish_libs}" GUI)
    target_include_directories(bwish BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../libtermio)
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
