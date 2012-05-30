
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

SET(LIBSTEPDAI_PRIVATE_HDRS
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
    ${SCL_SOURCE_DIR}/src/clstepcore
    ${SCL_SOURCE_DIR}/src/clutils
)

SCL_ADDLIB(stepdai "${LIBSTEPDAI_SRCS}" "steputils")
