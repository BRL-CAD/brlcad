# Because the fedex_plus outputs are used by both step-g and g-step,
# the logic both commands need is the same and is defined up front.

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

# This Schema determines which version of STEP the command will support
# TODO - see if there is some way to automatically support multiple versions
set(SCHEMA_FILE ${BRLCAD_SOURCE_DIR}/src/other/stepcode/data/ap203/ap203.exp)
#set(SCHEMA_FILE ${BRLCAD_SOURCE_DIR}/src/other/stepcode/data/ap203e2/ap203e2_mim_lf.exp)

# read the schema name from a line like 'SCHEMA AUTOMOTIVE_DESIGN;'
file(STRINGS ${SCHEMA_FILE} SCHEMA_STATEMENT LIMIT_COUNT 1 REGEX "SCHEMA .*")
string(REGEX REPLACE "^SCHEMA \(.*\)\;$" "\\1" SCHEMA_N ${SCHEMA_STATEMENT} )
string(TOUPPER ${SCHEMA_N} SCHEMA_NAME) #fedex_plus always uses upper case for file names

# Because SCHEMA_OUT_DIR is the working directory for fedex_plus, it must be
# created at configure time and be present when fedex_plus is run
set(SCHEMA_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA_NAME})
make_directory(${SCHEMA_OUT_DIR})
include_directories(${SCHEMA_OUT_DIR})
DISTCLEAN(${SCHEMA_OUT_DIR})

# These files are the standard fedex_plus outputs that need to be compiled.
set(fedex_srcs
  ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}.cc
  ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}.init.cc
  ${SCHEMA_OUT_DIR}/SdaiAll.cc
  ${SCHEMA_OUT_DIR}/compstructs.cc
  ${SCHEMA_OUT_DIR}/schema.cc
  )

# For the subdirectory add_custom_command definitions, need the generated
# headers as well.
set(FEDEX_OUTPUT
  ${fedex_srcs}
  ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}.h
  ${SCHEMA_OUT_DIR}/Sdaiclasses.h
  ${SCHEMA_OUT_DIR}/schema.h
  )

# Define a common custom build target that ensures the fedex_plus outputs
# are present.  Do this rather than relying on custom commands in the
# subdirectories to avoid multiple build targets trying to run fedex_plus
# simultaneously to generate identical inputs (unknown if that is problematic)
# and to add extra robustness for parallel build target ordering.
add_custom_command(OUTPUT ${SCHEMA_OUT_DIR}/step_fedex_${SCHEMA_NAME}.done
  COMMAND ${FEDEX_PLUS_EXEC} ARGS ${SCHEMA_FILE}
  COMMAND ${CMAKE_COMMAND} -E touch ${SCHEMA_OUT_DIR}/step_fedex_${SCHEMA_NAME}.done
  DEPENDS ${SCHEMA_FILE} ${FEDEX_PLUS_EXECUTABLE_TARGET}
  WORKING_DIRECTORY ${SCHEMA_OUT_DIR}
  COMMENT "Generating C++ code to express ${SCHEMA_NAME}..."
  VERBATIM)
add_custom_target(step-fedex-${SCHEMA_NAME}
  DEPENDS ${SCHEMA_OUT_DIR}/step_fedex_${SCHEMA_NAME}.done)

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
