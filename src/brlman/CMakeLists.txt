set(BRLCAD_ROOT "${CMAKE_BINARY_DIR}")
set(BRLCAD_DATA "${DATA_DIR}")
set(prefix "${CMAKE_BINARY_DIR}")
set(datarootdir "\${prefix}/share")
set(mandir "\${datarootdir}/man")
configure_file(brlman.sh.in ${CMAKE_BINARY_DIR}/src/brlman/brlman @ONLY)
file(COPY ${CMAKE_BINARY_DIR}/src/brlman/brlman
  DESTINATION ${CMAKE_BINARY_DIR}/bin
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
  GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
file(REMOVE ${CMAKE_BINARY_DIR}/src/brlman/brlman)
DISTCLEAN(${CMAKE_BINARY_DIR}/bin/brlman)
foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
  file(COPY ${CMAKE_BINARY_DIR}/bin/brlman
    DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/bin
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})

set(BRLCAD_ROOT "${CMAKE_INSTALL_PREFIX}")
set(BRLCAD_DATA "${DATA_DIR}")
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(datarootdir "\${prefix}/share")
set(mandir "\${datarootdir}/man")

configure_file(brlman.sh.in ${CMAKE_BINARY_DIR}/src/brlman/brlman @ONLY)
install(PROGRAMS ${CMAKE_BINARY_DIR}/src/brlman/brlman DESTINATION ${BIN_DIR})
install(FILES brlman.1 DESTINATION ${MAN_DIR}/man1)

CMAKEFILES(brlman.1 brlman.sh.in)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
