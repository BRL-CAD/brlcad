set(art_example_files
  README.adoc
  render_moss.cmake
  moss_studio.tcl
  moss_materials.tcl
  moss-before.png
  moss-after.png
)
if(BRLCAD_INSTALL_EXAMPLE_GEOMETRY)
  brlcad_adddata(art_example_files db/art)
else()
  cmakefiles(${art_example_files})
endif()
cmakefiles(CMakeLists.txt)
