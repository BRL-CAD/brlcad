#
#   g e o m e t r e e . t c l 
#
# Author -
# 	Christopher Sean Morrison
#
# Source -
#		The U.S. Army Research Laboratory
#		Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
# 	Redistribution of this software is restricted, as described in your
# 	"Statement of Terms and Conditions for the Release of The BRL-CAD
# 	Package" agreement.
#
# Description -
#
# This is the GeometryBrowser main script.  It manages the loading and
# unloading of a GeometryBrowser object into mged automatically.
#
# The development version codename is "geometree"
# The final version will be "geometree" :-)
#

#
# Extend Autopath
#
if { [info exists ::env(BRLCAD_ROOT)] } {
    set rootDir $::env(BRLCAD_ROOT)
} else {
    set rootDir [file join / usr brlcad ]
} 
set geometreeDir [ file join $rootDir tclscripts geometree ]

lappend auto_path $geometreeDir

package require GeometryBrowser

# All GeometryBrowser stuff is in the GeometryBrowser namespace
proc geometree { } {
	global mged_gui
	global tkPriv
	global mged_players

	# determine the framebuffer window id
	if { [ catch { set mged_players } _mgedFramebufferId ] } {
		puts $_mgedFramebufferId
		puts "assuming default mged framebuffer id: id_0"
		set _mgedFramebufferId "id_0"
	}
	# just in case there are more than one returned
	set _mgedFramebufferId [ lindex $_mgedFramebufferId 0 ]

	set gt .$_mgedFramebufferId.geometree

	# see if the window is already open.  If so, just raise it up.
	if [ winfo exists $gt ] {
		raise $gt
		return
	}

	GeometryBrowser $gt
}
