cmake_minimum_required(VERSION 3.6.3)
#c++ tests that depend on a particular schema

include_directories( ${SC_SOURCE_DIR}/src/cldai ${SC_SOURCE_DIR}/src/cleditor ${SC_SOURCE_DIR}/src/clutils
  ${SC_SOURCE_DIR}/src/clstepcore ${SC_SOURCE_DIR}/src/base )

# helper function for add_schema_dependent_test
# given an sdai target, set out_path_var to the include dir
# added as a workaround for changed behavior in newer cmake
# versions (changes somewhere between 2.8 and 3.1)
function(get_sdai_incl_dir out_path_var sdai_lib)
  if(NOT "${sdai_${sdai_lib}_SOURCE_DIR}" STREQUAL "")
    set(${out_path_var} "${sdai_${sdai_lib}_SOURCE_DIR}" PARENT_SCOPE)
    return()
  endif(NOT "${sdai_${sdai_lib}_SOURCE_DIR}" STREQUAL "")
  get_target_property(sdai_srcs sdai_${sdai_lib} SOURCES)
  foreach(src ${sdai_srcs})
    if(${src} MATCHES "schema.cc$")
      get_filename_component(incl_path ${src} PATH)
      break()
    endif(${src} MATCHES "schema.cc$")
    get_filename_component(src_dir ${src} PATH)
    if(${src_dir} MATCHES "(type|entity)$")
      get_filename_component(incl_path ${src_dir} PATH)
      break()
    endif(${src_dir} MATCHES "(type|entity)$")
  endforeach(src ${sdai_srcs})
  if(NOT ${incl_path} EQUAL "")
    set(${out_path_var} ${incl_path} PARENT_SCOPE)
  else()
    message(WARNING "failed to locate incl dir for ${sdai_lib}, given ${sdai_srcs}")
  endif(NOT ${incl_path} EQUAL "")
endfunction(get_sdai_incl_dir out_path_var sdai_target)

# ${name} is used for the C++ file name (${name}.cc), and as the suffix for the target and test names
# ${sdai_lib} is the name of the schema lib that is used
# ${exe_args} are additional args for the test executable
# three optional args: ARGV3, ARGV4, ARGV5 are include paths, compile flags, and libs, respectively.
function(add_schema_dependent_test name sdai_lib exe_args )
  get_sdai_incl_dir(sdai_src_path ${sdai_lib})
  message(STATUS "include dir for ${sdai_lib}: ${sdai_src_path}")
  add_executable(tst_${name} "${name}.cc")
  set_target_properties(tst_${name} PROPERTIES EXCLUDE_FROM_ALL ON)
  if(${CMAKE_VERSION} VERSION_GREATER 2.8.7)
    # INCLUDE_DIRECTORIES property added in cmake 2.8.8
    set_property(TARGET tst_${name} APPEND PROPERTY INCLUDE_DIRECTORIES ${sdai_src_path} ${ARGV3})
  else()
    foreach(ipath ${sdai_src_path} ${ARGV3})
      set_property(TARGET tst_${name} APPEND_STRING PROPERTY COMPILE_FLAGS " -I${ipath} ")
    endforeach(ipath ${sdai_src_path} ${ARGV3})
  endif(${CMAKE_VERSION} VERSION_GREATER 2.8.7)
  if(NOT ${ARGV4} STREQUAL "")
    set_property(TARGET tst_${name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ARGV4} )
  endif(NOT ${ARGV4} STREQUAL "")
  target_link_libraries(tst_${name} sdai_${sdai_lib} ${ARGV5})
  add_test(NAME build_cpp_${name}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} --build .
    --target tst_${name}
    --config $<CONFIGURATION> )
  set_tests_properties( build_cpp_${name} PROPERTIES DEPENDS build_cpp_sdai_${sdai_lib}
    LABELS cpp_schema_specific )
  add_test( NAME test_${name}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND $<TARGET_FILE:tst_${name}> ${exe_args} )
  set_tests_properties( test_${name} PROPERTIES DEPENDS build_cpp_${name}
    LABELS cpp_schema_specific )
endfunction(add_schema_dependent_test name sdai_lib exe_args )

#if you need a schema that won't be built automatically (i.e. not in sc/data), put it in here
#temporarily disabling testing ensures that the schema will be built before these tests run
set(SC_ENABLE_TESTING OFF)
SCHEMA_CMLIST(${SC_SOURCE_DIR}/test/unitary_schemas/array_bounds_expr.exp)
SCHEMA_CMLIST(${SC_SOURCE_DIR}/test/unitary_schemas/inverse_attr.exp)
set(SC_ENABLE_TESTING ON)

add_schema_dependent_test("aggregate_bound_runtime" "array_bounds_expr"
  "${SC_SOURCE_DIR}/test/p21/test_array_bounds.p21" "" "" "stepcore")

#can't use the macro for this one because it depends on the test above and needs WILL_FAIL
add_test( NAME test_aggregate_bound_runtime_FAIL1
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMAND $<TARGET_FILE:tst_aggregate_bound_runtime> "${SC_SOURCE_DIR}/test/p21/test_array_bounds_FAIL1.p21" )
set_tests_properties( test_aggregate_bound_runtime_FAIL1 PROPERTIES
  DEPENDS test_aggregate_bound_runtime WILL_FAIL true
  LABELS cpp_schema_specific )

add_schema_dependent_test( "inverse_attr1" "inverse_attr" "${SC_SOURCE_DIR}/test/p21/test_inverse_attr.p21" )
add_schema_dependent_test( "inverse_attr2" "inverse_attr" "${SC_SOURCE_DIR}/test/p21/test_inverse_attr.p21" )
add_schema_dependent_test( "inverse_attr3" "inverse_attr" "${SC_SOURCE_DIR}/test/p21/test_inverse_attr.p21"
                            "${SC_SOURCE_DIR}/src/cllazyfile;${SC_SOURCE_DIR}/src/base/judy/src" "" "steplazyfile" )
add_schema_dependent_test( "attribute" "inverse_attr" "${SC_SOURCE_DIR}/test/p21/test_inverse_attr.p21" )

if(HAVE_STD_THREAD)
  if(UNIX)
    set(thread_flags "-pthread -std=c++11" )
    set(thread_libs "pthread")
  endif(UNIX)
  # for best results, use a large file. as1-oc-214.stp is currently the largest file in the repo that sc works with.
  add_schema_dependent_test( "stepfile_rw_progress" "ap214e3" "${SC_SOURCE_DIR}/data/ap214e3/as1-oc-214.stp"
                            "" "${thread_flags}" "${thread_libs}")
endif(HAVE_STD_THREAD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

