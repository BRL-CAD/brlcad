# Currently, rtserver requires pthread to work - conditionalize on that test
if(HAVE_PTHREAD_H)
  # Include directories needed by librtserver users
  set(RTSERVER_INCLUDE_DIRS
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${BN_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${JNI_INCLUDE_DIRS}
    )
  BRLCAD_LIB_INCLUDE_DIRS(rtserver RTSERVER_INCLUDE_DIRS "")

  BRLCAD_ADDLIB(rtserver rtserver.c "librt;libbn;libbu;${WINSOCK_LIB};${ZLIB_LIBRARY}")
  set_target_properties(rtserver PROPERTIES VERSION 20.0.1 SOVERSION 20)
  set_target_properties(rtserver PROPERTIES SUFFIX ".jnilib")

  BRLCAD_ADDEXEC(rtserverTest rtserverTest.c "libbu;rtserver" NO_INSTALL)
else(HAVE_PTHREAD_H)
  CMAKEFILES(rtserver.c rtserverTest.c)
endif(HAVE_PTHREAD_H)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
