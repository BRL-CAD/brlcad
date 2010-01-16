#!/bin/sh
# The next line is executed by /bin/sh, but not tcl \
exec tclsh "$0" ${1+"$@"}

package require Tcl 8.6

# Convert Ousterhout format man pages into highly crosslinked hypertext.
#
# Along the way detect many unmatched font changes and other odd things.
#
# Note well, this program is a hack rather than a piece of software
# engineering.  In that sense it's probably a good example of things
# that a scripting language, like Tcl, can do well.  It is offered as
# an example of how someone might convert a specific set of man pages
# into hypertext, not as a general solution to the problem.  If you
# try to use this, you'll be very much on your own.
#
# Copyright (c) 1995-1997 Roger E. Critchlow Jr
# Copyright (c) 2004-2010 Donal K. Fellows
#
# CVS: $Id$

regexp {\d+\.\d+} {$Revision$} ::Version
set ::CSSFILE "docs.css"

##
## Source the utility functions that provide most of the
## implementation of the transformation from nroff to html.
##
source [file join [file dirname [info script]] tcltk-man2html-utils.tcl]

proc parse_command_line {} {
    global argv Version

    # These variables determine where the man pages come from and where
    # the converted pages go to.
    global tcltkdir tkdir tcldir webdir build_tcl build_tk verbose

    # Set defaults based on original code.
    set tcltkdir ../..
    set tkdir {}
    set tcldir {}
    set webdir ../html
    set build_tcl 0
    set build_tk 0
    set verbose 0
    # Default search version is a glob pattern
    set useversion {{,[8-9].[0-9]{,[.ab][0-9]{,[0-9]}}}}

    # Handle arguments a la GNU:
    #   --version
    #   --useversion=<version>
    #   --help
    #   --srcdir=/path
    #   --htmldir=/path

    foreach option $argv {
	switch -glob -- $option {
	    --version {
		puts "tcltk-man-html $Version"
		exit 0
	    }

	    --help {
		puts "usage: tcltk-man-html \[OPTION\] ...\n"
		puts "  --help              print this help, then exit"
		puts "  --version           print version number, then exit"
		puts "  --srcdir=DIR        find tcl and tk source below DIR"
		puts "  --htmldir=DIR       put generated HTML in DIR"
		puts "  --tcl               build tcl help"
		puts "  --tk                build tk help"
		puts "  --useversion        version of tcl/tk to search for"
		puts "  --verbose           whether to print longer messages"
		exit 0
	    }

	    --srcdir=* {
		# length of "--srcdir=" is 9.
		set tcltkdir [string range $option 9 end]
	    }

	    --htmldir=* {
		# length of "--htmldir=" is 10
		set webdir [string range $option 10 end]
	    }

	    --useversion=* {
		# length of "--useversion=" is 13
		set useversion [string range $option 13 end]
	    }

	    --tcl {
		set build_tcl 1
	    }

	    --tk {
		set build_tk 1
	    }

	    --verbose=* {
		set verbose [string range $option \
				 [string length --verbose=] end]
	    }
	    default {
		puts stderr "tcltk-man-html: unrecognized option -- `$option'"
		exit 1
	    }
	}
    }

    if {!$build_tcl && !$build_tk} {
	set build_tcl 1;
	set build_tk 1
    }

    if {$build_tcl} {
	# Find Tcl.
	set tcldir [lindex [lsort [glob -nocomplain -tails -type d \
		-directory $tcltkdir tcl$useversion]] end]
	if {$tcldir eq ""} {
	    puts stderr "tcltk-man-html: couldn't find Tcl below $tcltkdir"
	    exit 1
	}
	puts "using Tcl source directory $tcldir"
    }

    if {$build_tk} {
	# Find Tk.
	set tkdir [lindex [lsort [glob -nocomplain -tails -type d \
		-directory $tcltkdir tk$useversion]] end]
	if {$tkdir eq ""} {
	    puts stderr "tcltk-man-html: couldn't find Tk below $tcltkdir"
	    exit 1
	}
	puts "using Tk source directory $tkdir"
    }

    puts "verbose messages are [expr {$verbose ? {on} : {off}}]"

    # the title for the man pages overall
    global overall_title
    set overall_title ""
    if {$build_tcl} {
	append overall_title "[capitalize $tcldir]"
    }
    if {$build_tcl && $build_tk} {
	append overall_title "/"
    }
    if {$build_tk} {
	append overall_title "[capitalize $tkdir]"
    }
    append overall_title " Documentation"
}

