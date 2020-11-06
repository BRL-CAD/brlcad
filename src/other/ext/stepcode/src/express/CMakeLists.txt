include_directories(
  ${CMAKE_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../include/stepcode
  )

find_package(LEMON)
find_package(RE2C)
find_package(PERPLEX)

# Depending on whether we're using pre-generated sources or building them on
# the fly, set up targets and source lists.
if(PERPLEX_FOUND AND RE2C_FOUND AND LEMON_FOUND)

  LEMON_TARGET(ExpParser expparse.y)
  PERPLEX_TARGET(ExpScanner expscan.l)
  ADD_PERPLEX_LEMON_DEPENDENCY(ExpScanner ExpParser)
  set(STEPCODE_SO_SRCS
    ${LEMON_ExpParser_SRC}
    ${PERPLEX_ExpScanner_SRC}
    )

  include_directories(${CMAKE_CURRENT_BINARY_DIR}/ExpParser_expparse)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/ExpScanner_expscan)

else()

  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/generated)
  set(EXPRESS_GENERATED_SRCS
    generated/expparse.c
    generated/expscan.c
    )
  set(STEPCODE_SO_SRCS ${EXPRESS_GENERATED_SRCS})
  set(STEPCODE_STATIC_SRCS ${EXPRESS_GENERATED_SRCS})

endif()

set(EXPRESS_SOURCES
  ${STEPCODE_SO_SRCS}
  alg.c
  caseitem.c
  dict.c
  entity.c
  error.c
  expr.c
  express.c
  hash.c
  lexact.c
  linklist.c
  memory.c
  object.c
  ordered_attrs.cc
  resolve.c
  schema.c
  scope.c
  stmt.c
  symbol.c
  type.c
  variable.c
  )

add_library(express SHARED ${EXPRESS_SOURCES} ${STEPCODE_SO_SRCS})
set_target_properties(express PROPERTIES VERSION ${STEPCODE_ABI_VERSION} SOVERSION ${STEPCODE_ABI_SOVERSION})
if(WIN32)
  target_compile_definitions(express PRIVATE STEPCODE_EXPRESS_DLL_EXPORTS)
endif()
if(APPLE)
  set_target_properties(express PROPERTIES LINK_FLAGS "-flat_namespace -undefined suppress")
endif(APPLE)
install(TARGETS express
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

