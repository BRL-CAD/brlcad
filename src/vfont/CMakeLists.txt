include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${TCL_INCLUDE_DIRS}
)

add_library(libvfont getfont.c)
target_link_libraries(libvfont libbu)

BRLCAD_ADDFILE(fix.6r vfont)
BRLCAD_ADDFILE(nonie.r.12 vfont)
