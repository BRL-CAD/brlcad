#
# itk.tcl
# ----------------------------------------------------------------------
# Invoked automatically upon startup to customize the interpreter
# for [incr Tk].
# ----------------------------------------------------------------------
#   AUTHOR:  Michael J. McLennan
#            Bell Labs Innovations for Lucent Technologies
#            mmclennan@lucent.com
#            http://www.tcltk.com/itcl
#
#      RCS:  $Id$
# ----------------------------------------------------------------------
#            Copyright (c) 1993-1998  Lucent Technologies, Inc.
# ======================================================================
# See the file "license.terms" for information on usage and
# redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.

#
# Provide transparent access to all [incr Tk] commands
#
if {$tcl_platform(os) == "MacOS"} {
    source -rsrc itk:tclIndex
} else {
    lappend auto_path ${itk::library}
}

#
# Define "usual" option-handling code for the Tk widgets:
#
itk::usual Button {
    keep -background -cursor -foreground -font
    keep -activebackground -activeforeground -disabledforeground
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Canvas {
    keep -background -cursor
    keep -insertbackground -insertborderwidth -insertwidth
    keep -insertontime -insertofftime
    keep -selectbackground -selectborderwidth -selectforeground
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Checkbutton {
    keep -background -cursor -foreground -font
    keep -activebackground -activeforeground -disabledforeground
    keep -selectcolor
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Entry {
    keep -background -cursor -foreground -font
    keep -insertbackground -insertborderwidth -insertwidth
    keep -insertontime -insertofftime
    keep -selectbackground -selectborderwidth -selectforeground
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Frame {
    keep -background -cursor
}

itk::usual Label {
    keep -background -cursor -foreground -font
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Listbox {
    keep -background -cursor -foreground -font
    keep -selectbackground -selectborderwidth -selectforeground
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Menu {
    keep -background -cursor -foreground -font
    keep -activebackground -activeforeground -disabledforeground
    keep -selectcolor -tearoff
}

itk::usual Menubutton {
    keep -background -cursor -foreground -font
    keep -activebackground -activeforeground -disabledforeground
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Message {
    keep -background -cursor -foreground -font
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Radiobutton {
    keep -background -cursor -foreground -font
    keep -activebackground -activeforeground -disabledforeground
    keep -selectcolor
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Scale {
    keep -background -cursor -foreground -font -troughcolor
    keep -activebackground
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Scrollbar {
    keep -background -cursor -troughcolor
    keep -activebackground -activerelief
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Text {
    keep -background -cursor -foreground -font
    keep -insertbackground -insertborderwidth -insertwidth
    keep -insertontime -insertofftime
    keep -selectbackground -selectborderwidth -selectforeground
    keep -highlightcolor -highlightthickness
    rename -highlightbackground -background background Background
}

itk::usual Toplevel {
    keep -background -cursor
}
