# Note: elapsed.sh is installed because it is used by benchmark
set(sh_SCRIPTS
  elapsed.sh
  cmp.sh
  conversion.sh
  facetall.sh
  orbit.sh
  pixinfo.sh
  show.sh
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
  enumerate.sh
  footer.sh
  fs2org.sh
  header.sh
  indent.sh
  ios-icons.sh
  linkroot.sh
  make_deb.sh
  make_dmg.sh
  make_rpm.sh
  news2tracker.sh
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
