set(MGED_SOURCES
  adc.c
  arbs.c
  attach.c
  axes.c
  buttons.c
  chgmodel.c
  chgtree.c
  chgview.c
  cmd.c
  clone.c
  color_scheme.c
  columns.c
  dm-generic.c
  dodraw.c
  doevent.c
  dozoom.c
  edarb.c
  edars.c
  edpipe.c
  edsol.c
  facedef.c
  fbserv.c
  grid.c
  history.c
  mater.c
  menu.c
  mged.c
  mover.c
  overlay.c
  plot.c
  predictor.c
  rect.c
  rtif.c
  scroll.c
  set.c
  setup.c
  share.c
  tedit.c
  titles.c
  track.c
  update.c
  usepen.c
  utility1.c
  vparse.c
  wdb_obj.c
  )

if (HAVE_WINDOWS_H)
  # To associate an icon with the application for Windows (needed
  # for the WiX installer) we must use an rc file.
  enable_language(RC)
  set(MGED_SOURCES ${MGED_SOURCES} mged.rc)
endif (HAVE_WINDOWS_H)


set(mged_libs libtclcad libged libwdb libdm libnmg ${WS2_32_LIBRARY} ${OPENGL32_LIBRARY})
if(TARGET libtermio)
  set(mged_libs ${mged_libs} libtermio)
endif(TARGET libtermio)
if (ITK_LIBRARY)
  set(mged_libs ${mged_libs} ${ITK_LIBRARY})
endif (ITK_LIBRARY)

# Ideally we'd build as GUI for Windows to avoid the command window popping up,
# but I'm not sure if that would be a problem for scripting with mged.exe on
# Windows so leave it for now...
if (BRLCAD_ENABLE_TCL)

  set(MGED_INCLUDE_DIRS
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${TCLCAD_INCLUDE_DIRS}
    ${GED_INCLUDE_DIRS}
    ${WDB_INCLUDE_DIRS}
    ${DM_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    ${NMG_INCLUDE_DIRS}
    ${TCL_INCLUDE_PATH}
    ${TERMIO_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )
  if (TK_INCLUDE_PATH)
    set(MGED_INCLUDE_DIRS ${MGED_INCLUDE_DIRS} ${TK_INCLUDE_PATH})
  endif (TK_INCLUDE_PATH)

  BRLCAD_INCLUDE_DIRS(MGED_INCLUDE_DIRS)

  BRLCAD_ADDEXEC(mged "${MGED_SOURCES}" "${mged_libs}")
  ADD_TARGET_DEPS(mged Tkhtml Tktable itcl_pkgIndex itk_pkgIndex dm_plugins)

  # mged depends on the creation/installation of all the
  # tclscripts.
  foreach(item ${tclindex_target_list})
    add_dependencies(mged ${item})
  endforeach(item ${tclindex_target_list})

  BRLCAD_MANAGE_FILES("mged.bat" ${BIN_DIR} EXEC)

  # MGED is one of the programs that gets a start menu entry
  set_property(INSTALL "${BIN_DIR}/mged.bat"
    PROPERTY CPACK_START_MENU_SHORTCUTS "MGED ${BRLCAD_VERSION}"
    )
  set_property(INSTALL "${BIN_DIR}/mged.bat"
    PROPERTY CPACK_DESKTOP_SHORTCUTS "MGED ${BRLCAD_VERSION}"
    )

else (BRLCAD_ENABLE_TCL)

  CMAKEFILES(${MGED_SOURCES})

endif (BRLCAD_ENABLE_TCL)

if (NOT BRLCAD_ENABLE_TCL)
  set(MGED_INCLUDE_DIRS
    ${BU_INCLUDE_DIRS}
    )
  BRLCAD_INCLUDE_DIRS(MGED_INCLUDE_DIRS)
endif (NOT BRLCAD_ENABLE_TCL)

BRLCAD_ADDEXEC(cad_boundp cad_boundp.c "libbu;${M_LIBRARY}")

BRLCAD_ADDEXEC(cad_parea cad_parea.c libbu)

set(mged_ignore_files
  CMakeLists.txt
  bool_rewrite.c
  cmd.h
  comb_bool.h
  comb_bool_parse.y
  comb_bool_scan.l
  htmlLibraryUi.h
  menu.h
  mged.h
  mged_dm.h
  qray.h
  red.c
  scroll.h
  sedit.h
  titles.h
  mged.ico
  mged.rc
  )
CMAKEFILES(${mged_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
