set(docbook_implementation_EN
  bool_eval_development.xml
  )

DOCBOOK_TO_HTML(implementation docbook_implementation_EN html/implementation/en "")
DOCBOOK_TO_PHP(implementation docbook_implementation_EN html/implementation/en "")

if(BRLCAD_EXTRADOCS_PDF_MAN)
  DOCBOOK_TO_PDF(implementation docbook_implementation_EN pdf/implementation/en "")
endif(BRLCAD_EXTRADOCS_PDF_MAN)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
