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
    foreach cmd $extern_list {
	if {[string length [info command $cmd]] == 0} {
	    puts stderr "Application '$app' unsuited to use Tcl script '$s':"
	    puts stderr " Fails to define command '$cmd'"
	    set unsat 1
	}
    }
    if {$unsat == 1} {
	puts "Tcl script '$s' aborting"
	return -code return
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
