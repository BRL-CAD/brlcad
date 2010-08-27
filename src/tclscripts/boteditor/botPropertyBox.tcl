# BotPropertyBox class for viewing/manipulating BoT properties
#
# Usage: BotPropertyBox <instance name> <bot name> \
#    [-command <callback>] 
# 
# The callback function passed in the -command option is called whenever
# the supplied bot is modified.
#
package require Tk
package require Itcl
package require Itk

::itcl::class BotPropertyBox {
    inherit ::itk::Widget

    constructor {bot args} {}

    public {
	common TYPE_SURFACE 1
	common TYPE_VOLUME 2
	common TYPE_PLATE 3

	method update {bot}
    }

    itk_option define -command command Command {} {}
}

::itcl::body BotPropertyBox::constructor {bot args} {

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
	TypePane $itk_component(main).typePane $bot
    } {}
    itk_component add gpane {
	GeometryPane $itk_component(main).geometryPane $bot
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

# update information for bot
::itcl::body BotPropertyBox::update {bot} {
    $itk_component(gpane) component modfaces configure \
	-text [bot get faces $bot]

    $itk_component(gpane) component modverts configure \
	-text [bot get vertices $bot]

    set ::${itk_interior}Radio [bot get type $bot]
}

::itcl::class TypePane {
    inherit itk::Widget

    constructor {bot args} {
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

	# add radio widgets to content frame
	set ::${itk_interior}Radio 0
	itk_component add surfRadio {
	    ttk::radiobutton $itk_component(cframe).surfaceRadio \
	        -text Surface \
		-value $BotPropertyBox::TYPE_SURFACE \
		-variable ::${itk_interior}Radio
	} {}
	itk_component add volRadio {
	    ttk::radiobutton $itk_component(cframe).volumeRadio \
	        -text Volume \
		-value $BotPropertyBox::TYPE_VOLUME \
		-variable ${itk_interior}Radio
	} {}
	itk_component add plateRadio {
	    ttk::radiobutton $itk_component(cframe).plateRadio \
	        -text Plate \
		-value $BotPropertyBox::TYPE_PLATE \
		-variable ${itk_interior}Radio
	} {}

	# select appropriate radio
	set ::${itk_interior}Radio [bot get type $bot]
	
	# disabling for now
        $itk_component(surfRadio) configure -state disabled
        $itk_component(volRadio) configure -state disabled
        $itk_component(plateRadio) configure -state disabled

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

    constructor {bot args} {
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

	# add header widgets
	itk_component add faceslbl {
	    ttk::label $itk_component(cframe).facesLabel \
	        -text {Faces}
	} {}
	itk_component add vertlbl {
	    ttk::label $itk_component(cframe).verticesLabel \
	        -text {Vertices}
	} {}
	itk_component add orglbl {
	    ttk::label $itk_component(cframe).originalLabel \
		-text {Original Mesh}
	} {}
	itk_component add modlbl {
	    ttk::label $itk_component(cframe).modifiedLabel \
		-text {Working Mesh}
	} {}
	itk_component add hzbar {
	    ttk::separator $itk_component(cframe).horizontalBar \
		-orient horizontal
	} {}
	itk_component add vtbar {
	    ttk::separator $itk_component(cframe).verticalBar \
		-orient vertical
	} {}

	# add widgets for original geometry
	itk_component add orgfaces {
	    ttk::label $itk_component(cframe).originalFaces \
	        -text [bot get faces $bot]
	} {}
	itk_component add orgverts {
	    ttk::label $itk_component(cframe).originalVertices \
	        -text [bot get vertices $bot]
	} {}

	# add widgets for modified geometry 
	itk_component add modfaces {
	    ttk::label $itk_component(cframe).faces \
	        -text [bot get faces $bot]
	} {}
	itk_component add modverts {
	    ttk::label $itk_component(cframe).vertices \
	        -text [bot get vertices $bot]
	} {}

	# display container frame
	pack $itk_component(main) -expand yes -fill both
	grid rowconfigure $itk_component(main) 1 -weight 1
	grid columnconfigure $itk_component(main) 0 -weight 1

	# display layout frames in container frame
	grid $itk_component(sframe) -row 1 -column 0 \
	    -sticky news
	grid $itk_component(cframe) -row 0 -column 0 \
	    -padx {0 10} -pady {10 0}
	grid rowconfigure $itk_component(cframe) {1 2 3} -weight 1
	grid columnconfigure $itk_component(cframe) {1 2 3} -weight 1

	# display top headers 
	grid $itk_component(modlbl) -row 0 -column 2 \
	    -padx {0 5}
	grid $itk_component(orglbl) -row 0 -column 3 \
	    -padx {5 0}
	grid $itk_component(hzbar) -row 1 -column 2 \
	    -columnspan 2 \
	    -sticky ew

	# display side headers
	grid $itk_component(faceslbl) -row 2 -column 0
	grid $itk_component(vertlbl) -row 3 -column 0
	grid $itk_component(vtbar) -row 2 -column 1 \
	    -rowspan 2 \
	    -sticky ns

	# display widgets for modified geometry
	grid $itk_component(modfaces) -row 2 -column 2
	grid $itk_component(modverts) -row 3 -column 2

	# display widgets for original geometry
	grid $itk_component(orgfaces) -row 2 -column 3
	grid $itk_component(orgverts) -row 3 -column 3
    }
}
