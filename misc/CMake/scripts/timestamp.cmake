string(TIMESTAMP CTIME "%s")
if (NOT STAMP_FILE)
  message(FATAL_ERROR "No stamp file specified")
endif (NOT STAMP_FILE)
file(WRITE "${STAMP_FILE}" "${CTIME}\n")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
