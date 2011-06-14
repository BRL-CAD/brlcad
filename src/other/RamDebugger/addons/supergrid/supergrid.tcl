

###################################################################################
#
#    supergrid: automatically grids all the widgets of a frame and subframes based 
#               in information included at widget creation using -grid gridinfo
#
#      The information is included as a list with short typing.
#      supergrid tries to be clever by automatically assigning the weights to the
#      columns and rows
#
#    examples:
#
#              label .l3 -grid 0        grid this label in column 0
#              label .l3 -grid "1 3"    grid this label in column 1 with columnspan 3
#              label .l3 -grid "1 px3"  grid this label in column 1 with padx 3
#              label .l3 -grid "1 py2"  grid this label in column 1 with pady 2
#              label .l3 -grid "1 nw"   grid this label in column 1 with sticky nw
#              label .l3 -grid "1 nwwe" grid this label in column 1 with sticky nw
#                                       but weight 1 in column
#              label .l3 -grid "0 uca"  grid this label in column 0 with -uniform a
#              
#
#    Limitations: A widget cannot be gridded with rowconfigure > 1
#
#    Usage:
#
#      Add option -grid to all widgets of a frame and subframes
#      use: supergrid::go
#
#    Additional commands:
#
#    supergrid::gridinfo { w gridval } Enters the grid information for widget w
#
#    gridinfo:
#      This is a TCL list that can contain the following items:
#         -One digit   it is the column number
#         -two digits  the first one is the column number, the 2nd is the 
#                      columnspan (optional)
#         -any of 'nsew' the sticky option (optional)
#         -px{digit} or py{digit} the -padx -pady (optional)
#         -ucuniformgroup equivalent to: -uniform uniformgroup in current column
#         -uruniformgroup equivalent to: -uniform uniformgroup in current row
#
#    Note: Modify the supergrid definition to add new custom widgets
#
#    Variable UseGPrefix: if set to one prefix, it is necessary to add that prefix
#                         to the widgets names
#          example:   if UseGPrefix is g_ then use g_label .l3 -grid 0
#
###################################################################################

################################################################################
#  This software is copyrighted by Ramon Ribó (RAMSAN) ramsan@cimne.upc.es.
#  (http://gid.cimne.upc.es/ramsan) The following terms apply to all files 
#  associated with the software unless explicitly disclaimed in individual files.

#  The authors hereby grant permission to use, copy, modify, distribute,
#  and license this software and its documentation for any purpose, provided
#  that existing copyright notices are retained in all copies and that this
#  notice is included verbatim in any distributions. No written agreement,
#  license, or royalty fee is required for any of the authorized uses.
#  Modifications to this software may be copyrighted by their authors
#  and need not follow the licensing terms described here, provided that
#  the new terms are clearly indicated on the first page of each file where
#  they apply.

#  IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
#  FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
#  ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
#  DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.

#  THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
#  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
#  IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
#  NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
#  MODIFICATIONS.
################################################################################


package require Tcl 8.3
package require Tk 8.3
package provide supergrid 1.0

