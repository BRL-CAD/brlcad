set(GCV_INCLUDE_DIRS
  ${BU_INCLUDE_DIRS}
  ${BN_INCLUDE_DIRS}
  ${GCV_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(GCV_INCLUDE_DIRS)


BRLCAD_ADDEXEC(gcv gcv.cpp "libgcv;libbn;libbu;${M_LIBRARY}" NO_INSTALL)

CMAKEFILES(gcv.sh optionparser.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
