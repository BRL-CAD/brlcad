# ----------------------------------------------------------------------
#  PURPOSE:  drag&drop send routine for "file" data
#
#  Widgets that are to participate in drag&drop operations for
#  "file" data should be registered as follows:
#
#      drag&drop .win source handler text dd_send_file
#      drag&drop .win target handler text my_file_handler
#
#      proc my_file_handler {} {
#          global DragDrop
#
#          set data $DragDrop(file)
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
# COMMAND: dd_send_file <interp> <ddwin> <data>
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
#     Sends data to remote application DragDrop(file), and then
#     invokes the "file" handler for the drag&drop target.
# ----------------------------------------------------------------------
proc dd_send_file {interp ddwin data} {
	send $interp "
		foreach file [list $data] {
			if {!\[file exists \$file\]} {
				error \"not a file: \$file\"
			}
		}
		global DragDrop
		set DragDrop(file) [list $data]
	"
	send $interp "drag&drop target $ddwin handle file"
}
