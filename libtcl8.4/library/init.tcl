# init.tcl --
#
# Default system startup file for Tcl-based applications.  Defines
# "unknown" procedure and auto-load facilities.
#
# RCS: @(#) $Id$
#
# Copyright (c) 1991-1993 The Regents of the University of California.
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
# Copyright (c) 1998-1999 Scriptics Corporation.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

if {[info commands package] == ""} {
    error "version mismatch: library\nscripts expect Tcl version 7.5b1 or later but the loaded version is\nonly [info patchlevel]"
}
package require -exact Tcl 8.4

# Compute the auto path to use in this interpreter.
# The values on the path come from several locations:
#
# The environment variable TCLLIBPATH
#
# tcl_library, which is the directory containing this init.tcl script.
# tclInitScript.h searches around for the directory containing this
# init.tcl and defines tcl_library to that location before sourcing it.
#
# The parent directory of tcl_library. Adding the parent
# means that packages in peer directories will be found automatically.
#
# Also add the directory ../lib relative to the directory where the
# executable is located.  This is meant to find binary packages for the
# same architecture as the current executable.
#
# tcl_pkgPath, which is set by the platform-specific initialization routines
#	On UNIX it is compiled in
#       On Windows, it is not used
#	On Macintosh it is "Tool Command Language" in the Extensions folder

if {![info exists auto_path]} {
    if {[info exists env(TCLLIBPATH)]} {
	set auto_path $env(TCLLIBPATH)
    } else {
	set auto_path ""
    }
}
namespace eval tcl {
    variable Dir
    if {[info library] != ""} {
	foreach Dir [list [info library] [file dirname [info library]]] {
	    if {[lsearch -exact $::auto_path $Dir] < 0} {
		lappend ::auto_path $Dir
	    }
	}
    }
    set Dir [file join [file dirname [file dirname \
	    [info nameofexecutable]]] lib]
    if {[lsearch -exact $::auto_path $Dir] < 0} {
	lappend ::auto_path $Dir
    }
    if {[info exists ::tcl_pkgPath]} {
	foreach Dir $::tcl_pkgPath {
	    if {[lsearch -exact $::auto_path $Dir] < 0} {
		lappend ::auto_path $Dir
	    }
	}
    }
}
  
# Windows specific end of initialization

if {(![interp issafe]) && [string equal $tcl_platform(platform) "windows"]} {
    namespace eval tcl {
	proc EnvTraceProc {lo n1 n2 op} {
	    set x $::env($n2)
	    set ::env($lo) $x
	    set ::env([string toupper $lo]) $x
	}
	proc InitWinEnv {} {
	    global env tcl_platform
	    foreach p [array names env] {
		set u [string toupper $p]
		if {![string equal $u $p]} {
		    switch -- $u {
			COMSPEC -
			PATH {
			    if {![info exists env($u)]} {
				set env($u) $env($p)
			    }
			    trace variable env($p) w \
				    [namespace code [list EnvTraceProc $p]]
			    trace variable env($u) w \
				    [namespace code [list EnvTraceProc $p]]
			}
		    }
		}
	    }
	    if {![info exists env(COMSPEC)]} {
		if {[string equal $tcl_platform(os) "Windows NT"]} {
		    set env(COMSPEC) cmd.exe
		} else {
		    set env(COMSPEC) command.com
		}
	    }
	}
	InitWinEnv
    }
}

# Setup the unknown package handler

package unknown tclPkgUnknown

if {![interp issafe]} {
    # setup platform specific unknown package handlers
    if {[string equal $::tcl_platform(platform) "unix"] && \
	    [string equal $::tcl_platform(os) "Darwin"]} {
	package unknown [list tcl::MacOSXPkgUnknown [package unknown]]
    }
    if {[string equal $::tcl_platform(platform) "macintosh"]} {
	package unknown [list tcl::MacPkgUnknown [package unknown]]
    }
}

# Conditionalize for presence of exec.

