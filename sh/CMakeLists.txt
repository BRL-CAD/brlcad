# Note: elapsed.sh is installed because it is used by benchmark
set(sh_SCRIPTS elapsed.sh cmp.sh conversion.sh show.sh)
foreach(script ${sh_SCRIPTS})
  file(
    COPY ${script}
    DESTINATION ${CMAKE_BINARY_DIR}/bin
    FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
  )
  distclean("${CMAKE_BINARY_DIR}/bin/${script}")
endforeach(script ${sh_SCRIPTS})

install(
  PROGRAMS ${sh_SCRIPTS}
  DESTINATION ${BIN_DIR}
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
)

set(
  noinst_SCRIPTS
  copyright.sh
  enumerate.sh
  facetall.sh
  facetize_cmp.sh
  footer.sh
  fs2org.sh
  mirror_repos.sh
  header.sh
  indent.sh
  ios-icons.sh
  linkroot.sh
  make_deb.sh
  make_dmg.sh
  make_rpm.sh
  news2tracker.sh
  template.sh
  update_bext.sh
  ws.sh
  sandbox/metaring.sh
  sandbox/orbit.sh
  sandbox/pametric_demo.sh
  sandbox/visibility.sh
  sandbox/tile.sh
)
cmakefiles(${sh_SCRIPTS})
cmakefiles(${noinst_SCRIPTS})
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
