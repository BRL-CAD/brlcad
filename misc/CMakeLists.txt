if(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)
  add_subdirectory(win32-msvc)
endif(BUILD_STATIC_LIBS AND BRLCAD_ENABLE_BRLCAD_LIBRARY)

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(datarootdir "\${prefix}/share")
set(datadir "\${datarootdir}")

add_subdirectory(pkgconfig)
add_subdirectory(perl)
add_subdirectory(attributes)
add_subdirectory(opencl-raytracer-tests)

if(NOT CMAKE_CONFIGURATION_TYPES)
  configure_file(brlcad-config.in ${CMAKE_BINARY_DIR}/misc/brlcad-config @ONLY)
  file(COPY ${CMAKE_BINARY_DIR}/misc/brlcad-config
    DESTINATION ${CMAKE_BINARY_DIR}/${BIN_DIR}
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  DISTCLEAN(${CMAKE_BINARY_DIR}/${BIN_DIR}/brlcad-config)
  file(REMOVE ${CMAKE_BINARY_DIR}/misc/brlcad-config)
  install(PROGRAMS ${CMAKE_BINARY_DIR}/bin/brlcad-config DESTINATION ${BIN_DIR})
else(NOT CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    configure_file(brlcad-config.in ${CMAKE_BINARY_DIR}/${CFG_TYPE_UPPER}/misc/brlcad-config @ONLY)
    file(COPY ${CMAKE_BINARY_DIR}/${CFG_TYPE_UPPER}/misc/brlcad-config
      DESTINATION ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}
      FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
      GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    DISTCLEAN(${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${BIN_DIR}/brlcad-config)
    file(REMOVE ${CMAKE_BINARY_DIR}/${CFG_TYPE_UPPER}/misc/brlcad-config)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  install(PROGRAMS ${CMAKE_BINARY_DIR}/\${BUILD_TYPE}/bin/brlcad-config DESTINATION ${BIN_DIR})
endif(NOT CMAKE_CONFIGURATION_TYPES)

BRLCAD_ADDDATA(fortran_example.f sample_applications)

set(density_DATAFILES
  GQA_SAMPLE_DENSITIES
  NIST_DENSITIES
  )
BRLCAD_ADDDATA(density_DATAFILES data)

add_subdirectory(nirt)

BRLCAD_ADDDATA(profont/ProFont.ttf fonts)

BRLCAD_ADDDATA(nsis/brlcad.ico icons)
BRLCAD_ADDDATA(nsis/archer.ico icons)


add_subdirectory(doxygen)
add_subdirectory(CMake)
add_subdirectory(debian)
add_subdirectory(osl_shaders)

set(misc_ignore_files
  Bullet_Box_Chain_Demo.cpp
  CMakeLists.txt
  Description.txt
  Makefile.defs
  archlinux/PKGBUILD
  archlinux/brlcad.install
  astyle.opt
  auto-man-page/BIN_OPT_ARG_ANALYSIS.txt
  auto-man-page/README.auto-man-page-handling
  auto-man-page/auto-man-progs.txt
  batch-indent-region.el
  brlcad-config.in
  brlcad.spec.in
  check/adj_air.sh
  check/exp_air.sh
  check/overlaps.sh
  check/rtcheck.sh
  check/volume.sh
  check/gap.sh
  clang-static-analyzer-run.sh
  flawfinder
  hawaii_islands.sh
  jove.emacs
  macosx/README
  macosx/Resources/License.rtf
  macosx/Resources/ReadMe.rtfd/.typeAttributes.dict
  macosx/Resources/ReadMe.rtfd/TXT.rtf
  macosx/Resources/ReadMe.rtfd/brlcad_logo_tiny.png
  macosx/Resources/Welcome.rtfd/.typeAttributes.dict
  macosx/Resources/Welcome.rtfd/TXT.rtf
  macosx/Resources/Welcome.rtfd/brlcad_logo_tiny.png
  macosx/background.png
  macosx/openUp.c
  mime.types
  mime_cad.types
  nsis/archer.ico
  nsis/brlcad.ico
  nsis/header.bmp
  nsis/side.bmp
  nsis/uninstall.ico
  profont/LICENSE
  profont/profont-IIx-charmap.png
  profont/profont_IIx_readme.pdf
  profont/readme.txt
  uncpowerplant2g.sh
  repoconv/29886-note-template.fi
  repoconv/CONVERT.sh
  repoconv/NOTES
  repoconv/RESUME.sh
  repoconv/audit_merges/audit_merges.cxx
  repoconv/audit_merges/commit_list.txt
  repoconv/checkout_dercs.sh
  repoconv/common_structs.h
  repoconv/custom/29937-blob.fi
  repoconv/custom/29982-tree.fi
  repoconv/custom/30760-b.fi
  repoconv/custom/30760-commit.fi
  repoconv/custom/31039-b.fi
  repoconv/custom/31039-tree.fi
  repoconv/custom/32007-blob.fi
  repoconv/custom/32007-tree.fi
  repoconv/custom/32009-blob.fi
  repoconv/custom/32009-tree.fi
  repoconv/custom/32046-blob.fi
  repoconv/custom/32046-tree.fi
  repoconv/custom/32314-b.fi
  repoconv/custom/32314-tree.fi
  repoconv/custom/32430-blob.fi
  repoconv/custom/33115-tree.fi
  repoconv/custom/33147-blob.fi
  repoconv/custom/33713-blob.fi
  repoconv/custom/35881-tree.fi
  repoconv/custom/36053-b.fi
  repoconv/custom/36053-blob.fi
  repoconv/custom/36053-commit.fi
  repoconv/custom/36053-tree.fi
  repoconv/custom/36472-b.fi
  repoconv/custom/36472-commit.fi
  repoconv/custom/36631-blob.fi
  repoconv/custom/36631-tree.fi
  repoconv/custom/36633-b.fi
  repoconv/custom/36633-commit.fi
  repoconv/custom/36633-tree.fi
  repoconv/custom/36843-b.fi
  repoconv/custom/36843-commit.fi
  repoconv/custom/36843-tree.fi
  repoconv/custom/36941-tree.fi
  repoconv/custom/37277-tree.fi
  repoconv/custom/37288-tree.fi
  repoconv/custom/39465-b.fi
  repoconv/custom/39465-commit.fi
  repoconv/custom/39465-tree.fi
  repoconv/custom/39897-tree.fi
  repoconv/custom/40401-tree.fi
  repoconv/custom/40402-tree.fi
  repoconv/custom/41112-tree.fi
  repoconv/custom/41269-tree.fi
  repoconv/custom/41349-tree.fi
  repoconv/custom/41871-tree.fi
  repoconv/custom/42728-blob.fi
  repoconv/custom/42728-tree.fi
  repoconv/custom/43254-tree.fi
  repoconv/custom/44463-tree.fi
  repoconv/custom/44465-tree.fi
  repoconv/custom/54597-blob.fi
  repoconv/custom/54598-blob.fi
  repoconv/custom/54916-blob.fi
  repoconv/custom/57359-tree.fi
  repoconv/custom/58306-blob.fi
  repoconv/custom/60192-blob.fi
  repoconv/custom/60421-blob.fi
  repoconv/custom/61149-b.fi
  repoconv/custom/61149-commit.fi
  repoconv/custom/61579-blob.fi
  repoconv/custom/61579-tree.fi
  repoconv/custom/61844-blob.fi
  repoconv/custom/61845-blob.fi
  repoconv/custom/62029-b.fi
  repoconv/custom/64060-b.fi
  repoconv/custom/64060-commit.fi
  repoconv/custom/65867-b.fi
  repoconv/custom/65867-commit.fi
  repoconv/custom/68611-b.fi
  repoconv/custom/68611-commit.fi
  repoconv/custom/69909-b.fi
  repoconv/custom/69909-commit.fi
  repoconv/custom/69921-b.fi
  repoconv/custom/70326-b.fi
  repoconv/custom/70326-commit.fi
  repoconv/custom/71925-b.fi
  repoconv/custom/71925-commit.fi
  repoconv/custom/72855-b.fi
  repoconv/custom/72855-commit.fi
  repoconv/custom/73059-b.fi
  repoconv/custom/73059-commit.fi
  repoconv/custom/r29886_branches.fi
  repoconv/custom/r29886_cjohnson_mac_hack.fi
  repoconv/custom/r29886_cvs_svn_rel-5-1-branch_sync.fi
  repoconv/custom/r29886_cvs_svn_trunk_sync.fi
  repoconv/custom/r29886_tags.fi
  repoconv/account-map
  repoconv/account-map_svnfexport.txt
  repoconv/cvs_authormap
  repoconv/cvs_authormap_svnfexport.txt
  repoconv/cvs_info.sh
  repoconv/domap.sh
  repoconv/email_fixups.txt
  repoconv/cvs_repaired/sphflake.pix,v
  repoconv/cvs_svn_branches.txt
  repoconv/gitattributes
  repoconv/github_ci_actions.yml
  repoconv/gitignore.tar.gz
  repoconv/manual_merge_info.tar.gz
  repoconv/md5.hpp
  repoconv/sha1.hpp
  repoconv/sha1_update.sh
  repoconv/svn2git/README
  repoconv/svn2git/archive_branches.sh
  repoconv/svn2git/rules
  repoconv/svn2git/rules-geomcore
  repoconv/svn2git/svn-fast-export/CMake/FindAPR.cmake
  repoconv/svn2git/svn-fast-export/CMake/FindSubversionLibrary.cmake
  repoconv/svn2git/svn-fast-export/CMakeLists.txt
  repoconv/svn2git/svn-fast-export/CommandLineParser.cpp
  repoconv/svn2git/svn-fast-export/CommandLineParser.h
  repoconv/svn2git/svn-fast-export/LICENSE
  repoconv/svn2git/svn-fast-export/README.md
  repoconv/svn2git/svn-fast-export/main.cpp
  repoconv/svn2git/svn-fast-export/repository.cpp
  repoconv/svn2git/svn-fast-export/repository.h
  repoconv/svn2git/svn-fast-export/ruleparser.cpp
  repoconv/svn2git/svn-fast-export/ruleparser.h
  repoconv/svn2git/svn-fast-export/samples/ignore-branch.rules
  repoconv/svn2git/svn-fast-export/samples/merged-branches-tags.rules
  repoconv/svn2git/svn-fast-export/samples/min-max-revision.rules
  repoconv/svn2git/svn-fast-export/samples/recurse.rules
  repoconv/svn2git/svn-fast-export/samples/standardlayout.rules
  repoconv/svn2git/svn-fast-export/samples/two-projects.rules
  repoconv/svn2git/svn-fast-export/svn.cpp
  repoconv/svn2git/svn-fast-export/svn.h
  repoconv/svn2git/svn2git_gitignore_files-r29886_to_r72245.tar.gz
  repoconv/svn_date.cxx
  repoconv/svn_date.h
  repoconv/svn_de-rcs.cxx
  repoconv/svn_git_diff.cxx
  repoconv/svn_map_commit_revs.cxx
  repoconv/svn_msgs.cxx
  repoconv/svn_rev_updates.txt
  repoconv/branch_corrections.txt
  repoconv/remove_commits.txt
  repoconv/svnfexport.cxx
  repoconv/svnfexport_git.cxx
  repoconv/svnfexport_svn.cxx
  repoconv/sync_commit.sh
  repoconv/sync_commit_trunk.sh
  repoconv/tag_commits.txt
  repoconv/tagmap.sh
  repoconv/terra.dsp
  repoconv/verify/cvs.sh
  repoconv/verify/cvs_repaired/sphflake.pix,v
  repoconv/verify/cxxopts.hpp
  repoconv/verify/md5.hpp
  repoconv/verify/sha1.hpp
  repoconv/verify/svn.sh
  repoconv/verify/svn_de-rcs.cxx
  repoconv/verify/verify.cpp
  repoconv/verify/verify.sh
  repowork/COPYING
  repowork/README
  repowork/blob.cpp
  repowork/TextFlow.hpp
  repowork/repowork.cpp
  repowork/reset.cpp
  repowork/misc_cmds.cpp
  repowork/cxxopts.hpp
  repowork/tag.cpp
  repowork/repowork.h
  repowork/commit.cpp
  repowork/CMakeLists.txt
  win32-msvc/CMakeLists.txt
  win32-msvc/Dll/BrlcadCore.def
  win32-msvc/Dll/BrlcadCore.rc
  win32-msvc/Dll/CMakeLists.txt
  win32-msvc/Dll/TclDummies.c
  win32-msvc/Dll/brlcad.def
  win32-msvc/Dll/brlcad.rc
  win32-msvc/bootstrap.cpp
  wix/License.rtf
  wix/brlcad_banner.bmp
  wix/brlcad_dialog.bmp
  wix/brlcad_product.ico
  )
CMAKEFILES(${misc_ignore_files})
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8


