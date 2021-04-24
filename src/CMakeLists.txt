# Define high level features of BRL-CAD
DOXYGEN_FEATURE("Portability Library" DESCRIPTION "Portability layer that makes sure basic low level system functionality is present across all platforms via a uniform API.")
DOXYGEN_FEATURE("Solid Raytracing" DESCRIPTION "Ray/geometry intersection calculations that report not just first surface hits but in/out pairs of hits to support the characterization of closed geometric volumes.")


# Minimal directories required for any BRL-CAD functionality

if(MSVC)
  add_definitions(
    -D_CRT_SECURE_NO_WARNINGS
    )
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
  libbg
  libnmg
  libbrep
  librt
  libwdb
  )

# Level 2 directories are the remainder of BRL-CAD's libraries.  Setting
# the BRLCAD_ENABLE_TARGETS level to 2 will enable all libraries but no
# programs.

set(level_2_dirs
  libbview
  libpkg
  libgcv
  libanalyze
  liboptical
  libged
  libdm
  libfft
  libicv
  libpc
  libtclcad
  libtermio
  )

# Level 3 directories contain BRL-CAD's executables.  Setting
# the BRLCAD_ENABLE_TARGETS level to 3 will enable all programs.

set(level_3_dirs
  brlman
  burst
  bwish
  conv
  fb
  fbserv
  gtools
  nirt
  proc-db
  rt
  art
  shapes
  sig
  util
  qged
  external
  remrt
  # tclscripts must come before applications like
  # mged and archer that need the scripts in place to
  # run. The script target lists are defined when the tclscripts
  # directories are configured, and those lists are needed
  # as dependencies for the targets in these directories
  tclscripts
  adrt
  isst
  rtwizard
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
    endif(${current_level} LESS ${BRLCAD_ENABLE_TARGETS} OR ${current_level} EQUAL ${BRLCAD_ENABLE_TARGETS} )
  endforeach(subdir ${level_${current_level}_dirs})
endwhile(${current_level} LESS ${HIGHEST_TARGET_LEVEL})

# This corresponds roughly to the libbrlcad originally defined in BRL-CAD
# autotools build.
if(BRLCAD_ENABLE_BRLCAD_LIBRARY AND USE_OBJECT_LIBS)
  set(current_level 1)
  while(${current_level} LESS ${HIGHEST_TARGET_LEVEL})
    set(wl 1)
    math(EXPR cml "${current_level} + 1")
    while(${wl} LESS ${cml})
      foreach(llib ${level_${wl}_dirs})
	if(TARGET ${llib})
	  set(libbrlcad_objs ${libbrlcad_objs} $<TARGET_OBJECTS:${llib}-obj>)
	endif(TARGET ${llib})
      endforeach(llib ${level_${wl}_dirs})
      math(EXPR wl "${wl} + 1")
    endwhile(${wl} LESS ${cml})
    if("${current_level}" EQUAL 1)
      add_library(libbrlcad-core SHARED ${libbrlcad_objs})
      add_library(libbrlcad-core-static STATIC ${libbrlcad_objs})
    endif("${current_level}" EQUAL 1)
    if("${current_level}" EQUAL 2)
      add_library(libbrlcad SHARED ${libbrlcad_objs})
      add_library(libbrlcad-static STATIC ${libbrlcad_objs})
    endif("${current_level}" EQUAL 2)
    math(EXPR current_level "${current_level} + 1")
  endwhile(${current_level} LESS ${HIGHEST_TARGET_LEVEL})
endif(BRLCAD_ENABLE_BRLCAD_LIBRARY AND USE_OBJECT_LIBS)

CMAKEFILES(
  CMakeLists.txt
  README
  )

# Add a convenience directory that is only added to the
# build if it exists.  This directory is ignored by svn,
# so it can be used for experimentation that needs to be
# part of the build but the dev doesn't want to accidentally
# commit.
if("${BRLCAD_ENABLE_TARGETS}" GREATER 2 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/testing)
  add_subdirectory(testing)
endif("${BRLCAD_ENABLE_TARGETS}" GREATER 2 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/testing)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