if {[llength [info commands exec]] == 0} {

    # Some machines, such as the Macintosh, do not have exec. Also, on all
    # platforms, safe interpreters do not have exec.

    set auto_noexec 1
}
set errorCode ""
set errorInfo ""

# Define a log command (which can be overwitten to log errors
# differently, specially when stderr is not available)

if {[llength [info commands tclLog]] == 0} {
    proc tclLog {string} {
	catch {puts stderr $string}
    }
}

# unknown --
# This procedure is called when a Tcl command is invoked that doesn't
# exist in the interpreter.  It takes the following steps to make the
# command available:
#
#	1. See if the command has the form "namespace inscope ns cmd" and
#	   if so, concatenate its arguments onto the end and evaluate it.
#	2. See if the autoload facility can locate the command in a
#	   Tcl script file.  If so, load it and execute it.
#	3. If the command was invoked interactively at top-level:
#	    (a) see if the command exists as an executable UNIX program.
#		If so, "exec" the command.
#	    (b) see if the command requests csh-like history substitution
#		in one of the common forms !!, !<number>, or ^old^new.  If
#		so, emulate csh's history substitution.
#	    (c) see if the command is a unique abbreviation for another
#		command.  If so, invoke the command.
#
# Arguments:
# args -	A list whose elements are the words of the original
#		command, including the command name.

