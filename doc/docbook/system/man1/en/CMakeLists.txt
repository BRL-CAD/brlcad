SET(docbook_man1_EN
	bwscale.xml 
	coil.xml 
	db.xml 
	gqa.xml 
	mged.xml 
	nirt.xml 
	obj-g.xml 
	rt.xml 
	rtarea.xml 
	rtcheck.xml 
	rtedge.xml 
	tire.xml 
)

FOREACH(man1 ${docbook_man1_EN})
	STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" man1_root "${man1}")
	DOCBOOK_TO_MAN(${man1_root}_man1 ${man1} ${man1_root}.1 ${BRLCAD_INSTALL_MAN_DIR}/man1)
ENDFOREACH(man1 ${docbook_man1_EN})

