
catch { destroy .titles }

toplevel .titles
wm title .titles "MGED Display Variables"

frame .titles.l
frame .titles.r

pack .titles.l .titles.r -side left -fill both -expand yes

foreach name [array names mged_display] {
    label .titles.l.$name -text mged_display($name) -anchor w
    label .titles.r.$name -textvar mged_display($name) -anchor w

    pack .titles.l.$name -side top -fill y -expand yes -anchor w
    pack .titles.r.$name -side top -fill x -expand yes -anchor w
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
