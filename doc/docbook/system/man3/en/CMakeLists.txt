set(docbook_man3_EN
  libfb.xml
  )

DOCBOOK_TO_HTML(man3 docbook_man3_EN html/man3/en "")
DOCBOOK_TO_MAN(man3 docbook_man3_EN 3 3 man/man3 "")

if(BRLCAD_EXTRADOCS_PDF_MAN)
  DOCBOOK_TO_PDF(man3 docbook_man3_EN pdf/man3/en "")
endif(BRLCAD_EXTRADOCS_PDF_MAN)
