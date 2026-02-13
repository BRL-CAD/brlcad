set(PLUGIN_LIBS libged libbu)

set(
  LINT_SRCS
  bot.cpp
  cyclic.cpp
  invalid_shape.cpp
  lint.cpp
  missing.cpp
)

ged_plugin_library(ged-lint
  ${LINT_SRCS}
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
  LIBS ${PLUGIN_LIBS}
)

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
