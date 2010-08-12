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
	proc inRange {min max n} {}
	proc keepFocus {widget}

	method runBotCmd {cmd args}

	method simplify {}
	method validateSimplifyChecks {}

	method sortFaces {}
	method validateGroupsEntry {value} {}

	method decimate {}
	method updateDecimateState {}
	method validateNormalEntry {value} {}
	method validateChordEntry {value} {}
	method validateEdgeEntry {value} {}
    }

    
    private {
	variable panes 0
	variable bot ""

	method makePane {}
	method makeEconomyPane {}
	method makeDensityPane {}
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

    # add efficiency tab
    $itk_component(nb) add [makeEconomyPane] \
	-text {Economy} \
	-sticky w

    # add density tab
    $itk_component(nb) add [makeDensityPane] \
	-text {Density} \
	-sticky w

    # draw components
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

# makes Economy pane
::itcl::body BotTools::makeEconomyPane {} {

    # get new container pane
    set pane [makePane]

    # add simplification components
    set ::${itk_interior}CondenseCheck 1
    set ::${itk_interior}FacesCheck 1
    set ::${itk_interior}VerticesCheck 1
    itk_component add condenseCheck {
	ttk::checkbutton $pane.condense \
	    -text {Remove Unreferenced Vertices} \
	    -variable ${itk_interior}CondenseCheck \
	    -command "$this validateSimplifyChecks"
    } {}
    itk_component add verticesCheck {
	ttk::checkbutton $pane.fuse_vertices \
	    -text {Remove Duplicate Vertices} \
	    -variable ${itk_interior}VerticesCheck \
	    -command "$this validateSimplifyChecks"
    } {}
    itk_component add facesCheck {
	ttk::checkbutton $pane.fuse_faces \
	    -text {Remove Duplicate Faces} \
	    -variable ${itk_interior}FacesCheck \
	    -command "$this validateSimplifyChecks"
    } {}
    itk_component add simplify {
	ttk::button $pane.auto_simplify \
	    -text Simplify \
	    -command "$this simplify"
    } {}

    # add sort components
    set ::${itk_interior}GroupsEntry 4
    itk_component add sort {
	ttk::button $pane.sort_faces \
	    -text {Group Adjacent Faces} \
	    -command "$this sortFaces"
    } {}
    itk_component add gframe {
	ttk::frame $pane.groupFrame
    } {}
    itk_component add groupsEntry {
	ttk::entry $itk_component(gframe).groupsEntry \
	    -width 3 \
	    -textvariable ${itk_interior}GroupsEntry \
	    -validate all \
	    -validatecommand "$this validateGroupsEntry %P"
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
	-padx $padx
    grid $itk_component(sort) -row 0 -column 2 \
        -sticky nw
    grid $itk_component(gframe) -row 1 -column 2 \
        -sticky new
    pack $itk_component(groupsEntry) -side right
    pack $itk_component(groupsLbl) -side right -padx $padx

    # allow entry to maintain focus when being edited
    keepFocus $itk_component(groupsEntry)

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
    set size [set ::${itk_interior}GroupsEntry]

    bot_face_sort $size $bot
    eval $itk_option(-command)
}

# make sure at least one checkbox is selected
::itcl::body BotTools::validateSimplifyChecks {} {

    set condense [set ::${itk_interior}CondenseCheck]
    set vertices [set ::${itk_interior}VerticesCheck]
    set faces [set ::${itk_interior}FacesCheck]

    # enable or disable
    if {[expr {$condense + $vertices + $faces}] == 0} {
	$itk_component(simplify) configure -state disabled
    } else {
	$itk_component(simplify) configure -state normal
    }
}



# Valid values are [2, 999]
# Accept 1 and "" for usability, but then disable button
#
::itcl::body BotTools::validateGroupsEntry {value} {

    # accept [2, 999]
    # accept "" to let user clear box
    # accept 1 to let user type 2+ digit numbers starting with 1
    #
    if {[inRange 1 999 $value] || $value == ""} {

	# 1 and "" are ultimately not valid - disable button
	if {$value == "" || $value == 1} {
	    $itk_component(sort) configure -state disabled
	} else {
	    $itk_component(sort) configure -state normal
	}

	return 1
    }

    return 0
}

::itcl::body BotTools::makeDensityPane {} {

    # get new container pane
    set pane [makePane]

    # add decimate button
    itk_component add decimate {
	ttk::button $pane.decimateButton \
	    -text {Reduce Face Count} \
	    -command "$this decimate"
    } {}

    # add normal error widgets
    set ::${itk_interior}NormalCheck 1
    set ::${itk_interior}NormalEntry 10
    itk_component add normalCheck {
	ttk::checkbutton $pane.normalCheckbox \
	    -text {Max Normal Error} \
	    -variable ${itk_interior}NormalCheck \
	    -command "$this updateDecimateState"
    } {}
    itk_component add normalEntry {
	ttk::entry $pane.normalEntry \
	    -width 3 \
	    -textvariable ${itk_interior}NormalEntry \
	    -validate all \
	    -validatecommand "$this validateNormalEntry %P"
    } {}

    # add chord error widgets
    set ::${itk_interior}ChordCheck 0
    set ::${itk_interior}ChordEntry ""
    itk_component add chordCheck {
	ttk::checkbutton $pane.chordCheckbox \
	    -text {Max Chord Error} \
	    -variable ${itk_interior}ChordCheck \
	    -command "$this updateDecimateState"
    } {}
    itk_component add chordEntry {
	ttk::entry $pane.chordEntry \
	    -width 3 \
	    -textvariable ${itk_interior}ChordEntry \
	    -validate all \
	    -validatecommand "$this validateChordEntry %P"
    } {}

    # add edge length widgets
    set ::${itk_interior}EdgeCheck 0
    set ::${itk_interior}EdgeEntry ""
    itk_component add edgeCheck {
	ttk::checkbutton $pane.edgeCheckbox \
	    -text {Min Edge Lenth} \
	    -variable ${itk_interior}EdgeCheck \
	    -command "$this updateDecimateState"
    } {}
    itk_component add edgeEntry {
	ttk::entry $pane.edgeEntry \
	    -width 3 \
	    -textvariable ${itk_interior}EdgeEntry \
	    -validate all \
	    -validatecommand "$this validateEdgeEntry %P"
    } {}

    # draw decimate button
    grid $itk_component(decimate) -row 0 -column 0 \
	-sticky nw

    # draw flag components 
    set padx 5; set pady 2
    grid $itk_component(normalCheck) -row 1 -column 0 \
        -sticky nw \
	-padx $padx -pady $pady
    grid $itk_component(normalEntry) -row 1 -column 1 \
        -sticky nw \
	-padx $padx -pady $pady
    grid $itk_component(chordCheck) -row 2 -column 0 \
        -sticky nw \
	-padx $padx -pady $pady
    grid $itk_component(chordEntry) -row 2 -column 1 \
        -sticky nw \
	-padx $padx -pady $pady
    grid $itk_component(edgeCheck) -row 3 -column 0 \
        -sticky nw \
	-padx $padx -pady $pady
    grid $itk_component(edgeEntry) -row 3 -column 1 \
        -sticky nw \
	-padx $padx -pady $pady

    # allow entries to maintain focus when being edited 
    keepFocus $itk_component(normalEntry)
    keepFocus $itk_component(chordEntry)
    keepFocus $itk_component(edgeEntry)

    # update decimate button's state on entry change
    bind $itk_component(normalEntry) <KeyRelease> "+$this updateDecimateState"
    bind $itk_component(chordEntry) <KeyRelease> "+$this updateDecimateState"
    bind $itk_component(edgeEntry) <KeyRelease> "+$this updateDecimateState"

    return $pane
}

# When widget is clicked, let it keep focus until the user
# clicks somewhere else.
#
::itcl::body BotTools::keepFocus {widget} {

    bind $widget <FocusIn> "
	bind $widget <FocusOut> \"focus $widget\"
	bind all <ButtonPress> \"bind $widget <FocusOut> {}\"
    "
}

# test if n is an integer, not prefixed with 0s,
# in the range [min, max]
#
::itcl::body BotTools::inRange {min max n} {

    if {[regexp {^((0)|((-|\+)?[1-9]\d*))$} $n] == 1} {

	if {$n >= $min && $n <= $max} {
	    return 1
	}
    }

    return 0
}

# Valid values are [1, 180]
# Accept "" for usability
#
::itcl::body BotTools::validateNormalEntry {value} {

    # accept [1, 180]
    # accept "" to let user clear box
    #
    if {[inRange 1 180 $value] || $value == ""} {

	return 1
    }

    return 0
}

# Valid values are [1, 999]
# Accept "" for usability
#
::itcl::body BotTools::validateChordEntry {value} {

    # accept [1, 999]
    # accept "" to let user clear box
    #
    if {[inRange 1 999 $value] || $value == ""} {

	return 1
    }

    return 0
}

# Valid values are [1, 999]
# Accept "" for usability
#
::itcl::body BotTools::validateEdgeEntry {value} {

    # accept [1, 999]
    # accept "" to let user clear box
    #
    if {[inRange 1 999 $value] || $value == ""} {

	return 1
    }

    return 0
}

# enable/disable decimate button based on entry values
#
::itcl::body BotTools::updateDecimateState {} {

    # get check values
    set normal [set ::${itk_interior}NormalCheck]
    set chord [set ::${itk_interior}ChordCheck]
    set edge [set ::${itk_interior}EdgeCheck]

    # need at least one constraint 
    if {[expr {$normal + $chord + $edge}] == 0} {

	$itk_component(decimate) configure -state disabled

    } else {

	# get entry values
	set normalVal [set ::${itk_interior}NormalEntry]
	set chordVal [set ::${itk_interior}ChordEntry]
	set edgeVal [set ::${itk_interior}EdgeEntry]

	# need constraints to be non-empty
	if {[expr {$normal && $normalVal == ""} || \
	    {$chord && $chordVal == ""} || \
	    {$edge && $edgeVal == ""}]
	} then {

	    $itk_component(decimate) configure -state disabled
	} else {

	    $itk_component(decimate) configure -state normal
	}
    }
}

::itcl::body BotTools::decimate {} {
    set args ""
    set normal [set ::${itk_interior}NormalCheck]
    set chord [set ::${itk_interior}ChordCheck]
    set edge [set ::${itk_interior}EdgeCheck]

    if {$normal} {
	set normalVal [set ::${itk_interior}NormalEntry]
	set args "$args -n $normalVal"
    }

    if {$chord} {
	set chordVal [set ::${itk_interior}ChordEntry]
	set args "$args -c $chordVal"
    }

    if {$edge} {
	set edgeVal [set ::${itk_interior}EdgeEntry]
	set args "$args -e $edgeVal"
    }

    eval runBotCmd bot_decimate $args
    eval $itk_option(-command)
}
