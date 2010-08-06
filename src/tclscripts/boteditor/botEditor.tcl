# BotEditor class for editing a BoT primitive
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
	method onChange {}
	method apply {}
	method revert {}
	method accept {}
	method reject {}

	proc localizeDialog {d}
	proc focusOnEnter {d}
    }    

    private {
	variable original ""
	variable copy ""
	method showUnsaved {show}
    }

    itk_option define -prefix prefix Prefix "" {}
}

# open editor for bot named 'bot'
# this bot is assumed to exist in the current database
#
::itcl::body BotEditor::constructor {bot args} {

    eval itk_initialize $args

    # use proper command context
    set _ $itk_option(-prefix)

    # remove path prefix from bot name
    while {[regsub {.*/} $bot {} bot]} {}

    # show name of bot being edited
    $this configure -title "[eval $this cget -title] - $bot"
    set statusLine "editing \"$bot\""

    # make working copy
    set original $bot
    set copy $original.edit
    $_ cp $original $copy

    # create layout frames
    itk_component add histFrame {
	ttk::frame $itk_interior.historyFrame \
	    -padding 7
    } {}
    itk_component add eframe {
	EditPane $itk_interior.editPane $copy \
	    -prefix $itk_option(-prefix)
    } {}
    itk_component add closeframe {
	ttk::frame $itk_interior.closeFrame \
	    -padding 7
    } {}
    itk_component add statusframe {
	ttk::frame $itk_interior.statusFrame
    } {}
    
    # add widgets to history frame
    itk_component add apply {
	ttk::button $itk_component(histFrame).apply \
	    -text Apply \
	    -command "$this apply"
    } {}
    itk_component add revert {
	ttk::button $itk_component(histFrame).revert \
	    -text Revert \
	    -command "$this revert"
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
	    -text $statusLine
    } {}

    # display main frames
#    grid $itk_component(histFrame) -row 0 -column 0 -sticky news
    grid $itk_component(eframe) -row 1 -column 0 -sticky news
    grid $itk_component(closeframe) -row 2 -column 0 -sticky news
    grid $itk_component(statusframe) -row 3 -column 0 -sticky news
    grid rowconfigure $itk_interior 1 -weight 1
    grid columnconfigure $itk_interior 0 -weight 1

    # display history frame components
    pack $itk_component(apply) -side left
    pack $itk_component(revert) -side left -padx 5

    # display close frame components
    pack $itk_component(reject) -side right
    pack $itk_component(accept) -side right -padx 5

    # display status frame components
    pack $itk_component(rule) -expand yes -fill x
    pack $itk_component(status) -expand yes -fill both
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

# called whenever an edit is made
::itcl::body BotEditor::onChange {} {
    showUnsaved yes
}

# save current edit
::itcl::body BotEditor::apply {} {
    showUnsaved no
}

# return to last saved edit
::itcl::body BotEditor::revert {} {
    showUnsaved no
}

# apply changes to original
::itcl::body BotEditor::accept {} {

    set _ $itk_option(-prefix)
    set cmd "$_ kill $original; $_ mv $copy $original; delete object $this"

    # get confirmation
    itk_component add confirm {
	ConfirmationDialog $itk_interior.confirmDialog \
	    -title {Confirm Accept} \
	    -text "This operation will overwrite $original in the current database.\n\n\
		Are you sure you want to save your changes and exit?" \
	    -yescommand $cmd
    } {}
}

# discard all changes
::itcl::body BotEditor::reject {} {

    set _ $itk_option(-prefix)
    set cmd "$_ kill $copy; delete object $this"

    # get confirmation
    itk_component add confirm {
	ConfirmationDialog $itk_interior.confirmDialog \
	    -title {Confirm Reject} \
	    -text {Are you sure you want to discard all changes and exit?} \
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
    # (this minimizes the visual distraction when we redraw)
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
		-command "grab release $itk_interior; $itk_option(-yescommand)"
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
		-prefix $itk_option(-prefix)
	} {}
	itk_component add props {
	    BotPropertyBox $itk_component(rframe).properties
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

    itk_option define -prefix prefix Prefix "" {}
}
