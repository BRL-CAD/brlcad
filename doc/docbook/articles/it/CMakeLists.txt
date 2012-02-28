set(docbook_articles_IT
  about_it.xml
  )

set(docbook_articles_IT_IMAGES
  images/brlcad-overview-benchmark-Bldg_391.jpg
  images/brlcad-overview-benchmark-M35.jpg
  images/brlcad-overview-benchmark-Moss.jpg
  images/brlcad-overview-benchmark-Sphflake.jpg
  images/brlcad-overview-benchmark-Star.jpg
  images/brlcad-overview-benchmark-World.jpg
  images/brlcad-overview-data-flow.png
  )
BRLCAD_ADDDATA(docbook_articles_IT_IMAGES html/articles/it/images)

DOCBOOK_TO_HTML(article docbook_articles_IT html/articles/it docbook_articles_IT_IMAGES_cp)

if(BRLCAD_EXTRADOCS_PDF)
  DOCBOOK_TO_PDF(article docbook_articles_IT pdf/articles/it docbook_articles_IT_IMAGES_cp)
endif(BRLCAD_EXTRADOCS_PDF)
