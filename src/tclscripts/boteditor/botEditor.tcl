# BotEditor class for editing a BoT primitive
#
# Usage: BotEditor <instance name> <bot name> [-prefix <Archer instance name>]
# 
# The -prefix option is used when this class is instanced in Archer,
# where the ged commands are methods of the Archer mega-widget instance.
#
package require Tk
package require Itcl
package require Itk

# manually sourcing dependencies for now
if {[catch {
    set script [file join [bu_brlcad_data "tclscripts"] boteditor botPropertyBox.tcl]
    source $script

    set script [file join [bu_brlcad_data "tclscripts"] boteditor botTools.tcl]
    source $script
} errMsg] > 0} {
    puts "Couldn't load \"$script\"\n$errMsg"
    exit
} 

::itcl::class BotEditor {
    inherit ::itk::Toplevel

    constructor {bot args} {}

    public {
	common _ ""
	
	proc defineCommands {prefix}
	proc localizeDialog {d}
	proc focusOnEnter {d}

	method giveFeedback {}
	method animate {}
	method onChange {}
	method updateStatus {msg} {}
	method accept {}
	method reject {}
	method cancel {}
    }    

    private {
	variable original ""
	variable copy ""
	variable cmdDone 0
	variable lastTime 0

	method showUnsaved {show}
    }

    itk_option define -prefix prefix Prefix "" {}
}

# open editor for bot named 'bot'
# this bot is assumed to exist in the current database
#
::itcl::body BotEditor::constructor {bot args} {

    eval itk_initialize $args

    # setup commands
    defineCommands $itk_option(-prefix)

    # remove path prefix from bot name
    while {[regsub {.*/} $bot {} bot]} {}

    # show name of bot being edited
    $this configure -title "[eval $this cget -title] - $bot"
    set ::${itk_interior}statusLine "editing \"$bot\""

    # make working copy
    set original $bot
    set copy $original.edit
    cp $original $copy

    # handle window close
    wm protocol $itk_interior WM_DELETE_WINDOW "kill $copy; delete object $this"

    # create layout frames
    itk_component add histFrame {
	ttk::frame $itk_interior.historyFrame \
	    -padding 7
    } {}
    itk_component add eframe {
	EditPane $itk_interior.editPane $copy \
	    -beforecommand "$this giveFeedback" \
	    -aftercommand "$this onChange" \
	    -output "$this updateStatus"
    } {}
    itk_component add closeframe {
	ttk::frame $itk_interior.closeFrame \
	    -padding 7
    } {}
    itk_component add statusframe {
	ttk::frame $itk_interior.statusFrame
    } {}

    # add widgets to history frame
    itk_component add cancel {
	ttk::button $itk_component(histFrame).cancel \
	    -text {Start Over} \
	    -command "$this cancel"
    } {}
    
    # add widgets to close frame
    itk_component add accept {
	ttk::button $itk_component(closeframe).accept \
	    -text Accept \
	    -command "$this accept"
    } {}
    itk_component add reject {
	ttk::button $itk_component(closeframe).reject \
	    -text Reject \
	    -command "$this reject"
    } {}

    # add widgets to status frame
    itk_component add rule {
	ttk::separator $itk_component(statusframe).separator \
	    -orient horizontal
    } {}
    itk_component add status {
	ttk::label $itk_component(statusframe).statusLabel \
	    -textvariable ${itk_interior}statusLine
    } {}
    itk_component add progress {
	ttk::progressbar $itk_component(statusframe).progressBar \
	    -mode indeterminate
    } {}
    itk_component add mask {
	ttk::frame $itk_component(statusframe).progressMask
    } {}

    # display main frames
    grid $itk_component(histFrame) -row 0 -column 0 -sticky news
    grid $itk_component(eframe) -row 1 -column 0 -sticky news
    grid $itk_component(closeframe) -row 2 -column 0 -sticky news
    grid $itk_component(statusframe) -row 3 -column 0 -sticky news
    grid rowconfigure $itk_interior 1 -weight 1
    grid columnconfigure $itk_interior 0 -weight 1
    grid rowconfigure $itk_component(statusframe) {0 1} -weight 1
    grid columnconfigure $itk_component(statusframe) 0 -weight 1

    # display history frame components
    pack $itk_component(cancel) -side left -padx 5

    # display close frame components
    pack $itk_component(reject) -side right
    pack $itk_component(accept) -side right -padx 5

    # display status frame components
    grid $itk_component(rule) -row 0 -column 0 \
	-columnspan 2 \
	-sticky ew
    grid $itk_component(status) -row 1 -column 0 \
        -sticky ew
    grid $itk_component(progress) -row 1 -column 1 \
        -sticky ew \
	-padx {5 20}
    grid $itk_component(mask) -row 1 -column 1 \
        -sticky news \
	-padx {5 20}
}

