# ----------------------------------------------------------------------
#  PURPOSE:  drag&drop send routine for "text" data
#
#  Widgets that are to participate in drag&drop operations for
#  "text" data should be registered as follows:
#
#      drag&drop .win source handler text dd_send_text
#      drag&drop .win target handler text my_text_handler
#
#      proc my_text_handler {} {
#          global DragDrop
#
#          set data $DragDrop(text)
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
# COMMAND: dd_send_text <interp> <ddwin> <data>
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
#     Sends data to remote application DragDrop(text), and then
#     invokes the "text" handler for the drag&drop target.
# ----------------------------------------------------------------------
proc dd_send_text {interp ddwin data} {
	send $interp "
		global DragDrop
		set DragDrop(text) [list $data]
	"
	send $interp "drag&drop target $ddwin handle text"
}
