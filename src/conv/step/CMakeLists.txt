if(BRLCAD_ENABLE_STEP)
  brlcad_find_package(STEPCODE)
  if (NOT STEPCODE_FOUND)
    message("BRLCAD_ENABLE_STEP is on, but STEPCODE not found - disabling.")
    set(BRLCAD_ENABLE_STEP OFF)
  endif()
endif(BRLCAD_ENABLE_STEP)

# Because the exp2cxx outputs are used by both step-g and g-step,
# the logic both commands need is the same and is defined up front.

# Quiet MSVC warnings for all step converters due to SDAI sources and
# STEPcode headers used pervasively.  We intentionally do not quell
# all warnings since there are plenty of valid detectable issues also
# getting reported.
#
# FIXME: make STEPcode sources compile free of warnings
check_cxx_flag("wd4065") # switch contains default but no case labels
check_cxx_flag("wd4251") # exporting STL objects
check_cxx_flag("wd4800") # implicit conversions to bool

# Also quell a GCC warning for similar reasons.
check_cxx_flag("Wno-ignored-qualifiers")

# GCC enables assignment-level variable tracking implicitly for optimized
# builds with debug information.  Large exp2cxx translation units exceed its
# internal tracking limit, so GCC discards that work and compiles them again
# without it.  With LTO enabled the same discarded work can recur while
# linking.  Debugging STEPcode-generated bindings does not benefit enough to
# justify that cost, so test the standard negative option and apply it to C++
# compilation and linking in this STEPcode-based subtree only.
check_cxx_flag(
  "fno-var-tracking-assignments"
  VARS STEPCODE_SCHEMA_NO_VAR_TRACKING_ASSIGNMENTS_FLAG
)
if(STEPCODE_SCHEMA_NO_VAR_TRACKING_ASSIGNMENTS_FLAG)
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:${STEPCODE_SCHEMA_NO_VAR_TRACKING_ASSIGNMENTS_FLAG}>"
  )
  add_link_options(
    "$<$<LINK_LANGUAGE:CXX>:${STEPCODE_SCHEMA_NO_VAR_TRACKING_ASSIGNMENTS_FLAG}>"
  )
endif()

