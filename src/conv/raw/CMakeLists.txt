set(rawg_SRCS Bot.cpp raw-g.cpp RegionList.cpp)
brlcad_addexec(raw-g "${rawg_SRCS}" "libwdb;librt;libbu" NO_STRICT FOLDER Conv)

brlcad_addexec(g-raw g-raw.c "librt;libgcv;libnmg;libbu;${WINSOCK_LIB}" FOLDER Conv)
add_dependencies(g-raw gcv_plugins)

set(RAW_IGNORE_FILES Bot.h CMakeLists.txt RegionList.h)
cmakefiles(${RAW_IGNORE_FILES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