# give feedback on unsaved change
::itcl::body BotEditor::showUnsaved {show} {

    if {$show} {

	# append '*' to title to indicate unapplyed change
	set title [eval $this cget -title]
	
	if {[regexp ^.*\\*$ $title] == 0} {
	    $this configure -title $title*
	}
    } else {

	# remove '*' from title to indicate application
	set match ""; set title [eval $this cget -title]
	regexp ^(.*)\\*$ $title match title
	$this configure -title $title
    }
}

# animate progress bar
::itcl::body BotEditor::animate {} {

    if {!$cmdDone} {

	# current bar position
	set curr [$itk_component(progress) cget -value]

	# how long since last update
	set elapsed [expr [clock milliseconds] - $lastTime]

	# increment position - want to go entire length in 1s
	$itk_component(progress) configure -value [expr $curr + $elapsed / 10]
	update

	set lastTime [clock milliseconds]
	after 50 "$this animate"
    }
}

# called before a command is run
::itcl::body BotEditor::giveFeedback {} {

    # initialize
    set cmdDone 0
    set lastTime [clock milliseconds]
    $itk_component(progress) configure -value 0 

    # display bar
    raise $itk_component(progress)
    update

    # animate bar
    animate
}

# called whenever an edit is made
::itcl::body BotEditor::onChange {} {
    showUnsaved yes

    # update properties
    $itk_component(eframe) update $copy

    Z
    draw $copy

    set cmdDone 1
    lower $itk_component(progress)
}

# show msg in status line
::itcl::body BotEditor::updateStatus {msg} {

    set statusLine ::${itk_interior}statusLine
    set $statusLine "$msg"
    update
}

# defineCommands
#
# In Archer these commands are methods of an Archer instance (prefix).
# We'll create wrappers for these commands so that we can call them
# without an Archer object.
#
::itcl::body BotEditor::defineCommands {prefix} {

    if {$prefix == ""} return

    set _ $prefix

    proc ::bb {args} {eval $BotEditor::_ bb $args}
    proc ::bot {args} {eval $BotEditor::_ bot $args}
    proc ::bot_condense {args} {eval $BotEditor::_ bot_condense $args}
    proc ::bot_decimate {args} {eval $BotEditor::_ bot_decimate $args}
    proc ::bot_face_fuse {args} {eval $BotEditor::_ bot_face_fuse $args}
    proc ::bot_face_sort {args} {eval $BotEditor::_ bot_face_sort $args}
    proc ::bot_vertex_fuse {args} {eval $BotEditor::_ bot_vertex_fuse $args}
    proc ::cp {args} {eval $BotEditor::_ cp $args}
    proc ::draw {args} {eval $BotEditor::_ draw $args}
    proc ::kill {args} {eval $BotEditor::_ kill $args}
    proc ::mv {args} {eval $BotEditor::_ mv $args}
    proc ::Z {} {eval $BotEditor::_ Z}
}

# apply changes to original and exit
::itcl::body BotEditor::accept {} {

    set cmd "kill $original; \
	mv $copy $original; \
	bind all <ButtonPress> {}; \
	delete object $this"

    # get confirmation
    itk_component add confirm {
	ConfirmationDialog $itk_interior.confirmDialog \
	    -title {Apply Changes and Exit?} \
	    -text "This operation will overwrite $original in the current database.\n\n\
		Are you sure you want to save your changes and exit?" \
	    -yescommand $cmd
    } {}
}

# discard all changes and exit
::itcl::body BotEditor::reject {} {

    set cmd "kill $copy; \
	bind all <ButtonPress> {}; \
	delete object $this"

    # get confirmation
    itk_component add confirm {
	ConfirmationDialog $itk_interior.confirmDialog \
	    -title {Exit Without Applying Changes?} \
	    -text {Are you sure you want to discard all changes and exit?} \
	    -yescommand $cmd
    } {}
}

# discard all changes
::itcl::body BotEditor::cancel {} {

    # overwirte copy with original
    set cmd "kill $copy; cp $original $copy; $this onChange"

    # get confirmation
    itk_component add confirm {
	ConfirmationDialog $itk_interior.confirmDialog \
	    -title {Revert Edits?} \
	    -text {Are you sure you want to discard all changes to the working mesh?} \
	    -yescommand $cmd
    } {}
}

