set(comgeom_srcs
  ComGeomConverter.cpp
  ComGeomReader.cpp
  RegionManager.cpp
  SolidParser.cpp
  main.cpp
  )

brlcad_addexec(comgeom-g "${comgeom_srcs}" "libwdb;librt;libnmg;libbu" FOLDER Conv)
target_include_directories(comgeom-g BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

cmakefiles(
  CMakeLists.txt
  ComGeomConverter.h
  ComGeomReader.h
  RegionManager.h
  SolidParser.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
