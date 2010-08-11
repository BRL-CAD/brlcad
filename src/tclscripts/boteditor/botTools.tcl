# BotTools class for wrapping BoT commands
#     
# Usage: BotTools <instance name> <bot name> \
#    [-command <callback>] 
#
# The callback function passed in the -command option is called whenever
# the supplied bot is modified.
#
package require Tk
package require Itcl
package require Itk

::itcl::class BotTools {
    inherit itk::Widget

    constructor {bot args} {}

    public {
	method runBotCmd {cmd args}
	method simplify {}
	method sortFaces {}
	method keepFocus {}
	method loseFocus {}
	method validateEntry {value}
	method updateEntry {}
    }

    
    private {
	variable panes 0
	variable bot ""

	method makePane {}
	method makeEfficiencyPane {}
    }

    itk_option define -command command Command "" {}
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
    $itk_component(nb) add [makeEfficiencyPane] \
	-text {Efficiency} \
	-sticky w

    pack $itk_component(tframe) -expand yes -fill both
    pack $itk_component(nb) -expand yes -fill both
}

# convenience method for uniformly running bot_* commands
::itcl::body BotTools::runBotCmd {cmd args} {
    mv $bot $bot.old
    eval $cmd $args $bot $bot.old
    kill $bot.old
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

# makes Efficiency pane
::itcl::body BotTools::makeEfficiencyPane {} {

    # get new container pane
    set pane [makePane]

    # add simplification components
    set ::${itk_interior}CondenseCheck 1
    set ::${itk_interior}FacesCheck 1
    set ::${itk_interior}VerticesCheck 1
    itk_component add condenseCheck {
	ttk::checkbutton $pane.condense \
	    -text {Remove Unreferenced Vertices} \
	    -variable ${itk_interior}CondenseCheck
    } {}
    itk_component add verticesCheck {
	ttk::checkbutton $pane.fuse_vertices \
	    -text {Remove Duplicate Vertices} \
	    -variable ${itk_interior}VerticesCheck
    } {}
    itk_component add facesCheck {
	ttk::checkbutton $pane.fuse_faces \
	    -text {Remove Duplicate Faces} \
	    -variable ${itk_interior}FacesCheck
    } {}
    itk_component add simplify {
	ttk::button $pane.auto_simplify \
	    -text Simplify \
	    -command "$this simplify"
    } {}

    # add sort components
    set ::${itk_interior}Groups 4
    itk_component add sort {
	ttk::button $pane.sort_faces \
	    -text {Group Adjacent Faces} \
	    -command "$this sortFaces"
    } {}
    itk_component add gframe {
	ttk::frame $pane.groupFrame
    } {}
    itk_component add groups {
	ttk::entry $itk_component(gframe).groups \
	    -width 3 \
	    -textvariable ${itk_interior}Groups \
	    -validate all \
	    -validatecommand "$this validateEntry %P"
    } {}
    itk_component add groupsLbl {
	ttk::label $itk_component(gframe).groupsLabel \
	    -text {Group Size}
    } {}
    itk_component add vsep {
	ttk::separator $pane.separator \
	    -orient vertical
    } {}
#    itk_component add decimate {
#	ttk::button $pane.decimate \
#	    -text {Decimate} \
#	    -command "$this runBotCmd bot_decimate -n 5; $itk_option(-command)"
#    } {}

    # draw simplify components
    set padx 3; set pady 2
    grid $itk_component(simplify) -row 0 -column 0 \
        -padx $padx -pady $pady \
	-sticky nw
    grid $itk_component(condenseCheck) -row 1 -column 0 \
        -padx $padx -pady $pady \
	-sticky nw
    grid $itk_component(verticesCheck) -row 2 -column 0 \
        -padx $padx -pady $pady \
	-sticky nw
    grid $itk_component(facesCheck) -row 3 -column 0 \
        -padx $padx -pady $pady \
	-sticky nw

    # draw sort components
    grid $itk_component(vsep) -row 0 -column 1 \
        -rowspan 4 \
        -sticky ns \
	-padx 5
    grid $itk_component(sort) -row 0 -column 2 \
        -sticky nw
    grid $itk_component(gframe) -row 1 -column 2 \
        -sticky new
    pack $itk_component(groups) -side right
    pack $itk_component(groupsLbl) -side right

    # Entry will lose keyboard focus when it loses mouse focus.
    # Explicitly force focus to stay on entry until user clicks outside it.
    bind $itk_component(groups) <ButtonPress> "$this keepFocus"

    # make sure entry isn't empty when we're about to use it
    bind $itk_component(sort) <Enter> "$this updateEntry"
    bind $itk_component(sort) <FocusIn> "$this updateEntry"

    return $pane
}

# run selected simplification commands
::itcl::body BotTools::simplify {} {

    set condense [set ::${itk_interior}CondenseCheck]
    set vertices [set ::${itk_interior}VerticesCheck]
    set faces [set ::${itk_interior}FacesCheck]

    if {$condense} {
	$this runBotCmd bot_condense
    }
    if {$vertices} {
	$this runBotCmd bot_vertex_fuse
    }
    if {$faces} {
	$this runBotCmd bot_face_fuse
    }

    # run callback if commands were run
    if {$condense || $vertices || $faces} {
	eval $itk_option(-command)
    }
}

# run sort command using given group size
::itcl::body BotTools::sortFaces {} {
    set size [set ::${itk_interior}Groups]

    bot_face_sort $size $bot
    eval $itk_option(-command)
}

# keep focus on groups entry until button click
::itcl::body BotTools::keepFocus {} {

    bind $itk_component(groups) <FocusOut> "focus $itk_component(groups)"
    bind all <ButtonPress> "+$this loseFocus"
}

# let focus leave groups entry
::itcl::body BotTools::loseFocus {} {
    bind $itk_component(groups) <FocusOut> ""
    bind all <ButtonPress> ""
}

# valid entry values are [1, 999] or ""
::itcl::body BotTools::validateEntry {value} {


    return [regexp {^([1-9]\d?\d?)?$} $value]
}

# convert "" or 1 to default value
::itcl::body BotTools::updateEntry {} {

    set groups ::${itk_interior}Groups
    set value [set $groups]

    if {$value == ""} {
	set $groups 4
    }

    if {$value == 1} {
	set $groups 4
    }
}
