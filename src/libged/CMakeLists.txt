# LIBGED is set up based on a plugin architecture, with common functionality
# defined in a core library.

brlcad_find_package(LMDB REQUIRED)
brlcad_find_package(PNG REQUIRED)
brlcad_find_package(REGEX REQUIRED)

add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/CMakeTmp/ged_plugins.sentinel"
  COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/CMakeTmp/ged_plugins.sentinel"
)
add_custom_target(ged_plugins_sentinel DEPENDS ${CMAKE_BINARY_DIR}/CMakeTmp/ged_plugins.sentinel)
set_target_properties(ged_plugins_sentinel PROPERTIES FOLDER "BRL-CAD Plugins/ged")

# Utility program
set(GED_CMD_SCANNER ${CMAKE_BINARY_DIR}/CMakeTmp/ged_cmd_scanner${CMAKE_EXECUTABLE_SUFFIX})
file(REMOVE "${GED_CMD_SCANNER}")
try_compile(
  ged_cmd_scanner
  "${CMAKE_BINARY_DIR}/CMakeTmp"
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ged_cmd_scanner.cpp"
  OUTPUT_VARIABLE GED_SCANNER_BLD_OUT
  COPY_FILE "${GED_CMD_SCANNER}"
  )
if (NOT EXISTS ${GED_CMD_SCANNER})
  message(FATAL_ERROR "Unable to build ged command scanner: ${GED_SCANNER_BLD_OUT}")
endif()
set(GED_CMD_LIST ${CMAKE_CURRENT_BINARY_DIR}/../ged_cmds.txt)
file(REMOVE ${GED_CMD_LIST})

# Dynamic loading of GED commands carries a runtime overhead, so by default we
# build most (but not all) of the core command set into libged proper via a
# static registration mechanism.
option(LIBGED_STATIC_CORE "Statically register most ged commands into libged" ON)

# If static core is enabled, make the compile definition visible so REGISTER_GED_COMMAND works.
if(LIBGED_STATIC_CORE)
  add_compile_definitions(LIBGED_STATIC_CORE)
endif()

# ================================================================
# Accumulation helpers for libged static-core mode
# ================================================================
# Define global accumulation properties (if not already defined)
define_property(GLOBAL PROPERTY GED_COMMAND_STATIC_SRCS BRIEF_DOCS "libged static command sources" FULL_DOCS "")
define_property(GLOBAL PROPERTY GED_COMMAND_PLUGIN_SRCS BRIEF_DOCS "libged plugin command sources" FULL_DOCS "")
define_property(GLOBAL PROPERTY GED_COMMAND_ALL_SRCS BRIEF_DOCS "all libged command sources" FULL_DOCS "")
define_property(GLOBAL PROPERTY LIBGED_ACCUM_INCLUDES BRIEF_DOCS "libged aggregated includes" FULL_DOCS "")
define_property(GLOBAL PROPERTY LIBGED_ACCUM_SYSTEM_INCLUDES BRIEF_DOCS "libged aggregated system includes" FULL_DOCS "")
define_property(GLOBAL PROPERTY LIBGED_ACCUM_LIBS BRIEF_DOCS "libged aggregated libs" FULL_DOCS "")
define_property(GLOBAL PROPERTY LIBGED_ACCUM_DEFS BRIEF_DOCS "libged aggregated defs" FULL_DOCS "")
define_property(GLOBAL PROPERTY LIBGED_ACCUM_COPTS BRIEF_DOCS "libged aggregated compile options" FULL_DOCS "")

function(_libged_accumulate_global prop)
  set(items ${ARGN})
  if(NOT items)
    return()
  endif()
  get_property(curr GLOBAL PROPERTY ${prop})
  if(NOT curr)
    set(curr "")
  endif()
  foreach(it IN LISTS items)
    # For source list properties
    if("${prop}" MATCHES "^GED_COMMAND_.*_SRCS$")
      if(IS_ABSOLUTE "${it}")
        set(abs_it "${it}")
      elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${it}")
        get_filename_component(abs_it "${CMAKE_CURRENT_SOURCE_DIR}/${it}" ABSOLUTE)
      else()
        get_filename_component(abs_it "${it}" ABSOLUTE)
      endif()
      set(val "${abs_it}")
    # For library lists (i.e., things to go in target_link_libraries)
    else()
      set(val "${it}")
      # Not a target, might be library path; expand
      if(NOT TARGET "${it}" AND NOT IS_ABSOLUTE "${it}")
	if (EXISTS "${it}")
	  get_filename_component(abs_it "${it}" ABSOLUTE)
	  set(val "${abs_it}")
	elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${it}")
	  get_filename_component(abs_it "${CMAKE_CURRENT_SOURCE_DIR}/${it}" ABSOLUTE)
	  set(val "${abs_it}")
	endif()
      endif()
    endif()
    if(val)
      list(FIND curr "${val}" _idx)
      if(_idx EQUAL -1)
        list(APPEND curr "${val}")
      endif()
    endif()
  endforeach()
  set_property(GLOBAL PROPERTY ${prop} "${curr}")
