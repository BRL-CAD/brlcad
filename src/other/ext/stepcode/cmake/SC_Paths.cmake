macro(REVERSE_PATH in_path out_path drive_path)
  set(reversed_path)
  set(tmp_subpath "${in_path}")
  while(NOT "${tmp_subpath}" STREQUAL "" AND NOT "${tmp_subpath}" STREQUAL "/")
      get_filename_component(piece "${tmp_subpath}" NAME)
      get_filename_component(tmp_subpath "${tmp_subpath}" PATH)
      if(piece)
	set(reversed_path "${reversed_path}/${piece}")
      else(piece)
	set(${drive_path} ${tmp_subpath})
	set(tmp_subpath)
      endif(piece)
  endwhile(NOT "${tmp_subpath}" STREQUAL "" AND NOT "${tmp_subpath}" STREQUAL "/")
  set("${out_path}" "${reversed_path}")
endmacro(REVERSE_PATH)

macro(GET_COMMON_ROOT_PATH dir1 dir2 common_subpath)
  get_filename_component(testpath "${dir1}" REALPATH)
  set(dir1_rev)
  set(dir2_rev)
  REVERSE_PATH("${dir1}" dir1_rev drive_name_1)
  REVERSE_PATH("${dir2}" dir2_rev drive_name_2)
  if("${drive_name_1}" STREQUAL "${drive_name_2}")
    set(component_same 1)
    set(${common_subpath})
    while(component_same)
      get_filename_component(piece1 "${dir1_rev}" NAME)
      get_filename_component(dir1_rev "${dir1_rev}" PATH)
      get_filename_component(piece2 "${dir2_rev}" NAME)
      get_filename_component(dir2_rev "${dir2_rev}" PATH)
      if("${piece1}" STREQUAL "${piece2}")
	set(${common_subpath} "${${common_subpath}}/${piece1}")
      else("${piece1}" STREQUAL "${piece2}")
	set(component_same 0)
      endif("${piece1}" STREQUAL "${piece2}")
    endwhile(component_same)
    if(drive_name_1)
      set(${common_subpath} "${drive_name_1}${${common_subpath}}")
      string(REPLACE "//" "/" ${common_subpath} "${${common_subpath}}")
    endif(drive_name_1)
  endif("${drive_name_1}" STREQUAL "${drive_name_2}")
endmacro(GET_COMMON_ROOT_PATH)


macro(RELATIVE_PATH_TO_TOPLEVEL current_dir rel_path)
  set(common_root_path)
  GET_COMMON_ROOT_PATH("${SC_SOURCE_DIR}" "${current_dir}" common_root_path)
  string(REPLACE "${common_root_path}" "" subpath "${current_dir}")
  string(REPLACE "${common_root_path}" "" needed_src_path "${SC_SOURCE_DIR}")
  string(REGEX REPLACE "^/" "" subpath "${subpath}")
  string(REGEX REPLACE "^/" "" needed_src_path "${needed_src_path}")
  string(LENGTH "${subpath}" PATH_LENGTH)
  if(PATH_LENGTH GREATER 0)
    set(${rel_path} "..")
    get_filename_component(subpath "${subpath}" PATH)
    string(LENGTH "${subpath}" PATH_LENGTH)
    while(PATH_LENGTH GREATER 0)
      set(${rel_path} "${${rel_path}}/..")
      get_filename_component(subpath "${subpath}" PATH)
      string(LENGTH "${subpath}" PATH_LENGTH)
    endwhile(PATH_LENGTH GREATER 0)
  endif(PATH_LENGTH GREATER 0)
  set(${rel_path} "${${rel_path}}/${needed_src_path}")
  string(REPLACE "//" "/" ${rel_path} "${${rel_path}}")
endmacro(RELATIVE_PATH_TO_TOPLEVEL current_dir rel_path)

macro(LOCATE_SCHEMA SCHEMA_FILE _res_var)
  if(EXISTS "${CMAKE_BINARY_DIR}/${SCHEMA_FILE}")  #is it a path relative to build dir?
    set(${_res_var} "${CMAKE_BINARY_DIR}/${SCHEMA_FILE}")
  elseif(EXISTS "${SC_SOURCE_DIR}/data/${SCHEMA_FILE}")  # path relative to STEPcode/data?
    set(${_res_var} "${SC_SOURCE_DIR}/data/${SCHEMA_FILE}")
  elseif(EXISTS ${SCHEMA_FILE}) # already an absolute path
    set(${_res_var} ${SCHEMA_FILE})
  else()
    message(FATAL_ERROR "Cannot find ${CMAKE_BINARY_DIR}/${SCHEMA_FILE} or ${SC_SOURCE_DIR}/data/${SCHEMA_FILE}/*.exp or ${SCHEMA_FILE}")
  endif()

  if(IS_DIRECTORY ${${_res_var}}) #if it is a dir, look for one .exp file inside
    file(GLOB ${_res_var} ${${_res_var}}/*.exp)
  endif()

  if(NOT EXISTS ${${_res_var}})
    message(FATAL_ERROR "Expected one express file. Found '${${_res_var}}' instead.")
  endif()
endmacro(LOCATE_SCHEMA SCHEMA_FILE _res_var)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

