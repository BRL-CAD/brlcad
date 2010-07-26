::itcl::class BotPropertyBox {
    inherit ::itk::Widget

    constructor {args} {}
}

::itcl::body BotPropertyBox::constructor {args} {

    itk_component add boxLabel {
	ttk::labelframe $itk_interior.box -text {BoT Properties}
    } {}

    grid $itk_component(boxLabel) -row 0 -column 0

    eval itk_initialize $args
}
