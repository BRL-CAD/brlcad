# Note: elapsed.sh is installed because it is used by benchmark
set(sh_SCRIPTS
  elapsed.sh
  cmp.sh
  conversion.sh
  show.sh
  )
foreach(script ${sh_SCRIPTS})
  if(NOT CMAKE_CONFIGURATION_TYPES)
    file(COPY ${script} DESTINATION ${CMAKE_BINARY_DIR}/bin FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
    DISTCLEAN("${CMAKE_BINARY_DIR}/bin/${script}")
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      file(COPY ${script} DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)
      DISTCLEAN("${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin/${script}")
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
endforeach(script ${sh_SCRIPTS})

install(PROGRAMS ${sh_SCRIPTS} DESTINATION ${BIN_DIR} PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)

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
  sandbox/metaring.sh
  sandbox/visibility.sh
  )
CMAKEFILES(${sh_SCRIPTS})
CMAKEFILES(${noinst_SCRIPTS})
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
