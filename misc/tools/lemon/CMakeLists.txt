CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

if (COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
endif (COMMAND CMAKE_POLICY)

# set project name
PROJECT(LEMON)

# build the lemon binary
add_executable(lemon lemon.c)

# install the binary
if (NOT BIN_DIR)
  set (BIN_DIR bin)
endif (NOT BIN_DIR)
if (NOT LEMON_SKIP_INSTALL)
  install(TARGETS lemon DESTINATION ${BIN_DIR})
endif (NOT LEMON_SKIP_INSTALL)

# install the template
if (NOT DATA_DIR)
   set (DATA_DIR share)
endif (NOT DATA_DIR)
if (NOT LEMON_SKIP_INSTALL)
  install(FILES lempar.c DESTINATION ${DATA_DIR}/lemon)
endif (NOT LEMON_SKIP_INSTALL)

configure_file(lempar.c ${CMAKE_BINARY_DIR}/${DATA_DIR}/lemon COPYONLY)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
