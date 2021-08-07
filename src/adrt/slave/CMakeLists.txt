set(ADRT_SLAVE_SOURCES
  ../tienet.c
  slave.c
  tienet_slave.c
  )

BRLCAD_ADDEXEC(adrt_slave "${ADRT_SLAVE_SOURCES}" "librt;librender")

set(adrt_slave_ignore_files
  CMakeLists.txt
  tienet_slave.h
  slave.h
  )
CMAKEFILES(${adrt_slave_ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

