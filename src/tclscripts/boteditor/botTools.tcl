# BotTools class for wrapping BoT commands
#     
# Usage: BotTools <instance name> <bot name> \
#    [-command <callback>] \
#    [-output <callback>]
#
# The callback function passed in the -command option is called whenever
# the supplied bot is modified. It should take no arguments.
#
# The callback function passed in the -output option will ocassionally be
# passed notification strings. It should take a single string argument.
# 
package require Tk
package require Itcl
package require Itk

::itcl::class BotTools {
    inherit itk::Widget

    constructor {bot args} {}

    public {
	proc intInRange {min max n}
	proc isPositiveFloat {n}
	proc keepFocus {widget}

	method runBotCmd {cmd args}
	method print {msg}

	method simplify
	method validateSimplifyChecks

	method sortFaces
	method validateGroupsEntry {value}

	method decimate
	method loadDecimateSuggestions
	method updateDecimateState
	method validateNormalEntry {value}
	method validateChordEntry {value}
	method validateEdgeEntry {value}
    }

    
    private {
	variable panes 0
	variable bot ""
	variable lastEdge ""

	method makePane
	method makeDensityPane
	method makeEconomyPane
	method makePerformancePane
    }

    itk_option define -beforecommand beforecommand Command "" {}
    itk_option define -aftercommand aftercommand Command "" {}
    itk_option define -output output Command "" {}
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

    # add density tab
    $itk_component(nb) add [makeDensityPane] \
	-text {Density} \
	-sticky w

    # add economy tab
    $itk_component(nb) add [makeEconomyPane] \
	-text {Economy} \
	-sticky w

    # add performance tab
    $itk_component(nb) add [makePerformancePane] \
	-text {Performance} \
	-sticky nw

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

::itcl::body BotTools::print {msg} {
    set out $itk_option(-output)

    if {$out == ""} {
	return
    }

    catch {eval $out {$msg}}
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

::itcl::body BotTools::makeDensityPane {} {

    set pane [makePane]

    # add decimate components
    itk_component add decimate {
	ttk::button $pane.decimateButton \
	    -text {Reduce Face Count} \
	    -command "$this decimate"
    } {}

    loadDecimateSuggestions

    set ::${itk_interior}NormalCheck 0
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

    set ::${itk_interior}ChordCheck 0
    itk_component add chordCheck {
	ttk::checkbutton $pane.chordCheckbox \
	    -text {Max Chord Error} \
	    -variable ${itk_interior}ChordCheck \
	    -command "$this updateDecimateState"
    } {}
    itk_component add chordEntry {
	ttk::entry $pane.chordEntry \
	    -width 10 \
	    -textvariable ${itk_interior}ChordEntry \
	    -validate all \
	    -validatecommand "$this validateChordEntry %P"
    } {}

    set ::${itk_interior}EdgeCheck 1
    itk_component add edgeCheck {
	ttk::checkbutton $pane.edgeCheckbox \
	    -text {Min Edge Lenth} \
	    -variable ${itk_interior}EdgeCheck \
	    -command "$this updateDecimateState"
    } {}
    itk_component add edgeEntry {
	ttk::entry $pane.edgeEntry \
	    -width 10 \
	    -textvariable ${itk_interior}EdgeEntry \
	    -validate all \
	    -validatecommand "$this validateEdgeEntry %P"
    } {}

    # draw decimate components
    grid $itk_component(decimate) -row 0 -column 0 \
	-sticky nw
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

::itcl::body BotTools::makeEconomyPane {} {

    # get new container pane
    set pane [makePane]

    # add simplification components
    set ::${itk_interior}CondenseCheck 1
    set ::${itk_interior}FacesCheck 1
    set ::${itk_interior}VerticesCheck 1
    itk_component add condenseCheck {
	ttk::checkbutton $pane.condense \
	    -text {Remove Unused Vertices} \
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

    return $pane
}

::itcl::body BotTools::makePerformancePane {} {

    set pane [makePane]

    # add sort components
    set ::${itk_interior}GroupsEntry 4
    itk_component add sort {
	ttk::button $pane.sortFaces \
	    -text {Group Adjacent Faces}
    } {}
    $itk_component(sort) configure -command "$this sortFaces"
    itk_component add gframe {
	ttk::frame $pane.groupFrame \
	    -padding 3
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

    # draw sort components
    grid $itk_component(sort) -row 0 -column 0 \
        -sticky nw
    grid $itk_component(gframe) -row 1 -column 0 \
        -sticky new
    pack $itk_component(groupsEntry) -side right
    pack $itk_component(groupsLbl) -side right -padx 3

    # allow entry to maintain focus when being edited
    keepFocus $itk_component(groupsEntry)

    return $pane
}

# run selected simplification commands
::itcl::body BotTools::simplify {} {

    set condense ::${itk_interior}CondenseCheck
    set vertices ::${itk_interior}VerticesCheck
    set faces ::${itk_interior}FacesCheck

    # disable components
    $itk_component(simplify) configure -state disabled
    $itk_component(condenseCheck) configure -state disabled
    $itk_component(verticesCheck) configure -state disabled
    $itk_component(facesCheck) configure -state disabled

    if {[set $condense]} {

	# run command
	print "bot_condense"
	eval $itk_option(-beforecommand)
	$this runBotCmd bot_condense
	eval $itk_option(-aftercommand)

	# unset option - shouldn't need to do this more than once
	set $condense 0
    }
    if {[set $vertices]} {

	# run command
	print "bot_vertex_fuse"
	eval $itk_option(-beforecommand)
	$this runBotCmd bot_vertex_fuse
	eval $itk_option(-aftercommand)

	# unset option - shouldn't need to do this more than once
	set $vertices 0
    }
    if {[set $faces]} {

	# run command
	print "bot_face_fuse"
	eval $itk_option(-beforecommand)
	$this runBotCmd bot_face_fuse
	eval $itk_option(-aftercommand)

	# unset option - shouldn't need to do this more than once
	set $faces 0
    }

    # re-enable checks
    $itk_component(condenseCheck) configure -state normal
    $itk_component(verticesCheck) configure -state normal
    $itk_component(facesCheck) configure -state normal

    # only re-enable button if we didn't do everything 
    validateSimplifyChecks

    print "done"
}

# run sort command using given group size
::itcl::body BotTools::sortFaces {} {
    set size [set ::${itk_interior}GroupsEntry]

    print "bot_face_sort $size"
    eval $itk_option(-beforecommand)
    bot_face_sort $size $bot
    eval $itk_option(-aftercommand)
    print "done"
}

# make sure at least one checkbox is selected
::itcl::body BotTools::validateSimplifyChecks {} {

    set condense [set ::${itk_interior}CondenseCheck]
    set vertices [set ::${itk_interior}VerticesCheck]
    set faces [set ::${itk_interior}FacesCheck]

    # enable or disable
    if {[expr $condense + $vertices + $faces] == 0} {
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
    if {[intInRange 1 999 $value] || $value == ""} {

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
::itcl::body BotTools::intInRange {min max n} {

    if {[regexp {^((0)|((-|\+)?[1-9]\d*))$} $n] == 1} {

	if {$n >= $min && $n <= $max} {
	    return 1
	}
    }

    return 0
}

# test if n is a non-negative floating point number
#
::itcl::body BotTools::isPositiveFloat {n} {

    # accept '.' for usability
    if {$n == "."} {
	return 1
    }

    # accept positive integers
    if {[regexp {^(0|([1-9]\d*))$} $n]} {
	return 1
    }

    # accept all decimal patterns for usability
    if {[regexp {^((0|([1-9]\d*))?\.\d*)$} $n]} {
	    return 1
    }

    return 0
}


# Valid values are integers in [1, 180]
# Accept "" for usability
#
::itcl::body BotTools::validateNormalEntry {value} {

    # accept [1, 180]
    # accept "" to let user clear box
    #
    if {[intInRange 1 180 $value] || $value == ""} {

	return 1
    }

    return 0
}

# Valid values are positive reals
# Accept "" for usability
#
::itcl::body BotTools::validateChordEntry {value} {

    if {[isPositiveFloat $value] || $value == ""} {

	return 1
    }

    return 0
}

# Valid values are [1, 999]
# Accept "" for usability
#
::itcl::body BotTools::validateEdgeEntry {value} {

    if {[isPositiveFloat $value] || $value == ""} {

	return 1
    }

    return 0
}

# set suggested decimate values
::itcl::body BotTools::loadDecimateSuggestions {} {

    set ::${itk_interior}NormalEntry "" 
    set ::${itk_interior}ChordEntry "" 

    if {$lastEdge == ""} {
	set min [bot get minEdge $bot]
	set lastEdge $min
#	set max [bot get maxEdge $bot]

	# get min and max bounding box points
#	set bbMinMax [bb -eq $bot]
#	regexp {min \{\}} $bbMinMax match minX minY minZ
#	regexp {max \{\}} $bbMinMax match maxX maxY maxZ

	# calculate magnitude of bounding box max diagonal 
#	set x [expr $maxX - $minX]; set y [expr $maxY - $minY]; set z [expr $maxZ - $minZ] 
#	set mag [::tcl::mathfunc::sqrt [expr $x*$x + $y*$y + $z*$z]]

#	set lastEdge [expr $min + $min * ($max/$mag)]
	set ::${itk_interior}EdgeEntry $lastEdge
    } else {
	set ::${itk_interior}EdgeEntry [set lastEdge [expr $lastEdge * 2]]
    }
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

	# need constraints to be valid 
	if {[expr {$normal && $normalVal == ""} || \
	    {$chord && [expr {[regexp {\d} $chordVal] == 0} || {$chordVal == 0}]} || \
	    {$edge && [expr {[regexp {\d} $edgeVal] == 0} || {$edgeVal == 0}]}]
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
	set lastEdge $edgeVal
    }

    # disable
    $itk_component(decimate) configure -state disabled

    # run command
    print "bot_decimate $args"
    eval $itk_option(-beforecommand)
    runBotCmd "bot_decimate $args"
    eval $itk_option(-aftercommand)
    print "done"

    # re-enable
    $itk_component(decimate) configure -state normal

    loadDecimateSuggestions
}
