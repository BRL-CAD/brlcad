
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
