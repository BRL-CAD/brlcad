set(k-g_SRCF
  bot.cpp
  k-g.cpp
  k_parser.cpp
  region_list.cpp
  arbs.cpp
  geometry.cpp
  )
BRLCAD_ADDEXEC(k-g "${k-g_SRCF}" "libwdb;librt;libbu" FOLDER Conv)

set(k-g_ignore_files
  bot.h
  CMakeLists.txt
  k_parser.h
  region_list.h
  arbs.h
  geometry.h
  )
CMAKEFILES(${k-g_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
