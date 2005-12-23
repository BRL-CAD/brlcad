#!../src/bltwish

package require BLT

# --------------------------------------------------------------------------
# Starting with Tcl 8.x, the BLT commands are stored in their own 
# namespace called "blt".  The idea is to prevent name clashes with
# Tcl commands and variables from other packages, such as a "table"
# command in two different packages.  
#
# You can access the BLT commands in a couple of ways.  You can prefix
# all the BLT commands with the namespace qualifier "blt::"
#  
#    blt::graph .g
#    blt::table . .g -resize both
# 
# or you can import all the command into the global namespace.
#
#    namespace import blt::*
#    graph .g
#    table . .g -resize both
#
# --------------------------------------------------------------------------

if { $tcl_version >= 8.0 } {
    namespace import blt::*
    namespace import -force blt::tile::*
}

#source scripts/demo.tcl

set file ../demos/images/chalk.gif
set active ../demos/images/rain.gif

image create photo calendar.texture.1 -file $file
image create photo calendar.texture.2 -file $active

option add *Tile calendar.texture.1

option add *HighlightThickness		0
option add *calendar.weekframe*Tile	calendar.texture.2
option add *Calendar.Label.borderWidth	0
option add *Calendar.Label.relief	sunken
option add *Calendar.Frame.borderWidth	2
option add *Calendar.Frame.relief	raised
option add *Calendar.Label.font		{ Helvetica 11 }
option add *Calendar.Label.foreground	navyblue
option add *button.foreground		navyblue
option add *background 			grey85
#option add *button.activeForeground	red
#option add *button.activeBackground	blue4
option add *Label.ipadX			200

array set monthInfo {
    Jan { January 31 }
    Feb { February 28 } 
    Mar { March 31 } 
    Apr { April 30 } 
    May { May 31 } 
    Jun { June 30 } 
    Jul { July 31 }
    Aug { August 31 }
    Sep { September 30 }
    Oct { October 31 }
    Nov { November 30 }
    Dec { December 31 }
}

option add *tile calendar.texture.2 
set abbrDays { Sun Mon Tue Wed Thu Fri Sat }

proc Calendar { weekday day month year } {
    global monthInfo abbrDays 
    
    set wkdayOffset [lsearch $abbrDays $weekday]
    if { $wkdayOffset < 0 } {
	error "Invalid week day \"$weekday\""
    }
    set dayOffset [expr ($day-1)%7]
    if { $wkdayOffset < $dayOffset } {
	set wkdayOffset [expr $wkdayOffset+7]
    }
    set wkday [expr $wkdayOffset-$dayOffset-1]
    if { [info commands .calendar] == ".calendar" } {
	destroy .calendar 
    }
    frame .calendar -class Calendar -width 3i -height 3i

    if ![info exists monthInfo($month)] {
	error "Invalid month \"$month\""
    }

    set info $monthInfo($month)
    label .calendar.month \
	-text "[lindex $info 0] $year"  \
	-font { Courier 14 bold }
    table .calendar .calendar.month 1,0 -cspan 7  -pady 10
    
    set cnt 0
    frame .calendar.weekframe -relief sunken -bd 1
    table .calendar .calendar.weekframe 2,0 -columnspan 7 -fill both  
    foreach dayName $abbrDays {
	set name [string tolower $dayName]
	label .calendar.$name \
	    -text $dayName \
	    -font { Helvetica 12 }
	table .calendar .calendar.$name 2,$cnt -pady 2 -padx 2
	incr cnt
    }
    table configure .calendar c* r2 -pad 4 
    set week 0
    set numDays [lindex $info 1]
    for { set cnt 1 } { $cnt <= $numDays } { incr cnt } {
	label .calendar.day${cnt} -text $cnt 
	if { $cnt == $day } {
	    .calendar.day${cnt} configure -relief sunken -bd 1
	}
	incr wkday
	if { $wkday == 7 } {
	    incr week
	    set wkday 0
	}
	table .calendar .calendar.day${cnt} $week+3,$wkday \
	    -fill both -ipadx 10 -ipady 4 
    }
    frame .calendar.quit -bd 1 -relief sunken
    button .calendar.quit.button -command { exit } -text {Quit} -bd 2 
    table .calendar.quit \
	.calendar.quit.button -padx 4 -pady 4
    table .calendar \
	.calendar.quit $week+4,5 -cspan 2 -pady 4 
    table . \
	.calendar -fill both
    table configure .calendar r0 -resize none
    table configure .calendar c0 c6
}

set date [clock format [clock seconds] -format {%a %b %d %Y}]
scan $date { %s %s %d %d } weekday month day year

Calendar $weekday $day $month $year
