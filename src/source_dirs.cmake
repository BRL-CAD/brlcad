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
# CMakeLists.txt files - their formatting and content is not particularly
# constrained.
#
# To address this, we define the lists of the BRL-CAD dependencies used by
# various directories up front. Defining them here lets us leverage this
# knowledge when activating or deactivating components in partial build
# scenarios.
#
# For libraries these lists are also used in the subdirectories when targets
# are defined, so they represent the canonical dependency definitions used to
# create the actual build targets.  Application directories, on the other hand,
# list the "highest level" directories that any of the programs contained in
# them need since many of them contain large numbers of executables with
# different requriements.

if (NOT COMMAND)
  function(deps_expand seed_dir out_var)
    set(curr_deps ${${out_var}})
    set(working_deps ${${seed_dir}_deps})
    set(seed_deps)
    while (working_deps)
      list(POP_FRONT working_deps wdep)
      if (NOT BRLCAD_ENABLE_TCL AND "${wdep}" STREQUAL "libtclcad")
	continue()
      endif ()
      if (NOT BRLCAD_ENABLE_QT AND "${wdep}" STREQUAL "libqtcad")
	continue()
      endif ()
      list(FIND curr_deps ${wdep} fresult)
      if (${fresult} EQUAL -1)
	list(APPEND curr_deps ${wdep})
	set(seed_deps ${seed_deps} ${${wdep}_deps})
      endif (${fresult} EQUAL -1)
      if (NOT working_deps)
	set(working_deps ${seed_deps})
	set(seed_deps)
      endif (NOT working_deps)
    endwhile (working_deps)

    # Have the active dirs, sort them into
    # reverse dependency order
    set(odirs ${ordered_dirs})
    list(REVERSE odirs)
    set(fdeps)
    foreach(cod ${odirs})
      list(FIND curr_deps ${cod} fresult)
      if (NOT ${fresult} EQUAL -1)
	list(APPEND fdeps ${cod})
      endif (NOT ${fresult} EQUAL -1)
    endforeach(cod ${odirs})
    set(${out_var} ${fdeps} PARENT_SCOPE)
  endfunction(deps_expand)
endif (NOT COMMAND)

set(ordered_dirs)
macro(set_deps dirname deps_list)
  list(APPEND ordered_dirs ${dirname})
  set(${dirname}_deps ${deps_list})
endmacro(set_deps)

set_deps(libbu      "")
set_deps(libbn      "libbu")
set_deps(libbg      "libbn")
set_deps(libbv      "libbg")
set_deps(libnmg     "libbv")
set_deps(libbrep    "libbv")
set_deps(librt      "libbrep;libnmg")
set_deps(libwdb     "librt")
set_deps(libpkg     "libbu")
set_deps(libgcv     "libwdb")
set_deps(libanalyze "librt")
set_deps(liboptical "librt")
set_deps(libicv     "libbn")
set_deps(libdm      "librt;libicv;libpkg")
set_deps(libged     "libicv;libanalyze;libwdb;liboptical;libdm")
set_deps(libfft     "")
set_deps(libpc      "")
set_deps(libqtcad   "libged;libdm")
set_deps(libtclcad  "libged;libdm")


# Note - brlman can be compliled with Tcl, Qt,
# or no graphical support, so we list libbu explicitly
set_deps(brlman     "libqtcad;libtclcad;libbu")
set_deps(bwish      "libtclcad")
set_deps(conv       "libged;libgcv")
set_deps(fb         "libdm")
set_deps(fbserv     "libdm")
set_deps(gtools     "libged")
set_deps(nirt       "libanalyze")
set_deps(proc-db    "libwdb")
set_deps(rt         "libdm;librt;liboptical;libicv")
set_deps(art        "libged;libdm")
set_deps(shapes     "libged")
set_deps(sig        "libfft;libbu")
set_deps(util       "libdm;libicv;libwdb")
set_deps(qged       "libqtcad")
set_deps(external   "libwdb")
set_deps(remrt      "libdm;liboptical")
# tclscripts must come before applications like
# mged and archer that need the scripts in place to
# run. The script target lists are defined when the tclscripts
# directories are configured, and those lists are needed
# as dependencies for the targets in these directories
set_deps(tclscripts "libtclcad")
set_deps(adrt       "libtclcad")
set_deps(isst       "libtclcad;libqtcad")
set_deps(rtwizard   "libtclcad")
set_deps(archer     "libtclcad")
set_deps(mged       "libtclcad")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