endfunction()

# Sanitize static lib list: remove self target, expand only existing targets or keep raw names.
function(_libged_sanitize_and_link_libs target libs)
  if(NOT libs)
    return()
  endif()
  # Remove duplicates
  list(REMOVE_DUPLICATES libs)
  # Remove self-link entries (libged, or accidental alias)
  list(FILTER libs EXCLUDE REGEX "^(libged|${target})$")
  if(NOT libs)
    return()
  endif()

  set(valid_libs "")
  foreach(L ${libs})
    if(TARGET ${L})
      list(APPEND valid_libs ${L})
    else()
      # Try to detect imported namespace target missing
      if("${L}" MATCHES "::")
        message(WARNING "Static libged: requested target '${L}' not found. Did you run find_package for it before ged_plugin_library? Dropping it.")
        continue()
      endif()
      # Raw library name or path - keep it
      list(APPEND valid_libs ${L})
    endif()
  endforeach()

  if(valid_libs)
    # Plain signature to match original style and avoid keyword mixing error
    target_link_libraries(${target} ${valid_libs})
  endif()
endfunction()

# ================================================================
# ged_plugin_library:
#   Arguments:
#     <name> <sources...>
#     INCLUDES <dirs...>
#     SYSTEM_INCLUDES <dirs...>
#     LIBS <libs...>
#     DEFINITIONS <defs...>
#     COMPILE_OPTIONS <opts...>
#     [PLUGIN_ONLY] [FORCE_STATIC]
# ================================================================
function(ged_plugin_library name)
  set(options PLUGIN_ONLY FORCE_STATIC)
  set(oneValueArgs "")
  set(multiValueArgs INCLUDES SYSTEM_INCLUDES LIBS DEFINITIONS COMPILE_OPTIONS)
  cmake_parse_arguments(GEDPL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  set(plugin_sources ${GEDPL_UNPARSED_ARGUMENTS})
  if(NOT plugin_sources)
    message(FATAL_ERROR "ged_plugin_library(${name}) called with no source files")
  endif()

  # Decide static vs plugin
  set(DO_STATIC FALSE)
  if(LIBGED_STATIC_CORE)
    if(GEDPL_PLUGIN_ONLY AND NOT GEDPL_FORCE_STATIC)
      set(DO_STATIC FALSE)
    else()
      set(DO_STATIC TRUE)
    endif()
  else()
    set(DO_STATIC FALSE)
  endif()

  if (NOT ${name} STREQUAL "ged-bot")
    set(DO_STATIC FALSE)
  endif()

  foreach(sf ${plugin_sources})
    set(wsf ${sf})
    if(NOT EXISTS ${wsf})
      set(wsf ${CMAKE_CURRENT_SOURCE_DIR}/${sf})
    endif()
    if (EXISTS ${wsf})
      # Only files that exist at configure time can define libged commands for
      # the purposes of static inclusion or API definition.  Anything
      # dynamically generated will be limited to dynamic loading.
      if(DO_STATIC)
	_libged_accumulate_global(GED_COMMAND_STATIC_SRCS ${plugin_sources})
      endif()
      _libged_accumulate_global(GED_COMMAND_ALL_SRCS ${plugin_sources})
    endif()
  endforeach()

  if(DO_STATIC)
    target_sources(libged PRIVATE ${plugin_sources})

    if(GEDPL_INCLUDES)
      _libged_accumulate_global(LIBGED_ACCUM_INCLUDES ${GEDPL_INCLUDES})
    endif()
    if(GEDPL_SYSTEM_INCLUDES)
      _libged_accumulate_global(LIBGED_ACCUM_SYSTEM_INCLUDES ${GEDPL_SYSTEM_INCLUDES})
    endif()
    if(GEDPL_LIBS)
      # accumulate, self-filter handled later
      _libged_accumulate_global(LIBGED_ACCUM_LIBS ${GEDPL_LIBS})
    endif()
    if(GEDPL_DEFINITIONS)
      _libged_accumulate_global(LIBGED_ACCUM_DEFS ${GEDPL_DEFINITIONS})
    endif()
    if(GEDPL_COMPILE_OPTIONS)
      _libged_accumulate_global(LIBGED_ACCUM_COPTS ${GEDPL_COMPILE_OPTIONS})
    endif()

    # Find package calls will need to be repeated at the parent scope,
    # so pass the info up
    get_property(pkg_calls DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.." PROPERTY BRLCAD_LOCAL_PACKAGE_CALLS)
    if(NOT pkg_calls)
      set(pkg_calls "")
    endif()
    get_property(local_calls DIRECTORY PROPERTY BRLCAD_LOCAL_PACKAGE_CALLS)
    foreach(c ${local_calls})
      list(APPEND pkg_calls "${c}")
    endforeach()
    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.." PROPERTY BRLCAD_LOCAL_PACKAGE_CALLS "${pkg_calls}")
  else()
    add_library(${name} SHARED ${plugin_sources})
    target_compile_definitions(${name} PRIVATE GED_PLUGIN BRLCADBUILD HAVE_CONFIG_H)
    plugin_setup(${name} ged)
    validate_style(${name} ${plugin_sources})
    add_dependencies(ged_plugins_sentinel ${name})

    if(GEDPL_INCLUDES)
      brlcad_include_dirs(${name} GEDPL_INCLUDES PRIVATE)
    endif()
    if(GEDPL_SYSTEM_INCLUDES)
      brlcad_include_dirs(${name} GEDPL_SYSTEM_INCLUDES PRIVATE)
    endif()

    if(GEDPL_DEFINITIONS)
      target_compile_definitions(${name} PRIVATE ${GEDPL_DEFINITIONS})
    endif()
    if(GEDPL_COMPILE_OPTIONS)
      target_compile_options(${name} PRIVATE ${GEDPL_COMPILE_OPTIONS})
    endif()

    # Inside the plugin branch of ged_plugin_library, before target_link_libraries:
    if(NOT DO_STATIC)
      # Ensure libged is linked for Windows/import-lib correctness and explicitness
      if(TARGET libged)
	list(FIND GEDPL_LIBS "libged" _has_libged)
	if(_has_libged EQUAL -1)
	  set(GEDPL_LIBS libged ${GEDPL_LIBS})
	endif()
      endif()
    endif()

    if(GEDPL_LIBS)
      target_link_libraries(${name} ${GEDPL_LIBS})
    endif()

  endif()
endfunction()


# This target is supplied so applications wanting to run libged
# commands at build time can depend on the plugins being built as well as
# libged itself (otherwise LIBGED commands might fail due to their plugins not
# yet having been built.)
add_custom_target(ged_plugins ALL DEPENDS ged_plugins_sentinel)
set_target_properties(ged_plugins PROPERTIES FOLDER "BRL-CAD Plugins")

# In addition to the standard command plugins, there are plugins for the
# ged_exec subprocess executable, which are run as independent processes
# to allow them to be terminated by the parent command without bringing
# down the parent application
add_custom_command(
  OUTPUT "${CMAKE_BINARY_DIR}/ged_subprocesses.sentinel"
  COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/ged_subprocesses.sentinel"
)
add_custom_target(ged_subprocesses_sentinel DEPENDS ${CMAKE_BINARY_DIR}/ged_subprocesses.sentinel)
set_target_properties(ged_subprocesses_sentinel PROPERTIES FOLDER "BRL-CAD Plugins/ged_process")

function(ged_subprocess_library name)
  add_library(${name} ${ARGN})
  set_target_properties(${name} PROPERTIES PREFIX "")
  set(dep_includes)
  foreach(ll ${libslist})
    if(TARGET ${ll})
      get_target_property(IDIRS ${ll} INTERFACE_INCLUDE_DIRECTORIES)
      if(IDIRS)
        list(APPEND dep_includes ${IDIRS})
      endif(IDIRS)
    endif(TARGET ${ll})
  endforeach(ll ${libslist})
  brlcad_include_dirs(${name} dep_includes PRIVATE)
  add_dependencies(ged_subprocesses_sentinel ${name})
endfunction(ged_subprocess_library name)

# We need to define a couple of generated filenames here so
# the libged library will know to include them
set(GED_HDR_DIR ${PROJECT_BINARY_DIR}/${INCLUDE_DIR}/brlcad/ged)
file(MAKE_DIRECTORY ${GED_HDR_DIR})
set(GED_CMD_HDR ${GED_HDR_DIR}/ged_cmds.h)
distclean("${GED_CMD_HDR}")
set(GED_CMD_SRC ${CMAKE_CURRENT_BINARY_DIR}/ged_cmds.cpp)
distclean("${GED_CMD_SRC}")
set(GED_STATIC_REGISTRATION_CPP ${CMAKE_CURRENT_BINARY_DIR}/ged_static_registration.cpp)
distclean("${GED_STATIC_REGISTRATION_CPP}")
message("GED_CMD_SRC: ${GED_CMD_SRC}")
message("GED_STATIC_REGISTRATION_CPP : ${GED_STATIC_REGISTRATION_CPP}")

# This target is supplied so applications wanting to run libged
# commands at build time can depend on the subprocesses being built as well as
# libged itself (otherwise LIBGED commands might fail due to their subprocesses not
# yet having been built.)
add_custom_target(ged_subprocesses ALL DEPENDS ged_subprocesses_sentinel)
set_target_properties(ged_subprocesses PROPERTIES FOLDER "BRL-CAD Plugins")

set(
  LIBGED_SOURCES
  ${GED_CMD_SRC}
  ${GED_STATIC_REGISTRATION_CPP}
  exec.cpp
  ged_init.cpp
  columns.c
  dbi_state.cpp
  display_list.c
  draw.cpp
  ged.cpp
  ged_util.cpp
  get_obj_bounds.c
  get_solid_kp.c
  inside.c
  path.c
  pnts_util.c
  points_eval.c
  polyclip.cpp
  qray.cpp
  tab_complete.cpp
  trace.c
  track.c
  vers.c
  vutil.c
  view/data_lines.c
  wdb_importFg4Section.c
  wireframe_eval.c
)

set_property(
  SOURCE ged_init.cpp
  APPEND
  PROPERTY COMPILE_DEFINITIONS "GED_PLUGIN_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\""
)

# Note - libged_deps is defined by ${BRLCAD_SOURCE_DIR}/src/source_dirs.cmake
set(
  GED_LIBS
  ${libged_deps}
  ${PNG_LIBRARIES}
  ${LMDB_LIBRARIES}
  ${REGEX_LIBRARIES}
  ${WINSOCK_LIB}
  ${M_LIBRARY}
)

# Local include directories
# includes from plugins
set(
  GED_LOCAL_INCLUDE_DIRS
  ${PNG_INCLUDE_DIRS}
  ${REGEX_INCLUDE_DIRS}
  ${LMDB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/../libbg
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}
)

brlcad_addlib(libged "${LIBGED_SOURCES}" "${GED_LIBS}" "" "${GED_LOCAL_INCLUDE_DIRS}")
set_target_properties(libged PROPERTIES VERSION 20.0.1 SOVERSION 20)

# This executable is used to launch processes from LIBGED commands that may
# need to be terminated due to excessive runtime length or infinite looping.
# It is NOT intended be run directly by users (although it CAN be so run
# for debugging purposes if necessary.)
brlcad_addexec(ged_exec ged_exec.cpp libged NO_MAN)

add_subdirectory(tests)
add_subdirectory(simulate/tests)

brlcad_addexec(test_help help/test_help.c libged TEST)

# Plugins
add_subdirectory(3ptarb)
add_subdirectory(adc)
add_subdirectory(adjust)
add_subdirectory(ae2dir)
add_subdirectory(analyze)
add_subdirectory(annotate)
add_subdirectory(arb)
add_subdirectory(arced)
add_subdirectory(arot)
add_subdirectory(attr)
add_subdirectory(bb)
add_subdirectory(bev)
add_subdirectory(blast)
add_subdirectory(bo)
add_subdirectory(bot)
add_subdirectory(brep)
add_subdirectory(cat)
add_subdirectory(cc)
add_subdirectory(check)
add_subdirectory(clear)
add_subdirectory(clone)
add_subdirectory(close)
add_subdirectory(coil)
add_subdirectory(color)
add_subdirectory(comb)
add_subdirectory(comb_color)
add_subdirectory(comb_std)
add_subdirectory(combmem)
add_subdirectory(concat)
add_subdirectory(constraint)
add_subdirectory(copy)
add_subdirectory(copyeval)
add_subdirectory(copymat)
add_subdirectory(cpi)
add_subdirectory(dbip)
add_subdirectory(debug)
add_subdirectory(debugbu)
add_subdirectory(debugdir)
add_subdirectory(debuglib)
add_subdirectory(debugnmg)
add_subdirectory(decompose)
add_subdirectory(delay)
add_subdirectory(dir2ae)
add_subdirectory(dm)
add_subdirectory(draw)
add_subdirectory(dsp)
add_subdirectory(dump)
add_subdirectory(dup)
add_subdirectory(eac)
add_subdirectory(echo)
add_subdirectory(edcodes)
add_subdirectory(edcomb)
add_subdirectory(edit)
add_subdirectory(editit)
add_subdirectory(edmater)
add_subdirectory(env)
add_subdirectory(erase)
add_subdirectory(exists)
add_subdirectory(expand)
add_subdirectory(eye_pos)
add_subdirectory(facetize)
add_subdirectory(fb2pix)
add_subdirectory(fbclear)
add_subdirectory(find)
add_subdirectory(form)
add_subdirectory(fracture)
add_subdirectory(garbage_collect)
add_subdirectory(gdiff)
add_subdirectory(get)
add_subdirectory(get_autoview)
add_subdirectory(get_comb)
add_subdirectory(get_eyemodel)
add_subdirectory(get_type)
add_subdirectory(glob)
add_subdirectory(gqa)
add_subdirectory(graph)
add_subdirectory(grid)
add_subdirectory(grid2model_lu)
add_subdirectory(grid2view_lu)
add_subdirectory(group)
add_subdirectory(heal)
add_subdirectory(help)
add_subdirectory(hide)
add_subdirectory(how)
add_subdirectory(human)
add_subdirectory(illum)
add_subdirectory(importFg4Section)
add_subdirectory(inside)
add_subdirectory(instance)
add_subdirectory(isize)
add_subdirectory(item)
add_subdirectory(joint)
add_subdirectory(joint2)
add_subdirectory(keep)
add_subdirectory(keypoint)
add_subdirectory(kill)
add_subdirectory(killall)
add_subdirectory(killrefs)
add_subdirectory(killtree)
add_subdirectory(label)
add_subdirectory(lc)
add_subdirectory(libfuncs)
add_subdirectory(lint)
add_subdirectory(list)
add_subdirectory(lod)
add_subdirectory(log)
add_subdirectory(ls)
add_subdirectory(lt)
add_subdirectory(m2v_point)
add_subdirectory(make)
add_subdirectory(make_name)
add_subdirectory(man)
add_subdirectory(match)
add_subdirectory(mater)
add_subdirectory(material)
add_subdirectory(metaball)
add_subdirectory(mirror)
add_subdirectory(model2grid_lu)
add_subdirectory(model2view)
add_subdirectory(model2view_lu)
add_subdirectory(move)
add_subdirectory(move_all)
add_subdirectory(move_arb_edge)
add_subdirectory(move_arb_face)
add_subdirectory(mrot)
add_subdirectory(nirt)
add_subdirectory(nmg)
add_subdirectory(npush)
add_subdirectory(ocenter)
add_subdirectory(open)
add_subdirectory(orient)
add_subdirectory(orotate)
add_subdirectory(oscale)
add_subdirectory(otranslate)
add_subdirectory(overlay)
add_subdirectory(pathlist)
add_subdirectory(pathsum)
add_subdirectory(perspective)
add_subdirectory(pipe)
add_subdirectory(pix2fb)
add_subdirectory(plot)
add_subdirectory(pmat)
add_subdirectory(pmodel2view)
add_subdirectory(png)
add_subdirectory(png2fb)
add_subdirectory(pnts)
add_subdirectory(prcolor)
add_subdirectory(prefix)
add_subdirectory(process)
add_subdirectory(ps)
add_subdirectory(pset)
add_subdirectory(pull)
add_subdirectory(push)
add_subdirectory(put)
add_subdirectory(put_comb)
add_subdirectory(putmat)
add_subdirectory(qray)
add_subdirectory(quit)
add_subdirectory(rcodes)
add_subdirectory(rect)
add_subdirectory(red)
add_subdirectory(regdef)
add_subdirectory(region)
add_subdirectory(remove)
add_subdirectory(rfarb)
add_subdirectory(rmap)
add_subdirectory(rmat)
add_subdirectory(rmater)
add_subdirectory(rot)
add_subdirectory(rot_point)
add_subdirectory(rrt)
add_subdirectory(rt)
add_subdirectory(rtabort)
add_subdirectory(rtcheck)
add_subdirectory(rtwizard)
add_subdirectory(savekey)
add_subdirectory(scale)
add_subdirectory(search)
add_subdirectory(select)
add_subdirectory(set_output_script)
add_subdirectory(set_transparency)
add_subdirectory(set_uplotOutputMode)
add_subdirectory(setview)
add_subdirectory(shaded_mode)
add_subdirectory(shader)
add_subdirectory(shells)
add_subdirectory(showmats)
add_subdirectory(simulate)
add_subdirectory(slew)
add_subdirectory(solid_report)
add_subdirectory(solids_on_ray)
add_subdirectory(sphgroup)
add_subdirectory(stat)
add_subdirectory(summary)
add_subdirectory(sync)
add_subdirectory(tables)
add_subdirectory(tire)
add_subdirectory(title)
add_subdirectory(tol)
add_subdirectory(tops)
add_subdirectory(tra)
add_subdirectory(track)
add_subdirectory(tree)
add_subdirectory(typein)
add_subdirectory(unhide)
add_subdirectory(units)
add_subdirectory(v2m_point)
add_subdirectory(vdraw)
add_subdirectory(version)
add_subdirectory(view)
add_subdirectory(view2grid_lu)
add_subdirectory(view2model)
add_subdirectory(view2model_lu)
add_subdirectory(view2model_vec)
add_subdirectory(voxelize)
add_subdirectory(vrot)
add_subdirectory(wcodes)
add_subdirectory(whatid)
add_subdirectory(which)
add_subdirectory(which_shader)
add_subdirectory(who)
add_subdirectory(wmater)
add_subdirectory(xpush)
add_subdirectory(zap)
add_subdirectory(zoom)

# ================================================================
# Apply accumulated properties to libged (static-core mode only)
# ================================================================
if(LIBGED_STATIC_CORE AND TARGET libged)

  # Find package calls from plugins will need to be repeated at the parent scope,
  # so libged can find the package libraries needed to link correctly with plugin
  # code included (find_package calls in subdirectories are scoped only in that
  # subdirectory)
  message("Repeating libged plugin find_package calls for libged library...")
  get_property(pkg_calls DIRECTORY PROPERTY BRLCAD_LOCAL_PACKAGE_CALLS)
  if (pkg_calls)
    list(REMOVE_DUPLICATES pkg_calls)
  endif()
  foreach(c ${pkg_calls})
    # Undo escaped semicolons
    string(REPLACE "\\;" "<SEMICOLON>" safe_call "${c}")
    string(REPLACE "|" ";" safe_call "${safe_call}")
    string(REPLACE "<SEMICOLON>" ";" safe_call "${safe_call}")
    separate_arguments(safe_call)
    # Replay with all original arguments, and add QUIET so we don't
    # see replays of the same find_package output in the config.
    brlcad_find_package(${safe_call} QUIET)
  endforeach()
  message("Repeating libged plugin find_package calls for libged library... done.")

  foreach(prop LIBGED_ACCUM_INCLUDES LIBGED_ACCUM_SYSTEM_INCLUDES LIBGED_ACCUM_LIBS LIBGED_ACCUM_DEFS LIBGED_ACCUM_COPTS)
    get_property(val GLOBAL PROPERTY ${prop})
    if(val)
      list(REMOVE_DUPLICATES val)
    endif()
    set(${prop}_DEDUP "${val}")
  endforeach()

  # Includes
  if(LIBGED_ACCUM_INCLUDES_DEDUP)
    set(_TMP_INCS ${LIBGED_ACCUM_INCLUDES_DEDUP})
    brlcad_include_dirs(libged _TMP_INCS PRIVATE)
  endif()

  # System Includes
  if(LIBGED_ACCUM_SYSTEM_INCLUDES_DEDUP)
    set(_TMP_SYSINCS ${LIBGED_ACCUM_SYSTEM_INCLUDES_DEDUP})
    brlcad_include_dirs(libged _TMP_SYSINCS PRIVATE)
  endif()

  # Compile definitions
  if(LIBGED_ACCUM_DEFS_DEDUP)
    target_compile_definitions(libged PRIVATE ${LIBGED_ACCUM_DEFS_DEDUP})
  endif()

  # Compile options
  if(LIBGED_ACCUM_COPTS_DEDUP)
    target_compile_options(libged PRIVATE ${LIBGED_ACCUM_COPTS_DEDUP})
  endif()

  # Libraries (sanitize before linking)
  if(LIBGED_ACCUM_LIBS_DEDUP)
    _libged_sanitize_and_link_libs(libged "${LIBGED_ACCUM_LIBS_DEDUP}")
  endif()
endif()

#############################################
# Plugins defined - now generate the wrappers
message("Generating plugin wrapper files...")

# We'll need to write input files with source lists
# for the scanner
function(write_list_to_file list_var out_filename)
  # Expand the output to a full path if needed
  get_filename_component(abs_out_filename "${out_filename}" ABSOLUTE)
  file(WRITE "${abs_out_filename}" "")
  foreach(abs_item IN LISTS ${list_var})
    if(EXISTS "${abs_item}")
      file(APPEND "${abs_out_filename}" "${abs_item}\n")
    else()
      message(WARNING "Skipping non-existent source file: ${abs_item}")
    endif()
  endforeach()
endfunction()

# For pre-defined commands (static or plugin) we generate a C API and wrapper
# functions to allow our internal code to call ged_exec_<cmdname> rather than
# just raw ged_exec.  This is a minimal measure to sanity check that code is at
# least calling ged_exec for commands that exist at build time - it does not
# validate option usage, but will at least catch one category of breakage
# (using completely removed commands) at build time rather than run time.
file(REMOVE ${GED_CMD_SRC} ${GED_CMD_HDR})
get_property(GED_COMMAND_ALL_SRCS GLOBAL PROPERTY GED_COMMAND_ALL_SRCS)
set(ged_plugins_all_srcs "${CMAKE_BINARY_DIR}/CMakeTmp/ged_plugins_all_sources.txt")
write_list_to_file(GED_COMMAND_ALL_SRCS ${ged_plugins_all_srcs})
execute_process(COMMAND ${GED_CMD_SCANNER} --input-list ${ged_plugins_all_srcs} --gen-exec-api-cpp ${GED_CMD_SRC} --gen-exec-api-hdr ${GED_CMD_HDR} OUTPUT_VARIABLE SCAN_OUT ERROR_VARIABLE SCAN_OUT)
file(REMOVE ${ged_plugins_all_srcs})

# For static plugins bundled with libged, we generate
# (TODO - add description of the purpose of this file)
file(REMOVE ${GED_STATIC_REGISTRATION_CPP})
get_property(GED_COMMAND_STATIC_SRCS GLOBAL PROPERTY GED_COMMAND_STATIC_SRCS)
set(ged_plugins_static_srcs "${CMAKE_BINARY_DIR}/CMakeTmp/ged_plugins_static_sources.txt")
write_list_to_file(GED_COMMAND_STATIC_SRCS ${ged_plugins_static_srcs})
execute_process(COMMAND ${GED_CMD_SCANNER} --input-list ${ged_plugins_static_srcs} --gen-force-cpp ${GED_STATIC_REGISTRATION_CPP} OUTPUT_VARIABLE SCAN_OUT ERROR_VARIABLE SCAN_OUT)
file(REMOVE ${ged_plugins_static_srcs})

message("Generating plugin wrapper files... done.")

set(
  ged_ignore_files
  CMakeLists.txt
  NOTES
  README
  TODO
  alphanum.h
  bot/ged_bot.h
  brep/ged_brep.h
  check/check_private.h
  dbi.h
  ged_cmd_scanner.cpp
  ged_private.h
  include/plugin.h
  joint/joint.h
  osg.cpp
  pnts_util.h
  qray.h
  simulate/NOTES
  simulate/rt_collision_algorithm.hpp
  simulate/rt_collision_shape.hpp
  simulate/rt_debug_draw.hpp
  simulate/rt_instance.hpp
  simulate/rt_motion_state.hpp
  simulate/simulation.hpp
  simulate/utility.hpp
)
cmakefiles(${ged_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
