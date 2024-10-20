set(k-g_soure_files
  bot.cpp
  k-g.cpp
  k_parser.cpp
  region_list.cpp
  arbs.cpp
  geometry.cpp
  pipe.cpp
  sketch.cpp
  extrude.cpp
  )

BRLCAD_ADDEXEC(k-g "${k-g_soure_files}" "libwdb;librt;libbu" FOLDER Conv)

set(k-g_ignore_files
  bot.h
  CMakeLists.txt
  k_parser.h
  region_list.h
  arbs.h
  geometry.h
  pipe.h
  sketch.h
  extrude.h
  )

CMAKEFILES(${k-g_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
