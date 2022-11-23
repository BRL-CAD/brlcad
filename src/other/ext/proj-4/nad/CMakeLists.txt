# files containing dictionary of useful projection

set(PROJ_DICTIONARY
  epsg
  esri
  world
  esri.extra
  other.extra
  IGNF
  nad27
  GL27
  nad83
  nad.lst
  proj_def.dat
  CH
  )

install(FILES ${PROJ_DICTIONARY} DESTINATION ${DATA_DIR}/proj)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
