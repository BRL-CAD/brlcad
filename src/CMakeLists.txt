# Minimal directories required for any BRL-CAD functionality

# Define some common definitions here that are used
# in most BRL-CAD directories 

add_definitions(
  -DHAVE_CONFIG_H
  -DBRLCADBUILD=1
  )

if(MSVC)
  add_definitions(
    -D_CRT_SECURE_NO_WARNINGS
    -DCMAKE_HEADERS
    )
  # FIXME: these should be proper library checks, not based on MSVC "platform"
  set(WINSOCK_LIB ws2_32.lib)
  set(PSAPI_LIB psapi.lib)
  set(RPCRT_LIB rpcrt4.lib)
endif(MSVC)

# We will always need the required directories,
# so go ahead and add them

set(required_dirs
  libbu
  libbn
  libsysv
  librt
  )

foreach(subdir ${required_dirs})
  add_subdirectory(${subdir})
endforeach(subdir ${required_dirs})

# Directories needed to run benchmarks
set(benchmark_dirs
  libpkg
  libfb
  libwdb
  libgcv
  libanalyze
  libged
  liboptical
  liborle
  conv
  fbserv
  rt
  )
# The interaction of MSVC and Boost looks problematic -
# since we're not using libpc at the moment, conditionalize
if(NOT MSVC)
  set(LIBPC libpc)
endif(NOT MSVC)
# anything not included from above
set(remaining_dirs
  libcursor
  libdm
  libfft
  libicv
  libmultispectral
  ${LIBPC}
  libtclcad
  libtermio
  adrt
  anim
  brlman
  burst
  bwish
  canon
  fb
  vfont
  fbed
  gtools
  halftone
  irprep
  lgt
  nirt
  proc-db
  shapes
  sig
  tab
  util
  vdeck
  # tclscripts must come before mged and archer, so the
  # target lists are defined when the archer and mged
  # targets are defined
  tclscripts
  archer
  mged
  )

# FIXME: Ideally these should all be in the above list,
# need to fix them on Windows.
set(non_win32_dirs
  remrt
  rttherm
  )

#######################
# put it all together #
#######################
foreach(subdir ${benchmark_dirs})
  add_subdirectory(${subdir})
endforeach(subdir ${benchmark_dirs})
if(BRLCAD_ENABLE_RTSERVER)
  add_subdirectory(librtserver)
else(BRLCAD_ENABLE_RTSERVER)
  CMAKEFILES(librtserver)
endif(BRLCAD_ENABLE_RTSERVER)
foreach(subdir ${remaining_dirs})
  add_subdirectory(${subdir})
endforeach(subdir ${remaining_dirs})
if(NOT WIN32)
  foreach(subdir ${non_win32_dirs})
    add_subdirectory(${subdir})
  endforeach(subdir ${non_win32_dirs})
endif(NOT WIN32)

CMAKEFILES(README external)
CMAKEFILES(Makefile.am)
