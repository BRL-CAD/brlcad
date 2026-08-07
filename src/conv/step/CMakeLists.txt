if(BRLCAD_ENABLE_STEP)
  # STEPCODE_ROOT is also useful for testing an installed development build.
  # The normal BRL-CAD package wrapper deliberately prefers the dependency
  # staged in the build tree, so honor an explicit root before entering that
  # bundled-package path.
  if(STEPCODE_ROOT)
    find_package(STEPCODE)
    if(STEPCODE_FOUND)
      set(STEPCODE_STATUS "System" CACHE STRING "STEPcode bundled status" FORCE)
    else()
      set(STEPCODE_STATUS "NotFound" CACHE STRING "STEPcode bundled status" FORCE)
    endif()
  else()
    brlcad_find_package(STEPCODE)
  endif()
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

# Keep generated schema artifacts in one directory even though the helper is
# invoked from several importer/exporter subdirectories.
set(STEP_SCHEMA_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# exp2cxx emits deterministic, bounded unity chunks.  Keep the default small
# enough for modern AP242 schemas to compile without depending on LTO's
# deferred code generation to reduce compiler memory.  Builders with unusually
# tight memory limits may lower this at the cost of additional translation
# units.
set(
  BRLCAD_STEP_SCHEMA_CHUNK_SIZE
  "64:8"
  CACHE STRING
  "Maximum entity:type counts per generated STEPcode translation unit (N applies to both)"
)
mark_as_advanced(BRLCAD_STEP_SCHEMA_CHUNK_SIZE)
if(NOT BRLCAD_STEP_SCHEMA_CHUNK_SIZE MATCHES "^[1-9][0-9]*(:[1-9][0-9]*)?$")
  message(FATAL_ERROR "BRLCAD_STEP_SCHEMA_CHUNK_SIZE must use positive N or N:N form")
endif()

# STEPcode API v2's compact late-bound output is intended for conversion
# clients: schema structure remains complete, while thousands of generated
# early-bound C++ classes and their transitive declaration graph disappear.
# Keep these controls explicit so API v1 can still be selected while comparing
# behavior during the experimental rollout.
set(BRLCAD_STEP_GENERATED_API "2" CACHE STRING "STEPcode generated API version")
set_property(CACHE BRLCAD_STEP_GENERATED_API PROPERTY STRINGS 1 2)
option(BRLCAD_STEP_LATE_BOUND "Use compact descriptor-backed STEPcode entities" ON)
set(BRLCAD_STEP_METADATA_PROFILE "structural" CACHE STRING "STEPcode generated metadata profile")
set_property(CACHE BRLCAD_STEP_METADATA_PROFILE PROPERTY STRINGS full structural)
mark_as_advanced(
  BRLCAD_STEP_GENERATED_API
  BRLCAD_STEP_LATE_BOUND
  BRLCAD_STEP_METADATA_PROFILE
)
if(NOT BRLCAD_STEP_GENERATED_API MATCHES "^[12]$")
  message(FATAL_ERROR "BRLCAD_STEP_GENERATED_API must be 1 or 2")
endif()
if(BRLCAD_STEP_LATE_BOUND AND NOT BRLCAD_STEP_GENERATED_API STREQUAL "2")
  message(FATAL_ERROR "BRLCAD_STEP_LATE_BOUND requires STEPcode generated API 2")
endif()
if(NOT BRLCAD_STEP_METADATA_PROFILE MATCHES "^(full|structural)$")
  message(FATAL_ERROR "BRLCAD_STEP_METADATA_PROFILE must be full or structural")
endif()
if(BRLCAD_STEP_METADATA_PROFILE STREQUAL "structural" AND NOT BRLCAD_STEP_LATE_BOUND)
  message(FATAL_ERROR "structural STEPcode metadata requires late-bound API 2")
endif()

set(_step_exp2cxx_args --api-version ${BRLCAD_STEP_GENERATED_API})
if(BRLCAD_STEP_LATE_BOUND)
  list(APPEND _step_exp2cxx_args --late-bound)
endif()
list(APPEND _step_exp2cxx_args --metadata ${BRLCAD_STEP_METADATA_PROFILE})
list(APPEND _step_exp2cxx_args -k ${BRLCAD_STEP_SCHEMA_CHUNK_SIZE})
string(JOIN " " _step_generator_signature ${_step_exp2cxx_args})

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

# The converters spend their time in parsing, allocation, graph traversal and
# geometry construction rather than small cross-translation-unit hot loops.
# LTO adds substantial schema-module link cost and, now that generated schema
# sources are compiled as bounded chunks below, is not needed to keep AP242
# compiler memory under control.  Preserve all ordinary optimized-build flags
# while disabling LTO only in this STEPcode-based subtree.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  check_cxx_flag("fno-lto" VARS STEPCODE_NO_LTO_FLAG)
  if(STEPCODE_NO_LTO_FLAG)
    add_compile_options(
      "$<$<COMPILE_LANGUAGE:CXX>:${STEPCODE_NO_LTO_FLAG}>"
    )
    add_link_options(
      "$<$<LINK_LANGUAGE:CXX>:${STEPCODE_NO_LTO_FLAG}>"
    )
  endif()
endif()

function(GENERATE_SCHEMA_INPUTS SCHEMA_FILE TARGET_SUFFIX)
  if(BRLCAD_ENABLE_STEP)
    get_filename_component(SCHEMA_FILE "${SCHEMA_FILE}" REALPATH)
    # The optional third argument is the stable schema key used for the
    # generated binding target.  Multiple consumers may call this function,
    # but generation and compilation are intentionally performed only once
    # for a given key.
    if(ARGC GREATER 2)
      set(_schema_key "${ARGV2}")
    else()
      set(_schema_key "${TARGET_SUFFIX}")
    endif()
    string(TOLOWER "${_schema_key}" _schema_key)
    string(MAKE_C_IDENTIFIER "${_schema_key}" _schema_key)
    set(_bindings_target "step_schema_${_schema_key}_bindings")

    # This Schema determines which version of STEP the command will support
    # TODO - see if there is some way to automatically support multiple versions
    # read the schema name from a line like 'SCHEMA AUTOMOTIVE_DESIGN;'
    file(STRINGS ${SCHEMA_FILE} SCHEMA_STATEMENT LIMIT_COUNT 1 REGEX "SCHEMA .*")
    string(REGEX REPLACE "^SCHEMA \(.*\)\;$" "\\1" SCHEMA_N ${SCHEMA_STATEMENT})
    string(TOUPPER ${SCHEMA_N} _schema_name) # exp2cxx always uses upper case for file names

    # Because SCHEMA_OUT_DIR is the working directory for exp2cxx, it must be
    # created at configure time and be present when exp2cxx is run
    # Different editions may retain the same EXPRESS schema name while
    # generating incompatible C++ bindings and Part 21 attribute layouts.
    # An optional fourth argument gives those bindings distinct output
    # directories; the stable schema key already gives their targets distinct
    # names.
    if(ARGC GREATER 3)
      set(_schema_output_name "${ARGV3}")
    else()
      set(_schema_output_name "${_schema_name}")
    endif()
    set(_schema_out_dir ${STEP_SCHEMA_BINARY_DIR}/${_schema_output_name})

    set(_schema_manifest ${_schema_out_dir}/Sdai${_schema_name}.sources.cmake)
    set(_schema_chunk_stamp ${_schema_out_dir}/.brlcad_exp2cxx_settings)
    set(_bootstrap_schema FALSE)
    if(NOT EXISTS "${_schema_manifest}")
      set(_bootstrap_schema TRUE)
    elseif("${SCHEMA_FILE}" IS_NEWER_THAN "${_schema_manifest}")
      set(_bootstrap_schema TRUE)
    elseif("${EXP2CXX_EXECUTABLE}" IS_NEWER_THAN "${_schema_manifest}")
      set(_bootstrap_schema TRUE)
    endif()
    if(EXISTS "${_schema_chunk_stamp}")
      file(READ "${_schema_chunk_stamp}" _generated_chunk_size)
      string(STRIP "${_generated_chunk_size}" _generated_chunk_size)
    else()
      set(_generated_chunk_size "")
    endif()
    if(NOT "${_generated_chunk_size}" STREQUAL "${_step_generator_signature}")
      set(_bootstrap_schema TRUE)
    endif()

    # CMake needs the generated source list while configuring.  Generating one
    # schema takes only a few seconds even for AP242e4 and avoids either a
    # hard-coded maximum chunk count or compiling the compatibility aggregate
    # that includes every chunk back into one enormous translation unit.
    if(_bootstrap_schema)
      file(MAKE_DIRECTORY "${_schema_out_dir}")
      execute_process(
        COMMAND "${EXP2CXX_EXECUTABLE}" ${_step_exp2cxx_args} "${SCHEMA_FILE}"
        WORKING_DIRECTORY "${_schema_out_dir}"
        RESULT_VARIABLE _bootstrap_result
        OUTPUT_VARIABLE _bootstrap_output
        ERROR_VARIABLE _bootstrap_error
      )
      if(NOT _bootstrap_result EQUAL 0 OR NOT EXISTS "${_schema_manifest}")
        message(FATAL_ERROR
          "Unable to generate the ${_schema_name} STEPcode source manifest "
          "(${_bootstrap_result}):\n${_bootstrap_output}${_bootstrap_error}"
        )
      endif()
      file(WRITE "${_schema_chunk_stamp}" "${_step_generator_signature}\n")
    endif()
    if(NOT EXISTS "${_schema_out_dir}/step_express_${_schema_name}.done")
      file(TOUCH "${_schema_out_dir}/step_express_${_schema_name}.done")
    endif()

    unset(STEPCODE_GENERATED_SCHEMA_SOURCES)
    include("${_schema_manifest}")
    if(NOT STEPCODE_GENERATED_SCHEMA_SOURCES)
      message(FATAL_ERROR "${_schema_manifest} did not list generated schema sources")
    endif()
    set(_express_srcs)
    foreach(_generated_source IN LISTS STEPCODE_GENERATED_SCHEMA_SOURCES)
      if(IS_ABSOLUTE "${_generated_source}" OR "${_generated_source}" MATCHES "(^|/)\\.\\.(/|$)")
        message(FATAL_ERROR "Unsafe generated STEPcode source path: ${_generated_source}")
      endif()
      list(APPEND _express_srcs "${_schema_out_dir}/${_generated_source}")
    endforeach()
    unset(STEPCODE_GENERATED_SCHEMA_SOURCES)

    set(_express_compat_srcs)
    if(NOT BRLCAD_STEP_LATE_BOUND)
      # The aggregate implementation files remain compatibility outputs for
      # consumers that cannot use the manifest.  BRL-CAD deliberately does not
      # compile them, since each one includes all bounded chunks into a single
      # translation unit.
      set(
        _express_compat_srcs
        ${_schema_out_dir}/Sdai${_schema_name}_unity_entities.cc
        ${_schema_out_dir}/Sdai${_schema_name}_unity_types.cc
      )
    endif()
    # For the subdirectory add_custom_command definitions, need the generated
    # headers as well.
    set(
      _express_hdrs
      ${_schema_out_dir}/Sdai${_schema_name}.h
      ${_schema_out_dir}/Sdai${_schema_name}Names.h
      ${_schema_out_dir}/Sdaiclasses.h
      ${_schema_out_dir}/schema.h
    )
    if(NOT BRLCAD_STEP_LATE_BOUND)
      list(APPEND _express_hdrs
        ${_schema_out_dir}/Sdai${_schema_name}_unity_entities.h
        ${_schema_out_dir}/Sdai${_schema_name}_unity_types.h
      )
    endif()

    # We want to precompile the generated headers, since they will be reused a
    # great deal in the build.  However, for CMake's target_precompile_headers
    # function we need a file that exists at configure time to act as the
    # target.  Keep that header outside the exp2cxx output directory: the
    # generator can clear or overwrite files in SCHEMA_OUT_DIR, which leaves
    # the later PCH compile trying to include a header that no longer exists.
    set(_precompile_header ${STEP_SCHEMA_BINARY_DIR}/precompile_${_schema_output_name}.h)

    set(
      _express_output
      ${_express_srcs}
      ${_express_compat_srcs}
      ${_express_hdrs}
      ${_schema_manifest}
      ${_schema_chunk_stamp}
    )

    if(NOT TARGET ${_bindings_target})
      file(MAKE_DIRECTORY ${_schema_out_dir})
      distclean(${_schema_out_dir})
      set(_precompile_contents "// Precompiled header for STEPcode generated files\n")
      distclean("${_precompile_header}")
      foreach(header ${_express_hdrs})
        # Use relative include for portability
        get_filename_component(header_name "${header}" NAME)
        string(APPEND _precompile_contents "#include \"${header_name}\"\n")
      endforeach()
      # file(CONFIGURE) preserves the timestamp when content is unchanged.
      # Reconfiguring must not force all generated binding objects to rebuild.
      file(CONFIGURE OUTPUT "${_precompile_header}" CONTENT "${_precompile_contents}" @ONLY)

      add_custom_command(
        OUTPUT ${_express_output} ${_schema_out_dir}/step_express_${_schema_name}.done
        COMMAND ${EXP2CXX_EXECUTABLE}
        ARGS ${_step_exp2cxx_args} ${SCHEMA_FILE} > ${_schema_out_dir}/step_express_${_schema_name}.log 2>&1
        COMMAND ${CMAKE_COMMAND} -E echo ${_step_generator_signature} > ${_schema_chunk_stamp}
        COMMAND ${CMAKE_COMMAND} -E touch ${_schema_out_dir}/step_express_${_schema_name}.done
        DEPENDS ${SCHEMA_FILE} ${EXP2CXX_EXECUTABLE} ${EXP2CXX_EXECUTABLE_TARGET}
        WORKING_DIRECTORY ${_schema_out_dir}
        COMMENT "Generating C++ code to express ${_schema_name}..."
        VERBATIM
      )
      set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_schema_manifest}")

      add_custom_target(
        step-express-${_schema_key}
        DEPENDS ${_schema_out_dir}/step_express_${_schema_name}.done
      )
      set_target_properties(step-express-${_schema_key} PROPERTIES FOLDER "Compilation Utilities")

      # Generated bindings are a schema resource, not converter sources.  A
      # static PIC target lets importers, exporters and modules all reuse the
      # exact same objects.
      add_library(${_bindings_target} STATIC ${_express_srcs})
      add_dependencies(${_bindings_target} step-express-${_schema_key})
      target_include_directories(
        ${_bindings_target} BEFORE PRIVATE
        ${_schema_out_dir}
        ${STEPCODE_INCLUDE_DIRS}
      )
      target_compile_definitions(
        ${_bindings_target} PRIVATE
        BRLCADBUILD HAVE_CONFIG_H SC_SCHEMA_DLL_EXPORTS
      )
      if(BRLCAD_STEP_GENERATED_API STREQUAL "2")
        target_compile_definitions(${_bindings_target} PRIVATE STEPCODE_GENERATED_API_V2)
      endif()
      if(BRLCAD_STEP_LATE_BOUND)
        target_compile_definitions(${_bindings_target} PRIVATE STEPCODE_LATE_BOUND)
      endif()
      set_target_properties(
        ${_bindings_target} PROPERTIES
        POSITION_INDEPENDENT_CODE TRUE
        FOLDER "BRL-CAD Static Libraries/STEP"
      )
      if(COMMAND target_precompile_headers)
        target_precompile_headers(${_bindings_target} PRIVATE "${_precompile_header}")
      endif()
      if(MSVC)
        target_compile_options(${_bindings_target} PRIVATE /bigobj)
      endif()

      set(
        clean_files
        ${_express_output}
        ${_precompile_header}
        ${_schema_out_dir}/step_express_${_schema_name}.done
        ${_schema_out_dir}/step_express_${_schema_name}.log
      )
      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${clean_files}")

      set_property(GLOBAL PROPERTY STEP_SCHEMA_${_schema_key}_FILE "${SCHEMA_FILE}")
    else()
      get_property(_canonical_file GLOBAL PROPERTY STEP_SCHEMA_${_schema_key}_FILE)
      if(NOT "${SCHEMA_FILE}" STREQUAL "${_canonical_file}")
        message(FATAL_ERROR
          "STEP schema ${_schema_key} was already configured from ${_canonical_file}; "
          "${TARGET_SUFFIX} requested non-canonical input ${SCHEMA_FILE}"
        )
      endif()
    endif()

    # Some schema-bound converter sources include generated declarations, so
    # consumers still receive the directory explicitly as a private include.
    # They do not inherit it from the binding target.
    set(_consumer_precompile_header ${STEP_SCHEMA_BINARY_DIR}/precompile_${_schema_output_name}_${TARGET_SUFFIX}.h)
    set(_consumer_precompile_contents "// Precompiled header for ${TARGET_SUFFIX}\n")
    distclean("${_consumer_precompile_header}")
    foreach(header ${_express_hdrs})
      get_filename_component(header_name "${header}" NAME)
      string(APPEND _consumer_precompile_contents "#include \"${header_name}\"\n")
    endforeach()
    file(CONFIGURE
      OUTPUT "${_consumer_precompile_header}"
      CONTENT "${_consumer_precompile_contents}"
      @ONLY
    )

    # Export only names scoped to this schema consumer.  In particular, do not
    # leak SCHEMA_NAME or express_srcs into sibling converter directories.
    # Generated translation units belong exclusively to the binding library.
    set(${TARGET_SUFFIX}_express_srcs "" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_express_hdrs "${_express_hdrs}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_EXPRESS_OUTPUT "${_express_output}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_PRECOMPILE_HEADER "${_consumer_precompile_header}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_SCHEMA_NAME "${_schema_name}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_SCHEMA_OUT_DIR "${_schema_out_dir}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_SCHEMA_TARGET "${_bindings_target}" PARENT_SCOPE)
    set(${TARGET_SUFFIX}_SCHEMA_BINDINGS_TARGET "${_bindings_target}" PARENT_SCOPE)
  endif(BRLCAD_ENABLE_STEP)
endfunction(GENERATE_SCHEMA_INPUTS SCHEMA_FILE TARGET_SUFFIX)

# Record converter sources as absolute paths so a combined schema module can
# be assembled after all importer and exporter subdirectories are processed.
# Generator expressions (notably common OBJECT libraries) are retained.
function(STEP_RECORD_SCHEMA_SOURCES schema direction source_var)
  set(_absolute_sources)
  foreach(_source IN LISTS ${source_var})
    if(_source MATCHES "^\\$<")
      list(APPEND _absolute_sources "${_source}")
    elseif(IS_ABSOLUTE "${_source}")
      list(APPEND _absolute_sources "${_source}")
    else()
      get_filename_component(_absolute "${CMAKE_CURRENT_SOURCE_DIR}/${_source}" ABSOLUTE)
      list(APPEND _absolute_sources "${_absolute}")
    endif()
  endforeach()
  set_property(GLOBAL PROPERTY "STEP_${schema}_${direction}_SOURCES" "${_absolute_sources}")
endfunction()

if(BRLCAD_ENABLE_STEP)
  include(${CMAKE_CURRENT_SOURCE_DIR}/StepImportCommon.cmake)

  # Compile the libbu plugin implementation once and consume the same object
  # from both public STEP hosts.
  add_library(
    step_plugin_host_objects OBJECT
    StepPluginHost.cpp
    STEPHeaderSchema.cpp
    STEPString.cpp
  )
  target_include_directories(
    step_plugin_host_objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_link_libraries(step_plugin_host_objects PRIVATE libbu ${CMAKE_DL_LIBS})
  target_compile_definitions(step_plugin_host_objects PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  set_target_properties(
    step_plugin_host_objects PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    FOLDER "BRL-CAD Object Libraries/STEP"
  )

  add_library(
    step_export_common_objects OBJECT
    BRLCADWrapper.cpp
    STEPMetadata.cpp
    StepExportPlan.cpp
  )
  target_include_directories(
    step_export_common_objects BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/libbrep
  )
  target_compile_definitions(step_export_common_objects PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  target_link_libraries(
    step_export_common_objects PRIVATE
    libwdb librt libbrep libbn libbu OPENNURBS::OPENNURBS
  )
  set_target_properties(
    step_export_common_objects PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    FOLDER "BRL-CAD Object Libraries/STEP"
  )

  list(APPEND STEP_IMPORT_COMMON_SOURCES StepSchemaRuntime.cpp)
  if(STEPCODE_LAZY_FOUND)
    list(APPEND STEP_IMPORT_COMMON_SOURCES STEPLazySession.cpp)
  endif()
  add_library(step_import_common_objects OBJECT ${STEP_IMPORT_COMMON_SOURCES})
  target_include_directories(
    step_import_common_objects BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/step-g
    ${STEPCODE_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src/libbrep
  )
  target_compile_definitions(
    step_import_common_objects PRIVATE BRLCADBUILD HAVE_CONFIG_H
  )
  if(STEPCODE_LAZY_FOUND)
    target_compile_definitions(step_import_common_objects PRIVATE HAVE_STEPCODE_LAZY)
  endif()
  target_link_libraries(
    step_import_common_objects PRIVATE
    libwdb
    librt
    libbu
    libbrep
    ${STEPCODE_UTILS_LIBRARY}
    ${STEPCODE_EDITOR_LIBRARY}
    ${STEPCODE_DAI_LIBRARY}
    ${STEPCODE_CORE_LIBRARY}
    OPENNURBS::OPENNURBS
  )
  set_target_properties(
    step_import_common_objects PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE
    FOLDER "BRL-CAD Object Libraries/STEP"
    UNITY_BUILD_CODE_BEFORE_INCLUDE "#undef CLASSNAME\n#undef ENTITYNAME"
  )
endif()

# AP203
add_subdirectory(step-g)
add_subdirectory(g-step)

# Schema modules supply AP203e2, AP214, and AP242 support to the common
# step-g and g-step hosts.  The historical fixed-schema executables are not
# built: one public converter pair keeps normal use and schema regressions on
# the same plugin-dispatch path.
add_subdirectory(ap203e2)
add_subdirectory(ap214)
add_subdirectory(ap242)

if(BRLCAD_ENABLE_STEP)
  function(STEP_ADD_SCHEMA_PLUGIN schema property_key schema_define schema_directory import_cli export_cli)
    get_property(_has_import GLOBAL PROPERTY "STEP_${property_key}_IMPORT_SOURCES" SET)
    get_property(_has_export GLOBAL PROPERTY "STEP_${property_key}_EXPORT_SOURCES" SET)
    if(NOT _has_import OR NOT _has_export)
      return()
    endif()
    get_property(_import_sources GLOBAL PROPERTY "STEP_${property_key}_IMPORT_SOURCES")
    get_property(_export_sources GLOBAL PROPERTY "STEP_${property_key}_EXPORT_SOURCES")
    set(_plugin_sources
      ${_import_sources}
      ${_export_sources}
      ${CMAKE_CURRENT_SOURCE_DIR}/StepSchemaPlugin.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/StepSchemaRuntimeGenerated.cpp
    )
    list(REMOVE_DUPLICATES _plugin_sources)
    # BRLCADWrapper and schema-neutral traversal are compiled once in the
    # export plan object target, which is also usable by importer code.
    list(FILTER _plugin_sources EXCLUDE REGEX "/BRLCADWrapper\\.cpp$")
    list(FILTER _plugin_sources EXCLUDE REGEX "/STEPLazySession\\.cpp$")
    list(APPEND _plugin_sources $<TARGET_OBJECTS:step_export_common_objects>)

    set(_target "step-schema-${schema}")
    add_library(${_target} MODULE ${_plugin_sources})
    target_include_directories(
      ${_target} BEFORE PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${CMAKE_CURRENT_SOURCE_DIR}/step-g
      ${CMAKE_CURRENT_SOURCE_DIR}/g-step
      ${CMAKE_CURRENT_SOURCE_DIR}/ap203e2
      ${CMAKE_CURRENT_SOURCE_DIR}/ap214
      ${CMAKE_CURRENT_SOURCE_DIR}/ap242
      ${STEP_SCHEMA_BINARY_DIR}/${schema_directory}
      ${STEPCODE_INCLUDE_DIRS}
      ${CMAKE_SOURCE_DIR}/src/libbrep
    )
    target_compile_definitions(
      ${_target} PRIVATE
      BRLCADBUILD
      HAVE_CONFIG_H
      STEP_PLUGIN_BUILD
      ${schema_define}
      SC_SCHEMA_DLL_EXPORTS
      "STEP_PLUGIN_SCHEMA=\"${schema}\""
      "STEP_PLUGIN_SCHEMA_DISPLAY=\"${schema}\""
      "STEP_PLUGIN_COMMAND_PREFIX=\"step.${schema}\""
      "STEP_PLUGIN_IMPORT_CLI=${import_cli}"
      "STEP_PLUGIN_EXPORT_CLI=${export_cli}"
    )
    if(schema STREQUAL "ap242e1")
      target_compile_definitions(${_target} PRIVATE AP242E1)
    elseif(schema STREQUAL "ap242e2")
      target_compile_definitions(${_target} PRIVATE AP242E2)
    elseif(schema STREQUAL "ap242e3")
      target_compile_definitions(${_target} PRIVATE AP242E3)
    elseif(schema STREQUAL "ap242e4")
      target_compile_definitions(${_target} PRIVATE AP242E4)
    endif()
    if(STEPCODE_LAZY_FOUND)
      target_compile_definitions(${_target} PRIVATE HAVE_STEPCODE_LAZY)
    endif()
    target_link_libraries(
      ${_target} PRIVATE
      libwdb
      librt
      libbrep
      libbn
      libbu
      step_schema_${schema}_bindings
      ${STEPCODE_UTILS_LIBRARY}
      ${STEPCODE_EDITOR_LIBRARY}
      ${STEPCODE_DAI_LIBRARY}
      ${STEPCODE_CORE_LIBRARY}
      OPENNURBS::OPENNURBS
    )
    if(STEPCODE_LAZY_FOUND)
      target_link_libraries(${_target} PRIVATE ${STEPCODE_LAZY_LIBRARY})
    endif()
    add_dependencies(${_target} step_schema_${schema}_bindings)
    set_target_properties(
      ${_target} PROPERTIES
      POSITION_INDEPENDENT_CODE TRUE
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN TRUE
      UNITY_BUILD ${BRLCAD_ENABLE_UNITY_BUILD}
      UNITY_BUILD_CODE_BEFORE_INCLUDE "#undef CLASSNAME\n#undef ENTITYNAME"
    )
    plugin_setup(${_target} step)
  endfunction()

  step_add_schema_plugin(
    ap203 AP203 AP203 CONFIG_CONTROL_DESIGN
    step_ap203_import_cli step_ap203_export_cli
  )
  step_add_schema_plugin(
    ap203e2 AP203E2 AP203e2
    AP203_CONFIGURATION_CONTROLLED_3D_DESIGN_OF_MECHANICAL_PARTS_AND_ASSEMBLIES_MIM_LF
    step_ap203e2_import_cli step_ap203e2_export_cli
  )
  step_add_schema_plugin(
    ap214 AP214 AP214e3 AUTOMOTIVE_DESIGN
    step_ap214_import_cli step_ap214_export_cli
  )
  step_add_schema_plugin(
    ap242e1 AP242E1 AP242
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E1
    step_ap242_import_cli step_ap242_export_cli
  )
  step_add_schema_plugin(
    ap242e2 AP242E2 AP242
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E2
    step_ap242_import_cli step_ap242_export_cli
  )
  step_add_schema_plugin(
    ap242e3 AP242E3 AP242
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E3
    step_ap242_import_cli step_ap242_export_cli
  )
  step_add_schema_plugin(
    ap242e4 AP242E4 AP242
    AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF_E4
    step_ap242_import_cli step_ap242_export_cli
  )

  set(STEP_PLUGIN_HAVE_AP203 0)
  set(STEP_PLUGIN_HAVE_AP203E2 0)
  set(STEP_PLUGIN_HAVE_AP214 0)
  set(STEP_PLUGIN_HAVE_AP242E1 0)
  set(STEP_PLUGIN_HAVE_AP242E2 0)
  set(STEP_PLUGIN_HAVE_AP242E3 0)
  set(STEP_PLUGIN_HAVE_AP242E4 0)
  if(TARGET step-schema-ap203)
    set(STEP_PLUGIN_HAVE_AP203 1)
  endif()
  if(TARGET step-schema-ap203e2)
    set(STEP_PLUGIN_HAVE_AP203E2 1)
  endif()
  if(TARGET step-schema-ap214)
    set(STEP_PLUGIN_HAVE_AP214 1)
  endif()
  if(TARGET step-schema-ap242e1)
    set(STEP_PLUGIN_HAVE_AP242E1 1)
  endif()
  if(TARGET step-schema-ap242e2)
    set(STEP_PLUGIN_HAVE_AP242E2 1)
  endif()
  if(TARGET step-schema-ap242e3)
    set(STEP_PLUGIN_HAVE_AP242E3 1)
  endif()
  if(TARGET step-schema-ap242e4)
    set(STEP_PLUGIN_HAVE_AP242E4 1)
  endif()
  configure_file(StepPluginConfig.h.in StepPluginConfig.h @ONLY)
endif()

# FIXME: the IFC converter is incomplete
add_subdirectory(ifc-g)

add_subdirectory(util)

if(BRLCAD_ENABLE_STEP)
  target_sources(step-g PRIVATE $<TARGET_OBJECTS:step_plugin_host_objects>)
  target_sources(g-step PRIVATE $<TARGET_OBJECTS:step_plugin_host_objects>)
  target_link_libraries(step-g ${CMAKE_DL_LIBS})
  target_link_libraries(g-step ${CMAKE_DL_LIBS})
  foreach(_schema ap203 ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
    if(TARGET step-schema-${_schema})
      add_dependencies(step-g step-schema-${_schema})
      add_dependencies(g-step step-schema-${_schema})
    endif()
  endforeach()

  if(BUILD_TESTING)
    add_executable(
      step_plugin_api_test
      tests/step_plugin_api.cpp
      $<TARGET_OBJECTS:step_plugin_host_objects>
    )
    target_include_directories(step_plugin_api_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(step_plugin_api_test PRIVATE libbu ${CMAKE_DL_LIBS})
    set_target_properties(step_plugin_api_test PROPERTIES FOLDER "BRL-CAD Tests/STEP")
    brlcad_add_test(NAME step_plugin_api COMMAND step_plugin_api_test)

    add_executable(
      step_header_schema_test
      tests/step_header_schema.cpp
      STEPHeaderSchema.cpp
      STEPString.cpp
    )
    target_include_directories(step_header_schema_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(step_header_schema_test PRIVATE libbu)
    set_target_properties(step_header_schema_test PROPERTIES FOLDER "BRL-CAD Tests/STEP")
    brlcad_add_test(NAME step_header_schema COMMAND step_header_schema_test)

    add_executable(
      step_metadata_test
      tests/step_metadata.cpp
      STEPMetadata.cpp
    )
    target_include_directories(step_metadata_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(step_metadata_test PRIVATE librt libbn libbu)
    set_target_properties(step_metadata_test PROPERTIES FOLDER "BRL-CAD Tests/STEP")
    brlcad_add_test(NAME step_metadata COMMAND step_metadata_test)

    add_executable(
      step_plugin_load_test
      tests/step_plugin_load.cpp
      $<TARGET_OBJECTS:step_plugin_host_objects>
    )
    target_include_directories(step_plugin_load_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(step_plugin_load_test PRIVATE libbu ${CMAKE_DL_LIBS})
    set_target_properties(step_plugin_load_test PROPERTIES FOLDER "BRL-CAD Tests/STEP")
    add_library(step_plugin_bad_abi MODULE tests/step_plugin_bad_abi.cpp)
    target_include_directories(step_plugin_bad_abi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(step_plugin_bad_abi PRIVATE libbu)
    set_target_properties(step_plugin_bad_abi PROPERTIES FOLDER "BRL-CAD Tests/STEP")
    brlcad_add_test(
      NAME step_plugin_reject_bad_abi
      COMMAND step_plugin_load_test $<TARGET_FILE:step_plugin_bad_abi> reject
    )
    foreach(_schema ap203 ap203e2 ap214 ap242e1 ap242e2 ap242e3 ap242e4)
      if(TARGET step-schema-${_schema})
        brlcad_add_test(
          NAME step_plugin_load_${_schema}
          COMMAND step_plugin_load_test
            $<TARGET_FILE:step-schema-${_schema}>
            ${_schema}
            step.${_schema}.import
        )
      endif()
    endforeach()
    brlcad_add_test(
      NAME step_enum_schemas
      COMMAND ${CMAKE_COMMAND}
        -DSTEP_G=$<TARGET_FILE:step-g>
        -DMGED=$<TARGET_FILE:mged>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/step-g/tests/ap203_unclamped_closed_curve_knots.stp
        -DWORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/step_enum_schemas.cmake
    )
  endif()
endif()

cmakefiles(
  AP_Common.h
  BRLCADWrapper.h
  CMakeLists.txt
  README.csg.md
  STEPMechanicalExport.cpp
  STEPMechanicalExport.h
  STEPConfigurationExport.cpp
  STEPConfigurationExport.h
  STEPConfigurationIdentityExport.cpp
  STEPConfigurationIdentityExport.h
  STEPExportMetadata.cpp
  STEPExportMetadata.h
  STEPExportContext.h
  STEPGeneratedAPI.h
  STEPMetadata.cpp
  STEPMetadata.h
  STEPMaterialExport.cpp
  STEPMaterialExport.h
  STEPMaterialImport.cpp
  STEPMaterialImport.h
  STEPPresentation.cpp
  STEPPresentation.h
  STEPSweptSolid.cpp
  STEPSweptSolid.h
  STEPTessellatedMesh.cpp
  STEPTessellatedMesh.h
  STEPNativeCSGExport.cpp
  STEPNativeCSGExport.h
  STEPNativeCSGImport.cpp
  STEPNativeCSGImport.h
  STEPBudget.h
  STEPBRepFallback.h
  STEPBrepFinalize.cpp
  STEPBrepLoopRepair.cpp
  STEPBrepPeriodic.cpp
  STEPBrepPullback.cpp
  STEPBrepRepairInternal.h
  STEPBrepSeamRepair.cpp
  STEPBrepTopology.cpp
  STEPConversionStatus.h
  STEPBrepValidation.cpp
  STEPBrepValidation.h
  STEPDocument.h
  STEPGeometricSet.cpp
  STEPGeometricSet.h
  STEPImportInternal.h
  STEPImportPipeline.cpp
  STEPImportArchitecture.md
  STEPReport.cpp
  STEPReport.h
  STEPString.cpp
  STEPString.h
  STEPUnit.h
  STEPHeaderSchema.cpp
  STEPHeaderSchema.h
  StepExportPlan.cpp
  StepExportPlan.h
  STEPProductExport.cpp
  STEPWrapperDocument.inc
  STEPWrapper.h
  StepImportCommon.cmake
  StepPluginHost.cpp
  StepPluginHost.h
  StepPluginConfig.h.in
  StepSchemaPlugin.cpp
  StepSchemaRuntime.cpp
  StepSchemaRuntime.h
  StepSchemaRuntimeGenerated.cpp
  StepImportHost.cpp
  StepExportHost.cpp
  step_plugin.h
  tests/step_plugin_api.cpp
  tests/step_header_schema.cpp
  tests/step_metadata.cpp
  tests/step_enum_schemas.cmake
  tests/step_plugin_load.cpp
  tests/step_plugin_bad_abi.cpp
  tests/step_direct_advanced_brep.cmake
  tests/step_direct_advanced_brep.stp.in
  tests/step_brep_with_voids_baseline.cmake
  tests/step_invalid_brep_policy.cmake
  tests/step_invalid_surface_brep.stp.in
  tests/step_malformed_product_representation.stp.in
  tests/step_mapped_assembly_baseline.cmake
  tests/step_non_geometric_product.stp.in
  tests/step_partial_policy_outcome.cmake
  tests/step_schema_policy_outcomes.cmake
  tests/step_surface_models_baseline.cmake
  tests/step_unsupported_product_geometry.stp.in
  tests/step_wireframe.stp.in
  tests/step_wireframe_baseline.cmake
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
