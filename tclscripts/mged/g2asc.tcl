#
#			G 2 A S C . T C L
#
#	Tool for saving the current database to an ascii file.
#
#	Author - Robert G. Parker
#

proc init_g2asc { id } {
    global player_screen

    set top .$id.ascii
    catch { destroy $top }

    set db_name [_mged_opendb]
    set default_name [file rootname $db_name].asc
    set ret [mged_input_dialog $top $player_screen($id) "Save as Ascii"\
	    "Enter ascii filename:" ascii_filename $default_name 0 OK CANCEL]

    if { $ascii_filename != "" } {
	if { $ret == 0 } {
	    if [file exists $ascii_filename] {
		set result [mged_dialog $top $player_screen($id)\
			"Overwrite $ascii_filename?"\
			"Overwrite $ascii_filename?"\
			"" 0 OK CANCEL]

		if { $result } {
		    return
		}
	    }

	    catch {exec g2asc < $db_name > $ascii_filename} msg
	}
    } else {
	if { $ret == 0 } {
	    mged_dialog $top $player_screen($id)\
		    "No file name specified!"\
		    "No file name specified!"\
		    "" 0 OK
	}
    }
}
