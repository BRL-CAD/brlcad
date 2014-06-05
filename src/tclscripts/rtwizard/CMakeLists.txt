add_subdirectory(lib)

set(rtwizard_TCLSCRIPTS
  RaytraceWizard.tcl
  rtwizard
  rtwizard.bat
  )
BRLCAD_ADDDATA(rtwizard_TCLSCRIPTS tclscripts/rtwizard)
pkgIndex_BUILD(tclscripts/rtwizard)
tclIndex_BUILD(tclscripts/rtwizard)

# Examples

set(PictureTypeA_DATA
  examples/PictureTypeA/desc.txt
  examples/PictureTypeA/helpstr.txt
  examples/PictureTypeA/intro.txt
  examples/PictureTypeA/title.txt
  examples/PictureTypeA/preview.small.gif
  )
BRLCAD_ADDDATA(PictureTypeA_DATA tclscripts/rtwizard/examples/PictureTypeA)

set(PictureTypeB_DATA
  examples/PictureTypeB/desc.txt
  examples/PictureTypeB/helpstr.txt
  examples/PictureTypeB/intro.txt
  examples/PictureTypeB/title.txt
  examples/PictureTypeB/preview.small.gif
  )
BRLCAD_ADDDATA(PictureTypeB_DATA tclscripts/rtwizard/examples/PictureTypeB)

set(PictureTypeC_DATA
  examples/PictureTypeC/desc.txt
  examples/PictureTypeC/helpstr.txt
  examples/PictureTypeC/intro.txt
  examples/PictureTypeC/title.txt
  examples/PictureTypeC/preview.small.gif
  )
BRLCAD_ADDDATA(PictureTypeC_DATA tclscripts/rtwizard/examples/PictureTypeC)

set(PictureTypeD_DATA
  examples/PictureTypeD/desc.txt
  examples/PictureTypeD/helpstr.txt
  examples/PictureTypeD/intro.txt
  examples/PictureTypeD/title.txt
  examples/PictureTypeD/preview.small.gif
  )
BRLCAD_ADDDATA(PictureTypeD_DATA tclscripts/rtwizard/examples/PictureTypeD)

set(PictureTypeE_DATA
  examples/PictureTypeE/desc.txt
  examples/PictureTypeE/helpstr.txt
  examples/PictureTypeE/intro.txt
  examples/PictureTypeE/title.txt
  examples/PictureTypeE/preview.small.gif
  )
BRLCAD_ADDDATA(PictureTypeE_DATA tclscripts/rtwizard/examples/PictureTypeE)

set(PictureTypeF_DATA
  examples/PictureTypeF/desc.txt
  examples/PictureTypeF/helpstr.txt
  examples/PictureTypeF/intro.txt
  examples/PictureTypeF/title.txt
  examples/PictureTypeF/preview.small.gif
  )
BRLCAD_ADDDATA(PictureTypeF_DATA tclscripts/rtwizard/examples/PictureTypeF)

if(WIN32)
  BRLCAD_MANAGE_FILES("rtwizard.bat" ${BIN_DIR} EXEC)
endif(WIN32)
BRLCAD_MANAGE_FILES("rtwizard" ${BIN_DIR} EXEC)

add_custom_target(rtwizard ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard)
set_target_properties(rtwizard PROPERTIES FOLDER "BRL-CAD Executables")
add_dependencies(rtwizard bwish rt rtedge fbserv fb-png fb-pix fbclear fblabel fbfree fbline fb-fb pix-bw bwmod bw-pix pixmatte pix-fb)
if(TARGET tkpng)
  add_dependencies(rtwizard tkpng)
endif(TARGET tkpng)
foreach(item ${tclindex_target_list})
  add_dependencies(rtwizard ${item})
endforeach(item ${tclindex_target_list})

CMAKEFILES(examples)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
