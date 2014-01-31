# Because the exp2cxx outputs are used by both step-g and g-step,
# the logic both commands need is the same and is defined up front.

set(STEP_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${WDB_INCLUDE_DIRS}
  )
LIST(REMOVE_DUPLICATES STEP_INCLUDE_DIRS)
include_directories(${STEP_INCLUDE_DIRS})

set(STEPCODE_DIR ${BRLCAD_SOURCE_DIR}/src/other/stepcode)

# local includes
include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${STEPCODE_DIR}/include
  ${STEPCODE_DIR}/src/base
  ${STEPCODE_DIR}/src/clstepcore
  ${STEPCODE_DIR}/src/cleditor
  ${STEPCODE_DIR}/src/clutils
  ${STEPCODE_DIR}/src/cldai
  ${STEPCODE_DIR}/src/test
  ${CMAKE_BINARY_DIR}/src/other/stepcode/include
  ${TNT_INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/src/libbrep #Temporary until headers reworked
  )

macro(GENERATE_SCHEMA_INPUTS SCHEMA_FILE TARGET_SUFFIX)
  # This Schema determines which version of STEP the command will support
  # TODO - see if there is some way to automatically support multiple versions
  # read the schema name from a line like 'SCHEMA AUTOMOTIVE_DESIGN;'
  file(STRINGS ${SCHEMA_FILE} SCHEMA_STATEMENT LIMIT_COUNT 1 REGEX "SCHEMA .*")
  string(REGEX REPLACE "^SCHEMA \(.*\)\;$" "\\1" SCHEMA_N ${SCHEMA_STATEMENT} )
  string(TOUPPER ${SCHEMA_N} SCHEMA_NAME) #exp2cxx always uses upper case for file names

  # Because SCHEMA_OUT_DIR is the working directory for exp2cxx, it must be
  # created at configure time and be present when exp2cxx is run
  set(SCHEMA_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA_NAME})
  make_directory(${SCHEMA_OUT_DIR})
  include_directories(${SCHEMA_OUT_DIR})
  DISTCLEAN(${SCHEMA_OUT_DIR})

  # These files are the standard exp2cxx outputs that need to be compiled.
  set(express_srcs
    ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}.cc
    ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}.init.cc
    ${SCHEMA_OUT_DIR}/SdaiAll.cc
    ${SCHEMA_OUT_DIR}/compstructs.cc
    ${SCHEMA_OUT_DIR}/schema.cc
    )

  # For the subdirectory add_custom_command definitions, need the generated
  # headers as well.
  set(EXPRESS_OUTPUT
    ${express_srcs}
    ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}.h
    ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}Helpers.h
    ${SCHEMA_OUT_DIR}/Sdai${SCHEMA_NAME}Names.h
    ${SCHEMA_OUT_DIR}/Sdaiclasses.h
    ${SCHEMA_OUT_DIR}/schema.h
    )

  # Define a common custom build target that ensures the exp2cxx outputs
  # are present.  Do this rather than relying on custom commands in the
  # subdirectories to avoid multiple build targets trying to run exp2cxx
  # simultaneously to generate identical inputs (unknown if that is problematic)
  # and to add extra robustness for parallel build target ordering.
  set(WATCHED_FILES ${EXPRESS_OUTPUT})
  string(REPLACE " " "\\ " WATCHED_FILES "${WATCHED_FILES}")
  set(STAMP_OUT_FILE ${SCHEMA_OUT_DIR}/step_express_${SCHEMA_NAME}.done)
  configure_file(${BRLCAD_CMAKE_DIR}/md5_compiletest.cmake.in
    ${SCHEMA_OUT_DIR}/md5_${SCHEMA_NAME}_test.cmake @ONLY)
  add_custom_command(OUTPUT ${EXPRESS_OUTPUT} ${SCHEMA_OUT_DIR}/step_express_${SCHEMA_NAME}.done
    COMMAND ${EXP2CXX_EXEC} ARGS ${SCHEMA_FILE}
    COMMAND ${CMAKE_COMMAND} -P ${SCHEMA_OUT_DIR}/md5_${SCHEMA_NAME}_test.cmake
    DEPENDS ${SCHEMA_FILE} ${EXP2CXX_EXECUTABLE_TARGET}
    WORKING_DIRECTORY ${SCHEMA_OUT_DIR}
    COMMENT "Generating C++ code to express ${SCHEMA_NAME}..."
    VERBATIM)
  add_custom_target(step-express-${SCHEMA_NAME}-${TARGET_SUFFIX}
    DEPENDS ${EXPRESS_OUTPUT} ${SCHEMA_OUT_DIR}/step_express_${SCHEMA_NAME}.done)

  set(clean_files ${EXPRESS_OUTPUT} ${SCHEMA_OUT_DIR}/step_express_${SCHEMA_NAME}.done)
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${clean_files}")
endmacro(GENERATE_SCHEMA_INPUTS SCHEMA_FILE)

add_subdirectory(step-g)
add_subdirectory(g-step)

# Support for AP214 and AP242 is under development and not
# yet functional.  Since the large C++ files generated for these
# tools take a hideously long time to build on Windows, and are
# noticable even on other platforms, wrap them with conditionals
# until they do something useful.  It may be that work in STEPcode
# to generate multiple files instead of single huge files will
# help alleviate the issue - something to check into.
if(BRLCAD_ENABLE_AP214)
  add_subdirectory(g-ap214)
endif(BRLCAD_ENABLE_AP214)
if(BRLCAD_ENABLE_AP242)
  add_subdirectory(g-ap242)
endif(BRLCAD_ENABLE_AP242)

CMAKEFILES(
  AP_Common.h
  BRLCADWrapper.h
  STEPWrapper.h
  ap_schema.h
  ap203_elements.txt
  ap203e2_elements.txt
  ap214e3_elements.txt
  g-ap214
  g-ap242
  )

DISTCLEAN(${SCHEMA_OUT_DIR}/make_schema)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
