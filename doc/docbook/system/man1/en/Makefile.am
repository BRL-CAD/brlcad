man1DOCBOOK_EN = \
	system/man1/en/3ptarb.xml \
	system/man1/en/B.xml \
	system/man1/en/E.xml \
	system/man1/en/M.xml \
	system/man1/en/Z.xml \
	system/man1/en/adc.xml \
	system/man1/en/ae.xml \
	system/man1/en/alias-pix.xml \
	system/man1/en/all_sf.xml \
	system/man1/en/analyze.xml \
	system/man1/en/anim_cascade.xml \
	system/man1/en/anim_fly.xml \
	system/man1/en/anim_hardtrack.xml \
	system/man1/en/anim_keyread.xml \
	system/man1/en/anim_lookat.xml \
	system/man1/en/anim_offset.xml \
	system/man1/en/anim_orient.xml \
	system/man1/en/animmate.xml \
	system/man1/en/apropos.xml \
	system/man1/en/aproposdevel.xml \
	system/man1/en/aproposlib.xml \
	system/man1/en/arb.xml \
	system/man1/en/arced.xml \
	system/man1/en/area.xml \
	system/man1/en/arot.xml \
	system/man1/en/attach.xml \
	system/man1/en/attr.xml \
	system/man1/en/autoview.xml \
	system/man1/en/bb.xml \
	system/man1/en/bev.xml \
	system/man1/en/bo.xml \
	system/man1/en/bolt.xml \
	system/man1/en/bot_condense.xml \
	system/man1/en/bot_decimate.xml \
	system/man1/en/bot_face_fuse.xml \
	system/man1/en/bot_face_sort.xml \
	system/man1/en/bot_vertex_fuse.xml \
	system/man1/en/build_region.xml \
	system/man1/en/bwscale.xml \
	system/man1/en/c.xml \
	system/man1/en/cat.xml \
	system/man1/en/center.xml \
	system/man1/en/coil.xml \
	system/man1/en/color.xml \
	system/man1/en/comb_color.xml \
	system/man1/en/copyeval.xml \
	system/man1/en/copymat.xml \
	system/man1/en/cp.xml \
	system/man1/en/cpi.xml \
	system/man1/en/d.xml \
	system/man1/en/dall.xml \
	system/man1/en/db.xml \
	system/man1/en/db_glob.xml \
	system/man1/en/dbconcat.xml \
	system/man1/en/debugbu.xml \
	system/man1/en/debugdir.xml \
	system/man1/en/debuglib.xml \
	system/man1/en/debugmem.xml \
	system/man1/en/debugnmg.xml \
	system/man1/en/decompose.xml \
	system/man1/en/delay.xml \
	system/man1/en/dm.xml \
	system/man1/en/gastank.xml \
	system/man1/en/gqa.xml \
	system/man1/en/handle.xml \
	system/man1/en/mged.xml \
	system/man1/en/nirt.xml \
	system/man1/en/picket_fence.xml \
	system/man1/en/rt.xml \
	system/man1/en/rtarea.xml \
	system/man1/en/rtcheck.xml \
	system/man1/en/rtedge.xml \
	system/man1/en/search.xml \
	system/man1/en/tire.xml \
	system/man1/en/window.xml \
	system/man1/en/window_frame.xml \
	system/man1/en/wire.xml

man1INTRO_EN = \
	system/man1/en/Introduction.xml

man1DOCBOOK_ENhtmldir = $(BRLCAD_DATA)/html/man1/en
man1DOCBOOK_ENhtml_DATA = $(BUILTMAN_ENHTML)

BUILTMAN_ENHTML = \
	${man1DOCBOOK_EN:xml=html} \
	${man1INTRO_EN:xml=html} 

if BUILD_PDF_DOCS

BUILTMAN_ENPDF = ${man1DOCBOOK_EN:xml=pdf}

endif

man1DOCBOOK_ENpdfdir = $(BRLCAD_DATA)/pdf/man1/en
man1DOCBOOK_ENpdf_DATA = $(BUILTMAN_ENPDF)
