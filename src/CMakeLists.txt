# Define high level features of BRL-CAD
DOXYGEN_FEATURE("Portability Library" DESCRIPTION "Portability layer that makes sure basic low level system functionality is present across all platforms via a uniform API.")
DOXYGEN_FEATURE("Solid Raytracing" DESCRIPTION "Ray/geometry intersection calculations that report not just first surface hits but in/out pairs of hits to support the characterization of closed geometric volumes.")


# Minimal directories required for any BRL-CAD functionality

if(MSVC)
  add_definitions(
    -D_CRT_SECURE_NO_WARNINGS
    )
endif(MSVC)

# globally consider anything in an 'ext' path a warning-free system header
set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} ext CACHE STRING "Bundled system include dirs" FORCE)



# The BRL-CAD package provides a large number of tools and utilities.  Not all
# applications require all of these components, so we need a mechanism to allow
# applications to specify subsets to build.  However, we also don't want those
# specifications to be verbose - for example, if a client wants librt, what they
# will need is librt and its immediate dependencies. Rather than requiring the
# client to know and list all of those dependencies for building, we need to
# bake that knowledge into the build itself.
#
# Obviously, the build definitions for each library must have knowledge of the
# needed dependencies.  However, that knowledge isn't available to us at this
# level since none of those subdirectories have been added.  Nor is it
# necessarily trivial to programmatically extract that information from the
# CMakeLists.txt files - the formatting and content of them is not particularly
# constrained.
#
# To address this, we define the lists of the BRL-CAD dependencies used by various
# library targets here, up front.  These lists are also used in the subdirectories
# when targets are defined, so they represent the canonical dependency definitions
# used to create the actual build targets.  Defining them here, in turn, lets us
# also leverage this knowledge when activating or deactivating components in
# partial build scenarios.
set(libbu_deps)
set(libbn_deps      libbu)
set(libbv_deps      libbn libbu)
set(libbg_deps      libbv libbn libbu)
set(libnmg_deps     libbg libbv libbn libbu)
set(libbrep_deps    libbg libbv libbn libbu)
set(librt_deps      libbrep libnmg libbg libbv libbn libbu)
set(libwdb_deps     librt libbrep libnmg libbg libbv libbn libbu)
set(libpkg_deps     libbu)
set(libgcv_deps     libwdb librt libnmg libbu)
set(libanalyze_deps librt libbu)
set(liboptical_deps librt libbn libbu)
set(libicv_deps     libbn libbu)
set(libged_deps     libicv libanalyze libwdb liboptical librt libbrep libnmg libbv libbg libbn libbu)
set(libdm_deps      librt libicv libbv libbn libbg libbu libpkg)
set(libfft_deps)
set(libqtcad_deps   libged libdm librt libbv libbu)
set(libtclcad_deps  libged libdm libnmg libbg libbv libbn libbu)

set(ACTIVE_COMPONENTS)
if (DEFINED BRLCAD_COMPONENTS)
  set(WC ${BRLCAD_COMPONENTS})
  set(NC)
  while(WC)
    foreach(component ${WC})
      list(APPEND NC ${${component}_deps})
      list(REMOVE_DUPLICATES NC)
    endforeach(component ${WC})
    list(APPEND ACTIVE_COMPONENTS ${WC})
    list(REMOVE_DUPLICATES ACTIVE_COMPONENTS)
    set(WC ${NC})
    set(NC)
  endwhile(WC)
endif (DEFINED BRLCAD_COMPONENTS)
if (ACTIVE_COMPONENTS)
  list(SORT ACTIVE_COMPONENTS)
  string(REPLACE ";" " " AC "${ACTIVE_COMPONENTS}")
  message("Supplied BRLCAD_COMPONENTS (${BRLCAD_COMPONENTS} have activated the following:\n ${AC}")
endif (ACTIVE_COMPONENTS)

# We next define a list of all src/ directories to be added, in their build
# order.
set(brlcad_lib_dirs
  libbu
  libbn
  libbv
  libbg
  libnmg
  libbrep
  librt
  libwdb
  libpkg
  libgcv
  libanalyze
  liboptical
  libged
  libdm
  libfft
  libicv
  libpc
  libqtcad
  libtclcad
  )

set(brlcad_app_dirs
  brlman
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

set(brlcad_dirs ${brlcad_lib_dirs} ${brlcad_app_dirs})

# Now we know what to do - proceed.
foreach(subdir ${brlcad_dirs})
  if (NOT ACTIVE_COMPONENTS)
    verbose_add_subdirectory(src ${subdir})
  else (NOT ACTIVE_COMPONENTS)
    list(FIND ACTIVE_COMPONENTS ${subdir} IS_ACTIVE)
    if (NOT "${IS_ACTIVE}" EQUAL "-1")
      verbose_add_subdirectory(src ${subdir})
    endif (NOT "${IS_ACTIVE}" EQUAL "-1")
  endif (NOT ACTIVE_COMPONENTS)
endforeach(subdir ${brlcad_dirs})

# This corresponds roughly to the libbrlcad originally defined in BRL-CAD
# autotools build.
if(BRLCAD_ENABLE_BRLCAD_LIBRARY AND USE_OBJECT_LIBS)
  foreach(llib ${brlcad_lib_dirs})
    if(TARGET ${llib})
      set(libbrlcad_objs ${libbrlcad_objs} $<TARGET_OBJECTS:${llib}-obj>)
    endif(TARGET ${llib})
  endforeach(llib ${level_${wl}_dirs})
  add_library(libbrlcad SHARED ${libbrlcad_objs})
  add_library(libbrlcad-static STATIC ${libbrlcad_objs})
endif(BRLCAD_ENABLE_BRLCAD_LIBRARY AND USE_OBJECT_LIBS)

CMAKEFILES(
  CMakeLists.txt
  README
  libtermio/libtermio.h
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
