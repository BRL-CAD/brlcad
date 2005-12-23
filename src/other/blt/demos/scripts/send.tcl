

# --------------------------------------------------------------------------
#
# SendInit --
#	
#	Creates a "send" proc to replace the former Tk send command.
#	Uses DDE services to simulate the transfer.  This must be
#	called before any drag&drop targets are registered. Otherwise
#	they will pick up the wrong application name.
#
#	The first trick is to determine a unique application name. This
#	is what other applications will use to send to us.  Tk used to
#	do this for us.  
#	
#	Note that we can generate the same name for two different Tk 
#	applications.  This can happen if two Tk applications picking
#	names at exactly the same time.   [In the future, we should 
#	probably generate a name based upon a global system value, such
#	as the handle of the main window ".".]   The proc "SendVerify"
#	below will verify that you have only one DDE server registered
#	with this application's name.
#	
# Arguments:
#	myInterp	Sets the application name explicitly to this 
#			string. If the argument isn't given, or is the
#			empty string, then the routine picks a name for
#			us.
#
# Results:
#	Returns the name of the application.
#
# Side Effects:
#	Sets the name of our application.  You can call "tk appname" to
#	get the name.  A DDE topic using the same name is also created.
#	A send proc is also automatically created.  Be careful that you
#	don't overwrite an existing send command.
#
# --------------------------------------------------------------------------

proc SendInit { {myInterp ""} } {

    # Load the DDE package.
    package require dde

    if { $myInterp == "" } {

	# Pick a unique application name, replicating what Tk used to do.
	# This is what other applications will use to "send" to us. We'll
	# use DDE topics to represent interpreters.

	set appName [tk appname]
	set count 0
	set suffix {}
	
	# Keep generating interpreter names by suffix-ing the original
	# application name with " #number".  Sooner of later we'll find
	# one that's not currently use. 

	while { 1 } {
	    set myInterp "${appName}${suffix}"
	    set myServer [list TclEval $myInterp]
	    if { [lsearch [dde services TclEval {}] $myServer] < 0 } {
		break
	    }
	    incr count
	    set suffix " \#$count"
	}
    }
    tk appname $myInterp
    dde servername $myInterp
    proc send { interp args } {
	dde eval $interp $args
    }
    return $myInterp
}


# --------------------------------------------------------------------------
#
# SendVerify --
#	
#	Verifies that application name picked is uniquely registered
#	as a DDE server.  This checks that two Tk applications don't
#	accidently use the same name.
#
# Arguments:
#	None		Used the current application name.
#
# Results:
#	Generates an error if either a server can't be found or more
#	than one server is registered.
#
# --------------------------------------------------------------------------

proc SendVerify {} {
    # Load the DDE package.
    package require dde

    set count 0
    set appName [tk appname]
    foreach server [dde services TclEval {}] {
	set topic [lindex $server 1]
	if { [string compare $topic $appName] == 0 } {
	    incr count
	}
    }
    if {$count == 0} {
	error "Service not found: wrong name registered???"
    } 
    if { $count > 1 } {
	error "Duplicate names found for \"[tk appname]\""
    } 
}

