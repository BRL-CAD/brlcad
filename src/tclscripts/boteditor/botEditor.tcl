package require Itcl
package require Itk

if {[catch {
    set script [file join [bu_brlcad_data "tclscripts"] boteditor botPropertyBox.tcl]
    source $script

    set script [file join [bu_brlcad_data "tclscripts"] boteditor botTools.tcl]
    source $script
} errMsg] > 0} {
    puts "Couldn't load \"$script\"\n$errMsg"
} 

::itcl::class BotEditor {
    inherit ::itk::Toplevel

    constructor {args} {}

    public {
	method load {bot}
	method onChange {}
	method apply {}
	method accept {}
	method reject {}
    }    
}

::itcl::body BotEditor::constructor {args} {
    eval itk_initialize $args
    
    grid rowconfigure $itk_interior 0 -weight 1
    grid columnconfigure $itk_interior 0 -weight 1

    $this configure -title "BoT Editor"
}

::itcl::body BotEditor::load {bot} {

    # show name of bot being edited
    $this configure -title "[eval $this cget -title] - $bot"

    # make edit widgets frame
    itk_component add eframe {
	ttk::frame $itk_interior.editFrame \
	    -padding 7
    } {}

    # make close buttons frame
    itk_component add bframe {
	ttk::frame $itk_interior.buttonsFrame \
	    -padding 7
    } {}
    
    # make tool tabs
    itk_component add tools {
	BotTools $itk_component(eframe).tools
    } {}

    # make property box
    itk_component add propBox {
	BotPropertyBox $itk_component(eframe).propBox \
	    -command "$this onChange"
    } {}

    # make apply button
    itk_component add apply {
	ttk::button $itk_component(eframe).apply \
	    -text Apply \
	    -command "$this apply"
    } {}

    # make accept/reject buttons
    itk_component add accept {
	ttk::button $itk_component(bframe).accept \
	    -text Accept \
	    -command "$this accept"
    } {}

    itk_component add reject {
	ttk::button $itk_component(bframe).reject \
	    -text Reject \
	    -command "$this reject"
    } {}

#    itk_component add disp {
#	Display $itk_component(eframe).display
#    }

    # display frames
    grid $itk_component(eframe) -row 0 -column 0 -sticky news
    grid $itk_component(bframe) -row 1 -column 0 -sticky news

    # allow contents to expand
    grid rowconfigure $itk_component(eframe) 2 -weight 1
    grid columnconfigure $itk_component(eframe) 0 -weight 1

    # display components
    set row 0; set col 0
    grid $itk_component(apply) -row $row -column [expr "$col + 1"] -sticky se -pady {5 0}

    incr row
    grid $itk_component(tools) -row $row -column $col -sticky news -padx {0 10}
    grid $itk_component(propBox) -row $row -column [expr $col + 1] -sticky nes

#    incr row
#    grid $itk_component(disp) -row $row -column $col -columnspan 2 -sticky nesw

    pack $itk_component(reject) -side right
    pack $itk_component(accept) -side right -padx 5
}

::itcl::body BotEditor::onChange {} {

    # append '*' to title to indicate unapplyed change
    set title [eval $this cget -title]
    
    if {[regexp ^.*\\*$ $title] == 0} {
	$this configure -title $title*
    }
}

::itcl::body BotEditor::apply {} {

    # remove '*' from title to indicate application
    set match ""; set title [eval $this cget -title]
    regexp ^(.*)\\*$ $title match title
    $this configure -title $title
}

::itcl::body BotEditor::accept {} {
    # close window
    delete object $this
}

::itcl::body BotEditor::reject {} {
    # close window
    delete object $this

    # TODO - modal confirmation dialog
}
