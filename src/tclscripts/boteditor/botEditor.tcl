package require Itcl
package require Itk

if {[catch {
    set script [file join [bu_brlcad_data "tclscripts"] boteditor botPropertyBox.tcl]
    source $script
} errMsg] > 0} {
    puts "Couldn't load \"botPropertyBox.tcl\"\n$errMsg"
} 

::itcl::class BotEditor {
    inherit ::itk::Toplevel

    constructor {args} {}

    public {
	method load {bot}
    }    
}

::itcl::body BotEditor::constructor {args} {
    eval itk_initialize $args

    $this configure -title "BoT Editor"
}

::itcl::body BotEditor::load {bot} {

    # show name of bot being edited
    $this configure -title "BoT Editor - $bot"

    itk_component add propBox {
	BotPropertyBox $itk_interior.propBox
    } {}

    grid $itk_component(propBox) -row 0 -column 0
}