proc unknown args {
    global auto_noexec auto_noload env unknown_pending tcl_interactive
    global errorCode errorInfo

    # If the command word has the form "namespace inscope ns cmd"
    # then concatenate its arguments onto the end and evaluate it.

    set cmd [lindex $args 0]
    if {[regexp "^:*namespace\[ \t\n\]+inscope" $cmd] && [llength $cmd] == 4} {
        set arglist [lrange $args 1 end]
	set ret [catch {uplevel 1 ::$cmd $arglist} result]
        if {$ret == 0} {
            return $result
        } else {
	    return -code $ret -errorcode $errorCode $result
        }
    }

    # Save the values of errorCode and errorInfo variables, since they
    # may get modified if caught errors occur below.  The variables will
    # be restored just before re-executing the missing command.

    set savedErrorCode $errorCode
    set savedErrorInfo $errorInfo
    set name [lindex $args 0]
    if {![info exists auto_noload]} {
	#
	# Make sure we're not trying to load the same proc twice.
	#
	if {[info exists unknown_pending($name)]} {
	    return -code error "self-referential recursion in \"unknown\" for command \"$name\"";
	}
	set unknown_pending($name) pending;
	set ret [catch {auto_load $name [uplevel 1 {::namespace current}]} msg]
	unset unknown_pending($name);
	if {$ret != 0} {
	    append errorInfo "\n    (autoloading \"$name\")"
	    return -code $ret -errorcode $errorCode -errorinfo $errorInfo $msg
	}
	if {![array size unknown_pending]} {
	    unset unknown_pending
	}
	if {$msg} {
	    set errorCode $savedErrorCode
	    set errorInfo $savedErrorInfo
	    set code [catch {uplevel 1 $args} msg]
	    if {$code ==  1} {
		#
		# Compute stack trace contribution from the [uplevel].
		# Note the dependence on how Tcl_AddErrorInfo, etc. 
		# construct the stack trace.
		#
		set cinfo $args
		set ellipsis ""
		while {[string bytelength $cinfo] > 150} {
		    set cinfo [string range $cinfo 0 end-1]
		    set ellipsis "..."
		}
		append cinfo $ellipsis "\"\n    (\"uplevel\" body line 1)"
		append cinfo "\n    invoked from within"
		append cinfo "\n\"uplevel 1 \$args\""
		#
		# Try each possible form of the stack trace
		# and trim the extra contribution from the matching case
		#
		set expect "$msg\n    while executing\n\"$cinfo"
		if {$errorInfo eq $expect} {
		    #
		    # The stack has only the eval from the expanded command
		    # Do not generate any stack trace here.
		    #
		    return -code error -errorcode $errorCode $msg
		}
		#
		# Stack trace is nested, trim off just the contribution
		# from the extra "eval" of $args due to the "catch" above.
		#
		set expect "\n    invoked from within\n\"$cinfo"
		set exlen [string length $expect]
		set eilen [string length $errorInfo]
		set i [expr {$eilen - $exlen - 1}]
		set einfo [string range $errorInfo 0 $i]
		#
		# For now verify that $errorInfo consists of what we are about
		# to return plus what we expected to trim off.
		#
		if {$errorInfo ne "$einfo$expect"} {
		    error "Tcl bug: unexpected stack trace in \"unknown\"" {} \
			[list CORE UNKNOWN BADTRACE $expect $errorInfo]
		}
		return -code error -errorcode $errorCode \
			-errorinfo $einfo $msg
	    } else {
		return -code $code $msg
	    }
	}
    }

    if {([info level] == 1) && [string equal [info script] ""] \
	    && [info exists tcl_interactive] && $tcl_interactive} {
	if {![info exists auto_noexec]} {
	    set new [auto_execok $name]
	    if {$new != ""} {
		set errorCode $savedErrorCode
		set errorInfo $savedErrorInfo
		set redir ""
		if {[string equal [info commands console] ""]} {
		    set redir ">&@stdout <@stdin"
		}
		return [uplevel 1 exec $redir $new [lrange $args 1 end]]
	    }
	}
	set errorCode $savedErrorCode
	set errorInfo $savedErrorInfo
	if {[string equal $name "!!"]} {
	    set newcmd [history event]
	} elseif {[regexp {^!(.+)$} $name dummy event]} {
	    set newcmd [history event $event]
	} elseif {[regexp {^\^([^^]*)\^([^^]*)\^?$} $name dummy old new]} {
	    set newcmd [history event -1]
	    catch {regsub -all -- $old $newcmd $new newcmd}
	}
	if {[info exists newcmd]} {
	    tclLog $newcmd
	    history change $newcmd 0
	    return [uplevel 1 $newcmd]
	}

	set ret [catch {set cmds [info commands $name*]} msg]
	if {[string equal $name "::"]} {
	    set name ""
	}
	if {$ret != 0} {
	    return -code $ret -errorcode $errorCode \
		"error in unknown while checking if \"$name\" is a unique command abbreviation: $msg"
	}
	if {[llength $cmds] == 1} {
	    return [uplevel 1 [lreplace $args 0 0 $cmds]]
	}
	if {[llength $cmds]} {
	    if {[string equal $name ""]} {
		return -code error "empty command name \"\""
	    } else {
		return -code error \
			"ambiguous command name \"$name\": [lsort $cmds]"
	    }
	}
    }
    return -code error "invalid command name \"$name\""
}

# auto_load --
# Checks a collection of library directories to see if a procedure
# is defined in one of them.  If so, it sources the appropriate
# library file to create the procedure.  Returns 1 if it successfully
# loaded the procedure, 0 otherwise.
#
# Arguments: 
# cmd -			Name of the command to find and load.
# namespace (optional)  The namespace where the command is being used - must be
#                       a canonical namespace as returned [namespace current]
#                       for instance. If not given, namespace current is used.

proc auto_load {cmd {namespace {}}} {
    global auto_index auto_oldpath auto_path

    if {[string length $namespace] == 0} {
	set namespace [uplevel 1 [list ::namespace current]]
    }
    set nameList [auto_qualify $cmd $namespace]
    # workaround non canonical auto_index entries that might be around
    # from older auto_mkindex versions
    lappend nameList $cmd
    foreach name $nameList {
	if {[info exists auto_index($name)]} {
	    namespace eval :: $auto_index($name)
	    # There's a couple of ways to look for a command of a given
	    # name.  One is to use
	    #    info commands $name
	    # Unfortunately, if the name has glob-magic chars in it like *
	    # or [], it may not match.  For our purposes here, a better
	    # route is to use 
	    #    namespace which -command $name
	    if {[namespace which -command $name] ne ""} {
		return 1
	    }
	}
    }
    if {![info exists auto_path]} {
	return 0
    }

    if {![auto_load_index]} {
	return 0
    }
    foreach name $nameList {
	if {[info exists auto_index($name)]} {
	    namespace eval :: $auto_index($name)
	    if {[namespace which -command $name] ne ""} {
		return 1
	    }
	}
    }
    return 0
}

