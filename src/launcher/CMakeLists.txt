set(
  LAUNCHER_SRCS
  launcher.c
  registry.c
  launch.c
  fbtext.c
  ui_fb.c
  ui_text.c
)

brlcad_addexec(brlcad-launcher "${LAUNCHER_SRCS}" "libdm;libicv;libbu")
target_include_directories(brlcad-launcher BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Application manifests, discovered at run time by the launcher (nothing is
# hard-coded).  New GUI programs appear in the launcher simply by installing an
# additional manifest here.
brlcad_adddata(apps/mged.app launcher/apps)
brlcad_adddata(apps/archer.app launcher/apps)
brlcad_adddata(apps/rtwizard.app launcher/apps)
brlcad_adddata(apps/isst.app launcher/apps)
brlcad_adddata(apps/shell.app launcher/apps)

# Rendered splash image (optional).  If present it is installed and shown by the
# windowed UI; otherwise the launcher falls back to the bundled BRL-CAD logo.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/splash.png")
  brlcad_adddata(assets/splash.png launcher)
endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/splash.png")

cmakefiles(
  CMakeLists.txt
  README
  launcher.h
  fbtext.h
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
