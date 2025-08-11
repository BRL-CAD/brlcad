brlcad_addexec(gcv gcv.c "libgcv;libbn;libbu;${M_LIBRARY}" FOLDER Conv)
add_dependencies(gcv gcv_plugins)

cmakefiles(gcv.sh)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
