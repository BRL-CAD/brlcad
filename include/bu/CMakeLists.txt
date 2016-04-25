set(bu_headers
  avs.h
  bitv.h
  cmd.h
  color.h
  cv.h
  debug.h
  defines.h
  endian.h
  env.h
  file.h
  getopt.h
  hash.h
  hist.h
  list.h
  log.h
  magic.h
  malloc.h
  mapped_file.h
  observer.h
  opt.h
  parallel.h
  parse.h
  path.h
  ptbl.h
  rb.h
  simd.h
  sort.h
  str.h
  time.h
  units.h
  uuid.h
  version.h
  vfont.h
  vlb.h
  vls.h
  )
install(FILES ${bu_headers} DESTINATION ${INCLUDE_DIR}/brlcad/bu)

CMAKEFILES(${bu_headers})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
