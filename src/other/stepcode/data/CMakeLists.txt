# To build one or more schemas, configure with
# 'cmake -DSC_BUILD_SCHEMAS="path/to/schema.exp;path/to/schema2.exp"
# you can specify a schema in 3 ways:
#  * path relative to build dir
#  * path relative to sc/data (this dir)
#  * absolute path
#
# cmake goes through the above options in order until it finds one
# that exists. if ${path} is a dir, cmake expects to find a single
# .exp file inside, which it uses. otherwise, ${path} is assumed to
# be an express file.

if(NOT "${SC_BUILD_SCHEMAS}" STREQUAL "")
  include(${SC_CMAKE_DIR}/schema_scanner/schemaScanner.cmake)
  foreach(src ${SC_SDAI_ADDITIONAL_EXES_SRCS})
    get_filename_component(name ${src} NAME_WE)
    message(STATUS "Additional SDAI executable: ${name}")
  endforeach(src ${SC_SDAI_ADDITIONAL_EXES_SRCS})

  if(SC_BUILD_SCHEMAS STREQUAL "ALL")
    file(GLOB_RECURSE SC_BUILD_SCHEMAS ${SC_SOURCE_DIR}/data/*.exp)
  endif()
  foreach(FILE ${SC_BUILD_SCHEMAS})
    # figure out an absolute path to the file
    LOCATE_SCHEMA(${FILE} abspath)
    SCHEMA_CMLIST(${abspath})
  endforeach()
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

