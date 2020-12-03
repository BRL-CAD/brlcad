set(STEPDAI_SRCS
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

SET(STEPDAI_HDRS
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

add_library(stepdai-obj OBJECT ${STEPDAI_SRCS})
set_property(TARGET stepdai-obj PROPERTY POSITION_INDEPENDENT_CODE ON)
add_dependencies(stepdai-obj base)
if(MSVC)
  set_property(TARGET stepdai-obj APPEND PROPERTY COMPILE_DEFINITIONS "SC_DAI_DLL_EXPORTS")
endif(MSVC)

add_library(stepdai SHARED $<TARGET_OBJECTS:stepdai-obj>)
target_link_libraries(stepdai steputils base)
if(OPENBSD)
  set_target_properties(stepdai PROPERTIES VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})
else(OPENBSD)
  set_target_properties(stepdai PROPERTIES VERSION ${SC_VERSION} SOVERSION ${SC_VERSION_MAJOR})
endif(OPENBSD)
if(APPLE)
  set_property(TARGET stepdai APPEND PROPERTY LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
if(MSVC)
  set_property(TARGET stepdai APPEND PROPERTY COMPILE_DEFINITIONS "SC_DAI_DLL_EXPORTS")
  set_property(TARGET stepdai APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "SC_DAI_DLL_IMPORTS")
endif(MSVC)
install(TARGETS stepdai
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (BUILD_STATIC_LIBS)
  add_library(stepdai-static STATIC $<TARGET_OBJECTS:stepdai-obj>)
  install(TARGETS stepdai-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

install(FILES ${STEPDAI_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/cldai)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

