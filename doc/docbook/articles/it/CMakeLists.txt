set(articles_IT
  about_it.xml
  )

set(articles_IT_IMAGES
  images/bc2.gif
  images/brlcad-overview-benchmark-Bldg_391.jpg
  images/brlcad-overview-benchmark-M35.jpg
  images/brlcad-overview-benchmark-Moss.jpg
  images/brlcad-overview-benchmark-Sphflake.jpg
  images/brlcad-overview-benchmark-Star.jpg
  images/brlcad-overview-benchmark-World.jpg
  images/brlcad-overview-data-flow.png
  images/sflogo.png
  )
if(BRLCAD_EXTRADOCS)
  ADD_DOC(articles_IT_IMAGES html/articles/it/images)
else(BRLCAD_EXTRADOCS)
  CMAKEFILES(${articles_IT_IMAGES})
endif(BRLCAD_EXTRADOCS)

ADD_DOCBOOK("HTML;PHP;PDF" articles_IT articles/it articles_IT_IMAGES_cp)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
