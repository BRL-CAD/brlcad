include_directories(
  ${DM_INCLUDE_DIRS}
  )

BRLCAD_ADDEXEC(fbserv "fbserv.c;server.c" libdm)
ADD_TARGET_DEPS(fbserv dm_plugins)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
