set(
  man3_EN
  libbv.adoc
  libicv.adoc
  libpkg.adoc
  librt.adoc
  libwdb.adoc
)

foreach(mp ${man3_EN})
  get_filename_component(tname ${mp} NAME_WE)

  # BRLCAD_ADDLIB doesn't take care of the man pages currently - most of our
  # libs don't have one at the moment - so do it here if we have the associated
  # target.
  if(TARGET ${tname} AND BRLCAD_EXTRADOCS)
    add_asciidoc("MAN3" ${mp} man3 "")
  endif(TARGET ${tname} AND BRLCAD_EXTRADOCS)

  # Warn if we've got a man page but no target, unless we're doing a partial
  # build (if the build is partial it's expected that some of the man pages
  # will be disabled)
  if(NOT BRLCAD_COMPONENTS)
    if(NOT TARGET ${tname})
      message("Warning: man3 man page ${tname} has no associated build target")
    endif(NOT TARGET ${tname})
  endif(NOT BRLCAD_COMPONENTS)
endforeach(mp ${man3_EN})

cmakefiles(
  ${man3_EN}
  libbu.adoc
  libdm.adoc
  TODO
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
