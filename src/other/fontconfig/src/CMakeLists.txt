set(fontconfig_SRCS
  fcatomic.c
  fccache.c
  fccfg.c
  fccharset.c
  fccompat.c
  fcdbg.c
  fcdefault.c
  fcdir.c
  fcformat.c
  fcfreetype.c
  fcfs.c
  fchash.c
  fcinit.c
  fclang.c
  fclist.c
  fcmatch.c
  fcmatrix.c
  fcname.c
  fcobjs.c
  fcpat.c
  fcptrlist.c
  fcrange.c
  fcserialize.c
  fcstat.c
  fcstr.c
  fcweight.c
  fcxml.c
  ftglue.c
  )

add_definitions(-DHAVE_CONFIG_H)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../expat
  ${FREETYPE_INCLUDE_DIR_freetype2}
  ${PNG_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIRS}
  )

set(expat_SRCS
  ../expat/xmlparse.c
  ../expat/xmlrole.c
  ../expat/xmltok.c
  )

add_definitions(-DHAVE_EXPAT_CONFIG_H -DXML_ENABLE_VISIBILITY=1)

add_library(fontconfig SHARED ${fontconfig_SRCS} ${expat_SRCS})
set_target_properties(fontconfig PROPERTIES VERSION 2.13 SOVERSION 1)
target_link_libraries(fontconfig ${FREETYPE_LIBRARIES} ${PNG_LIBRARIES} ${EXPAT_LIBRARY})

install(TARGETS fontconfig
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib )


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

