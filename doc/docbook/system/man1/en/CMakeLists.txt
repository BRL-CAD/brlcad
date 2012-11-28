set(docbook_man1_EN
  g2asc.xml
  asc2g.xml
  pix2asc.xml
  asc2pix.xml
  asc2dsp.xml
  bot_dump.xml
  bot_shell-vtk.xml
  bwscale.xml
  coil.xml
  comgeom-g.xml
  conv-vg2g.xml
  cy-g.xml
  db.xml
  dbclean.xml
  dbupgrade.xml
  dxf-g.xml
  g-dxf.xml
  enf-g.xml
  euclid-g.xml
  g-euclid.xml
  fast4-g.xml
  g-acad.xml
  g-egg.xml
  g-nff.xml
  g-obj.xml
  g-shell-rect.xml
  g-var.xml
  g-vrml.xml
  g-x3d.xml
  gqa.xml
  g-iges.xml
  iges-g.xml
  mged.xml
  nastran-g.xml
  nirt.xml
  asc-nmg.xml
  g-nmg.xml
  nmg-bot.xml
  nmg-rib.xml
  obj-g.xml
  patch-g.xml
  rpatch.xml
  ply-g.xml
  poly-bot.xml
  proe-g.xml
  rt.xml
  rtarea.xml
  rtcheck.xml
  rtedge.xml
  rtwizard.xml
  g-stl.xml
  stl-g.xml
  g-tankill.xml
  tankill-g.xml
  tire.xml
  vdeck.xml
  viewpoint-g.xml
  )

DOCBOOK_TO_HTML(man1 docbook_man1_EN html/man1/en "")
DOCBOOK_TO_MAN(man1 docbook_man1_EN 1 1 man1 "")

if(BRLCAD_EXTRADOCS_PDF_MAN)
  DOCBOOK_TO_PDF(man1 docbook_man1_EN pdf/man1/en "")
endif(BRLCAD_EXTRADOCS_PDF_MAN)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
