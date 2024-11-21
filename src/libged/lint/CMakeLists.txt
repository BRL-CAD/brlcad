include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  ${BU_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
)

set(
  LINT_SRCS
  bot.cpp
  cyclic.cpp
  invalid_shape.cpp
  lint.cpp
  missing.cpp
)

ged_plugin_library(ged-lint ${LINT_SRCS})
target_link_libraries(ged-lint libged libbu)

cmakefiles(
  CMakeLists.txt
  ${LINT_SRCS}
  ged_lint.h
  json.hpp
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
