::itcl::class BotTools {
    inherit itk::Widget

    constructor {args} {}

    private {
	variable panes 0
	method makePane {}
	method makeQualityPane {}
    }
}

::itcl::body BotTools::constructor {args} {
    # container frame
    itk_component add tframe {
	ttk::frame $itk_interior.toolFrame
    } {}

    # tabbed widget
    itk_component add nb {
	ttk::notebook $itk_component(tframe).notebook
    } {}

    # add quality tab
    $itk_component(nb) add [makeQualityPane] -text {Mesh Quality}

    pack $itk_component(tframe) -expand yes -fill both
    pack $itk_component(nb) -expand yes -fill both

    eval itk_initialize $args
}

::itcl::body BotTools::makePane {} {
    set name "pane$panes"
    incr panes

    itk_component add $name {
	ttk::frame $itk_interior.$name \
	    -padding 5
    } {}

    return $itk_component($name)
}

::itcl::body BotTools::makeQualityPane {} {
    set pane [makePane]

    itk_component add condense {
	ttk::button $pane.condense \
	    -text Condense
    } {}

    itk_component add ffaces {
	ttk::button $pane.fuse_faces \
	    -text {Fuse Faces}
    } {}

    itk_component add fvertices {
	ttk::button $pane.fuse_vertices \
	    -text {Fuse Vertices}
    } {}

    itk_component add sort {
	ttk::button $pane.sort_faces \
	    -text {Sort Faces}
    } {}

    itk_component add decimate {
	ttk::button $pane.decimate \
	    -text {Decimate}
    } {}

    itk_component add auto {
	ttk::button $pane.auto_simplify \
	    -text {Automatically Simplify}
    } {}

    set padx 3; set pady 2
    grid $itk_component(condense) -row 0 -column 0 -padx $padx -pady $pady
    grid $itk_component(ffaces) -row 1 -column 0 -padx $padx -pady $pady
    grid $itk_component(fvertices) -row 2 -column 0 -padx $padx -pady $pady
    grid $itk_component(sort) -row 0 -column 1 -padx $padx -pady $pady
    grid $itk_component(decimate) -row 1 -column 1 -padx $padx -pady $pady
    grid $itk_component(auto) -row 3 -column 0 -columnspan 2 -padx $padx -pady $pady

    return $pane
}
