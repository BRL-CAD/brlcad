#
#			C H E C K _ E X T E R N S
#
#	Ensure that all commands that the caller uses but doesn't define
#	are provided by the application
#
proc check_externs {extern_list} {
    set unsat 0
    set s [info script]
    upvar #0 argv0 app
    set msg ""
    foreach cmd $extern_list {
	if {[info command $cmd] == ""} {
	    # append this string once
	    if {$msg == ""} {
		if {[info exists app]} {
		    append msg "Application '$app' unsuited to use Tcl script '$s':\n"
		} else {
		    append msg "Application unsuited to use Tcl script '$s':\n"
		}

		append msg " Fails to define the following commands: $cmd"
		continue
	    }
	    append msg ", $cmd"
	}
    }
    if {$msg != ""} {
	error $msg
    }
}

#
#			D E L E T E _ P R O C S
#
#		    Silently delete Tcl procedures
#
#	Delete each of the specified procedures that exist.
#	This guard is necessary since Tcl's rename command
#	squawks whenever its argument is not the name of
#	an existing procedure.
#
proc delete_procs {proc_list} {
    foreach proc_name $proc_list {
	if {[string length [info command $proc_name]] != 0} {
	    rename $proc_name {}
	}
    }
}
