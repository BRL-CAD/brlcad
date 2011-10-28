
set(LIBSTEPDAI_SRCS
    sdaiApplication_instance_set.cc
    sdaiSession_instance.cc
    sdaiObject.cc
    sdaiDaObject.cc
    sdaiEntity_extent.cc
    sdaiEntity_extent_set.cc
    sdaiModel_contents.cc
    sdaiModel_contents_list.cc
)

SET(LIBSTEPDAI_PRIVATE_HDRS
    ErrorMap.hh
    ErrorRpt.hh
    impconv.hh
    imptypes.hh
    sdaiApplication_instance_set.h
    sdaiDaObject.h
    sdaiEntity_extent.h
    sdaiEntity_extent_set.h
    sdaiModel_contents.h
    sdaiModel_contents_list.h
    sdaiObject.h
    sdaiSession_instance.h
    StrUtil.hh
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SCL_SOURCE_DIR}/src/clstepcore
    ${SCL_SOURCE_DIR}/src/clutils
)

add_definitions(
    -DHAVE_CONFIG_H
)

SCL_ADDLIB(stepdai "${LIBSTEPDAI_SRCS}" "steputils stepcore")
