#                     S T D D L G S . T C L
# BRL-CAD
#
# Copyright (c) 2006-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# DESCRIPTION
#    General dialogs.
#
# AUTHOR
#    Keith Bowman
#    Doug Howard
#    Bob Parker
#
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


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
