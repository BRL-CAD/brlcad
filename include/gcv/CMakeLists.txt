set(gcv_headers defines.h api.h util.h)
brlcad_manage_files(gcv_headers ${INCLUDE_DIR}/brlcad/gcv REQUIRED libgcv)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
