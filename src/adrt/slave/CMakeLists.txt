set(ADRT_SLAVE_SOURCES ../tienet.c slave.c tienet_slave.c)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../librender)

brlcad_addexec(adrt_slave "${ADRT_SLAVE_SOURCES}" "librt;librender;${ZLIB_LIBRARIES}")

set(adrt_slave_ignore_files CMakeLists.txt tienet_slave.h slave.h)
cmakefiles(${adrt_slave_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
