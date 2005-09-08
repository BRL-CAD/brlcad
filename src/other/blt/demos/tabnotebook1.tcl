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
source scripts/demo.tcl

# Create a tabnotebook widget.  

tabnotebook .tnb

# The notebook is initially empty.  Insert tabs (pages) into the notebook.  

foreach label { First Second Third Fourth } {
    .tnb insert end -text $label
}

# Tabs are referred to by their index.  Tab indices can be one of the 
# following:
#
#	 number		Position of tab the notebook's list of tabs.
# 	 @x,y		Tab closest to the specified X-Y screen coordinates.
# 	 "active"	Tab currently under the mouse pointer.
# 	 "focus"	Tab that has focus.  
# 	 "select"	The currently selected tab.
# 	 "right"	Next tab from "focus".
# 	 "left"		Previous tab from "focus".
# 	 "up"		Next tab from "focus".
# 	 "down"		Previous tab from "focus".
# 	 "end"		Last tab in list.
#	 string		Tab identifier.  The "insert" operation returns 
#			a unique identifier for the new tab (e.g. "tab0").  
#			This ID is valid for the life of the tab, even if
#			the tabs are moved or reordered.  

# Each tab has a text label and an optional Tk image.

set image [image create photo -file ./images/mini-book1.gif]
.tnb tab configure 0 -image $image

#
# How to embed a widget into a page.  
#

# 1. The widget must be a child of the tabnotebook.

set image [image create photo -file ./images/blt98.gif]
label .tnb.label -image $image -relief sunken -bd 2

# 2. Use the -window option to embed the widget.

.tnb tab configure 0 -window .tnb.label

# The tearoff perforation, displayed on the selected tab, is
# controlled by the tabnotebook's -tearoff option.  
#
# If you don't want tearoff pages, configure -tearoff to "no".

.tnb configure -tearoff yes

table . \
    0,0 .tnb -fill both 

focus .tnb

