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

    constructor {args} {}

    public {
	method load {bot}
	method onChange {}
	method apply {}
	method revert {}
	method accept {}
	method reject {}
    }    

    private {
	method showUnsaved {show}
    }
}

# basic initialization - nothing can really be done until load
#
::itcl::body BotEditor::constructor {args} {
    eval itk_initialize $args
    
    $this configure -title "BoT Editor"
}

# open editor for bot named 'bot'
# this bot is assumed to exist in the current database
#
::itcl::body BotEditor::load {bot} {

    # show name of bot being edited
    $this configure -title "[eval $this cget -title] - $bot"

    # create frames for organization
    itk_component add histFrame {
	ttk::frame $itk_interior.historyFrame \
	    -padding 7
    } {}
    itk_component add eframe {
	EditPane $itk_interior.editPane
    } {}
    itk_component add closeframe {
	ttk::frame $itk_interior.closeFrame \
	    -padding 7
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

    # display main frames
    grid $itk_component(histFrame) -row 0 -column 0 -sticky news
    grid $itk_component(eframe) -row 1 -column 0 -sticky news
    grid $itk_component(closeframe) -row 2 -column 0 -sticky news
    grid rowconfigure $itk_interior 1 -weight 1
    grid columnconfigure $itk_interior 0 -weight 1

    # display history frame components
    pack $itk_component(apply) -side left
    pack $itk_component(revert) -side left -padx 5

    # display close frame components
    pack $itk_component(reject) -side right
    pack $itk_component(accept) -side right -padx 5
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
    # close window
    delete object $this
}

# discard all changes
::itcl::body BotEditor::reject {} {
    # close window
    delete object $this

    # TODO - modal confirmation dialog
}

::itcl::class EditPane {
    inherit itk::Widget

    constructor {args} {
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
	    BotTools $itk_component(lframe).tools
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
}
