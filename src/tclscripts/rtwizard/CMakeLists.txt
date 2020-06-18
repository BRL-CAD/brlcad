add_subdirectory(lib)

set(rtwizard_TCLSCRIPTS
  RaytraceWizard.tcl
  rtwizard
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

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