# auto_load_index --
# Loads the contents of tclIndex files on the auto_path directory
# list.  This is usually invoked within auto_load to load the index
# of available commands.  Returns 1 if the index is loaded, and 0 if
# the index is already loaded and up to date.
#
# Arguments: 
# None.

proc auto_load_index {} {
    global auto_index auto_oldpath auto_path errorInfo errorCode

    if {[info exists auto_oldpath] && \
	    [string equal $auto_oldpath $auto_path]} {
	return 0
    }
    set auto_oldpath $auto_path

    # Check if we are a safe interpreter. In that case, we support only
    # newer format tclIndex files.

    set issafe [interp issafe]
    for {set i [expr {[llength $auto_path] - 1}]} {$i >= 0} {incr i -1} {
	set dir [lindex $auto_path $i]
	set f ""
	if {$issafe} {
	    catch {source [file join $dir tclIndex]}
	} elseif {[catch {set f [open [file join $dir tclIndex]]}]} {
	    continue
	} else {
	    set error [catch {
		set id [gets $f]
		if {[string equal $id \
			"# Tcl autoload index file, version 2.0"]} {
		    eval [read $f]
		} elseif {[string equal $id "# Tcl autoload index file: each line identifies a Tcl"]} {
		    while {[gets $f line] >= 0} {
			if {[string equal [string index $line 0] "#"] \
				|| ([llength $line] != 2)} {
			    continue
			}
			set name [lindex $line 0]
			set auto_index($name) \
				"source [file join $dir [lindex $line 1]]"
		    }
		} else {
		    error "[file join $dir tclIndex] isn't a proper Tcl index file"
		}
	    } msg]
	    if {$f != ""} {
		close $f
	    }
	    if {$error} {
		error $msg $errorInfo $errorCode
	    }
	}
    }
    return 1
}

# auto_qualify --
#
# Compute a fully qualified names list for use in the auto_index array.
# For historical reasons, commands in the global namespace do not have leading
# :: in the index key. The list has two elements when the command name is
# relative (no leading ::) and the namespace is not the global one. Otherwise
# only one name is returned (and searched in the auto_index).
#
# Arguments -
# cmd		The command name. Can be any name accepted for command
#               invocations (Like "foo::::bar").
# namespace	The namespace where the command is being used - must be
#               a canonical namespace as returned by [namespace current]
#               for instance.

proc auto_qualify {cmd namespace} {

    # count separators and clean them up
    # (making sure that foo:::::bar will be treated as foo::bar)
    set n [regsub -all {::+} $cmd :: cmd]

    # Ignore namespace if the name starts with ::
    # Handle special case of only leading ::

    # Before each return case we give an example of which category it is
    # with the following form :
    # ( inputCmd, inputNameSpace) -> output

    if {[regexp {^::(.*)$} $cmd x tail]} {
	if {$n > 1} {
	    # ( ::foo::bar , * ) -> ::foo::bar
	    return [list $cmd]
	} else {
	    # ( ::global , * ) -> global
	    return [list $tail]
	}
    }
    
    # Potentially returning 2 elements to try  :
    # (if the current namespace is not the global one)

    if {$n == 0} {
	if {[string equal $namespace ::]} {
	    # ( nocolons , :: ) -> nocolons
	    return [list $cmd]
	} else {
	    # ( nocolons , ::sub ) -> ::sub::nocolons nocolons
	    return [list ${namespace}::$cmd $cmd]
	}
    } elseif {[string equal $namespace ::]} {
	#  ( foo::bar , :: ) -> ::foo::bar
	return [list ::$cmd]
    } else {
	# ( foo::bar , ::sub ) -> ::sub::foo::bar ::foo::bar
	return [list ${namespace}::$cmd ::$cmd]
    }
}

