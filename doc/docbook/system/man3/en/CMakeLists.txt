set(docbook_man3_EN
  htond.xml
  libcursor.xml
  libfb.xml
  liborle.xml
  libplot3.xml
  librt.xml
  libwdb.xml
  redblack.xml
  )

# Some DocBook man pages generate multiple output files, if they
# describe more than one command.  Those files need special
# handling, since the CMake logic has no way to know about the
# extra outputs without being told.
set_source_files_properties(htond.xml PROPERTIES EXTRA_MAN_OUTPUTS "ntohd.3")

DOCBOOK_TO_HTML(man3 docbook_man3_EN html/man3/en "")
DOCBOOK_TO_MAN(man3 docbook_man3_EN 3 3 man3 "")

if(BRLCAD_EXTRADOCS_PDF_MAN)
  DOCBOOK_TO_PDF(man3 docbook_man3_EN pdf/man3/en "")
endif(BRLCAD_EXTRADOCS_PDF_MAN)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
