# TYPE: tcltk
##############################################################
#
# Stddlgs.tcl
#
##############################################################
#****c* Stddlgs/Stddlgs
# NAME
#    Stddlgs
#
# DESCRIPTION
#    General dialogs.
#
# COPYRIGHT
#    Portions Copyright (c) 2002 SURVICE Engineering Company.
#    All Rights Reserved. This file contains Original Code 
#    and/or Modifications of Original Code as defined in and 
#    that are subject to the SURVICE Public Source License
#    (Version 1.3, dated March 12, 2002).
#
# AUTHOR
#    Keith Bowman
#    Doug Howard
#    Bob Parker
#
#***
##############################################################

::itcl::class sdialogs::Stddlgs {
    public {
	proc errordlg {title message {buttons "ok"} {args ""}}
	proc warningdlg {title message {buttons "ok"} {args ""}}
	proc infodlg {title message {buttons "ok"} {args ""}}
	proc questiondlg {title message {buttons "yesno"} {args ""}}
	proc messagedlg {title icon buttons message {args ""}}
    }
}

::itcl::body sdialogs::Stddlgs::errordlg {title message {buttons "ok"} {args ""}} {
    return [Stddlgs::messagedlg $title \
	    error \
	    $buttons \
	    $message \
	    $args]
}

::itcl::body sdialogs::Stddlgs::warningdlg {title message {buttons "ok"} {args ""}} {
    return [Stddlgs::messagedlg $title \
	    warning \
	    $buttons \
	    $message \
	    $args]
}

::itcl::body sdialogs::Stddlgs::infodlg {title message {buttons "ok"} {args ""}} {
    return [Stddlgs::messagedlg $title \
	    info \
	    $buttons \
	    $message \
	    $args]
}

::itcl::body sdialogs::Stddlgs::questiondlg {title message {buttons "yesno"} {args ""}} {
    return [Stddlgs::messagedlg $title \
	    question \
	    $buttons \
	    $message \
	    $args]
}

::itcl::body sdialogs::Stddlgs::messagedlg {title icon buttons message {args ""}} {
    return [eval tk_messageBox -title {$title} \
	    -icon {$icon} \
	    -type {$buttons} \
	    -message {$message} [lindex $args 0]]
}


