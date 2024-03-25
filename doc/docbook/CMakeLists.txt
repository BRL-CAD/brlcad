add_subdirectory(articles)
add_subdirectory(books)
add_subdirectory(devguides)
add_subdirectory(lessons)
add_subdirectory(presentations)
add_subdirectory(specifications)
add_subdirectory(system)

set(ignore_files
  CMakeLists.txt
  README
  README.DB_authors_notes
  css/brlcad.css
  db_dir_info.c
  fop.xconf.in
  log4j.properties
  )
CMAKEFILES(${ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
