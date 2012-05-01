include_directories(
  ${BRLCAD_BINARY_DIR}/include/brlcad
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD
  )

if(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  if(CRYPT_LIBRARY)
    add_executable(enigma enigma.c)
    target_link_libraries(enigma ${BSD_LIBRARY} ${CRYPT_LIBRARY})
  else(CRYPT_LIBRARY)
    add_executable(enigma enigma.c crypt.c)
    target_link_libraries(enigma ${BSD_LIBRARY})
  endif(CRYPT_LIBRARY)
  install(TARGETS enigma DESTINATION ${BIN_DIR})
  set_target_properties(enigma PROPERTIES COMPILE_FLAGS "-w")
else(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  CMAKEFILES(enigma.c)
endif(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)

ADD_MAN_PAGES(1 enigma.1)
set(enigma_ignore_files
  AUTHORS
  COPYING
  ChangeLog
  INSTALL
  NEWS
  README
  configure.ac
  crypt.c
  )
CMAKEFILES(${enigma_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
