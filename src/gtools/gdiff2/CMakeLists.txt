set(GDIFF_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
list(REMOVE_DUPLICATES GDIFF_INCLUDE_DIRS)
include_directories(${GDIFF_INCLUDE_DIRS})

set(gdiff_srcs
  gdiff2.c
  )

BRLCAD_ADDEXEC(gdiff2 ${gdiff_srcs} "librt")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
