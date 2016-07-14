if(DYNAMIC_OPENSCENEGRAPH)
  add_definitions(-DOSGTEXT_LIBRARY)
else()
  add_definitions(-DOSG_LIBRARY_STATIC)
endif()

# FIXME: For OS X, need flag for Framework or dylib
set(OSGTEXT_SRCS
  DefaultFont.cpp
  DefaultFont.h
  GlyphGeometry.h
  GlyphGeometry.cpp
  Font.cpp
  FadeText.cpp
  Glyph.cpp
  String.cpp
  Style.cpp
  TextBase.cpp
  Text.cpp
  Text3D.cpp
  Version.cpp
  ${OPENSCENEGRAPH_VERSIONINFO_RC}
  )

add_library(osgText ${OPENSCENEGRAPH_USER_DEFINED_DYNAMIC_OR_STATIC} ${OSGTEXT_SRCS})
target_link_libraries(osgText osgDB osgUtil osg OpenThreads ${CMAKE_THREAD_LIBS_INIT}
  ${MATH_LIBRARY} ${RT_LIBRARY} ${DL_LIBRARY})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