proc capitalize {string} {
    return [string toupper $string 0]
}

##
## Returns the style sheet.
##
proc css-style args {
    upvar 1 style style
    set body [uplevel 1 [list subst [lindex $args end]]]
    set tokens [join [lrange $args 0 end-1] ", "]
    append style $tokens " \{" $body "\}\n"
}
proc css-stylesheet {} {
    set hBd "1px dotted #11577b"

    css-style body div p th td li dd ul ol dl dt blockquote {
	font-family: Verdana, sans-serif;
    }
    css-style pre code {
	font-family: 'Courier New', Courier, monospace;
    }
    css-style pre {
	background-color:  #f6fcec;
	border-top:        1px solid #6A6A6A;
	border-bottom:     1px solid #6A6A6A;
	padding:           1em;
	overflow:          auto;
    }
    css-style body {
	background-color:  #FFFFFF;
	font-size:         12px;
	line-height:       1.25;
	letter-spacing:    .2px;
	padding-left:      .5em;
    }
    css-style h1 h2 h3 h4 {
	font-family:       Georgia, serif;
	padding-left:      1em;
	margin-top:        1em;
    }
    css-style h1 {
	font-size:         18px;
	color:             #11577b;
	border-bottom:     $hBd;
	margin-top:        0px;
    }
    css-style h2 {
	font-size:         14px;
	color:             #11577b;
	background-color:  #c5dce8;
	padding-left:      1em;
	border:            1px solid #6A6A6A;
    }
    css-style h3 h4 {
	color:             #1674A4;
	background-color:  #e8f2f6;
	border-bottom:     $hBd;
	border-top:        $hBd;
    }
    css-style h3 {
	font-size: 12px;
    }
    css-style h4 {
	font-size: 11px;
    }
    css-style ".keylist dt" ".arguments dt" {
	width: 20em;
	float: left;
	padding: 2px;
	border-top: 1px solid #999;
    }
    css-style ".keylist dt" { font-weight: bold; }
    css-style ".keylist dd" ".arguments dd" {
	margin-left: 20em;
	padding: 2px;
	border-top: 1px solid #999;
    }
    css-style .copy {
	background-color:  #f6fcfc;
	white-space:       pre;
	font-size:         80%;
	border-top:        1px solid #6A6A6A;
	margin-top:        2em;
    }
    css-style .tablecell {
	font-size:	   12px;
	padding-left:	   .5em;
	padding-right:	   .5em;
    }
}

