
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

SC_ADDLIB(stepdai "${LIBSTEPDAI_SRCS}" "steputils;base")

if (NOT SKIP_INSTALL_HEADERS)
	install(FILES ${SC_CLDAI_HDRS}
		DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/cldai)
endif (NOT SKIP_INSTALL_HEADERS)
