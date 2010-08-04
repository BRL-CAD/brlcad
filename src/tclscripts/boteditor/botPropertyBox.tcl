::itcl::class BotPropertyBox {
    inherit ::itk::Widget

    constructor {args} {}

    itk_option define -command command Command {} {}
}

::itcl::body BotPropertyBox::constructor {args} {

    eval itk_initialize $args

    # create main box
    itk_component add box {
	ttk::labelframe $itk_interior.box \
	    -text {BoT Properties} \
	    -padding 5
    } {}

    # create type selection widgets
    itk_component add typeLabel {
	ttk::label $itk_component(box).typeLbl \
	    -text {Type}
    } {}
    
    itk_component add typeLine {
	ttk::separator $itk_component(box).typeSep \
	    -orient horizontal
    } {}
    
    itk_component add surfRadio {
	ttk::radiobutton $itk_component(box).surfRadio \
	    -text Surface \
	    -command $itk_option(-command)
    } {}

    itk_component add volRadio {
	ttk::radiobutton $itk_component(box).volRadio \
	    -text Volume
    } {}

    itk_component add plateRadio {
	ttk::radiobutton $itk_component(box).plateRadio \
	    -text Plate
    } {}

    # create geometry info widgets
    itk_component add infoLabel {
	ttk::label $itk_component(box).infoLbl \
	    -text {Geometry}
    } {}

    itk_component add infoLine {
	ttk::separator $itk_component(box).infoSep \
	    -orient horizontal
    } {}

    itk_component add faces {
	ttk::label $itk_component(box).faces \
	    -text {Faces: }
    } {}

    itk_component add vertices {
	ttk::label $itk_component(box).vertices \
	    -text {Vertices: }
    } {}

    # display main box
    grid $itk_component(box) -row 0 -column 0 -sticky news
    grid rowconfigure $itk_component(box) {0 1 3 4} -weight 1
    grid rowconfigure $itk_interior 0 -weight 1
    grid columnconfigure $itk_interior 0 -weight 1

    # display bot type selection widgets
    set r 0; set c 0
    grid $itk_component(typeLabel) \
        -row $r -column $c \
	-sticky nw \
	-padx 2
    incr r
    grid $itk_component(typeLine) \
        -row $r -column $c \
	-columnspan 4 \
	-sticky ew \
	-padx {2 10} -pady {0 5} 
    incr r; set c 0
    grid $itk_component(surfRadio) \
        -row $r -column $c \
	-padx 5
    incr c
    grid $itk_component(volRadio) \
        -row $r -column $c \
	-padx 5
    incr c
    grid $itk_component(plateRadio) \
        -row $r -column $c \
	-padx 5

    # display geometry info widgets
    incr r; set c 0
    grid $itk_component(infoLabel) \
        -row $r -column $c \
	-sticky nw \
	-padx 2 -pady {20 0}
    incr r
    grid $itk_component(infoLine) \
        -row $r -column $c \
	-columnspan 3 \
	-sticky ew \
	-padx {2 10} -pady {0 5}
    incr r
    grid $itk_component(faces) \
        -row $r -column $c \
	-sticky w \
	-padx 5
    incr r
    grid $itk_component(vertices) \
        -row $r -column $c \
	-sticky w \
	-padx 5
}
