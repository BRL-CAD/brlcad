##                 M G E D . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	The Mged class inherits from Panedwindow and contains a QuadDisplay
#       object and a Command object.
#
class Mged {
    inherit iwidgets::Panedwindow

    constructor {file args} {}
    destructor {}

    itk_option define -prompt prompt Prompt "mged> "

    public method opendb {args}
    public method draw {args}
    public method zap {}
    public method ls {args}
    public method l {args}
    public method tops {}
    public method who {}
    public method autoview {}
    public method pane {args}
    public method exit {}
    public method q {}
    public method aet {args}
    public method center {args}
    public method rot {args}
    public method slew {x y}
    public method tra {args}
    public method size {args}
    public method scale {args}
    public method zoom {sf}
    public method multi_pane {args}
    public method reset_panes {}

    private variable qd ""
    private variable db ""
    private variable cmd ""
}

body Mged::constructor {file args} {
    # process options
    eval itk_initialize $args

    add qd
    set qd [QuadDisplay [childsite qd].qd ogl]
    pack $qd -fill both -expand yes

    add cmd
    set cmd [Command [childsite cmd].cmd -prompt $itk_option(-prompt) -cmd_prefix $this]
    pack $cmd -fill both -expand yes

    set db [Database #auto $file]
    $qd addall [$db Drawable::get_name]

    # initialize pane
    paneconfigure 0 -margin 0
    paneconfigure 1 -margin 4

    # default real estate allocation:  geometry ---> 75%, command window ---> 25%
    fraction 75 25
}

body Mged::destructor {} {
    ::delete object $qd
    ::delete object $db
    ::delete object $cmd
}

#########################  User Interface #########################
body Mged::exit {} {
    ::q
}

body Mged::q {} {
    ::q
}

body Mged::opendb {args} {
    eval $db open $args
}

body Mged::draw {args} {
    set who [who]

    if {$who == ""} {
	set blank 1
    } else {
	set blank 0
    }

    set result [eval $db draw $args]

    if {$blank} {
	autoview
    }

    return $result
}

body Mged::zap {} {
    $db zap
}

body Mged::ls {args} {
    eval $db ls $args
}

body Mged::l {args} {
    eval $db l $args
}

body Mged::tops {} {
    $db tops
}

body Mged::who {} {
    $db who
}

body Mged::autoview {} {
    $qd autoviewall
}

body Mged::pane {args} {
    eval $qd pane $args
}

body Mged::aet {args} {
    eval $qd aet $args
}

body Mged::center {args} {
    eval $qd center $args
}

body Mged::rot {args} {
    eval $qd rot $args
}

body Mged::slew {x y} {
    eval $qd slew $x $y
}

body Mged::tra {args} {
    eval $qd tra $args
}

body Mged::size {args} {
    eval $qd size $args
}

body Mged::scale {args} {
    eval $qd scale $args
}

body Mged::zoom {sf} {
    eval $qd zoom $sf
}

body Mged::multi_pane {args} {
    eval $qd multi_pane $args
}

body Mged::reset_panes {} {
    # default real estate allocation:  geometry ---> 75%, command window ---> 25%
    fraction 75 25
    $qd resetall
}

######################### Private Methods #########################
