# index.tcl --
#
# This file defines procedures that are used during the first pass of
# the man page conversion.  It is used to extract information used to
# generate a table of contents and a keyword list.
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# SCCS: @(#) index.tcl 1.2 96/10/17 14:44:16
# 

# Global variables used by these scripts:
#
# state -	state variable that controls action of text proc.
#				
# topics -	array indexed by (package,section,topic) with value
# 		of topic ID.
#
# keywords -	array indexed by keyword string with value of topic ID.
#
# curID - 	current topic ID, starts at 0 and is incremented for
# 		each new topic file.
#
# curPkg -	current package name (e.g. Tcl).
#
# curSect -	current section title (e.g. "Tcl Built-In Commands").
#

# getPackages --
#
# Generate a sorted list of package names from the topics array.
#
# Arguments:
# none.

proc getPackages {} {
    global topics
    foreach i [array names topics] {
	regsub {^(.*),.*,.*$} $i {\1} i
	set temp($i) {}
    }
    return [lsort [array names temp]]
}

# getSections --
#
# Generate a sorted list of section titles in the specified package
# from the topics array.
#
# Arguments:
# pkg -			Name of package to search.

proc getSections {pkg} {
    global topics
    foreach i [array names topics "${pkg},*"] {
	regsub {^.*,(.*),.*$} $i {\1} i
	set temp($i) {}
    }
    return [lsort [array names temp]]
}

# getSections --
#
# Generate a sorted list of topics in the specified section of the
# specified package from the topics array.
#
# Arguments:
# pkg -			Name of package to search.
# sect -		Name of section to search.

proc getTopics {pkg sect} {
    global topics

    # array names is stupid, so we do a little quoting hell to help it.
    regsub -all -- { } $sect {\ } sect
    regsub -all -- {\[} $sect {\[} sect
    regsub -all -- {\]} $sect {\]} sect

    foreach i [array names topics "${pkg},${sect},*"] {
	regsub {^.*,.*,(.*)$} $i {\1} i
	set temp($i) {}
    }
    return [lsort [array names temp]]
}

# text --
#
# This procedure adds entries to the hypertext arrays topics and keywords.
#
# Arguments:
# string -		Text to index.


proc text string {
    global state curID curPkg curSect topics keywords

    switch $state {
	NAME {
	    foreach i [split $string ","] {
		set topic [string trim $i]
		set index "$curPkg,$curSect,$topic"
		if {[info exists topics($index)]
		    && [string compare $topics($index) $curID] != 0} {
		    puts stderr "duplicate topic $topic in $curPkg"
		}
		set topics($index) $curID
		lappend keywords($topic) $curID
	    }
	}
	KEY {
	    foreach i [split $string ","] {
		lappend keywords([string trim $i]) $curID
	    }
	}
	DT -
	OFF -
	DASH {}
	default {
	    puts stderr "text: unknown state: $state"
	}
    }
}


# macro --
#
# This procedure is invoked to process macro invocations that start
# with "." (instead of ').
#
# Arguments:
# name -	The name of the macro (without the ".").
# args -	Any additional arguments to the macro.

proc macro {name args} {
    switch $name {
	SH {
	    global state

	    switch $args {
		NAME {
		    if {$state == "INIT" } {
			set state NAME
		    }
		}
		DESCRIPTION {set state DT}
		INTRODUCTION {set state DT}
		KEYWORDS {set state KEY}
		default {set state OFF}
	    }
	    
	}
        HS {
            # this is specific for [incr *] packages
	    global state curID curPkg curSect topics keywords
	    set state INIT
	    if {[llength $args] != 2} {
		set args [join $args " "]
		puts stderr "Bad .HS macro: .$name $args"
	    }
	    incr curID
            switch [lindex $args 1] {
              iwid {
                set curPkg "iwid"
                set curSect "\[incr Widgets\]"
              }
              default {
                set curPkg "unknown!"
                puts stderr "bad package name: [lindex $args 1]"
              }
            }
	    set topic	[lindex $args 0]	;# Tcl_UpVar
	    set index "$curPkg,$curSect,$topic"
	    set topics($index) $curID
	    lappend keywords($topic) $curID
	}
	TH {
	    global state curID curPkg curSect topics keywords
	    set state INIT
	    if {[llength $args] != 5} {
		set args [join $args " "]
		puts stderr "Bad .TH macro: .$name $args"
	    }
	    incr curID
	    set topic	[lindex $args 0]	;# Tcl_UpVar
	    set curPkg	[lindex $args 3]	;# Tcl
	    set curSect	[lindex $args 4]	;# {Tcl Library Procedures}
	    set index "$curPkg,$curSect,$topic"
	    set topics($index) $curID
	    lappend keywords($topic) $curID
	}
    }
}


# dash --
#
# This procedure is invoked to handle dash characters ("\-" in
# troff).  It only function in pass1 is to terminate the NAME state.
#
# Arguments:
# None.

proc dash {} {
    global state
    if {$state == "NAME"} {
	set state DASH
    }
}



# initGlobals, tab, font, char, macro2 --
#
# These procedures do nothing during the first pass. 
#
# Arguments:
# None.

proc initGlobals {} {}
proc newline {} {}
proc tab {} {}
proc font type {}
proc char name {}
proc macro2 {name args} {}

