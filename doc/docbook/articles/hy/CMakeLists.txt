set(docbook_articles_HY
  about_hy.xml
  )

DOCBOOK_TO_HTML(article docbook_articles_HY html/articles/hy "")

if(BRLCAD_EXTRADOCS_PDF)
  DOCBOOK_TO_PDF(article docbook_articles_HY pdf/articles/hy "")
endif(BRLCAD_EXTRADOCS_PDF)
