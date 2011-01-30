ADD_SUBDIRECTORY(images)
ADD_SUBDIRECTORY(scripts)

SET(swidgets_TCLSCRIPTS
	swidgets.tcl
)
tclscripts_BUILD(swidgets_TCLSCRIPTS tclscripts/swidgets)
pkgIndex_BUILD(swidgets tclscripts/swidgets)
tclIndex_BUILD(swidgets tclscripts/swidgets)