function(GENERATE_SCHEMA_INPUTS SCHEMA_FILE TARGET_SUFFIX)
  if(BRLCAD_ENABLE_STEP)
    # This Schema determines which version of STEP the command will support
    # TODO - see if there is some way to automatically support multiple versions
    # read the schema name from a line like 'SCHEMA AUTOMOTIVE_DESIGN;'
    file(STRINGS ${SCHEMA_FILE} SCHEMA_STATEMENT LIMIT_COUNT 1 REGEX "SCHEMA .*")
    string(REGEX REPLACE "^SCHEMA \(.*\)\;$" "\\1" SCHEMA_N ${SCHEMA_STATEMENT})
    string(TOUPPER ${SCHEMA_N} _schema_name) # exp2cxx always uses upper case for file names

    # Because SCHEMA_OUT_DIR is the working directory for exp2cxx, it must be
    # created at configure time and be present when exp2cxx is run
    set(_schema_out_dir ${CMAKE_CURRENT_BINARY_DIR}/${_schema_name}_${TARGET_SUFFIX})
    file(MAKE_DIRECTORY ${_schema_out_dir})
    distclean(${_schema_out_dir})

    # These files are the standard exp2cxx outputs that need to be compiled.
    set(
      _express_srcs
      ${_schema_out_dir}/Sdai${_schema_name}.cc
      ${_schema_out_dir}/Sdai${_schema_name}.init.cc
      ${_schema_out_dir}/Sdai${_schema_name}_unity_entities.cc
      ${_schema_out_dir}/Sdai${_schema_name}_unity_types.cc
      ${_schema_out_dir}/SdaiAll.cc
      ${_schema_out_dir}/compstructs.cc
      ${_schema_out_dir}/schema.cc
    )
    # For the subdirectory add_custom_command definitions, need the generated
    # headers as well.
    set(
      _express_hdrs
      ${_schema_out_dir}/Sdai${_schema_name}.h
      ${_schema_out_dir}/Sdai${_schema_name}Names.h
      ${_schema_out_dir}/Sdai${_schema_name}_unity_entities.h
      ${_schema_out_dir}/Sdai${_schema_name}_unity_types.h
      ${_schema_out_dir}/Sdaiclasses.h
      ${_schema_out_dir}/schema.h
    )

    # We want to precompile the generated headers, since they will be reused a
    # great deal in the build.  However, for CMake's target_precompile_headers
    # function we need a file that exists at configure time to act as the
    # target.  Keep that header outside the exp2cxx output directory: the
    # generator can clear or overwrite files in SCHEMA_OUT_DIR, which leaves
    # the later PCH compile trying to include a header that no longer exists.
    set(_precompile_header ${CMAKE_CURRENT_BINARY_DIR}/precompile_${_schema_name}_${TARGET_SUFFIX}.h)
    file(WRITE "${_precompile_header}" "// Precompiled header for STEPcode generated files\n")
    distclean("${_precompile_header}")
    foreach(header ${_express_hdrs})
      # Use relative include for portability
      get_filename_component(header_name "${header}" NAME)
      file(APPEND "${_precompile_header}" "#include \"${header_name}\"\n")
    endforeach()

    set(
      _express_output
      ${_express_srcs}
      ${_express_hdrs}
    )

    add_custom_command(
      OUTPUT ${_express_output} ${_schema_out_dir}/step_express_${_schema_name}.done
      COMMAND ${EXP2CXX_EXECUTABLE}
      ARGS ${SCHEMA_FILE} > ${_schema_out_dir}/step_express_${_schema_name}.log 2>&1
      COMMAND ${CMAKE_COMMAND} -E touch ${_schema_out_dir}/step_express_${_schema_name}.done
      DEPENDS ${SCHEMA_FILE} ${EXP2CXX_EXECUTABLE_TARGET}
      WORKING_DIRECTORY ${_schema_out_dir}
      COMMENT "Generating C++ code to express ${_schema_name}..."
      VERBATIM
    )

    add_custom_target(
      step-express-${_schema_name}-${TARGET_SUFFIX}
      DEPENDS ${_schema_out_dir}/step_express_${_schema_name}.done
    )
    set_target_properties(step-express-${_schema_name}-${TARGET_SUFFIX} PROPERTIES FOLDER "Compilation Utilities")

    set(
      clean_files
      ${_express_output}
      ${_precompile_header}
      ${_schema_out_dir}/step_express_${_schema_name}.done
      ${_schema_out_dir}/step_express_${_schema_name}.log
    )
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${clean_files}")

    # Export only names scoped to this schema consumer.  In particular, do not
    # leak SCHEMA_NAME or express_srcs into sibling converter directories.
    set(${TARGET_SUFFIX}_express_srcs "${_express_srcs}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_express_hdrs "${_express_hdrs}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_EXPRESS_OUTPUT "${_express_output}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_PRECOMPILE_HEADER "${_precompile_header}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_SCHEMA_NAME "${_schema_name}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_SCHEMA_OUT_DIR "${_schema_out_dir}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_SCHEMA_TARGET "step-express-${_schema_name}-${TARGET_SUFFIX}" PARENT_SCOPE)
  endif(BRLCAD_ENABLE_STEP)
endfunction(GENERATE_SCHEMA_INPUTS SCHEMA_FILE TARGET_SUFFIX)

# AP203
add_subdirectory(step-g)
add_subdirectory(g-step)

# Support for AP203e2, AP214 and AP242 is under development and not
# yet functional.  Since the large C++ files generated for these
# tools take a hideously long time to build on Windows, and are
# noticeable even on other platforms, wrap them with conditionals
# until they do something useful.  It may be that work in STEPcode
# to generate multiple files instead of single huge files will
# help alleviate the issue - something to check into.
add_subdirectory(g-ap203e2)
add_subdirectory(ap203e2-g)
add_subdirectory(ap214-g)
add_subdirectory(g-ap214)
add_subdirectory(ap242-g)
add_subdirectory(g-ap242)

# FIXME: the IFC converter is incomplete
add_subdirectory(ifc-g)

add_subdirectory(util)

cmakefiles(
  AP_Common.h
  BRLCADWrapper.h
  CMakeLists.txt
  STEPBudget.h
  STEPDocument.h
  STEPString.cpp
  STEPString.h
  STEPWrapperBrep.inc
  STEPWrapperDocument.inc
  STEPWrapper.h
  ap203_elements.txt
  ap203e2_elements.txt
  ap214e3_elements.txt
  ap_schema.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
