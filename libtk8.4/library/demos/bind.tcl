# bind.tcl --
#
# This demonstration script creates a text widget with bindings set
# up for hypertext-like effects.
#
# RCS: @(#) $Id$

if {![info exists widgetDemo]} {
    error "This script should be run from the \"widget\" demo."
}

set w .bind
catch {destroy $w}
toplevel $w
wm title $w "Text Demonstration - Tag Bindings"
wm iconname $w "bind"
positionWindow $w

frame $w.buttons
pack $w.buttons -side bottom -fill x -pady 2m
button $w.buttons.dismiss -text Dismiss -command "destroy $w"
button $w.buttons.code -text "See Code" -command "showCode $w"
pack $w.buttons.dismiss $w.buttons.code -side left -expand 1

text $w.text -yscrollcommand "$w.scroll set" -setgrid true \
	-width 60 -height 24 -font $font -wrap word
scrollbar $w.scroll -command "$w.text yview"
pack $w.scroll -side right -fill y
pack $w.text -expand yes -fill both

# Set up display styles.

if {[winfo depth $w] > 1} {
    set bold "-background #43ce80 -relief raised -borderwidth 1"
    set normal "-background {} -relief flat"
} else {
    set bold "-foreground white -background black"
    set normal "-foreground {} -background {}"
}

# Add text to widget.

$w.text insert 0.0 {\
The same tag mechanism that controls display styles in text widgets can also be used to associate Tcl commands with regions of text, so that mouse or keyboard actions on the text cause particular Tcl commands to be invoked.  For example, in the text below the descriptions of the canvas demonstrations have been tagged.  When you move the mouse over a demo description the description lights up, and when you press button 1 over a description then that particular demonstration is invoked.

}
$w.text insert end \
{1. Samples of all the different types of items that can be created in canvas widgets.} d1
$w.text insert end \n\n
$w.text insert end \
{2. A simple two-dimensional plot that allows you to adjust the positions of the data points.} d2
$w.text insert end \n\n
$w.text insert end \
{3. Anchoring and justification modes for text items.} d3
$w.text insert end \n\n
$w.text insert end \
{4. An editor for arrow-head shapes for line items.} d4
$w.text insert end \n\n
$w.text insert end \
{5. A ruler with facilities for editing tab stops.} d5
$w.text insert end \n\n
$w.text insert end \
{6. A grid that demonstrates how canvases can be scrolled.} d6

# Create bindings for tags.

foreach tag {d1 d2 d3 d4 d5 d6} {
    $w.text tag bind $tag <Any-Enter> "$w.text tag configure $tag $bold"
    $w.text tag bind $tag <Any-Leave> "$w.text tag configure $tag $normal"
}
$w.text tag bind d1 <1> {source [file join $tk_library demos items.tcl]}
$w.text tag bind d2 <1> {source [file join $tk_library demos plot.tcl]}
$w.text tag bind d3 <1> {source [file join $tk_library demos ctext.tcl]}
$w.text tag bind d4 <1> {source [file join $tk_library demos arrow.tcl]}
$w.text tag bind d5 <1> {source [file join $tk_library demos ruler.tcl]}
$w.text tag bind d6 <1> {source [file join $tk_library demos cscroll.tcl]}

$w.text mark set insert 0.0
$w.text configure -state disabled
