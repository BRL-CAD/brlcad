BRLCAD_ADDEXEC(gcv gcv.c "libgcv;libbn;libbu;${M_LIBRARY}" FOLDER Conv)
add_dependencies(gcv gcv_plugins)

CMAKEFILES(gcv.sh)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
