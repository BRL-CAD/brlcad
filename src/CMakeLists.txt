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


# We need to define a number of "components" to allow for easy building of
# subsets of BRL-CAD.  We will control this process with an advanced
# variable named BRLCAD_ENABLE_TARGETS - if set to 0 (default) everything
# will be enabled.

# Level 1 directories are required for all other BRL-CAD components.  If you
# build anything else, you will need to build these.  If you want JUST librt
# and its requirements, set BRLCAD_ENABLE_TARGETS to 1
set(level_1_dirs
  libbu
  libbn
  libsysv
  libbrep
  librt
  )

# Level 2 directories are the remainder of BRL-CAD's libraries.  Setting
# the BRLCAD_ENABLE_TARGETS level to 2 will enable all libraries but no
# programs.

# The interaction of MSVC and Boost looks problematic -
# since we're not using libpc at the moment, conditionalize
if(NOT MSVC)
  set(LIBPC_DIR libpc)
else(NOT MSVC)
  CMAKEFILES(libpc)
endif(NOT MSVC)

# Only add librtserver if it's enabled
if(BRLCAD_ENABLE_RTSERVER)
  set(LIBRTSERVER_DIR librtserver)
else(BRLCAD_ENABLE_RTSERVER)
  CMAKEFILES(librtserver)
endif(BRLCAD_ENABLE_RTSERVER)

set(level_2_dirs
  libpkg
  libfb
  libwdb
  libgcv
  libanalyze
  libged
  liboptical
  liborle
  libcursor
  libdm
  libfft
  libicv
  libmultispectral
  ${LIBPC_DIR}
  ${LIBRTSERVER_DIR}
  libtclcad
  libtermio
  )

# Level 3 directories contain BRL-CAD's executables.  Setting
# the BRLCAD_ENABLE_TARGETS level to 3 will enable all programs.

# FIXME: Ideally these should all be in the above list,
# need to fix them on Windows.
if(NOT WIN32)
  set(non_win32_dirs remrt rttherm)
endif(NOT WIN32)

set(level_3_dirs
  anim
  brlman
  burst
  bwish
  canon
  conv
  fb
  fbed
  fbserv
  gtools
  halftone
  irprep
  lgt
  nirt
  proc-db
  rt
  shapes
  sig
  tab
  util
  vfont
  ${non_win32_dirs}
  # tclscripts must come before applications like
  # mged and archer that need the scripts in place to
  # run. The script target lists are defined when the tclscripts
  # directories are configured, and those lists are needed
  # as dependencies for the targets in these directories
  tclscripts
  adrt
  archer
  mged
  )

# Now that we have our lists, proceed with the actual
# add_subdirectory commands.  We find the highest
# number used to define a level_#_dir, and add the
# levels in order up to the limit specified by
# BRLCAD_ENABLE_TARGETS (or all of them if the value
# is 0)
set(HIGHEST_TARGET_LEVEL 0)
set(has_contents 1)
while(has_contents)
  math(EXPR current_level "${HIGHEST_TARGET_LEVEL} + 1")
  if(NOT "${level_${current_level}_dirs}" STREQUAL "")
    set(HIGHEST_TARGET_LEVEL ${current_level})
  else(NOT "${level_${current_level}_dirs}" STREQUAL "")
    set(has_contents 0)
  endif(NOT "${level_${current_level}_dirs}" STREQUAL "")
endwhile(has_contents)

# If we didn't already a limit, set to the highest level
if(NOT BRLCAD_ENABLE_TARGETS)
  set(BRLCAD_ENABLE_TARGETS ${HIGHEST_TARGET_LEVEL})
else(NOT BRLCAD_ENABLE_TARGETS)
  if(${BRLCAD_ENABLE_TARGETS} GREATER ${HIGHEST_TARGET_LEVEL})
    message(WARNING "BRL-CAD Target level ${BRLCAD_ENABLE_TARGETS} specified, but highest level defined is ${HIGHEST_TARGET_LEVEL}")
    set(BRLCAD_ENABLE_TARGETS ${HIGHEST_TARGET_LEVEL})
  endif(${BRLCAD_ENABLE_TARGETS} GREATER ${HIGHEST_TARGET_LEVEL})
endif(NOT BRLCAD_ENABLE_TARGETS)

# Now we know what to do - proceed.  If we're not using it,
# be sure to marked it as an ignored directory for CMake.
set(current_level 0)
while(${current_level} LESS ${HIGHEST_TARGET_LEVEL})
  math(EXPR current_level "${current_level} + 1")
  foreach(subdir ${level_${current_level}_dirs})
    if(${current_level} LESS ${BRLCAD_ENABLE_TARGETS} OR ${current_level} EQUAL ${BRLCAD_ENABLE_TARGETS} )
      verbose_add_subdirectory(src ${subdir})
    else(${current_level} LESS ${BRLCAD_ENABLE_TARGETS} OR ${current_level} EQUAL ${BRLCAD_ENABLE_TARGETS} )
      CMAKEFILES(${subdir})
    endif(${current_level} LESS ${BRLCAD_ENABLE_TARGETS} OR ${current_level} EQUAL ${BRLCAD_ENABLE_TARGETS} )
  endforeach(subdir ${level_${current_level}_dirs})
endwhile(${current_level} LESS ${HIGHEST_TARGET_LEVEL})

# This corresponds roughly to the libbrlcad definition in
# the autotools build.  It needs CMake 2.8.8 plus changes
# to the BRLCAD_ADDLIB macro to define OBJECT add_library
# targets, and is commented out for now, but this is the
# starting point for a "proper" libbrlcad
#if(${HIGHEST_TARGET_LEVEL} GREATER 1)
#  add_library(libbrlcad STATIC
#    $<TARGET_OBJECTS:libbu_OBJECTS>
#    $<TARGET_OBJECTS:libbn_OBJECTS>
#    $<TARGET_OBJECTS:librt_OBJECTS>
#    $<TARGET_OBJECTS:libwdb_OBJECTS>
#    $<TARGET_OBJECTS:libicv_OBJECTS>
#    $<TARGET_OBJECTS:libged_OBJECTS>
#    )
#  target_link_libraries(libbrlcad ${OPENNURBS_LIBRARY} ${TCL_LIBRARY})
#endif(${HIGHEST_TARGET_LEVEL} GREATER 1)

CMAKEFILES(README external)
CMAKEFILES(compat)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
