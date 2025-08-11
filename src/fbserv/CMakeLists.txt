include_directories(${DM_INCLUDE_DIRS})

brlcad_addexec(fbserv "fbserv.c;server.c" libdm)
add_target_deps(fbserv dm_plugins)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
