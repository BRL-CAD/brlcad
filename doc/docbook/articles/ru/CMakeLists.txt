set(docbook_articles_RU
  about_ru.xml
  )

DOCBOOK_TO_HTML(article docbook_articles_RU html/articles/ru "")

if(BRLCAD_EXTRADOCS_PDF)
  DOCBOOK_TO_PDF(article docbook_articles_RU pdf/articles/ru "")
endif(BRLCAD_EXTRADOCS_PDF)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
