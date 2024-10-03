brlcad_addexec(ply-g ply-g.c "libgcv" FOLDER Conv)
brlcad_addexec(g-ply g-ply.c "libgcv" FOLDER Conv)
add_dependencies(ply-g gcv_plugins)
add_dependencies(g-ply gcv_plugins)

cmakefiles(
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
