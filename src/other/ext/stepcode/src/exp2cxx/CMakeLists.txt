set(FEDEX_COMMON_SRCS
  classes_misc.c
  ../express/fedex.c
  )

set(exp2cxx_SOURCES
  ${FEDEX_COMMON_SRCS}
  fedex_main.c
  classes_wrapper.cc
  classes.c
  classes_attribute.c
  classes_entity.c
  classes_type.c
  class_strings.c
  selects.c
  multpass.c
  rules.c
  collect.cc
  complexlist.cc
  entlist.cc
  multlist.cc
  orlist.cc
  entnode.cc
  expressbuild.cc
  non-ors.cc
  match-ors.cc
  trynext.cc
  write.cc
  print.cc
  genCxxFilenames.c
  )

include_directories(
  ${SC_SOURCE_DIR}/include
  ${SC_SOURCE_DIR}/include/exppp
  ${SC_SOURCE_DIR}/include/express
  ${SC_SOURCE_DIR}/src/base
  )

SC_ADDEXEC(exp2cxx SOURCES ${exp2cxx_SOURCES} LINK_LIBRARIES libexppp express base)

if(SC_ENABLE_TESTING)
  add_subdirectory(test)
endif(SC_ENABLE_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