# auto_import --
#
# Invoked during "namespace import" to make see if the imported commands
# reside in an autoloaded library.  If so, the commands are loaded so
# that they will be available for the import links.  If not, then this
# procedure does nothing.
#
# Arguments -
# pattern	The pattern of commands being imported (like "foo::*")
#               a canonical namespace as returned by [namespace current]

proc auto_import {pattern} {
    global auto_index

    # If no namespace is specified, this will be an error case

    if {![string match *::* $pattern]} {
	return
    }

    set ns [uplevel 1 [list ::namespace current]]
    set patternList [auto_qualify $pattern $ns]

    auto_load_index

    foreach pattern $patternList {
        foreach name [array names auto_index $pattern] {
            if {([namespace which -command $name] eq "")
		    && ([namespace qualifiers $pattern] eq [namespace qualifiers $name])} {
                namespace eval :: $auto_index($name)
            }
        }
    }
}

# auto_execok --
#
# Returns string that indicates name of program to execute if 
# name corresponds to a shell builtin or an executable in the
# Windows search path, or "" otherwise.  Builds an associative 
# array auto_execs that caches information about previous checks, 
# for speed.
#
# Arguments: 
# name -			Name of a command.

if {[string equal windows $tcl_platform(platform)]} {
# Windows version.
#
# Note that info executable doesn't work under Windows, so we have to
# look for files with .exe, .com, or .bat extensions.  Also, the path
# may be in the Path or PATH environment variables, and path
# components are separated with semicolons, not colons as under Unix.
#
proc auto_execok name {
    global auto_execs env tcl_platform

    if {[info exists auto_execs($name)]} {
	return $auto_execs($name)
    }
    set auto_execs($name) ""

    set shellBuiltins [list cls copy date del erase dir echo mkdir \
	    md rename ren rmdir rd time type ver vol]
    if {[string equal $tcl_platform(os) "Windows NT"]} {
	# NT includes the 'start' built-in
	lappend shellBuiltins "start"
    }
    if {[info exists env(PATHEXT)]} {
	# Add an initial ; to have the {} extension check first.
	set execExtensions [split ";$env(PATHEXT)" ";"]
    } else {
	set execExtensions [list {} .com .exe .bat]
    }

    if {[lsearch -exact $shellBuiltins $name] != -1} {
	# When this is command.com for some reason on Win2K, Tcl won't
	# exec it unless the case is right, which this corrects.  COMSPEC
	# may not point to a real file, so do the check.
	set cmd $env(COMSPEC)
	if {[file exists $cmd]} {
	    set cmd [file attributes $cmd -shortname]
	}
	return [set auto_execs($name) [list $cmd /c $name]]
    }

    if {[llength [file split $name]] != 1} {
	foreach ext $execExtensions {
	    set file ${name}${ext}
	    if {[file exists $file] && ![file isdirectory $file]} {
		return [set auto_execs($name) [list $file]]
	    }
	}
	return ""
    }

    set path "[file dirname [info nameof]];.;"
    if {[info exists env(WINDIR)]} {
	set windir $env(WINDIR) 
    }
    if {[info exists windir]} {
	if {[string equal $tcl_platform(os) "Windows NT"]} {
	    append path "$windir/system32;"
	}
	append path "$windir/system;$windir;"
    }

    foreach var {PATH Path path} {
	if {[info exists env($var)]} {
	    append path ";$env($var)"
	}
    }

    foreach dir [split $path {;}] {
	# Skip already checked directories
	if {[info exists checked($dir)] || [string equal {} $dir]} { continue }
	set checked($dir) {}
	foreach ext $execExtensions {
	    set file [file join $dir ${name}${ext}]
	    if {[file exists $file] && ![file isdirectory $file]} {
		return [set auto_execs($name) [list $file]]
	    }
	}
    }
    return ""
}

} else {
# Unix version.
#
proc auto_execok name {
    global auto_execs env

    if {[info exists auto_execs($name)]} {
	return $auto_execs($name)
    }
    set auto_execs($name) ""
    if {[llength [file split $name]] != 1} {
	if {[file executable $name] && ![file isdirectory $name]} {
	    set auto_execs($name) [list $name]
	}
	return $auto_execs($name)
    }
    foreach dir [split $env(PATH) :] {
	if {[string equal $dir ""]} {
	    set dir .
	}
	set file [file join $dir $name]
	if {[file executable $file] && ![file isdirectory $file]} {
	    set auto_execs($name) [list $file]
	    return $auto_execs($name)
	}
    }
    return ""
}

}

