if(SC_PYTHON_GENERATOR)

  include_directories(
    ${SC_SOURCE_DIR}/include
    ${SC_SOURCE_DIR}/include/express
    ${SC_SOURCE_DIR}/src/base
   )

  add_definitions(-DHAVE_CONFIG_H)

  set(exp2python_SOURCES
    src/classes_misc_python.c
    ../express/fedex.c
    src/fedex_main_python.c
    src/classes_wrapper_python.cc
    src/classes_python.c
    src/selects_python.c
    src/multpass_python.c
    ../exp2cxx/collect.cc
    ../exp2cxx/complexlist.cc
    ../exp2cxx/entlist.cc
    ../exp2cxx/multlist.cc
    ../exp2cxx/orlist.cc
    ../exp2cxx/entnode.cc
    ../exp2cxx/expressbuild.cc
    ../exp2cxx/non-ors.cc
    ../exp2cxx/match-ors.cc
    ../exp2cxx/trynext.cc
    ../exp2cxx/write.cc
    ../exp2cxx/print.cc
   )
 SC_ADDEXEC(exp2python SOURCES ${exp2python_SOURCES} LINK_LIBRARIES express base)

  if(NOT SC_IS_SUBBUILD AND SC_GIT_VERSION)
    add_dependencies(exp2python version_string)
  endif(NOT SC_IS_SUBBUILD AND SC_GIT_VERSION)
endif(SC_PYTHON_GENERATOR)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

