set(
  man3_EN
  libanalyze.adoc
  libbg.adoc
  libbn.adoc
  libbrep.adoc
  libbu.adoc
  libbv.adoc
  libdm.adoc
  libfft.adoc
  libgcv.adoc
  libged.adoc
  libicv.adoc
  libnmg.adoc
  liboptical.adoc
  libpc.adoc
  libpkg.adoc
  libqtcad.adoc
  librt.adoc
  libtclcad.adoc
  libtermio.adoc
  libwdb.adoc
)

# Libraries that ship as headers/sources without a standalone linkable
# target but still have reference man pages we want to publish.
set(man3_NOTARGET libpc libtermio)

foreach(mp ${man3_EN})
  get_filename_component(tname ${mp} NAME_WE)

  # BRLCAD_ADDLIB doesn't take care of the man pages currently - most of our
  # libs don't have one at the moment - so do it here if we have the associated
  # target (or if the library is documented but intentionally has no target).
  list(FIND man3_NOTARGET "${tname}" tname_notarget)
  if((TARGET ${tname} OR NOT tname_notarget EQUAL -1) AND BRLCAD_EXTRADOCS)
    add_asciidoc("MAN3" ${mp} man3 "")
  endif()

  # Warn if we've got a man page but no target, unless we're doing a partial
  # build (if the build is partial it's expected that some of the man pages
  # will be disabled) or the library intentionally has no target.
  if(NOT BRLCAD_COMPONENTS)
    if(NOT TARGET ${tname} AND tname_notarget EQUAL -1)
      message("Warning: man3 man page ${tname} has no associated build target")
    endif()
  endif(NOT BRLCAD_COMPONENTS)
endforeach(mp ${man3_EN})

cmakefiles(
  ${man3_EN}
  TODO
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
