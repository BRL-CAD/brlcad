SET(swidgets_scripts_TCLSCRIPTS
	selectlists.itk
	tkgetdir.itk
	togglearrow.itk
	tooltip.itk
	tree.itk
	)
tclscripts_BUILD(swidgets_scripts_TCLSCRIPTS tclscripts/swidgets/scripts)
tclIndex_BUILD(swidgets_scripts tclscripts/swidgets/scripts)