# center dialog with respect to its parent
::itcl::body BotEditor::localizeDialog {d} {

    # get parent geometry
    set p [winfo parent $d]
    set pwidth [winfo width $p]
    set pheight [winfo height $p]
    set px [winfo x $p]
    set py [winfo y $p]

    # first draw tiny at center
    # (trying to minimize the visual distraction when we redraw)
    wm geometry $d 0x0+[expr "$pwidth / 2"]+[expr "$pheight / 2"]
    update

    # now that the window is drawn, we can get accurate
    # values for the requested dimensions
    set width [winfo reqwidth $d]
    set height [winfo reqheight $d]

    # draw again, properly centered 
    set x [expr "$px + ($pwidth - $width) / 2"]
    set y [expr "$py + ($pheight - $height) / 2"]
    wm geometry $d ${width}x${height}+${x}+${y}
}

# automatically focus window when mouse enters it
::itcl::body BotEditor::focusOnEnter {d} {

    bind $d <Enter> "focus $d"
}

# Confirmation Dialog helper class
#
::itcl::class ConfirmationDialog {
    inherit itk::Toplevel

    constructor {args} {
	eval itk_initialize $args

	# make container frame
	itk_component add main {
	    ttk::frame $itk_interior.confirmationDialogFrame \
		-padding 7
	} {}

	# make layout frames
	itk_component add info {
	    ttk::frame $itk_interior.infoFrame \
		-padding 7
	} {}
	itk_component add act {
	    ttk::frame $itk_interior.actionsFrame \
		-padding 7
	} {}

	# add components
	itk_component add msg {
	    ttk::label $itk_component(info).message \
		-text $itk_option(-text) \
		-wraplength 300 
	} {}
	itk_component add yes {
	    ttk::button $itk_component(act).confirm \
		-text Yes \
		-command "grab release $itk_interior; $itk_option(-yescommand); catch {delete object $this}"
	} {}
	itk_component add no {
	    ttk::button $itk_component(act).deny \
		-text No \
		-command "grab release $itk_interior; delete object $this"
	} {}

	# display frames
	pack $itk_component(main) -expand yes -fill both
	pack $itk_component(act) -side bottom -fill x
	pack $itk_component(info) -expand yes -fill both

	# display components
	pack $itk_component(msg) -expand yes
	pack $itk_component(no) -side right
	pack $itk_component(yes) -side right -padx 5


	# make modal
	grab $itk_interior

	# disable resize
	wm resizable $itk_interior 0 0

	# center and auto focus dialog
	BotEditor::localizeDialog $itk_interior
	BotEditor::focusOnEnter $itk_interior
    }

    itk_option define -text text Text "" {}
    itk_option define -yescommand yesCommand Command "" {}
}

# Edit Pane helper class to wrap edit widgets
#
::itcl::class EditPane {
    inherit itk::Widget

    constructor {bot args} {
	eval itk_initialize $args

	# make container frame
	itk_component add main {
	    ttk::frame $itk_interior.editPaneFrame \
		-padding 7
	} {}

	# add layout frames to main frame
	itk_component add tframe {
	    ttk::frame $itk_component(main).topFrame
	} {}
	itk_component add bframe {
	    ttk::frame $itk_component(main).bottomFrame
	} {}

	# add layout frames to top frame
	itk_component add lframe {
	    ttk::frame $itk_component(tframe).leftFrame \
		-padding 5
	} {}
	itk_component add rframe {
	    ttk::frame $itk_component(tframe).rightFrame \
		-padding 5
	} {}

	# add components
	itk_component add tools {
	    BotTools $itk_component(lframe).tools $bot \
		-beforecommand $itk_option(-beforecommand) \
		-aftercommand $itk_option(-aftercommand) \
		-output $itk_option(-output)
	} {}
	itk_component add props {
	    BotPropertyBox $itk_component(rframe).properties $bot \
		-command $itk_option(-aftercommand)
	} {}

	# display main frame
	pack $itk_component(main) -expand yes -fill both

	# display main layout frames
	grid $itk_component(tframe) -row 0 -column 0 -sticky news
	grid $itk_component(bframe) -row 1 -column 0 -sticky news
	grid rowconfigure $itk_component(main) 1 -weight 1
	grid columnconfigure $itk_component(main) 0 -weight 1

	# display top subframes
	grid $itk_component(lframe) -row 0 -column 0 -sticky news
	grid $itk_component(rframe) -row 0 -column 1 -sticky news
	grid rowconfigure $itk_component(tframe) 0 -weight 1
	grid columnconfigure $itk_component(tframe) 0 -weight 1

	# display components
	pack $itk_component(tools) -expand yes -fill both
	pack $itk_component(props) -expand yes -fill both
    }

    itk_option define -beforecommand command Command "" {}
    itk_option define -aftercommand command Command "" {}
    itk_option define -output output Command "" {}

    public {
	method update {bot} {$itk_component(props) update $bot}
    }
}
