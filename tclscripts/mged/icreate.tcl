set ic(winnum) 0
set ic(default,type) ""
set ic(default,indexvar) index

proc icreate args {
    global ic
    global $ic(default,indexvar)

    set w $ic(winnum)
    incr ic(winnum)

    if { [llength $args]>0 } then {
	set ic(default,type) [lindex $args 0]
    }

    catch { destroy .ic$w }
    toplevel .ic$w
    wm title .ic$w "Instance Creation"

    frame .ic$w.top
    frame .ic$w.top.left
    frame .ic$w.top.right

    pack .ic$w.top -side top -fill x -expand yes
    pack .ic$w.top.left -side left -fill y
    pack .ic$w.top.right -side left -fill x -expand yes

    label .ic$w.top.left.index -text "Index" -anchor w
    label .ic$w.top.left.indexvar -text "Index var" -anchor w
    label .ic$w.top.left.format -text "Format" -anchor w
    label .ic$w.top.left.type -text "Prototype" -anchor w
    label .ic$w.top.left.comb -text "Comb to add to" -anchor w

    pack .ic$w.top.left.index .ic$w.top.left.indexvar \
	    .ic$w.top.left.format .ic$w.top.left.type .ic$w.top.left.comb \
	    -side top -fill y -expand yes -anchor w

    if { [catch { set ic($w,index) [set $ic(default,indexvar)] }]!=0 } {
	set ic($w,index) 1
    }
    entry .ic$w.top.right.index -relief sunken -width 12 \
	    -textvariable ic($w,index)

    set ic($w,indexvar) $ic(default,indexvar)
    entry .ic$w.top.right.indexvar -relief sunken -width 12 \
	    -textvariable ic($w,indexvar)

    set ic($w,format) "my$ic(default,type).\$$ic(default,indexvar)"
    entry .ic$w.top.right.format -relief sunken -width 12 \
	    -textvariable ic($w,format)

    set ic($w,type) $ic(default,type)
    entry .ic$w.top.right.type -relief sunken -width 12 \
	    -textvariable ic($w,type)

    set ic($w,comb) ""
    entry .ic$w.top.right.comb -relief sunken -width 12 \
	    -textvariable ic($w,comb)

    pack .ic$w.top.right.index .ic$w.top.right.indexvar \
	    .ic$w.top.right.format .ic$w.top.right.index \
	    .ic$w.top.right.type .ic$w.top.right.comb \
	    -side top -fill x -expand yes

    frame .ic$w.bot
    pack .ic$w.bot -side top -fill x -expand yes

    button .ic$w.bot.quit -text "Quit" -command "icquit $w"
    button .ic$w.bot.create -text "Create" -command "icdoit $w"
    pack .ic$w.bot.quit .ic$w.bot.create -side left -fill x -expand yes

    button .ic$w.accept -text "Accept" -state disabled -command "icaccept $w"
    pack .ic$w.accept -side top -fill x -expand yes

}

proc icaccept { w } {
    .ic$w.accept configure -state disabled
    .ic$w.bot.create configure -state normal
    press accept
}

proc icquit { w } {
    global ic
    global $ic($w,indexvar)

    set ic(default,indexvar) $ic($w,indexvar)
    set ic(default,type) $ic($w,type)

    destroy .ic$w
}

proc icdoit { w } {
    global ic
    global $ic($w,indexvar)
    
    set $ic($w,indexvar) $ic($w,index)

    set objname [eval list $ic($w,format)]
    
    incr ic($w,index)
    set $ic($w,indexvar) $ic($w,index)
    set ic(default,indexvar) $ic($w,indexvar)
    set ic(default,type) $ic($w,type)

    g $objname $ic($w,type)
    if { $ic($w,comb)!="" } then {
	i $ic($w,comb) $objname
    }
    E $objname
    press oill

    .ic$w.accept configure -state normal
    .ic$w.bot.create configure -state disabled
}







