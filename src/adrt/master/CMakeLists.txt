set(ADRT_MASTER_SOURCES
  ../tienet.c
  compnet.c
  dispatcher.c
  master.c
  tienet_master.c
  )

BRLCAD_ADDEXEC(adrt_master "${ADRT_MASTER_SOURCES}" librt)

set(adrt_master_ignore_files
  compnet.h
  dispatcher.h
  master.h
  tienet_master.h
  )
CMAKEFILES(${adrt_master_ignore_files})