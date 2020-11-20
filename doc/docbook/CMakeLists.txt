# Define a build target that builds a no-op executable, so we can ensure
# a target for generator expression use regardless of other changes elsewhere
# in the build system
add_executable(db_dir_info db_dir_info.c)
set_target_properties(db_dir_info PROPERTIES FOLDER "DocBook")

# We need various configuration files set up for DocBook processing tools
add_subdirectory(resources)

# Rework CMake binary and source paths for DocBook templates - if we
# have spaces in the paths, they will cause a problem.
string(REPLACE " " "%20" DOCBOOK_BINARY_DIR "${CMAKE_BINARY_DIR}")
string(REPLACE " " "%20" DOCBOOK_SOURCE_DIR "${CMAKE_SOURCE_DIR}")

# Files for PDF products
if(BRLCAD_EXTRADOCS_PDF)
  # Apache FOP needs a couple of config files set up
  set(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
  configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/fop.xconf.in ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf)
  DISTCLEAN("${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf")
endif(BRLCAD_EXTRADOCS_PDF)

# For the html files, we need brlcad.css
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/brlcad_css-${CMAKE_CFG_INTDIR}-done
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/css/brlcad.css $<TARGET_FILE_DIR:db_dir_info>/${RBIN_DIR}/${DOC_DIR}/html/css/brlcad.css
  COMMAND "${CMAKE_COMMAND}" -E touch  ${CMAKE_CURRENT_BINARY_DIR}/brlcad_css-${CMAKE_CFG_INTDIR}-done
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/css/brlcad.css
  )
add_custom_target(brlcad_css DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/brlcad_css-${CMAKE_CFG_INTDIR}-done)
set_target_properties(brlcad_css PROPERTIES FOLDER "DocBook")
install(FILES css/brlcad.css DESTINATION ${DOC_DIR}/html/css)
DISTCLEAN(${CMAKE_CFG_INTDIR}/${DOC_DIR}/html/css/brlcad.css)

# Include CMake macros for DocBook.
include(${BRLCAD_SOURCE_DIR}/misc/CMake/DocBook.cmake)

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
