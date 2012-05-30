set(docbook_man5_EN
  gattributes.xml
  )

DOCBOOK_TO_HTML(man5 docbook_man5_EN html/man5/en "")
DOCBOOK_TO_MAN(man5 docbook_man5_EN 5 5 man/man5 "")

if(BRLCAD_EXTRADOCS_PDF_MAN)
  DOCBOOK_TO_PDF(man5 docbook_man5_EN pdf/man5/en "")
endif(BRLCAD_EXTRADOCS_PDF_MAN)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
