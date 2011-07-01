SET(sh_SCRIPTS
  elapsed.sh 
  any-png.sh 
  cadbug.sh 
  conversion.sh
  cray.sh 
  facetall.sh 
  indent.sh 
  news2tracker.sh 
  pixinfo.sh 
  pixread.sh 
  pixwrite.sh 
  sgisnap.sh 
  show.sh 
  tracker.sh 
  trimconfig.sh
  )
FOREACH(script ${sh_SCRIPTS})
	configure_file(${script} ${CMAKE_BINARY_DIR}/bin/${script} COPYONLY)
ENDFOREACH(script ${sh_SCRIPTS})

INSTALL(PROGRAMS ${sh_SCRIPTS} DESTINATION ${BIN_DIR})

SET(noinst_SCRIPTS
  cmakecheck.sh 
  copyright.sh 
  enumerate.sh 
  footer.sh 
  header.sh 
  linkroot.sh 
  libtoolfilename.sh 
  make_deb.sh 
  make_dmg.sh 
  make_pkg.sh 
  make_rpm.sh 
  make_tar.sh 
  make_bz2.sh 
  make_tgz.sh 
  make_zip.sh 
  orbit.sh 
  shtool 
  template.sh 
  ws.sh
)
CMAKEFILES(${sh_SCRIPTS})
CMAKEFILES(${noinst_SCRIPTS})
