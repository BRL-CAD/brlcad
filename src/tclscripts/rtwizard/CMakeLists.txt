add_subdirectory(lib)

if(HAVE_SYMLINK)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.tcl ${CMAKE_BINARY_DIR}/${BIN_DIR}/rtwizard)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.tcl ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/rtwizard)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
else(HAVE_SYMLINK)
  if(NOT CMAKE_CONFIGURATION_TYPES)
    add_custom_command(
      OUTPUT ${CMAKE_BINARY_DIR}/${BIN_DIR}/rtwizard ${CMAKE_BINARY_DIR}/${BIN_DIR}/rtwizard.bat
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.bat ${CMAKE_BINARY_DIR}/${BIN_DIR}/rtwizard.bat
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.tcl ${CMAKE_BINARY_DIR}/${BIN_DIR}/rtwizard
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.tcl ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.bat
      )
    set(rtwizard_cmd_outputs ${CMAKE_BINARY_DIR}/${BIN_DIR}/rtwizard ${CMAKE_BINARY_DIR}/${BIN_DIR}/rtwizard.bat)
  else(NOT CMAKE_CONFIGURATION_TYPES)
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
      add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/rtwizard ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/rtwizard.bat
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.bat ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/rtwizard.bat
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.tcl ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/rtwizard
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard ${CMAKE_CURRENT_SOURCE_DIR}/rtwizard.bat
        )
      set(rtwizard_cmd_outputs ${rtwizard_cmd_outputs} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/rtwizard ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/rtwizard.bat)
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  endif(NOT CMAKE_CONFIGURATION_TYPES)
  install(PROGRAMS rtwizard.bat DESTINATION ${BIN_DIR})
  add_custom_target(rtwizard ALL DEPENDS ${rtwizard_cmd_outputs})
endif(HAVE_SYMLINK)

install(PROGRAMS rtwizard.tcl DESTINATION ${BIN_DIR} RENAME rtwizard)

set(rtwizard_TCLSCRIPTS
  RaytraceWizard.tcl
  rtwizard.tcl
  rtwizard.bat
  )
BRLCAD_ADDDATA(rtwizard_TCLSCRIPTS tclscripts/rtwizard)
pkgIndex_BUILD(rtwizard tclscripts/rtwizard)
tclIndex_BUILD(rtwizard tclscripts/rtwizard)

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

CMAKEFILES(examples pkgIndex.tcl tclIndex)
CMAKEFILES(Makefile.am)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
