# safetk.tcl --
#
# Support procs to use Tk in safe interpreters.
#
# SCCS: @(#) safetk.tcl 1.8 97/10/29 14:59:16
#
# Copyright (c) 1997 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

# see safetk.n for documentation

#
#
# Note: It is UNSAFE to let any untrusted code being executed
#       between the creation of the interp and the actual loading
#       of Tk in that interp.
#       You should "loadTk $slave" right after safe::tkInterpCreate
#       Otherwise, if you are using an application with Tk
#       and don't want safe slaves to have access to Tk, potentially
#       in a malevolent way, you should use 
#            ::safe::interpCreate -nostatics -accesspath {directories...}
#       where the directory list does NOT contain any Tk dynamically
#       loadable library
#

# We use opt (optional arguments parsing)
package require opt 0.1;

namespace eval ::safe {

    # counter for safe toplevels
    variable tkSafeId 0;

    #
    # tkInterpInit : prepare the slave interpreter for tk loading
    #
    # returns the slave name (tkInterpInit does)
    #
    proc ::safe::tkInterpInit {slave} {
	global env tk_library
	if {[info exists env(DISPLAY)]} {
	    $slave eval [list set env(DISPLAY) $env(DISPLAY)];
	}
	# there seems to be an obscure case where the tk_library
	# variable value is changed to point to a sym link destination
	# dir instead of the sym link itself, and thus where the $tk_library
	# would then not be anymore one of the auto_path dir, so we use
	# the addToAccessPath which adds if it's not already in instead
	# of the more conventional findInAccessPath
	::interp eval $slave [list set tk_library [::safe::interpAddToAccessPath $slave $tk_library]]
	return $slave;
    }


# tkInterpLoadTk : 
# Do additional configuration as needed (calling tkInterpInit) 
# and actually load Tk into the slave.
# 
# Either contained in the specified windowId (-use) or
# creating a decorated toplevel for it.

# empty definition for auto_mkIndex
proc ::safe::loadTk {} {}
   
    ::tcl::OptProc loadTk {
	{slave -interp "name of the slave interpreter"}
	{-use  -windowId {} "window Id to use (new toplevel otherwise)"}
    } {
	if {![::tcl::OptProcArgGiven "-use"]} {
	    # create a decorated toplevel
	    ::tcl::Lassign [tkTopLevel $slave] w use;
	    # set our delete hook (slave arg is added by interpDelete)
	    Set [DeleteHookName $slave] [list tkDelete {} $w];
	}
	tkInterpInit $slave;
	::interp eval $slave [list set argv [list "-use" $use]];
	::interp eval $slave [list set argc 2];
	load {} Tk $slave
	# Remove env(DISPLAY) if it's in there (if it has been set by
	# tkInterpInit)
	::interp eval $slave {catch {unset env(DISPLAY)}}
	return $slave
    }

    proc ::safe::tkDelete {W window slave} {
	# we are going to be called for each widget... skip untill it's
	# top level
	Log $slave "Called tkDelete $W $window" NOTICE;
	if {[::interp exists $slave]} {
	    if {[catch {::safe::interpDelete $slave} msg]} {
		Log $slave "Deletion error : $msg";
	    }
	}
	if {[winfo exists $window]} {
	    Log $slave "Destroy toplevel $window" NOTICE;
	    destroy $window;
	}
    }

proc ::safe::tkTopLevel {slave} {
    variable tkSafeId;
    incr tkSafeId;
    set w ".safe$tkSafeId";
    if {[catch {toplevel $w -class SafeTk} msg]} {
	return -code error "Unable to create toplevel for\
		safe slave \"$slave\" ($msg)";
    }
    Log $slave "New toplevel $w" NOTICE

    set msg "Untrusted Tcl applet ($slave)"
    wm title $w $msg;

    # Control frame
    set wc $w.fc
    frame $wc -bg red -borderwidth 3 -relief ridge ;

    # We will destroy the interp when the window is destroyed
    bindtags $wc [concat Safe$wc [bindtags $wc]]
    bind Safe$wc <Destroy> [list ::safe::tkDelete %W $w $slave];

    label $wc.l -text $msg \
	    -padx 2 -pady 0 -anchor w;

    # We want the button to be the last visible item
    # (so be packed first) and at the right and not resizing horizontally

    # frame the button so it does not expand horizontally
    # but still have the default background instead of red one from the parent
    frame  $wc.fb -bd 0 ;
    button $wc.fb.b -text "Delete" \
	    -bd 1  -padx 2 -pady 0 -highlightthickness 0 \
	    -command [list ::safe::tkDelete $w $w $slave]
    pack $wc.fb.b -side right -fill both ;
    pack $wc.fb -side right -fill both -expand 1;
    pack $wc.l -side left  -fill both -expand 1;
    pack $wc -side bottom -fill x ;

    # Container frame
    frame $w.c -container 1;
    pack $w.c -fill both -expand 1;
    
    # return both the toplevel window name and the id to use for embedding
    list $w [winfo id $w.c] ;
}

}
