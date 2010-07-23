package require Itcl
package require Itk

::itcl::class BotEditor {
    inherit ::itk::Toplevel

    constructor {args} {}

    public {
	method load {bot}
    }    

    private {
    }
}

::itcl::body BotEditor::constructor {args} {
    eval itk_initialize $args

    $this configure -title "BoT Editor"
}

::itcl::body BotEditor::load {bot} {

    # show name of bot being edited
    $this configure -title "BoT Editor - $bot"
}
