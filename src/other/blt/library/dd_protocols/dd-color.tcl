# ----------------------------------------------------------------------
#  PURPOSE:  drag&drop send routine for "color" data
#
#  Widgets that are to participate in drag&drop operations for
#  "color" data should be registered as follows:
#
#      drag&drop .win source handler color dd_send_color
#      drag&drop .win target handler color my_color_handler
#
#      proc my_color_handler {} {
#          global DragDrop
#
#          set data $DragDrop(color)
#            .
#            .  do something with $data
#            .
#      }
#
#   AUTHOR:  Michael J. McLennan       Phone: (215)770-2842
#            AT&T Bell Laboratories   E-mail: aluxpo!mmc@att.com
#
#     SCCS:  %W% (%G%)
# ----------------------------------------------------------------------
#            Copyright (c) 1993  AT&T  All Rights Reserved
# ======================================================================

# ----------------------------------------------------------------------
# COMMAND: dd_send_color <interp> <ddwin> <data>
#
#   INPUTS
#     <interp> = interpreter for target application
#      <ddwin> = pathname for target drag&drop window
#       <data> = data returned from -tokencmd
#
#   RETURNS
#     ""
#
#   SIDE-EFFECTS
#     Sends data to remote application DragDrop(color), and then
#     invokes the "color" handler for the drag&drop target.
# ----------------------------------------------------------------------
proc dd_send_color {interp ddwin data} {
	send $interp "
		foreach color [list $data] {
			winfo rgb . \$color
		}
		global DragDrop
		set DragDrop(color) [list $data]
	"
	send $interp "drag&drop target $ddwin handle color"
}
