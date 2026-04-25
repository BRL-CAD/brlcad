set(
  man1_EN
  3dm-g.adoc
  alias-pix.adoc
  ap-pix.adoc
  asc-nmg.adoc
  asc-plot3.adoc
  asc2dsp.adoc
  asc2g.adoc
  bolt.adoc
  bot_dump.adoc
  brep_simple.adoc
  brlman.adoc
  btclsh.adoc
  bw-fb.adoc
  bw-pix.adoc
  bw-png.adoc
  bw-ps.adoc
  bwcrop.adoc
  bwdiff.adoc
  bwfilter.adoc
  bwhist.adoc
  bwhisteq.adoc
  bwish.adoc
  bwmod.adoc
  bwrect.adoc
  bwrot.adoc
  bwscale.adoc
  bwshrink.adoc
  bwstat.adoc
  bwthresh.adoc
  cad_boundp.adoc
  cad_parea.adoc
  coil.adoc
  comgeom-g.adoc
  conv-vg2g.adoc
  csg-g.adoc
  cv.adoc
  dbupgrade.adoc
  decimate.adoc
  dpix-pix.adoc
  dxf-g.adoc
  enf-g.adoc
  fast4-g.adoc
  fb-bw.adoc
  fb-fb.adoc
  fb-pix.adoc
  fb-png.adoc
  fbcbars.adoc
  fbclear.adoc
  fbcmap.adoc
  fbcolor.adoc
  fbfade.adoc
  fbframe.adoc
  fbfree.adoc
  fbgamma.adoc
  fbgrid.adoc
  fbhelp.adoc
  fblabel.adoc
  fbline.adoc
  fbpoint.adoc
  fbserv.adoc
  fbstretch.adoc
  fbzoom.adoc
  g-acad.adoc
  g-dot.adoc
  g-dxf.adoc
  g-egg.adoc
  g-iges.adoc
  g-nff.adoc
  g-obj.adoc
  g-ply.adoc
  g-raw.adoc
  g-scad.adoc
  g-shell-rect.adoc
  g-step.adoc
  g-stl.adoc
  g-var.adoc
  g-voxel.adoc
  g-vrml.adoc
  g-x3d.adoc
  g2asc.adoc
  gastank.adoc
  gdiff.adoc
  gencolor.adoc
  gif-fb.adoc
  gif2fb.adoc
  gist.adoc
  glint.adoc
  gqa.adoc
  halftone.adoc
  handle.adoc
  icv.adoc
  iges-g.adoc
  imgdims.adoc
  loop.adoc
  mac-pix.adoc
  mged.adoc
  nastran-g.adoc
  nirt.adoc
  obj-g.adoc
  patch-g.adoc
  picket_fence.adoc
  pix-alias.adoc
  pix-bw.adoc
  pix-fb.adoc
  pix-png.adoc
  pix-ppm.adoc
  pix-ps.adoc
  pix-sun.adoc
  pix2asc.adoc
  pixautosize.adoc
  pixbackgnd.adoc
  pixbgstrip.adoc
  pixblend.adoc
  pixborder.adoc
  pixbustup.adoc
  pixclump.adoc
  pixcmp.adoc
  pixcolors.adoc
  pixcrop.adoc
  pixdiff.adoc
  pixfade.adoc
  pixfilter.adoc
  pixhalve.adoc
  pixhist.adoc
  pixhist3d-plot3.adoc
  pixhist3d.adoc
  pixinterp2x.adoc
  pixmatte.adoc
  pixmerge.adoc
  pixmorph.adoc
  pixrect.adoc
  pixrot.adoc
  pixsaturate.adoc
  pixscale.adoc
  pixshrink.adoc
  pixstat.adoc
  pixsubst.adoc
  pixtile.adoc
  pixuntile.adoc
  plot3-asc.adoc
  plot3-fb.adoc
  plot3-plot3.adoc
  plot3-ps.adoc
  plot3color.adoc
  plot3getframe.adoc
  plot3line2.adoc
  plot3rot.adoc
  ply-g.adoc
  png-bw.adoc
  png-fb.adoc
  png-pix.adoc
  random.adoc
  remrt.adoc
  reshoot.adoc
  rpatch.adoc
  rt.adoc
  rtarea.adoc
  rtcheck.adoc
  rtdepth.adoc
  rtedge.adoc
  rtexample.adoc
  rtg3.adoc
  rthide.adoc
  rtscale.adoc
  rtshot.adoc
  rtweight.adoc
  rtwizard.adoc
  rtxray.adoc
  showshot.adoc
  stl-g.adoc
  sun-pix.adoc
  tankill-g.adoc
  terrain.adoc
  tire.adoc
  ug-g.adoc
  vdeck.adoc
  wavelet.adoc
  wdb_example.adoc
  window.adoc
  window_frame.adoc
  wire.adoc
)

if(BRLCAD_EXTRADOCS)
  add_asciidoc("MAN1" benchmark.adoc man1 "" REQUIRED "rt")
  add_asciidoc("MAN1" brlcad-config.adoc man1 "" REQUIRED "rt")
  add_asciidoc("MAN1" brlcad.adoc man1 "" REQUIRED "mged")
  add_asciidoc("MAN1" dbclean.adoc man1 "" REQUIRED "asc2g")

  # BRLCAD_ADDEXEC takes care of the add_asciidoc call - we just need to warn if
  # we've got a man page in the list indicating we should have an associated
  # target but not showing one.  Generally this will be an indication of a tool
  # not ported to the current platform.  Only do this if we're doing a full build
  # rather than a subset, since the latter case will likely report many man pages
  # without associated targets and isn't useful.
  if(NOT BRLCAD_COMPONENTS)
    foreach(mp ${man1_EN})
      get_filename_component(tname ${mp} NAME_WE)
      if(NOT TARGET ${tname})
        message("Warning: man1 man page ${tname} has no associated build target")
      endif(NOT TARGET ${tname})
    endforeach(mp ${man3_EN})
  endif(NOT BRLCAD_COMPONENTS)
endif(BRLCAD_EXTRADOCS)

cmakefiles(
  ${man1_EN}
  benchmark.adoc
  brlcad-config.adoc
  brlcad.adoc
  dbclean.adoc
  CMakeLists.txt
)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
