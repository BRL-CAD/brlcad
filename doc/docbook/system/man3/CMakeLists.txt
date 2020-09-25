set(man3_EN
  libfb.xml
  libplot3.xml
  libpkg.xml
  librt.xml
  libwdb.xml
  redblack.xml
  libicv.xml
  )

# Some DocBook man pages generate multiple output files, if they
# describe more than one command.  Those files need special
# handling, since the CMake logic has no way to know about the
# extra outputs without being told.
#set_source_files_properties(bu_htond.xml PROPERTIES EXTRA_MAN_OUTPUTS "bu_ntohd.3")

ADD_DOCBOOK("HTML;PHP;MAN3;PDF" man3_EN man3 "")

CMAKEFILES(libbu.xml libdm.xml TODO)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
