find_package_eigen(REQUIRED)

set(PLUGIN_LIBS libged libbu)

ged_plugin_library(ged-tire
  tire.cpp
  tire_conic.cpp
  tire_csg.cpp
  tire_surface.cpp
  tire_tread.cpp
  tire_tread_def.cpp
  tire_tread_profile.cpp
  tire_wheel.cpp
  SYSTEM_INCLUDES ${EIGEN3_INCLUDE_DIR}
  LIBS ${PLUGIN_LIBS}
  DEFINITIONS "EIGEN_DONT_VECTORIZE"
)

cmakefiles(
  CMakeLists.txt
  tire_private.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
