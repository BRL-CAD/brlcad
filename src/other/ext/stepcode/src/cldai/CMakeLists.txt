
set(LIBSTEPDAI_SRCS
  sdaiApplication_instance_set.cc
  sdaiBinary.cc
  sdaiDaObject.cc
  sdaiEntity_extent.cc
  sdaiEntity_extent_set.cc
  sdaiEnum.cc
  sdaiModel_contents.cc
  sdaiModel_contents_list.cc
  sdaiObject.cc
  sdaiSession_instance.cc
  sdaiString.cc
  )

SET(SC_CLDAI_HDRS
  sdaiApplication_instance_set.h
  sdaiBinary.h
  sdaiDaObject.h
  sdaiEntity_extent.h
  sdaiEntity_extent_set.h
  sdaiEnum.h
  sdaiModel_contents.h
  sdaiModel_contents_list.h
  sdaiObject.h
  sdaiSession_instance.h
  sdaiString.h
  )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${SC_SOURCE_DIR}/src/base
  ${SC_SOURCE_DIR}/src/clstepcore
  ${SC_SOURCE_DIR}/src/clutils
  )

set(_libdeps steputils base)

if($CACHE{SC_BUILD_SHARED_LIBS})
  SC_ADDLIB(stepdai SHARED SOURCES ${LIBSTEPDAI_SRCS} LINK_LIBRARIES ${_libdeps})
  if(WIN32)
    target_compile_definitions(stepdai PRIVATE SC_DAI_DLL_EXPORTS)
  endif()
endif()

if($CACHE{SC_BUILD_STATIC_LIBS})
  SC_ADDLIB(stepdai-static STATIC SOURCES ${LIBSTEPDAI_SRCS} LINK_LIBRARIES $<JOIN:${_libdeps},-static >-static)
endif()

install(FILES ${SC_CLDAI_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/cldai)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

