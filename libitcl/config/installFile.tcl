#!/bin/sh
#
# installFile.tcl - a Tcl version of install-sh
#	that copies a file and preserves its permission bits.
#	This also optimizes out installation of existing files
#	that have the same size and time stamp as the source.
#
# \
exec tclsh8.3 "$0" ${1+"$@"}

set doCopy 0	;# Rename files instead of copy
set doStrip 0	;# Strip the symbols from installed copy
set verbose 0
set src ""
set dst ""

# Process command line arguments, compatible with install-sh

for {set i 0} {$i < $argc} {incr i} {
    set arg [lindex $argv $i]
    switch -- $arg {
	-c {
	    set doCopy 1
	}
	-m  {
	    incr i
	    # Assume UNIX standard "644", etc, so force Tcl to think octal
	    set permissions 0[lindex $argv $i]
	}
	-o  {
	    incr i
	    set owner [lindex $argv $i]
	}
	-g  {
	    incr i
	    set group [lindex $argv $i]
	}
	-s {
	    set doStrip 1
	}
	-v {
	    set verbose 1
	}
	default {
	    set src $arg
	    incr i
	    set dst [lindex $argv $i]
	    break
	}
    }
}
if {[string length $src] == 0} {
    puts stderr "$argv0: no input file specified"
    exit 1
}
if {[string length $dst] == 0} {
    puts stderr "$argv0: no destination file specified"
    exit 1
}

# Compatibility with CYGNUS-style pathnames
regsub {^/(cygdrive)?/(.)/(.*)} $src {\2:/\3} src
regsub {^/(cygdrive)?/(.)/(.*)} $dst {\2:/\3} dst

if {$verbose && $doStrip} {
    puts stderr "Ignoring -s (strip) option for $dst"
}
if {[file isdirectory $dst]} {
    set dst [file join $dst [file tail $src]]
}

# Temporary file name

set dsttmp [file join [file dirname $dst] #inst.[pid]#]

# Optimize out install if the file already exists

set actions ""
if {[file exists $dst] &&
	([file mtime $src] == [file mtime $dst]) &&
	([file size $src] == [file size $dst])} {

    # Looks like the same file, so don't bother to copy.
    # Set dsttmp in case we still need to tweak mode, group, etc.

    set dsttmp $dst
    lappend actions "already installed"
} else {
    file copy -force $src $dsttmp
    lappend actions copied
}

# At this point "$dsttmp" is installed, but might not have the
# right permissions and may need to be renamed.


foreach attrName {owner group permissions} {
    upvar 0 $attrName attr

    if {[info exists attr]} {
	if {![catch {file attributes $dsttmp -$attrName} dstattr]} {

	    # This system supports "$attrName" kind of attributes

	    if {($attr != $dstattr)} {
		file attributes $dsttmp -$attrName $attr
		lappend actions "set $attrName to $attr"
	    }
	}
    }
}

if {[string compare $dst $dsttmp] != 0} {
    file rename -force $dsttmp $dst
}
if {$verbose} {
    puts stderr "$dst: [join $actions ", "]"
}
exit 0