namespace eval supergrid {
    variable UseGPrefix ""
    variable widgets [list label entry button checkbutton radiobutton frame canvas \
		          scrollbar text listbox menubutton scale]
    variable widgets2 [list panedwindow TitleFrame SpinBox ComboBox date::datefield \
		           ScrolledWindow PanedWindow ButtonBox MainFrame Button Label NoteBook \
		           Separator Entry treectrl hugecombo labelframe spinbox]
    variable SubGridClasses [list Frame Labelframe]
    variable HorizontalClasses [list Entry Text Canvas  Listbox PanedWindow TitleFrame ComboBox \
	NoteBook MainFrame ScrolledWindow Panedwindow TreeCtrl Labelframe Spinbox Hugecombo]
    variable VerticalClasses [list Text Canvas Listbox PanedWindow TitleFrame NoteBook MainFrame \
	ScrolledWindow panedwindow Panedwindow TreeCtrl Labelframe]
    variable ScrollClasses [list Scrollbar]
    variable DiscardClasses [list Toplevel Menu]
    variable DiscardWidgets {^\.#BWidget}

    variable data
}

proc supergrid::init {} {
    variable widgets
    variable widgets2
    variable data
    variable UseGPrefix

    if { $UseGPrefix != "" } {
	if { [info command ::$UseGPrefix[lindex $widgets 0]] != "" } { return }
	set prefix ::$UseGPrefix
	set body {
	    set ipos [lsearch $args -grid]
	    if { $ipos != -1 } {
		set supergrid::data($w) [lindex $args [expr $ipos+1]]
		set args [lreplace $args $ipos [expr $ipos+1]]
	    }
	    return [eval [string range [lindex [info level 0] 0] LENGTH end] \
		        $w $args]
	}
	regsub LENGTH $body [string length $UseGPrefix] body
    } else {
	if { [info command ::[lindex $widgets 0]_supergrid] != "" } { return }
	set prefix ::
	set body {
	    set ipos [lsearch $args -grid]
	    if { $ipos != -1 } {
		set supergrid::data($w) [lindex $args [expr $ipos+1]]
		set args [lreplace $args $ipos [expr $ipos+1]]
	    }
	    return [eval [lindex [info level 0] 0]_supergrid $w $args]
	}
    }
    foreach i $widgets {
	if { [info command ::$i] == "" } { auto_load ::$i }
	if { [info command ::$i] == "" } { error "command ::$i does not exists" }
	if { $UseGPrefix == "" } { rename $prefix$i $prefix${i}_supergrid }
	proc $prefix$i { w args } $body
    }
    foreach i $widgets2 {
	if { [info command ::$i] == "" } { auto_load ::$i }
	if { [info command ::$i] == "" } { continue }
	if { $UseGPrefix == "" } { rename $prefix$i $prefix${i}_supergrid }
	proc $prefix$i { w args } $body
    }
}

proc supergrid::gridinfo { w gridval } {
    variable data

    set data($w) $gridval
}

proc supergrid::go { f { isrecursive 0 } } {
    variable SubGridClasses
    variable HorizontalClasses
    variable VerticalClasses
    variable ScrollClasses
    variable DiscardClasses
    variable DiscardWidgets
    variable data

    if { ![info exists data] } {
	error "error: before running supergrid::go it is necessary to use -grid in widgets"
    }

#     foreach "cols rows" [grid size $f] break
#     for { set i 0 } { $i < $cols } { incr i } {
#         grid columnconfigure $f $i -weight 0 -minsize 0 -pad 0
#     }
#     for { set i 0 } { $i < $rows } { incr i } {
#         grid rowconfigure $f $i -weight 0 -minsize 0 -pad 0
#     }

    set needsweightx 0
    set needsweighty 0
    
    # doing first pass, horizontal weights only applied for columnspan 1
    set maxcolumnspan 0
    set currentcol -1
    set currentrow 0

    foreach i [winfo children $f] {
	if { [lsearch $DiscardClasses [winfo class $i]] != -1 } {
	    continue
	}
	if { [regexp $DiscardWidgets $i] } { continue }
	if { ![info exists data($i)] } {
	    error "error: before running supergrid::go it is necessary to use -grid in widget $i"
	}
	if { $data($i) == "no" } { continue }

	set needsweightxL 0; set needsweightyL 0
	if { [lsearch $SubGridClasses [winfo class $i]] != -1 } {
	    if { [catch {
		foreach "needsweightxL needsweightyL" [go $i 1] break
	    }] } {
		set needsweightxL 1
		set needsweightyL 1
	    }
	}
	set col -1
	set columnspan 1
	set sticky ""
	set padx 0
	set pady 0
	set uniformc ""
	set uniformr ""
	foreach item $data($i) {
	    if { [string is integer -strict $item] } {
		if { $col == -1 } {
		    set col $item
		} else { 
		    set columnspan $item
		}
	    } elseif { [string match px* $item] } {
		set padx [string range $item 2 end]
	    } elseif { [string match py* $item] } {
		set pady [string range $item 2 end]
	    } elseif { [string match uc* $item] } {
		set uniformc [string range $item 2 end]
	    } elseif { [string match ur* $item] } {
		set uniformr [string range $item 2 end]
	    } else { set sticky $item }
	}
	if { $col == -1 } {
	    error "error bad widget name '$i' for supergrid"
	}
	if { $col <= $currentcol } { incr currentrow }
	set currentcol $col

	if { $columnspan > $maxcolumnspan } { set maxcolumnspan $columnspan }
	if { [lsearch $HorizontalClasses [winfo class $i]] != -1 } {
	    set needsweightxL 1
	}
	if { [lsearch $VerticalClasses [winfo class $i]] != -1 } {
	    set needsweightyL 1
	}
	if { [lsearch $ScrollClasses [winfo class $i]] != -1 } {
	    switch [$i cget -orient] {
		horizontal { set needsweightxL 1 }
		vertical { set needsweightyL 1 }
	    }
	}

	set rsticky ""
	foreach j "e w n s" {
	    set c($j) [llength [regexp -inline -all $j $sticky]]
	}
	switch $c(w) {
	    2 {
		switch $c(e) {
		    0 - 2 {
		        error "invalid sticky '$sticky"
		    }
		}
		set needsweightxL 1
		append rsticky w
	    }
	    1 {
		switch $c(e) {
		    2 { set needsweightxL 1 ; append rsticky e }
		    1 { set needsweightxL 1 ; append rsticky we }
		    0 { set needsweightxL 0 ; append rsticky w }
		}
	    }
	    0 {
		switch $c(e) {
		    2 { error "invalid sticky '$sticky" }
		    1 { set needsweightxL 0 ; append rsticky e }
		    0 { if { $needsweightxL } { append rsticky ew } }
		}
	    }
	}
	switch $c(n) {
	    2 {
		switch $c(s) {
		    0 - 2 {
		        error "invalid sticky '$sticky"
		    }
		}
		set needsweightyL 1
		append rsticky n
	    }
	    1 {
		switch $c(s) {
		    2 { set needsweightyL 1 ; append rsticky s }
		    1 { set needsweightyL 1 ; append rsticky ns }
		    0 { set needsweightyL 0 ; append rsticky n }
		}
	    }
	    0 {
		switch $c(s) {
		    2 { error "invalid sticky '$sticky" }
		    1 { set needsweightyL 0 ; append rsticky s }
		    0 { if { $needsweightyL } { append rsticky ns } }
		}
	    }
	}
	grid $i -row $currentrow -column $currentcol -columnspan $columnspan -padx $padx \
	    -pady $pady -sticky $rsticky

	if { $columnspan == 1 && $needsweightxL } {
	    grid columnconfigure $f $currentcol -weight 1
	} else {
	    grid columnconfigure $f $currentcol -weight 0
	}
	if { $uniformc != "" } { grid columnconfigure $f $currentcol -uniform $uniformc }

	if { $needsweightyL } {
	    grid rowconfigure $f $currentrow  -weight 1
	} else {
	    grid rowconfigure $f $currentrow  -weight 0
	}
	if { $uniformr != "" } { grid rowconfigure $f $currentrow -uniform $uniformr }

	if { $needsweightxL } {set needsweightx 1}
	if { $needsweightyL } { set needsweighty 1}

    }
    # doing second pass, horizontal weights applied for columnspan >1
    # need to be done as many times as columnspan to find the best resizing
    for { set span 2 } { $span <= $maxcolumnspan} { incr span } {
	set currentcol -1
	set currentrow 0
	foreach i [winfo children $f] {
	    if { [lsearch $DiscardClasses [winfo class $i]] != -1 } {
		continue
	    }
	    if { [regexp $DiscardWidgets $i] } { continue }
	    set needsweightxL 0; set needsweightyL 0
	    if { [lsearch $SubGridClasses [winfo class $i]] != -1 } {
		if { [catch {
		    foreach "needsweightxL needsweightyL" [go $i 1] break
		}] } {
		    set needsweightxL 1
		    set needsweightyL 1
		}
	    }

	    set col -1
	    set columnspan 1
	    set sticky ""
	    set padx 0
	    set pady 0
	    set uniformc ""
	    set uniformr ""
	    foreach item $data($i) {
		if { [string is integer -strict $item] } {
		    if { $col == -1 } {
		        set col $item
		    } else { 
		        set columnspan $item
		    }
		} elseif { [string match px* $item] } {
		    set padx [string range $i 2 end]
		} elseif { [string match py* $item] } {
		    set pady [string range $i 2 end]
		} elseif { [string match uc* $item] } {
		    set uniformc [string range $i 2 end]
		} elseif { [string match ur* $item] } {
		    set uniformr [string range $i 2 end]
		} else { set sticky $item }
	    }
	    if { $col <= $currentcol } { incr currentrow }
	    set currentcol $col

	    if { [lsearch $HorizontalClasses [winfo class $i]] != -1 } {
		set needsweightxL 1
	    }
	    if { [lsearch $VerticalClasses [winfo class $i]] != -1 } {
		set needsweightyL 1
	    }
	    if { [lsearch $ScrollClasses [winfo class $i]] != -1 } {
		switch [$i cget -orient] {
		    horizontal { set needsweightxL 1 }
		    vertical { set needsweightyL 1 }
		}
	    }

	    set rsticky ""
	    foreach i "e w n s" {
		set c($i) [llength [regexp -inline -all $i $sticky]]
	    }
	    switch $c(w) {
		2 {
		    switch $c(e) {
		        0 - 2 {
		            error "invalid sticky '$sticky"
		        }
		    }
		    set needsweightxL 1
		    append rsticky w
		}
		1 {
		    switch $c(e) {
		        2 { set needsweightxL 1 ; append rsticky e }
		        1 { set needsweightxL 1 ; append rsticky we }
		        0 { set needsweightxL 0 ; append rsticky w }
		    }
		}
		0 {
		    switch $c(e) {
		        2 { error "invalid sticky '$sticky" }
		        1 { set needsweightxL 0 ; append rsticky e }
		        0 { if { $needsweightxL } { append rsticky ew } }
		    }
		}
	    }
	    switch $c(n) {
		2 {
		    switch $c(s) {
		        0 - 2 {
		            error "invalid sticky '$sticky"
		        }
		    }
		    set needsweightyL 1
		    append rsticky n
		}
		1 {
		    switch $c(s) {
		        2 { set needsweightyL 1 ; append rsticky s }
		        1 { set needsweightyL 1 ; append rsticky ns }
		        0 { set needsweightyL 0 ; append rsticky n }
		    }
		}
		0 {
		    switch $c(s) {
		        2 { error "invalid sticky '$sticky" }
		        1 { set needsweightyL 0 ; append rsticky s }
		        0 { if { $needsweightyL } { append rsticky ns } }
		    }
		}
	    }

	    if { $columnspan == $span && $needsweightxL } {
		set found 0
		for { set j $currentcol } { $j < [expr $currentcol+$columnspan] } { incr j } {
		    if { [grid columnconfigure $f $j -weight] > 0 } {
		        set found 1
		        break
		    }
		}
		if { !$found } {
#                    grid columnconfigure $f [expr $currentcol+$columnspan-1] -weight 1
		    for { set j $currentcol } { $j < [expr $currentcol+$columnspan] } { incr j } {
		        grid columnconfigure $f $j -weight 1
		    }
		}
	    }
	}
    }
    if { !$isrecursive } { cleandata $f }
    if { !$needsweightx } {
	grid columnconfigure $f [lindex [grid size $f] 0] -weight 1
    }
    if { !$needsweighty } {
	grid rowconfigure $f [lindex [grid size $f] 1] -weight 1
    }
    return [list $needsweightx $needsweighty]
}

proc supergrid::cleandata { f } {
    variable SubGridClasses
    variable DiscardClasses
    variable DiscardWidgets
    variable data

    foreach i [winfo children $f] {
	if { [lsearch $DiscardClasses [winfo class $i]] != -1 } {
	    continue
	}
	if { [regexp $DiscardWidgets $i] } { continue }
	if { [lsearch $SubGridClasses [winfo class $i]] != -1 } {
	    catch { cleandata $i }
	}
	catch { unset data($i) }
    }
}
################################################################################
# it is necessary to init the game
################################################################################

supergrid::init

################################################################################
# that's all
################################################################################



# # simple example

# toplevel .t
# label .t.l -text "Hello world!" -grid "0 2"
# button .t.b -text OK -width 8 -grid "0 px3"
# button .t.b1 -text Cancel -width 8 -command exit -grid "1 px3"
# supergrid::go .t

# # more complex example

# button .title -text "The TCL-TK poll" -font [list Helvetica 16 bold] \
#     -activeforeground red -bd 0 -grid "0 5"

# label .q -text "Do you like TCL?" -grid "0 2"
# radiobutton .r1 -text Yes -var kk2 -value y -grid 2
# radiobutton .r2 -text No -var kk2  -value n -grid 3
# radiobutton .r3 -text "Yes with Supergrid" -var kk2 -value ysg -grid 4
# set kk2 ysg

# label .n -text "Enter the best news group:" -grid "0 2 e"
# entry .e -grid "2 3" ; .e ins end comp.lang.tcl

# label .w -text "Who are you?" -grid 0
# tk_optionMenu .om kk Beginner Advanced Expert
# supergrid::gridinfo .om "1 2"

# frame .f -relief raised -bd 2 -grid "0 5" 
# label .f.l -text "Enter your impressions:" -grid "0 2 w"
# text .f.t -xscroll ".f.sh set" -yscroll ".f.sv set" \
#     -width 20 -height 4 -grid 0
# scrollbar .f.sv -orient vertical -command ".f.t yview" -grid 1
# scrollbar .f.sh -orient horizontal -command ".f.t xview" -grid 0

# frame .f2 -relief raised -bd 2 -grid "0 5"

# frame .buts -grid "0 5"
# button .buts.ok -text OK -width 8 -grid "0 px2 py3"
# button .buts.cancel -text Cancel -width 8 -command exit -grid "1 px2"

# supergrid::go .

# proc flash { b } {
#     $b flash
#     after 500 flash $b
# }
# flash .title
