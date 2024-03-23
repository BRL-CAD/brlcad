set(notarget_man3_EN
  redblack.xml
)
ADD_DOCBOOK("HTML;PHP;MAN3;PDF" notarget_man3_EN man3 "")

set(man3_EN
  libbv.xml
  libpkg.xml
  librt.xml
  libwdb.xml
  libicv.xml
)

foreach(mp ${man3_EN})
  get_filename_component(tname ${mp} NAME_WE)

  # BRLCAD_ADDLIB doesn't take care of the man pages currently - most of our
  # libs don't have one at the moment - so do it here if we have the associated
  # target.
  if (TARGET ${tname})
    ADD_DOCBOOK("HTML;PHP;MAN3;PDF" ${mp} man3 "")
  endif (TARGET ${tname})

  # Warn if we've got a man page but no target, unless we're doing a partial
  # build (if the build is partial its not surprising that some of the man
  # pages will be disabled
  if (NOT BRLCAD_COMPONENTS)
    if (NOT TARGET ${tname})
      message("Warning: man3 man page ${tname} has no associated build target")
    endif (NOT TARGET ${tname})
  endif (NOT BRLCAD_COMPONENTS)

endforeach(mp ${man3_EN})

CMAKEFILES(
  ${notarget_man3_EN}
  ${man3_EN}
  libbu.xml
  libdm.xml
  TODO
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
