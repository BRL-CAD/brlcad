set(docbook_articles_HY
  about_hy.xml
  )

DOCBOOK_TO_HTML(article docbook_articles_HY html/articles/hy "")
DOCBOOK_TO_PHP(article docbook_articles_HY html/articles/hy "")
DOCBOOK_TO_PDF(article docbook_articles_HY pdf/articles/hy "")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
