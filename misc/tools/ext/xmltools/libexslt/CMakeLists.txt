set(VERSION "0.8.15")
set(LIBEXSLT_VERSION_NUMBER 815)
set(LIBEXSLT_VERSION_EXTRA "-BRLCAD")
set(WITH_CRYPTO 0)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/exsltconfig.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/../include/libexslt/exsltconfig.h)
if(COMMAND DISTCLEAN)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../include/libexslt/exsltconfig.h")
endif(COMMAND DISTCLEAN)

set(libexslt_srcs
  src/common.c
  src/crypto.c
  src/date.c
  src/dynamic.c
  src/exslt.c
  src/functions.c
  src/math.c
  src/saxon.c
  src/sets.c
  src/strings.c
  )

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/../include/libxslt
  ${CMAKE_CURRENT_BINARY_DIR}/../include/libexslt
  ${CMAKE_CURRENT_BINARY_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../libxslt/include/libxslt
  ${CMAKE_CURRENT_SOURCE_DIR}/../libxslt/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/libexslt
  ${CMAKE_CURRENT_SOURCE_DIR}/../libxml/include
  /usr/local/include
  )

add_library(exslt SHARED ${libexslt_srcs})
target_link_libraries(exslt xml xslt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
