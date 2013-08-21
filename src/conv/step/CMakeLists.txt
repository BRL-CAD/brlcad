#
# FIXME: massively parallel builds consistently fail building step
#        sources.  presumably, something is wrong with the dependency
#        declarations or file protections with multiple processes
#        trying to run perplex or fedex simultaneously.
#

set(STEP_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES STEP_INCLUDE_DIRS)
include_directories(${STEP_INCLUDE_DIRS})

# local includes
include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_SOURCE_DIR}/src/other/stepcode/include
  ${BRLCAD_SOURCE_DIR}/src/other/stepcode/src/base
  ${BRLCAD_SOURCE_DIR}/src/other/stepcode/src/clstepcore
  ${BRLCAD_SOURCE_DIR}/src/other/stepcode/src/cleditor
  ${BRLCAD_SOURCE_DIR}/src/other/stepcode/src/clutils
  ${BRLCAD_SOURCE_DIR}/src/other/stepcode/src/cldai
  ${BRLCAD_SOURCE_DIR}/src/other/stepcode/src/test
  ${CMAKE_BINARY_DIR}/src/other/stepcode/include
  ${TNT_INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/src/libbrep #Temporary until headers reworked
  )

set(SCHEMA_FILE ${BRLCAD_SOURCE_DIR}/src/other/stepcode/data/ap203/ap203.exp)
#set(SCHEMA_FILE ${BRLCAD_SOURCE_DIR}/src/other/stepcode/data/ap203e2/ap203e2_mim_lf.exp)

# read the schema name from a line like 'SCHEMA AUTOMOTIVE_DESIGN;'
file(STRINGS ${SCHEMA_FILE} SCHEMA_STATEMENT LIMIT_COUNT 1 REGEX "SCHEMA .*")
string(REGEX REPLACE "^SCHEMA \(.*\)\;$" "\\1" SCHEMA_N ${SCHEMA_STATEMENT} )
string(TOUPPER ${SCHEMA_N} SCHEMA_NAME) #fedex_plus always uses upper case for file names

set(SCHEMA_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA_NAME})
make_directory(${SCHEMA_OUT_DIR})
DISTCLEAN(${SCHEMA_OUT_DIR})
include_directories(${SCHEMA_OUT_DIR})

set(fedex_srcs
  ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}.cc
  ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}.init.cc
  ${SCHEMA_OUT_DIR}/SdaiAll.cc
  ${SCHEMA_OUT_DIR}/compstructs.cc
  ${SCHEMA_OUT_DIR}/schema.cc
  )

set(FEDEX_OUTPUT
  ${fedex_srcs}
  ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}.h
  ${SCHEMA_OUT_DIR}/Sdaiclasses.h
  ${SCHEMA_OUT_DIR}/schema.h
  )

add_subdirectory(step-g)
add_subdirectory(g-step)

CMAKEFILES(
  BRLCADWrapper.h
  STEPWrapper.h
)

DISTCLEAN(${SCHEMA_OUT_DIR}/make_schema)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
