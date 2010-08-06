# BotTools class for wrapping BoT commands
#     
# Usage: BotTools <instance name> <bot name> [-prefix <Archer instance name>]
# 
# The -prefix option is used when this class is instanced in Archer,
# where the ged commands are methods of the Archer mega-widget instance.
#
package require Tk
package require Itcl
package require Itk

::itcl::class BotTools {
    inherit itk::Widget

    constructor {bot args} {}

    public {
	method apply {cmd args}
    }

    private {
	variable panes 0
	variable bot ""
	method makePane {}
	method makeQualityPane {}
    }

    itk_option define -prefix prefix Prefix "" {}
}

# initialize
::itcl::body BotTools::constructor {b args} {

    eval itk_initialize $args

    set bot $b

    # container frame
    itk_component add tframe {
	ttk::frame $itk_interior.toolFrame
    } {}

    # tabbed widget
    itk_component add nb {
	ttk::notebook $itk_component(tframe).notebook
    } {}

    # add quality tab
    $itk_component(nb) add [makeQualityPane] \
	-text {Mesh Quality} \
	-sticky w

    pack $itk_component(tframe) -expand yes -fill both
    pack $itk_component(nb) -expand yes -fill both
}

# convenience method for uniformly applying bot_* commands
::itcl::body BotTools::apply {cmd args} {

    # use proper context
    set _ $itk_option(-prefix)

    # run command; overwrite original
    $_ mv $bot $bot.old
    eval $_ $cmd $args $bot $bot.old
    $_ kill $bot.old
}

# convenience method for creating a new tab pane
::itcl::body BotTools::makePane {} {
    set name "pane$panes"
    incr panes

    itk_component add $name {
	ttk::frame $itk_interior.$name \
	    -padding 5
    } {}

    return $itk_component($name)
}

# makes 'Mesh Quality' pane
::itcl::body BotTools::makeQualityPane {} {

    # get new container pane
    set pane [makePane]

    # add pane components
    itk_component add condense {
	ttk::button $pane.condense \
	    -text Condense \
	    -command "$this apply bot_condense"
    } {}
    itk_component add ffaces {
	ttk::button $pane.fuse_faces \
	    -text {Fuse Faces} \
	    -command "$this apply bot_face_fuse"
    } {}
    itk_component add fvertices {
	ttk::button $pane.fuse_vertices \
	    -text {Fuse Vertices} \
	    -command "$this apply bot_vertex_fuse"
    } {}
    itk_component add sort {
	ttk::button $pane.sort_faces \
	    -text {Sort Faces} \
	    -command "$this apply bot_face_fuse"
    } {}
    itk_component add decimate {
	ttk::button $pane.decimate \
	    -text {Decimate} \
	    -command "$this apply bot_decimate -n 5"
    } {}
    itk_component add auto {
	ttk::button $pane.auto_simplify \
	    -text {Automatically Simplify} \
    } {}

    # draw components
    set padx 3; set pady 2
    grid $itk_component(condense) -row 0 -column 0 \
        -padx $padx -pady $pady
    grid $itk_component(ffaces) -row 1 -column 0 \
        -padx $padx -pady $pady
    grid $itk_component(fvertices) -row 2 -column 0 \
        -padx $padx -pady $pady
    grid $itk_component(sort) -row 0 -column 1 \
        -padx $padx -pady $pady
    grid $itk_component(decimate) -row 1 -column 1 \
        -padx $padx -pady $pady
    grid $itk_component(auto) -row 3 -column 0 \
        -columnspan 2 -padx $padx -pady $pady

    return $pane
}