# ::tcl::CopyDirectory --
#
# This procedure is called by Tcl's core when attempts to call the
# filesystem's copydirectory function fail.  The semantics of the call
# are that 'dest' does not yet exist, i.e. dest should become the exact
# image of src.  If dest does exist, we throw an error.  
# 
# Note that making changes to this procedure can change the results
# of running Tcl's tests.
#
# Arguments: 
# action -              "renaming" or "copying" 
# src -			source directory
# dest -		destination directory
proc tcl::CopyDirectory {action src dest} {
    set nsrc [file normalize $src]
    set ndest [file normalize $dest]
    if {[string equal $action "renaming"]} {
	# Can't rename volumes.  We could give a more precise
	# error message here, but that would break the test suite.
	if {[lsearch -exact [file volumes] $nsrc] != -1} {
	    return -code error "error $action \"$src\" to\
	      \"$dest\": trying to rename a volume or move a directory\
	      into itself"
	}
    }
    if {[file exists $dest]} {
	if {$nsrc == $ndest} {
	    return -code error "error $action \"$src\" to\
	      \"$dest\": trying to rename a volume or move a directory\
	      into itself"
	}
	if {[string equal $action "copying"]} {
	    return -code error "error $action \"$src\" to\
	      \"$dest\": file already exists"
	} else {
	    # Depending on the platform, and on the current
	    # working directory, the directories '.', '..'
	    # can be returned in various combinations.  Anyway,
	    # if any other file is returned, we must signal an error.
	    set existing [glob -nocomplain -directory $dest * .*]
	    eval [list lappend existing] \
	      [glob -nocomplain -directory $dest -type hidden * .*]
	    foreach s $existing {
		if {([file tail $s] != ".") && ([file tail $s] != "..")} {
		    return -code error "error $action \"$src\" to\
		      \"$dest\": file already exists"
		}
	    }
	}
    } else {
	if {[string first $nsrc $ndest] != -1} {
	    set srclen [expr {[llength [file split $nsrc]] -1}]
	    set ndest [lindex [file split $ndest] $srclen]
	    if {$ndest == [file tail $nsrc]} {
		return -code error "error $action \"$src\" to\
		  \"$dest\": trying to rename a volume or move a directory\
		  into itself"
	    }
	}
	file mkdir $dest
    }
    # Have to be careful to capture both visible and hidden files.
    # We will also be more generous to the file system and not
    # assume the hidden and non-hidden lists are non-overlapping.
    # 
    # On Unix 'hidden' files begin with '.'.  On other platforms
    # or filesystems hidden files may have other interpretations.
    set filelist [concat [glob -nocomplain -directory $src *] \
      [glob -nocomplain -directory $src -types hidden *]]
    
    foreach s [lsort -unique $filelist] {
	if {([file tail $s] != ".") && ([file tail $s] != "..")} {
	    file copy $s [file join $dest [file tail $s]]
	}
    }
    return
}
