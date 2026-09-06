set(iges-g_SRCS
  iges-g.cpp
  iges_brep_import.cpp
  iges_document.cpp
  iges_import.cpp
  iges_native.cpp
  iges_report.cpp
  iges_runtime.cpp
  iges_wire.cpp
)
brlcad_addexec(iges-g "${iges-g_SRCS}" "libwdb;librt;libnmg;libbrep;libbn;libbu;${M_LIBRARY}" FOLDER Conv)

brlcad_addexec(g-iges
  "g-iges.cpp;iges_writer.cpp;iges_runtime.cpp;iges_primitive_export.cpp;iges_brep_export.cpp;iges_nmg_export.cpp"
  "librt;libnmg;libbrep;libbu;${M_LIBRARY}" FOLDER Conv)

add_subdirectory(tests)

set(
  iges_ignore_files
  CMakeLists.txt
  DESIGN.md
  ge6.igs
  iges_brep_import.h
  iges_document.h
  iges_import.h
  iges_native.h
  iges_parameters.h
  iges_runtime.h
  iges_report.h
  iges_writer.h
  wing128.igs
  woodie.igs
)
cmakefiles(${iges_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