##
## foreach of the man directories specified by args
## convert manpages into hypertext in the directory
## specified by html.
##
proc make-man-pages {html args} {
    global manual overall_title tcltkdesc verbose
    global excluded_pages forced_index_pages process_first_patterns

    makedirhier $html
    set cssfd [open $html/$::CSSFILE w]
    puts $cssfd [css-stylesheet]
    close $cssfd
    set manual(short-toc-n) 1
    set manual(short-toc-fp) [open $html/[indexfile] w]
    puts $manual(short-toc-fp) [htmlhead $overall_title $overall_title]
    puts $manual(short-toc-fp) "<DL class=\"keylist\">"
    set manual(merge-copyrights) {}

    set LQ \u201c
    set RQ \u201d

    foreach arg $args {
	# preprocess to set up subheader for the rest of the files
	if {![llength $arg]} {
	    continue
	}
	set name [lindex $arg 1]
	set file [lindex $arg 2]
	lappend manual(subheader) $name $file
    }
    foreach arg $args {
	if {![llength $arg]} {
	    continue
	}
	set manual(wing-glob) [lindex $arg 0]
	set manual(wing-name) [lindex $arg 1]
	set manual(wing-file) [lindex $arg 2]
	set manual(wing-description) [lindex $arg 3]
	set manual(wing-copyrights) {}
	makedirhier $html/$manual(wing-file)
	set manual(wing-toc-fp) [open $html/$manual(wing-file)/[indexfile] w]
	# whistle
	puts stderr "scanning section $manual(wing-name)"
	# put the entry for this section into the short table of contents
	puts $manual(short-toc-fp) "<DT><A HREF=\"$manual(wing-file)/[indexfile]\">$manual(wing-name)</A></DT><DD>$manual(wing-description)</DD>"
	# initialize the wing table of contents
	puts $manual(wing-toc-fp) [htmlhead $manual(wing-name) \
		$manual(wing-name) $overall_title "../[indexfile]"]
	# initialize the short table of contents for this section
	set manual(wing-toc) {}
	# initialize the man directory for this section
	makedirhier $html/$manual(wing-file)
	# initialize the long table of contents for this section
	set manual(long-toc-n) 1
	# get the manual pages for this section
	set manual(pages) [lsort -dictionary [glob -nocomplain $manual(wing-glob)]]
	# Some pages have to go first so that their links override others
	foreach pat $process_first_patterns {
	    set n [lsearch -glob $manual(pages) $pat]
	    if {$n >= 0} {
		set f [lindex $manual(pages) $n]
		puts stderr "shuffling [file tail $f] to front of processing queue"
		set manual(pages) \
		    [linsert [lreplace $manual(pages) $n $n] 0 $f]
	    }
	}
	# set manual(pages) [lrange $manual(pages) 0 5]
	foreach manual_page $manual(pages) {
	    set manual(page) [file normalize $manual_page]
	    # whistle
	    if {$verbose} {
		puts stderr "scanning page $manual(page)"
	    } else {
		puts -nonewline stderr .
	    }
	    set manual(tail) [file tail $manual(page)]
	    set manual(name) [file root $manual(tail)]
	    set manual(section) {}
	    if {$manual(name) in $excluded_pages} {
		# obsolete
		if {!$verbose} {
		    puts stderr ""
		}
		manerror "discarding $manual(name)"
		continue
	    }
	    set manual(infp) [open $manual(page)]
	    set manual(text) {}
	    set manual(partial-text) {}
	    foreach p {.RS .DS .CS .SO} {
		set manual($p) 0
	    }
	    set manual(stack) {}
	    set manual(section) {}
	    set manual(section-toc) {}
	    set manual(section-toc-n) 1
	    set manual(copyrights) {}
	    lappend manual(all-pages) $manual(wing-file)/$manual(tail)
	    manreport 100 $manual(name)
	    while {[gets $manual(infp) line] >= 0} {
		manreport 100 $line
		if {[regexp {^[`'][/\\]} $line]} {
		    if {[regexp {Copyright (?:\(c\)|\\\(co).*$} $line copyright]} {
			lappend manual(copyrights) $copyright
		    }
		    # comment
		    continue
		}
		if {"$line" eq {'}} {
		    # comment
		    continue
		}
		if {![parse-directive $line code rest]} {
		    addbuffer $line
		    continue
		}
		switch -exact -- $code {
		    .if - .nr - .ti - .in -
		    .ad - .na - .so - .ne - .AS - .VE - .VS - . {
			# ignore
			continue
		    }
		}
		switch -exact -- $code {
		    .SH - .SS {
			flushbuffer
			if {[llength $rest] == 0} {
			    gets $manual(infp) rest
			}
			lappend manual(text) "$code [unquote $rest]"
		    }
		    .TH {
			flushbuffer
			lappend manual(text) "$code [unquote $rest]"
		    }
		    .QW {
			set rest [regexp -all -inline {\"(?:[^""]+)\"|\S+} $rest]
			addbuffer $LQ [unquote [lindex $rest 0]] $RQ \
			    [unquote [lindex $rest 1]]
		    }
		    .PQ {
			set rest [regexp -all -inline {\"(?:[^""]+)\"|\S+} $rest]
			addbuffer ( $LQ [unquote [lindex $rest 0]] $RQ \
			    [unquote [lindex $rest 1]] ) \
			    [unquote [lindex $rest 2]]
		    }
		    .QR {
			set rest [regexp -all -inline {\"(?:[^""]+)\"|\S+} $rest]
			addbuffer $LQ [unquote [lindex $rest 0]] - \
			    [unquote [lindex $rest 1]] $RQ \
			    [unquote [lindex $rest 2]]
		    }
		    .MT {
			addbuffer $LQ$RQ
		    }
		    .HS - .UL - .ta {
			flushbuffer
			lappend manual(text) "$code [unquote $rest]"
		    }
		    .BS - .BE - .br - .fi - .sp - .nf {
			flushbuffer
			if {"$rest" ne {}} {
			    if {!$verbose} {
				puts stderr ""
			    }
			    manerror "unexpected argument: $line"
			}
			lappend manual(text) $code
		    }
		    .AP {
			flushbuffer
			lappend manual(text) [concat .IP [process-text "[lindex $rest 0] \\fB[lindex $rest 1]\\fR ([lindex $rest 2])"]]
		    }
		    .IP {
			flushbuffer
			regexp {^(.*) +\d+$} $rest all rest
			lappend manual(text) ".IP [process-text [unquote [string trim $rest]]]"
		    }
		    .TP {
			flushbuffer
			while {[is-a-directive [set next [gets $manual(infp)]]]} {
			    if {!$verbose} {
				puts stderr ""
			    }
			    manerror "ignoring $next after .TP"
			}
			if {"$next" ne {'}} {
			    lappend manual(text) ".IP [process-text $next]"
			}
		    }
		    .OP {
			flushbuffer
			lappend manual(text) [concat .OP [process-text \
				"\\fB[lindex $rest 0]\\fR \\fB[lindex $rest 1]\\fR \\fB[lindex $rest 2]\\fR"]]
		    }
		    .PP - .LP {
			flushbuffer
			lappend manual(text) {.PP}
		    }
		    .RS {
			flushbuffer
			incr manual(.RS)
			lappend manual(text) $code
		    }
		    .RE {
			flushbuffer
			incr manual(.RS) -1
			lappend manual(text) $code
		    }
		    .SO {
			flushbuffer
			incr manual(.SO)
			if {[llength $rest] == 0} {
			    lappend manual(text) "$code options"
			} else {
			    lappend manual(text) "$code [unquote $rest]"
			}
		    }
		    .SE {
			flushbuffer
			incr manual(.SO) -1
			lappend manual(text) $code
		    }
		    .DS {
			flushbuffer
			incr manual(.DS)
			lappend manual(text) $code
		    }
		    .DE {
			flushbuffer
			incr manual(.DS) -1
			lappend manual(text) $code
		    }
		    .CS {
			flushbuffer
			incr manual(.CS)
			lappend manual(text) $code
		    }
		    .CE {
			flushbuffer
			incr manual(.CS) -1
			lappend manual(text) $code
		    }
		    .de {
			while {[gets $manual(infp) line] >= 0} {
			    if {[string match "..*" $line]} {
				break
			    }
			}
		    }
		    .. {
			if {!$verbose} {
			    puts stderr ""
			}
			error "found .. outside of .de"
		    }
		    default {
			if {!$verbose} {
			    puts stderr ""
			}
			flushbuffer
			manerror "unrecognized format directive: $line"
		    }
		}
	    }
	    flushbuffer
	    close $manual(infp)
	    # fixups
	    if {$manual(.RS) != 0} {
		if {!$verbose} {
		    puts stderr ""
		}
		puts "unbalanced .RS .RE"
	    }
	    if {$manual(.DS) != 0} {
		if {!$verbose} {
		    puts stderr ""
		}
		puts "unbalanced .DS .DE"
	    }
	    if {$manual(.CS) != 0} {
		if {!$verbose} {
		    puts stderr ""
		}
		puts "unbalanced .CS .CE"
	    }
	    if {$manual(.SO) != 0} {
		if {!$verbose} {
		    puts stderr ""
		}
		puts "unbalanced .SO .SE"
	    }
	    # output conversion
	    open-text
	    set haserror 0
	    if {[next-op-is .HS rest]} {
		set manual($manual(name)-title) \
			"[lrange $rest 1 end] [lindex $rest 0] manual page"
	    } elseif {[next-op-is .TH rest]} {
		set manual($manual(name)-title) "[lindex $rest 0] manual page - [lrange $rest 4 end]"
	    } else {
		set haserror 1
		if {!$verbose} {
		    puts stderr ""
		}
		manerror "no .HS or .TH record found"
	    }
	    if {!$haserror} {
		while {[more-text]} {
		    set line [next-text]
		    if {[is-a-directive $line]} {
			output-directive $line
		    } else {
			man-puts $line
		    }
		}
		man-puts [copyout $manual(copyrights) "../"]
		set manual(wing-copyrights) [merge-copyrights \
			$manual(wing-copyrights) $manual(copyrights)]
	    }
	    #
	    # make the long table of contents for this page
	    #
	    set manual(toc-$manual(wing-file)-$manual(name)) \
		    [concat <DL> $manual(section-toc) </DL>]
	}
	if {!$verbose} {
	    puts stderr ""
	}

	#
	# make the wing table of contents for the section
	#
	set width 0
	foreach name $manual(wing-toc) {
	    if {[string length $name] > $width} {
		set width [string length $name]
	    }
	}
	set perline [expr {118 / $width}]
	set nrows [expr {([llength $manual(wing-toc)]+$perline)/$perline}]
	set n 0
        catch {unset rows}
	foreach name [lsort -dictionary $manual(wing-toc)] {
	    set tail $manual(name-$name)
	    if {[llength $tail] > 1} {
		manerror "$name is defined in more than one file: $tail"
		set tail [lindex $tail [expr {[llength $tail]-1}]]
	    }
	    set tail [file tail $tail]
	    append rows([expr {$n%$nrows}]) \
		    "<td> <a href=\"$tail.htm\">$name</a> </td>"
	    incr n
	}
	puts $manual(wing-toc-fp) <table>
        foreach row [lsort -integer [array names rows]] {
	    puts $manual(wing-toc-fp) <tr>$rows($row)</tr>
	}
	puts $manual(wing-toc-fp) </table>

	#
	# insert wing copyrights
	#
	puts $manual(wing-toc-fp) [copyout $manual(wing-copyrights) "../"]
	puts $manual(wing-toc-fp) "</BODY></HTML>"
	close $manual(wing-toc-fp)
	set manual(merge-copyrights) [merge-copyrights \
		$manual(merge-copyrights) $manual(wing-copyrights)]
    }

    ##
    ## build the keyword index.
    ##
    file delete -force -- $html/Keywords
    makedirhier $html/Keywords
    set keyfp [open $html/Keywords/[indexfile] w]
    puts $keyfp [htmlhead "$tcltkdesc Keywords" "$tcltkdesc Keywords" \
		     $overall_title "../[indexfile]"]
    set letters {A B C D E F G H I J K L M N O P Q R S T U V W X Y Z}
    # Create header first
    set keyheader {}
    foreach a $letters {
	set keys [array names manual "keyword-\[[string totitle $a$a]\]*"]
	if {[llength $keys]} {
	    lappend keyheader "<A HREF=\"$a.htm\">$a</A>"
	} else {
	    # No keywords for this letter
	    lappend keyheader $a
	}
    }
    set keyheader <H3>[join $keyheader " |\n"]</H3>
    puts $keyfp $keyheader
    foreach a $letters {
	set keys [array names manual "keyword-\[[string totitle $a$a]\]*"]
	if {![llength $keys]} {
	    continue
	}
	# Per-keyword page
	set afp [open $html/Keywords/$a.htm w]
	puts $afp [htmlhead "$tcltkdesc Keywords - $a" \
		       "$tcltkdesc Keywords - $a" \
		       $overall_title "../[indexfile]"]
	puts $afp $keyheader
	puts $afp "<DL class=\"keylist\">"
	foreach k [lsort -dictionary $keys] {
	    set k [string range $k 8 end]
	    puts $afp "<DT><A NAME=\"$k\">$k</A></DT>"
	    puts $afp "<DD>"
	    set refs {}
	    foreach man $manual(keyword-$k) {
		set name [lindex $man 0]
		set file [lindex $man 1]
		lappend refs "<A HREF=\"../$file\">$name</A>"
	    }
	    puts $afp "[join $refs {, }]</DD>"
	}
	puts $afp "</DL>"
	# insert merged copyrights
	puts $afp [copyout $manual(merge-copyrights)]
	puts $afp "</BODY></HTML>"
	close $afp
    }
    # insert merged copyrights
    puts $keyfp [copyout $manual(merge-copyrights)]
    puts $keyfp "</BODY></HTML>"
    close $keyfp

    ##
    ## finish off short table of contents
    ##
    puts $manual(short-toc-fp) "<DT><A HREF=\"Keywords/[indexfile]\">Keywords</A><DD>The keywords from the $tcltkdesc man pages."
    puts $manual(short-toc-fp) "</DL>"
    # insert merged copyrights
    puts $manual(short-toc-fp) [copyout $manual(merge-copyrights)]
    puts $manual(short-toc-fp) "</BODY></HTML>"
    close $manual(short-toc-fp)

    ##
    ## output man pages
    ##
    unset manual(section)
    if {!$verbose} {
	puts stderr "Rescanning [llength $manual(all-pages)] pages to build cross links"
    }
    foreach path $manual(all-pages) {
	set manual(wing-file) [file dirname $path]
	set manual(tail) [file tail $path]
	set manual(name) [file root $manual(tail)]
	try {
	    set text $manual(output-$manual(wing-file)-$manual(name))
	    set ntext 0
	    foreach item $text {
		incr ntext [llength [split $item \n]]
		incr ntext
	    }
	    set toc $manual(toc-$manual(wing-file)-$manual(name))
	    set ntoc 0
	    foreach item $toc {
		incr ntoc [llength [split $item \n]]
		incr ntoc
	    }
	    if {$verbose} {
		puts stderr "rescanning page $manual(name) $ntoc/$ntext"
	    } else {
		puts -nonewline stderr .
	    }
	    set outfd [open $html/$manual(wing-file)/$manual(name).htm w]
	    puts $outfd [htmlhead "$manual($manual(name)-title)" \
		    $manual(name) $manual(wing-file) "[indexfile]" \
		    $overall_title "../[indexfile]"]
	    if {($ntext > 60) && ($ntoc > 32)} {
		foreach item $toc {
		    puts $outfd $item
		}
	    } elseif {$manual(name) in $forced_index_pages} {
		if {!$verbose} {puts stderr ""}
		manerror "forcing index generation"
		foreach item $toc {
		    puts $outfd $item
		}
	    }
	    foreach item $text {
		puts $outfd [insert-cross-references $item]
	    }
	    puts $outfd "</BODY></HTML>"
	} on error msg {
	    if {$verbose} {
		puts stderr $msg
	    } else {
		puts stderr "\nError when processing $manual(name): $msg"
	    }
	} finally {
	    catch {close $outfd}
	}
    }
    if {!$verbose} {
	puts stderr "\nDone"
    }
    return {}
}

##
## Helper for assembling the descriptions of base packages (i.e., Tcl and Tk).
##
proc plus-base {var glob name dir desc} {
    global tcltkdir
    if {$var} {
	return [list $tcltkdir/$glob $name $dir $desc]
    }
}

##
## Helper for assembling the descriptions of contributed packages.
##
proc plus-pkgs {type args} {
    global build_tcl tcltkdir tcldir
    if {$type ni {n 3}} {
	error "unknown type \"$type\": must be 3 or n"
    }
    if {!$build_tcl} return
    set result {}
    foreach {dir name} $args {
	set globpat $tcltkdir/$tcldir/pkgs/$dir/doc/*.$type
	if {![llength [glob -nocomplain $globpat]]} continue
	switch $type {
	    n {
		set title "$name Package Commands"
		set dir [string totitle $dir]Cmd
		set desc \
		    "The additional commands provided by the $name package."
	    }
	    3 {
		set title "$name Package Library"
		set dir [string totitle $dir]Lib
		set desc \
		    "The additional C functions provided by the $name package."
	    }
	}
	lappend result [list $globpat $title $dir $desc]
    }
    return $result
}

##
## Set up some special cases. It would be nice if we didn't have them,
## but we do...
##
set excluded_pages {case menubar pack-old}
set forced_index_pages {GetDash}
set process_first_patterns {*/ttk_widget.n */options.n}
set ensemble_commands {
    after array binary chan clock dde dict encoding file history info interp
    memory namespace package registry self string trace update zlib
    clipboard console font grab grid image option pack place selection tk
    tkwait ttk::style winfo wm
}
array set remap_link_target {
    stdin  Tcl_GetStdChannel
    stdout Tcl_GetStdChannel
    stderr Tcl_GetStdChannel
    safe   {Safe&nbsp;Base}
    style  ttk::style
    {style map} ttk::style
}
array set exclude_refs_map {
    clock.n		{next}
    history.n		{exec}
    next.n		{unknown}
    canvas.n		{bitmap text}
    checkbutton.n	{image}
    clipboard.n		{string}
    menu.n		{checkbutton radiobutton}
    options.n		{bitmap image set}
    radiobutton.n	{image}
    scrollbar.n		{set}
    selection.n		{string}
    tcltest.n		{error}
    tkvars.n		{tk}
    ttk_checkbutton.n	{variable}
    ttk_combobox.n	{selection}
    ttk_entry.n		{focus variable}
    ttk_intro.n		{focus}
    ttk_label.n		{font text}
    ttk_labelframe.n	{text}
    ttk_menubutton.n	{flush}
    ttk_notebook.n	{image text}
    ttk_progressbar.n	{variable}
    ttk_radiobutton.n	{variable}
    ttk_scale.n		{variable}
    ttk_scrollbar.n	{set}
    ttk_spinbox.n	{format}
    ttk_treeview.n	{text open}
    ttk_widget.n	{image text variable}
}
array set exclude_when_followed_by_map {
    canvas.n {
	bind widget
	focus widget
	image are
	lower widget
	raise widget
    }
    selection.n {
	clipboard selection
	clipboard ;
    }
    ttk_image.n {
	image imageSpec
    }
}

try {
    # Parse what the user told us to do
    parse_command_line

    # Some strings depend on what options are specified
    set tcltkdesc ""; set cmdesc ""; set appdir ""
    if {$build_tcl} {
	append tcltkdesc "Tcl"
	append cmdesc "Tcl"
	append appdir "$tcldir"
    }
    if {$build_tcl && $build_tk} {
	append tcltkdesc "/"
	append cmdesc " and "
	append appdir ","
    }
    if {$build_tk} {
	append tcltkdesc "Tk"
	append cmdesc "Tk"
	append appdir "$tkdir"
    }

    # Get the list of packages to try, and what their human-readable
    # names are.
    try {
	set packageDirNameMap {}
	if {$build_tcl} {
	    set f [open $tcltkdir/$tcldir/pkgs/package.list.txt]
	    try {
		foreach line [split [read $f] \n] {
		    if {[string trim $line] eq ""} continue
		    if {[string match #* $line]} continue
		    lappend packageDirNameMap {*}$line
		}
	    } finally {
		close $f
	    }
	}
    } trap {POSIX ENOENT} {} {
	set packageDirNameMap {
	    itcl {[incr Tcl]}
	    tdbc {TDBC}
	}
    }

    #
    # Invoke the scraper/converter engine.
    #
    make-man-pages $webdir \
	[list $tcltkdir/{$appdir}/doc/*.1 "$tcltkdesc Applications" UserCmd \
	     "The interpreters which implement $cmdesc."] \
	[plus-base $build_tcl $tcldir/doc/*.n {Tcl Commands} TclCmd \
	     "The commands which the <B>tclsh</B> interpreter implements."] \
	[plus-base $build_tk $tkdir/doc/*.n {Tk Commands} TkCmd \
	     "The additional commands which the <B>wish</B> interpreter implements."] \
	{*}[plus-pkgs n {*}$packageDirNameMap] \
	[plus-base $build_tcl $tcldir/doc/*.3 {Tcl Library} TclLib \
	     "The C functions which a Tcl extended C program may use."] \
	[plus-base $build_tk $tkdir/doc/*.3 {Tk Library} TkLib \
	     "The additional C functions which a Tk extended C program may use."] \
	{*}[plus-pkgs 3 {*}$packageDirNameMap]
} on error {msg opts} {
    # On failure make sure we show what went wrong. We're not supposed
    # to get here though; it represents a bug in the script.
    puts $msg\n[dict get $opts -errorinfo]
    exit 1
}

# Local-Variables:
# mode: tcl
# End:
