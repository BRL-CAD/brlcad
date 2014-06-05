set(sh_SCRIPTS
  elapsed.sh
  cmp.sh
  conversion.sh
  facetall.sh
  indent.sh
  ios-icons.sh
  news2tracker.sh
  orbit.sh
  pixinfo.sh
  show.sh
  tracker.sh
  )
foreach(script ${sh_SCRIPTS})
  if(NOT CMAKE_CONFIGURATION_TYPES)
    configure_file(${script} ${CMAKE_BINARY_DIR}/bin/${script} COPYONLY)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      configure_file(${script} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/${script} COPYONLY)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
endforeach(script ${sh_SCRIPTS})

install(PROGRAMS ${sh_SCRIPTS} DESTINATION ${BIN_DIR})

set(noinst_SCRIPTS
  copyright.sh
  distcheck_screen.sh
  enumerate.sh
  footer.sh
  header.sh
  linkroot.sh
  make_deb.sh
  make_dmg.sh
  make_pkg.sh
  make_rpm.sh
  make_tar.sh
  make_bz2.sh
  make_tgz.sh
  shtool
  template.sh
  ws.sh
  )
CMAKEFILES(${sh_SCRIPTS})
CMAKEFILES(${noinst_SCRIPTS})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
