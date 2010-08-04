# BotPropertyBox class for viewing/manipulating BoT properties
#
::itcl::class BotPropertyBox {
    inherit ::itk::Widget

    constructor {args} {}

    itk_option define -command command Command {} {}
}

::itcl::body BotPropertyBox::constructor {args} {

    eval itk_initialize $args

    # make container frame
    itk_component add main {
	ttk::frame $itk_interior.propertyFrame
    } {}

    # add notebook to container frame
    itk_component add nb {
	ttk::notebook $itk_component(main).notebook
    } {}

    # add tab panes to container frame
    itk_component add tpane {
	TypePane $itk_component(main).typePane
    } {}
    itk_component add gpane {
	GeometryPane $itk_component(main).geometryPane
    } {}

    # display main frame
    pack $itk_component(main) -expand yes -fill both

    # display notebook in main frame
    pack $itk_component(nb) -expand yes -fill both

    # display tab panes in notebook
    $itk_component(nb) add $itk_component(gpane) \
        -text Geometry \
	-sticky nw
    $itk_component(nb) add $itk_component(tpane) \
        -text Type \
	-sticky nw
}

::itcl::class TypePane {
    inherit itk::Widget

    constructor {args} {
	eval itk_initialize $args

	# make container frame
	itk_component add main {
	    ttk::frame $itk_interior.typePaneFrame
	} {}

	# add layout frames to container frame
	itk_component add cframe {
	    ttk::frame $itk_component(main).contentFrame \
	        -padding 5
	} {}
	itk_component add sframe {
	    ttk::frame $itk_component(main).springFrame
	} {}

	# add widgets to content frame
	itk_component add surfRadio {
	    ttk::radiobutton $itk_component(cframe).surfaceRadio \
	        -text Surface
	} {}
	itk_component add volRadio {
	    ttk::radiobutton $itk_component(cframe).volumeRadio \
	        -text Volume
	} {}
	itk_component add plateRadio {
	    ttk::radiobutton $itk_component(cframe).plateRadio \
	        -text Plate
	} {}

	# display container frame
	pack $itk_component(main) -expand yes -fill both

	# display layout frames in container frame
	grid $itk_component(cframe) -row 0 -column 0
	grid $itk_component(sframe) -row 1 -column 0 -sticky news
	grid rowconfigure $itk_component(main) 1 -weight 1
	grid columnconfigure $itk_component(main) 0 -weight 1

	# display widgets in content frame - no expansion
	grid $itk_component(surfRadio) -row 0 -column 0 -sticky nw
	grid $itk_component(volRadio) -row 1 -column 0 -sticky nw
	grid $itk_component(plateRadio) -row 2 -column 0 -sticky nw
    }
}

::itcl::class GeometryPane {
    inherit itk::Widget

    constructor {args} {
	eval itk_initialize $args

	# make container frame
	itk_component add main {
	    ttk::frame $itk_interior.geometryPaneFrame
	} {}

	# add layout frames to container frame
	itk_component add cframe {
	    ttk::frame $itk_component(main).contentFrame \
	        -padding 5
	} {}
	itk_component add sframe {
	    ttk::frame $itk_component(main).springFrame
	} {}

	# add widgets to content frame
	itk_component add faces {
	    ttk::label $itk_component(cframe).faces \
	        -text {Faces: }
	} {}

	itk_component add vertices {
	    ttk::label $itk_component(cframe).vertices \
	        -text {Vertices: }
	} {}

	# display container frame
	pack $itk_component(main) -expand yes -fill both

	# display layout frames in container frame
	grid $itk_component(cframe) -row 0 -column 0
	grid $itk_component(sframe) -row 1 -column 0 -sticky news
	grid rowconfigure $itk_component(main) 1 -weight 1
	grid columnconfigure $itk_component(main) 0 -weight 1

	# display widgets in content frame - no expansion
	grid $itk_component(faces) -row 0 -column 0 -sticky nw
	grid $itk_component(vertices) -row 1 -column 0 -sticky nw
    }
}
