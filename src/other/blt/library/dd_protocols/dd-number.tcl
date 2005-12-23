# ----------------------------------------------------------------------
#  PURPOSE:  drag&drop send routine for "number" data
#
#  Widgets that are to participate in drag&drop operations for
#  "number" data should be registered as follows:
#
#      drag&drop .win source handler number dd_send_number
#      drag&drop .win target handler number my_number_handler
#
#      proc my_number_handler {} {
#          global DragDrop
#
#          set data $DragDrop(number)
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
# COMMAND: dd_send_number <interp> <ddwin> <data>
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
#     Sends data to remote application DragDrop(number), and then
#     invokes the "number" handler for the drag&drop target.
# ----------------------------------------------------------------------
proc dd_send_number {interp ddwin data} {
	send $interp "
		foreach num [list $data] {
			expr \$num*1
		}
		global DragDrop
		set DragDrop(number) [list $data]
    "
	send $interp "drag&drop target $ddwin handle number"
}
