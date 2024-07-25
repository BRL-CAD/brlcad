set(man1_EN
  3dm-g.xml
  alias-pix.xml
  ap-pix.xml
  asc-nmg.xml
  asc-plot3.xml
  asc2dsp.xml
  asc2g.xml
  bolt.xml
  bot_dump.xml
  brep_simple.xml
  brlman.xml
  bw-fb.xml
  bw-pix.xml
  bw-png.xml
  bw-ps.xml
  bwcrop.xml
  bwdiff.xml
  bwfilter.xml
  bwhist.xml
  bwhisteq.xml
  bwmod.xml
  bwrect.xml
  bwrot.xml
  bwscale.xml
  bwshrink.xml
  bwstat.xml
  bwthresh.xml
  cad_boundp.xml
  cad_parea.xml
  coil.xml
  comgeom-g.xml
  conv-vg2g.xml
  cv.xml
  dbupgrade.xml
  decimate.xml
  dpix-pix.xml
  dxf-g.xml
  enf-g.xml
  fast4-g.xml
  fb-bw.xml
  fb-fb.xml
  fb-pix.xml
  fb-png.xml
  fbcbars.xml
  fbclear.xml
  fbcmap.xml
  fbcolor.xml
  fbfade.xml
  fbframe.xml
  fbfree.xml
  fbgamma.xml
  fbgrid.xml
  fbhelp.xml
  fblabel.xml
  fbline.xml
  fbpoint.xml
  fbserv.xml
  fbstretch.xml
  fbzoom.xml
  g-acad.xml
  g-dot.xml
  g-dxf.xml
  g-egg.xml
  g-iges.xml
  g-nff.xml
  g-obj.xml
  g-ply.xml
  g-raw.xml
  g-shell-rect.xml
  g-step.xml
  g-stl.xml
  g-var.xml
  g-voxel.xml
  g-vrml.xml
  g-x3d.xml
  g2asc.xml
  gastank.xml
  gdiff.xml
  gencolor.xml
  gif-fb.xml
  gif2fb.xml
  gist.xml
  glint.xml
  gqa.xml
  halftone.xml
  handle.xml
  icv.xml
  iges-g.xml
  imgdims.xml
  loop.xml
  mac-pix.xml
  mged.xml
  nastran-g.xml
  nirt.xml
  obj-g.xml
  patch-g.xml
  picket_fence.xml
  pix-alias.xml
  pix-bw.xml
  pix-fb.xml
  pix-png.xml
  pix-ppm.xml
  pix-ps.xml
  pix-sun.xml
  pix2asc.xml
  pixautosize.xml
  pixbackgnd.xml
  pixbgstrip.xml
  pixblend.xml
  pixborder.xml
  pixbustup.xml
  pixclump.xml
  pixcolors.xml
  pixcrop.xml
  pixdiff.xml
  pixfade.xml
  pixfilter.xml
  pixhalve.xml
  pixhist.xml
  pixhist3d-plot3.xml
  pixhist3d.xml
  pixinterp2x.xml
  pixmatte.xml
  pixmerge.xml
  pixmorph.xml
  pixrect.xml
  pixrot.xml
  pixsaturate.xml
  pixscale.xml
  pixshrink.xml
  pixstat.xml
  pixsubst.xml
  pixtile.xml
  pixuntile.xml
  plot3-asc.xml
  plot3-fb.xml
  plot3-plot3.xml
  plot3-ps.xml
  plot3color.xml
  plot3getframe.xml
  plot3line2.xml
  plot3rot.xml
  ply-g.xml
  png-bw.xml
  png-fb.xml
  png-pix.xml
  random.xml
  remrt.xml
  reshoot.xml
  rpatch.xml
  rt.xml
  rtarea.xml
  rtcheck.xml
  rtdepth.xml
  rtedge.xml
  rtexample.xml
  rtg3.xml
  rthide.xml
  rtscale.xml
  rtshot.xml
  rtweight.xml
  rtwizard.xml
  rtxray.xml
  showshot.xml
  stl-g.xml
  sun-pix.xml
  tankill-g.xml
  terrain.xml
  tire.xml
  ug-g.xml
  vdeck.xml
  wavelet.xml
  wdb_example.xml
  window.xml
  window_frame.xml
  wire.xml
  )

if (BRLCAD_EXTRADOCS)

  ADD_DOCBOOK("HTML;PHP;MAN1;PDF" benchmark.xml man1 "" REQUIRED "rt")
  ADD_DOCBOOK("HTML;PHP;MAN1;PDF" brlcad-config.xml man1 "" REQUIRED "rt")
  ADD_DOCBOOK("HTML;PHP;MAN1;PDF" brlcad.xml man1 "" REQUIRED "mged")
  ADD_DOCBOOK("HTML;PHP;MAN1;PDF" dbclean.xml man1 "" REQUIRED "asc2g")

  # BRLCAD_ADDEXEC takes care of the ADD_DOCBOOK call - we just need to warn if
  # we've got a man page in the list indicating we should have an associated
  # target but not showing one.  Generally this will be an indication of a tool
  # not ported to the current platform.  Only do this if we're doing a full build
  # rather than a subset, since the latter case will likely report many man pages
  # without associated targets and isn't useful.
  if (NOT BRLCAD_COMPONENTS)
    foreach(mp ${man1_EN})
      get_filename_component(tname ${mp} NAME_WE)
        if (NOT TARGET ${tname})
	message("Warning: man1 man page ${tname} has no associated build target")
      endif (NOT TARGET ${tname})
    endforeach(mp ${man3_EN})
  endif (NOT BRLCAD_COMPONENTS)

endif (BRLCAD_EXTRADOCS)

CMAKEFILES(
  ${man1_EN}
  benchmark.xml
  brlcad-config.xml
  brlcad.xml
  dbclean.xml
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
