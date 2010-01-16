##
## Utility functions for Man->HTML converter. Note that these
## functions are specifically intended to work with the format as used
## by Tcl and Tk; they do not cope with arbitrary nroff markup.
##
## Copyright (c) 1995-1997 Roger E. Critchlow Jr
## Copyright (c) 2004-2010 Donal K. Fellows
##
## CVS: $Id$

set ::manual(report-level) 1

proc manerror {msg} {
    global manual
    set name {}
    set subj {}
    set procname [lindex [info level -1] 0]
    if {[info exists manual(name)]} {
	set name $manual(name)
    }
    if {[info exists manual(section)] && [string length $manual(section)]} {
	puts stderr "$name: $manual(section): $procname: $msg"
    } else {
	puts stderr "$name: $procname: $msg"
    }
}

proc manreport {level msg} {
    global manual
    if {$level < $manual(report-level)} {
	uplevel 1 [list manerror $msg]
    }
}

proc fatal {msg} {
    global manual
    uplevel 1 [list manerror $msg]
    exit 1
}

##
## templating
##
proc indexfile {} {
    if {[info exists ::TARGET] && $::TARGET eq "devsite"} {
	return "index.tml"
    } else {
	return "contents.htm"
    }
}
proc copyright {copyright {level {}}} {
    # We don't actually generate a separate copyright page anymore
    #set page "${level}copyright.htm"
    #return "<A HREF=\"$page\">Copyright</A> &#169; [htmlize-text [lrange $copyright 2 end]]"
    # obfuscate any email addresses that may appear in name
    set who [string map {@ (at)} [lrange $copyright 2 end]]
    return "Copyright &copy; [htmlize-text $who]"
}
proc copyout {copyrights {level {}}} {
    set out "<div class=\"copy\">"
    foreach c $copyrights {
	append out "[copyright $c $level]\n"
    }
    append out "</div>"
    return $out
}
proc CSS {{level ""}} {
    return "<link rel=\"stylesheet\" href=\"${level}$::CSSFILE\" type=\"text/css\" media=\"all\">\n"
}
proc DOCTYPE {} {
    return "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">"
}
proc htmlhead {title header args} {
    set level ""
    if {[lindex $args end] eq "../[indexfile]"} {
	# XXX hack - assume same level for CSS file
	set level "../"
    }
    set out "[DOCTYPE]\n<HTML>\n<HEAD><TITLE>$title</TITLE>\n[CSS $level]</HEAD>\n"
    foreach {uptitle url} $args {
	set header "<a href=\"$url\">$uptitle</a> <small>&gt;</small> $header"
    }
    append out "<BODY><H2>$header</H2>"
    global manual
    if {[info exists manual(subheader)]} {
	set subs {}
	foreach {name subdir} $manual(subheader) {
	    if {$name eq $title} {
		lappend subs $name
	    } else {
		lappend subs "<A HREF=\"${level}$subdir/[indexfile]\">$name</A>"
	    }
	}
	append out "\n<H3>[join $subs { | }]</H3>"
    }
    return $out
}

##
## parsing
##
proc unquote arg {
    return [string map [list \" {}] $arg]
}

proc parse-directive {line codename restname} {
    upvar 1 $codename code $restname rest
    return [regexp {^(\.[.a-zA-Z0-9]*) *(.*)} $line all code rest]
}

proc htmlize-text {text {charmap {}}} {
    # contains some extras for use in nroff->html processing
    # build on the list passed in, if any
    lappend charmap \
	{&}	{&amp;} \
	{\\}	"&#92;" \
	{\e}	"&#92;" \
	{\ }	{&nbsp;} \
	{\|}	{&nbsp;} \
	{\0}	{ } \
	\"	{&quot;} \
	{<}	{&lt;} \
	{>}	{&gt;} \
	\u201c "&#8220;" \
	\u201d "&#8221;"

    return [string map $charmap $text]
}

proc process-text {text} {
    global manual
    # preprocess text; note that this is an incomplete map, and will probably
    # need to have things added to it as the manuals expand to use them.
    set charmap [list \
	    {\&}	"\t" \
	    {\%}	{} \
	    "\\\n"	"\n" \
	    {\(+-}	"&#177;" \
	    {\(co}	"&copy;" \
	    {\(em}	"&#8212;" \
	    {\(fm}	"&#8242;" \
	    {\(mu}	"&#215;" \
	    {\(mi}	"&#8722;" \
	    {\(->}	"<font size=\"+1\">&#8594;</font>" \
	    {\fP}	{\fR} \
	    {\.}	. \
	    {\(bu}	"&#8226;" \
	    ]
    lappend charmap {\o'o^'} {&ocirc;} ; # o-circumflex in re_syntax.n
    lappend charmap {\-\|\-} --        ; # two hyphens
    lappend charmap {\-} -             ; # a hyphen

    set text [htmlize-text $text $charmap]
    # General quoted entity
    regsub -all {\\N'(\d+)'} $text "\\&#\\1;" text
    while {[string first "\\" $text] >= 0} {
	# C R
	if {[regsub {^([^\\]*)\\fC([^\\]*)\\fR(.*)$} $text \
		{\1<TT>\2</TT>\3} text]} continue
	# B R
	if {[regsub {^([^\\]*)\\fB([^\\]*)\\fR(.*)$} $text \
		{\1<B>\2</B>\3} text]} continue
	# B I
	if {[regsub {^([^\\]*)\\fB([^\\]*)\\fI(.*)$} $text \
		{\1<B>\2</B>\\fI\3} text]} continue
	# I R
	if {[regsub {^([^\\]*)\\fI([^\\]*)\\fR(.*)$} $text \
		{\1<I>\2</I>\3} text]} continue
	# I B
	if {[regsub {^([^\\]*)\\fI([^\\]*)\\fB(.*)$} $text \
		{\1<I>\2</I>\\fB\3} text]} continue
	# B B, I I, R R
	if {
	    [regsub {^([^\\]*)\\fB([^\\]*)\\fB(.*)$} $text \
		{\1\\fB\2\3} ntext]
	    || [regsub {^([^\\]*)\\fI([^\\]*)\\fI(.*)$} $text \
		    {\1\\fI\2\3} ntext]
	    || [regsub {^([^\\]*)\\fR([^\\]*)\\fR(.*)$} $text \
		    {\1\\fR\2\3} ntext]
	} {
	    manerror "impotent font change: $text"
	    set text $ntext
	    continue
	}
	# unrecognized
	manerror "uncaught backslash: $text"
	set text [string map [list "\\" "&#92;"] $text]
    }
    return $text
}
##
## pass 2 text input and matching
##
proc open-text {} {
    global manual
    set manual(text-length) [llength $manual(text)]
    set manual(text-pointer) 0
}
proc more-text {} {
    global manual
    return [expr {$manual(text-pointer) < $manual(text-length)}]
}
proc next-text {} {
    global manual
    if {[more-text]} {
	set text [lindex $manual(text) $manual(text-pointer)]
	incr manual(text-pointer)
	return $text
    }
    manerror "read past end of text"
    error "fatal"
}
proc is-a-directive {line} {
    return [string match .* $line]
}
proc split-directive {line opname restname} {
    upvar 1 $opname op $restname rest
    set op [string range $line 0 2]
    set rest [string trim [string range $line 3 end]]
}
proc next-op-is {op restname} {
    global manual
    upvar 1 $restname rest
    if {[more-text]} {
	set text [lindex $manual(text) $manual(text-pointer)]
	if {[string equal -length 3 $text $op]} {
	    set rest [string range $text 4 end]
	    incr manual(text-pointer)
	    return 1
	}
    }
    return 0
}
proc backup-text {n} {
    global manual
    if {$manual(text-pointer)-$n >= 0} {
	incr manual(text-pointer) -$n
    }
}
proc match-text args {
    global manual
    set nargs [llength $args]
    if {$manual(text-pointer) + $nargs > $manual(text-length)} {
	return 0
    }
    set nback 0
    foreach arg $args {
	if {![more-text]} {
	    backup-text $nback
	    return 0
	}
	set arg [string trim $arg]
	set targ [string trim [lindex $manual(text) $manual(text-pointer)]]
	if {$arg eq $targ} {
	    incr nback
	    incr manual(text-pointer)
	    continue
	}
	if {[regexp {^@(\w+)$} $arg all name]} {
	    upvar 1 $name var
	    set var $targ
	    incr nback
	    incr manual(text-pointer)
	    continue
	}
	if {[regexp -nocase {^(\.[A-Z][A-Z])@(\w+)$} $arg all op name]\
		&& [string equal $op [lindex $targ 0]]} {
	    upvar 1 $name var
	    set var [lrange $targ 1 end]
	    incr nback
	    incr manual(text-pointer)
	    continue
	}
	backup-text $nback
	return 0
    }
    return 1
}
proc expand-next-text {n} {
    global manual
    return [join [lrange $manual(text) $manual(text-pointer) \
	    [expr {$manual(text-pointer)+$n-1}]] \n\n]
}
##
## pass 2 output
##
proc man-puts {text} {
    global manual
    lappend manual(output-$manual(wing-file)-$manual(name)) $text
}

##
## build hypertext links to tables of contents
##
proc long-toc {text} {
    global manual
    set here M[incr manual(section-toc-n)]
    set manual($manual(name)-id-$text) $here
    set there L[incr manual(long-toc-n)]
    lappend manual(section-toc) \
	    "<DD><A HREF=\"$manual(name).htm#$here\" NAME=\"$there\">$text</A>"
    return "<A NAME=\"$here\">$text</A>"
}
proc option-toc {name class switch} {
    global manual
    # Special case handling, oh we hate it but must do it
    if {[string match "*OPTIONS" $manual(section)]} {
	if {$manual(name) ne "ttk_widget" && ($manual(name) ne "ttk_entry" ||
		![string match validate* $name])} {
	    # link the defined option into the long table of contents
	    set link [long-toc "$switch, $name, $class"]
	    regsub -- "$switch, $name, $class" $link "$switch" link
	    return $link
	}
    } elseif {"$manual(name):$manual(section)" ne "options:DESCRIPTION"} {
	error "option-toc in $manual(name) section $manual(section)"
    }

    # link the defined standard option to the long table of contents and make
    # a target for the standard option references from other man pages.

    set first [lindex $switch 0]
    set here M$first
    set there L[incr manual(long-toc-n)]
    set manual(standard-option-$manual(name)-$first) \
	"<A HREF=\"$manual(name).htm#$here\">$switch, $name, $class</A>"
    lappend manual(section-toc) \
	"<DD><A HREF=\"$manual(name).htm#$here\" NAME=\"$there\">$switch, $name, $class</A>"
    return "<A NAME=\"$here\">$switch</A>"
}
proc std-option-toc {name page} {
    global manual
    if {[info exists manual(standard-option-$page-$name)]} {
	lappend manual(section-toc) <DD>$manual(standard-option-$page-$name)
	return $manual(standard-option-$page-$name)
    }
    manerror "missing reference to \"$name\" in $page.n"
    set here M[incr manual(section-toc-n)]
    set there L[incr manual(long-toc-n)]
    set other M$name
    lappend manual(section-toc) "<DD><A HREF=\"$page.htm#$other\">$name</A>"
    return "<A HREF=\"$page.htm#$other\">$name</A>"
}
##
## process the widget option section
## in widget and options man pages
##
proc output-widget-options {rest} {
    global manual
    man-puts <DL>
    lappend manual(section-toc) <DL>
    backup-text 1
    set para {}
    while {[next-op-is .OP rest]} {
	switch -exact -- [llength $rest] {
	    3 {
		lassign $rest switch name class
	    }
	    5 {
		set switch [lrange $rest 0 2]
		set name [lindex $rest 3]
		set class [lindex $rest 4]
	    }
	    default {
		fatal "bad .OP $rest"
	    }
	}
	if {![regexp {^(<.>)([-\w ]+)(</.>)$} $switch \
		all oswitch switch cswitch]} {
	    if {![regexp {^(<.>)([-\w ]+) or ([-\w ]+)(</.>)$} $switch \
		    all oswitch switch1 switch2 cswitch]} {
		error "not Switch: $switch"
	    }
	    set switch "$switch1$cswitch or $oswitch$switch2"
	}
	if {![regexp {^(<.>)([\w]*)(</.>)$} $name all oname name cname]} {
	    error "not Name: $name"
	}
	if {![regexp {^(<.>)([\w]*)(</.>)$} $class all oclass class cclass]} {
	    error "not Class: $class"
	}
	man-puts "$para<DT>Command-Line Name: $oswitch[option-toc $name $class $switch]$cswitch"
	man-puts "<DT>Database Name: $oname$name$cname"
	man-puts "<DT>Database Class: $oclass$class$cclass"
	man-puts <DD>[next-text]
	set para <P>

	if {[next-op-is .RS rest]} {
	    while {[more-text]} {
		set line [next-text]
		if {[is-a-directive $line]} {
		    split-directive $line code rest
		    switch -exact -- $code {
			.RE {
			    break
			}
			.SH - .SS {
			    manerror "unbalanced .RS at section end"
			    backup-text 1
			    break
			}
			default {
			    output-directive $line
			}
		    }
		} else {
		    man-puts $line
		}
	    }
	}
    }
    man-puts </DL>
    lappend manual(section-toc) </DL>
}

##
## process .RS lists
##
proc output-RS-list {} {
    global manual
    if {[next-op-is .IP rest]} {
	output-IP-list .RS .IP $rest
	if {[match-text .RE .sp .RS @rest .IP @rest2]} {
	    man-puts <P>$rest
	    output-IP-list .RS .IP $rest2
	}
	if {[match-text .RE .sp .RS @rest .RE]} {
	    man-puts <P>$rest
	    return
	}
	if {[next-op-is .RE rest]} {
	    return
	}
    }
    man-puts <DL><DD>
    while {[more-text]} {
	set line [next-text]
	if {[is-a-directive $line]} {
	    split-directive $line code rest
	    switch -exact -- $code {
		.RE {
		    break
		}
		.SH - .SS {
		    manerror "unbalanced .RS at section end"
		    backup-text 1
		    break
		}
		default {
		    output-directive $line
		}
	    }
	} else {
	    man-puts $line
	}
    }
    man-puts </DL>
}

##
## process .IP lists which may be plain indents,
## numeric lists, or definition lists
##
proc output-IP-list {context code rest} {
    global manual
    if {![string length $rest]} {
	# blank label, plain indent, no contents entry
	man-puts <DL><DD>
	while {[more-text]} {
	    set line [next-text]
	    if {[is-a-directive $line]} {
		split-directive $line code rest
		if {$code eq ".IP" && $rest eq {}} {
		    man-puts "<P>"
		    continue
		}
		if {$code in {.br .DS .RS}} {
		    output-directive $line
		} else {
		    backup-text 1
		    break
		}
	    } else {
		man-puts $line
	    }
	}
	man-puts </DL>
    } else {
	# labelled list, make contents
	if {$context ne ".SH" && $context ne ".SS"} {
	    man-puts <P>
	}
	set dl "<DL class=\"[string tolower $manual(section)]\">"
	man-puts $dl
	lappend manual(section-toc) $dl
	backup-text 1
	set accept_RE 0
	set para {}
	while {[more-text]} {
	    set line [next-text]
	    if {[is-a-directive $line]} {
		split-directive $line code rest
		switch -exact -- $code {
		    .IP {
			if {$accept_RE} {
			    output-IP-list .IP $code $rest
			    continue
			}
			if {$manual(section) eq "ARGUMENTS" || \
				[regexp {^\[\d+\]$} $rest]} {
			    man-puts "$para<DT>$rest<DD>"
			} elseif {"&#8226;" eq $rest} {
			    man-puts "$para<DT><DD>$rest&nbsp;"
			} else {
			    man-puts "$para<DT>[long-toc $rest]<DD>"
			}
		    }
		    .sp - .br - .DS - .CS {
			output-directive $line
		    }
		    .RS {
			if {[match-text .RS]} {
			    output-directive $line
			    incr accept_RE 1
			} elseif {[match-text .CS]} {
			    output-directive .CS
			    incr accept_RE 1
			} elseif {[match-text .PP]} {
			    output-directive .PP
			    incr accept_RE 1
			} elseif {[match-text .DS]} {
			    output-directive .DS
			    incr accept_RE 1
			} else {
			    output-directive $line
			}
		    }
		    .PP {
			if {[match-text @rest1 .br @rest2 .RS]} {
			    # yet another nroff kludge as above
			    man-puts "$para<DT>[long-toc $rest1]"
			    man-puts "<DT>[long-toc $rest2]<DD>"
			    incr accept_RE 1
			} elseif {[match-text @rest .RE]} {
			    # gad, this is getting ridiculous
			    if {!$accept_RE} {
				man-puts "</DL><P>$rest<DL>"
				backup-text 1
				set para {}
				break
			    } else {
				man-puts "<P>$rest"
				incr accept_RE -1
			    }
			} elseif {$accept_RE} {
			    output-directive $line
			} else {
			    backup-text 1
			    break
			}
		    }
		    .RE {
			if {!$accept_RE} {
			    backup-text 1
			    break
			}
			incr accept_RE -1
		    }
		    default {
			backup-text 1
			break
		    }
		}
	    } else {
		man-puts $line
	    }
	    set para <P>
	}
	man-puts "$para</DL>"
	lappend manual(section-toc) </DL>
	if {$accept_RE} {
	    manerror "missing .RE in output-IP-list"
	}
    }
}
##
## handle the NAME section lines
## there's only one line in the NAME section,
## consisting of a comma separated list of names,
## followed by a hyphen and a short description.
##
proc output-name {line} {
    global manual
    # split name line into pieces
    regexp {^([^-]+) - (.*)$} $line all head tail
    # output line to manual page untouched
    man-puts "$head &mdash; $tail"
    # output line to long table of contents
    lappend manual(section-toc) "<DL><DD>$head &mdash; $tail</DD></DL>"
    # separate out the names for future reference
    foreach name [split $head ,] {
	set name [string trim $name]
	if {[llength $name] > 1} {
	    manerror "name has a space: {$name}\nfrom: $line"
	}
	lappend manual(wing-toc) $name
	lappend manual(name-$name) $manual(wing-file)/$manual(name)
    }
}
##
## build a cross-reference link if appropriate
##
proc cross-reference {ref} {
    global manual remap_link_target
    global ensemble_commands exclude_refs_map exclude_when_followed_by_map
    set lref [string tolower $ref]
    if {[string match "Tcl_*" $ref] || [string match "Tk_*" $ref]} {
	set lref $ref
    } elseif {$ref eq "Tcl"} {
	set lref $ref
    } elseif {
	[regexp {^[A-Z0-9 ?!]+$} $ref]
	&& [info exists manual($manual(name)-id-$ref)]
    } {
	return "<A HREF=\"#$manual($manual(name)-id-$ref)\">$ref</A>"
    }
    ##
    ## apply a link remapping if available
    ##
    if {[info exists remap_link_target($lref)]} {
	set lref $remap_link_target($lref)
    }
    ##
    ## nothing to reference
    ##
    if {![info exists manual(name-$lref)]} {
	foreach name $ensemble_commands {
	    if {[regexp "^$name \[a-z0-9]*\$" $lref] && \
		    [info exists manual(name-$name)] && \
		    $manual(tail) ne "$name.n"} {
		return "<A HREF=\"../$manual(name-$name).htm\">$ref</A>"
	    }
	}
	if {$lref in {end}} {
	    # no good place to send this tcl token?
	}
	return $ref
    }
    ##
    ## would be a self reference
    ##
    foreach name $manual(name-$lref) {
	if {"$manual(wing-file)/$manual(name)" in $name} {
	    return $ref
	}
    }
    ##
    ## multiple choices for reference
    ##
    if {[llength $manual(name-$lref)] > 1} {
	set tcl_i [lsearch -glob $manual(name-$lref) *TclCmd*]
	set tcl_ref [lindex $manual(name-$lref) $tcl_i]
	set tk_i [lsearch -glob $manual(name-$lref) *TkCmd*]
	set tk_ref [lindex $manual(name-$lref) $tk_i]
	if {$tcl_i >= 0 && $manual(wing-file) eq "TclCmd"
		|| $manual(wing-file) eq "TclLib"} {
	    return "<A HREF=\"../$tcl_ref.htm\">$ref</A>"
	}
	if {$tk_i >= 0 && $manual(wing-file) eq "TkCmd"
		|| $manual(wing-file) eq "TkLib"} {
	    return "<A HREF=\"../$tk_ref.htm\">$ref</A>"
	}
	if {$lref eq "exit" && $manual(tail) eq "tclsh.1" && $tcl_i >= 0} {
	    return "<A HREF=\"../$tcl_ref.htm\">$ref</A>"
	}
	puts stderr "multiple cross reference to $ref in $manual(name-$lref) from $manual(wing-file)/$manual(tail)"
	return $ref
    }
    ##
    ## exceptions, sigh, to the rule
    ##
    if {[info exists exclude_when_followed_by_map($manual(tail))]} {
	upvar 1 tail tail
	set following_word [lindex [regexp -inline {\S+} $tail] 0]
	foreach {this that} $exclude_when_followed_by_map($manual(tail)) {
	    # only a ref if $this is not followed by $that
	    if {$lref eq $this && [string match $that* $following_word]} {
		return $ref
	    }
	}
    }
    if {
	[info exists exclude_refs_map($manual(tail))]
	&& $lref in $exclude_refs_map($manual(tail))
    } {
	return $ref
    }
    ##
    ## return the cross reference
    ##
    return "<A HREF=\"../$manual(name-$lref).htm\">$ref</A>"
}
##
## reference generation errors
##
proc reference-error {msg text} {
    global manual
    puts stderr "$manual(tail): $msg: {$text}"
    return $text
}
##
## insert as many cross references into this text string as are appropriate
##
proc insert-cross-references {text} {
    global manual
    ##
    ## we identify cross references by:
    ##     ``quotation''
    ##    <B>emboldening</B>
    ##    Tcl_ prefix
    ##    Tk_ prefix
    ##	  [a-zA-Z0-9]+ manual entry
    ## and we avoid messing with already anchored text
    ##
    ##
    ## find where each item lives
    ##
    array set offset [list \
	    anchor [string first {<A } $text] \
	    end-anchor [string first {</A>} $text] \
	    quote [string first {``} $text] \
	    end-quote [string first {''} $text] \
	    bold [string first {<B>} $text] \
	    end-bold [string first {</B>} $text] \
	    tcl [string first {Tcl_} $text] \
	    tk [string first {Tk_} $text] \
	    Tcl1 [string first {Tcl manual entry} $text] \
	    Tcl2 [string first {Tcl overview manual entry} $text] \
	    ]
    ##
    ## accumulate a list
    ##
    foreach name [array names offset] {
	if {$offset($name) >= 0} {
	    set invert($offset($name)) $name
	    lappend offsets $offset($name)
	}
    }
    ##
    ## if nothing, then we're done.
    ##
    if {![info exists offsets]} {
	return $text
    }
    ##
    ## sort the offsets
    ##
    set offsets [lsort -integer $offsets]
    ##
    ## see which we want to use
    ##
    switch -exact -- $invert([lindex $offsets 0]) {
	anchor {
	    if {$offset(end-anchor) < 0} {
		return [reference-error {Missing end anchor} $text]
	    }
	    set head [string range $text 0 $offset(end-anchor)]
	    set tail [string range $text [expr {$offset(end-anchor)+1}] end]
	    return $head[insert-cross-references $tail]
	}
	quote {
	    if {$offset(end-quote) < 0} {
		return [reference-error "Missing end quote" $text]
	    }
	    if {$invert([lindex $offsets 1]) eq "tk"} {
		set offsets [lreplace $offsets 1 1]
	    }
	    if {$invert([lindex $offsets 1]) eq "tcl"} {
		set offsets [lreplace $offsets 1 1]
	    }
	    switch -exact -- $invert([lindex $offsets 1]) {
		end-quote {
		    set head [string range $text 0 [expr {$offset(quote)-1}]]
		    set body [string range $text [expr {$offset(quote)+2}] \
			    [expr {$offset(end-quote)-1}]]
		    set tail [string range $text \
			    [expr {$offset(end-quote)+2}] end]
		    return "$head``[cross-reference $body]''[insert-cross-references $tail]"
		}
		bold -
		anchor {
		    set head [string range $text \
			    0 [expr {$offset(end-quote)+1}]]
		    set tail [string range $text \
			    [expr {$offset(end-quote)+2}] end]
		    return "$head[insert-cross-references $tail]"
		}
	    }
	    return [reference-error "Uncaught quote case" $text]
	}
	bold {
	    if {$offset(end-bold) < 0} {
		return $text
	    }
	    if {$invert([lindex $offsets 1]) eq "tk"} {
		set offsets [lreplace $offsets 1 1]
	    }
	    if {$invert([lindex $offsets 1]) eq "tcl"} {
		set offsets [lreplace $offsets 1 1]
	    }
	    switch -exact -- $invert([lindex $offsets 1]) {
		end-bold {
		    set head [string range $text 0 [expr {$offset(bold)-1}]]
		    set body [string range $text [expr {$offset(bold)+3}] \
			    [expr {$offset(end-bold)-1}]]
		    set tail [string range $text \
			    [expr {$offset(end-bold)+4}] end]
		    return "$head<B>[cross-reference $body]</B>[insert-cross-references $tail]"
		}
		anchor {
		    set head [string range $text \
			    0 [expr {$offset(end-bold)+3}]]
		    set tail [string range $text \
			    [expr {$offset(end-bold)+4}] end]
		    return "$head[insert-cross-references $tail]"
		}
	    }
	    return [reference-error "Uncaught bold case" $text]
	}
	tk {
	    set head [string range $text 0 [expr {$offset(tk)-1}]]
	    set tail [string range $text $offset(tk) end]
	    if {![regexp {^(Tk_\w+)(.*)$} $tail all body tail]} {
		return [reference-error "Tk regexp failed" $text]
	    }
	    return $head[cross-reference $body][insert-cross-references $tail]
	}
	tcl {
	    set head [string range $text 0 [expr {$offset(tcl)-1}]]
	    set tail [string range $text $offset(tcl) end]
	    if {![regexp {^(Tcl_\w+)(.*)$} $tail all body tail]} {
		return [reference-error {Tcl regexp failed} $text]
	    }
	    return $head[cross-reference $body][insert-cross-references $tail]
	}
	Tcl1 -
	Tcl2 {
	    set off [lindex $offsets 0]
	    set head [string range $text 0 [expr {$off-1}]]
	    set body Tcl
	    set tail [string range $text [expr {$off+3}] end]
	    return $head[cross-reference $body][insert-cross-references $tail]
	}
	end-anchor -
	end-bold -
	end-quote {
	    return [reference-error "Out of place $invert([lindex $offsets 0])" $text]
	}
    }
}
##
## process formatting directives
##
proc output-directive {line} {
    global manual
    # process format directive
    split-directive $line code rest
    switch -exact -- $code {
	.BS - .BE {
	    # man-puts <HR>
	}
	.SH - .SS {
	    # drain any open lists
	    # announce the subject
	    set manual(section) $rest
	    # start our own stack of stuff
	    set manual($manual(name)-$manual(section)) {}
	    lappend manual(has-$manual(section)) $manual(name)
	    if {$code ne ".SS"} {
		man-puts "<H3>[long-toc $manual(section)]</H3>"
	    } else {
		man-puts "<H4>[long-toc $manual(section)]</H4>"
	    }
	    # some sections can simply free wheel their way through the text
	    # some sections can be processed in their own loops
	    switch -exact -- [string index $code end]:$manual(section) {
		H:NAME {
		    set names {}
		    while {1} {
			set line [next-text]
			if {[is-a-directive $line]} {
			    backup-text 1
			    output-name [join $names { }]
			    return
			}
			lappend names [string trim $line]
		    }
		}
		H:SYNOPSIS {
		    lappend manual(section-toc) <DL>
		    while {1} {
			if {
			    [next-op-is .nf rest]
			    || [next-op-is .br rest]
			    || [next-op-is .fi rest]
			} {
			    continue
			}
			if {
			    [next-op-is .SH rest]
			    || [next-op-is .SS rest]
			    || [next-op-is .BE rest]
			    || [next-op-is .SO rest]
			} {
			    backup-text 1
			    break
			}
			if {[next-op-is .sp rest]} {
			    #man-puts <P>
			    continue
			}
			set more [next-text]
			if {[is-a-directive $more]} {
			    manerror "in SYNOPSIS found $more"
			    backup-text 1
			    break
			}
			foreach more [split $more \n] {
			    regexp {^(\s*)(.*)} $more -> spaces more
			    set spaces [string map {" " "&nbsp;"} $spaces]
			    if {[string length $spaces]} {
				set spaces <TT>$spaces</TT>
			    }
			    man-puts $spaces$more<BR>
			    if {$manual(wing-file) in {TclLib TkLib}} {
				lappend manual(section-toc) <DD>$more
			    }
			}
		    }
		    lappend manual(section-toc) </DL>
		    return
		}
		{H:SEE ALSO} {
		    while {[more-text]} {
			if {[next-op-is .SH rest] || [next-op-is .SS rest]} {
			    backup-text 1
			    return
			}
			set more [next-text]
			if {[is-a-directive $more]} {
			    manerror "$more"
			    backup-text 1
			    return
			}
			set nmore {}
			foreach cr [split $more ,] {
			    set cr [string trim $cr]
			    if {![regexp {^<B>.*</B>$} $cr]} {
				set cr <B>$cr</B>
			    }
			    if {[regexp {^<B>(.*)\([13n]\)</B>$} $cr all name]} {
				set cr <B>$name</B>
			    }
			    lappend nmore $cr
			}
			man-puts [join $nmore {, }]
		    }
		    return
		}
		H:KEYWORDS {
		    while {[more-text]} {
			if {[next-op-is .SH rest] || [next-op-is .SS rest]} {
			    backup-text 1
			    return
			}
			set more [next-text]
			if {[is-a-directive $more]} {
			    manerror "$more"
			    backup-text 1
			    return
			}
			set keys {}
			foreach key [split $more ,] {
			    set key [string trim $key]
			    lappend manual(keyword-$key) [list $manual(name) \
				    $manual(wing-file)/$manual(name).htm]
			    set initial [string toupper [string index $key 0]]
			    lappend keys "<A href=\"../Keywords/$initial.htm\#$key\">$key</A>"
			}
			man-puts [join $keys {, }]
		    }
		    return
		}
	    }
	    if {[next-op-is .IP rest]} {
		output-IP-list $code .IP $rest
		return
	    }
	    if {[next-op-is .PP rest]} {
		return
	    }
	    return
	}
	.SO {
	    # When there's a sequence of multiple .SO chunks, process into one
	    set optslist {}
	    while 1 {
		if {[match-text @stuff .SE]} {
		    foreach opt [split $stuff \n\t] {
			lappend optslist [list $opt $rest]
		    }
		} else {
		    manerror "unexpected .SO format:\n[expand-next-text 2]"
		}
		if {![next-op-is .SO rest]} {
		    break
		}
	    }
	    output-directive {.SH STANDARD OPTIONS}
	    man-puts <DL>
	    lappend manual(section-toc) <DL>
	    foreach optionpair [lsort -dictionary -index 0 $optslist] {
		lassign $optionpair option targetPage
		man-puts "<DT><B>[std-option-toc $option $targetPage]</B>"
	    }
	    man-puts </DL>
	    lappend manual(section-toc) </DL>
	}
	.OP {
	    output-widget-options $rest
	    return
	}
	.IP {
	    output-IP-list .IP .IP $rest
	    return
	}
	.PP {
	    man-puts <P>
	}
	.RS {
	    output-RS-list
	    return
	}
	.RE {
	    manerror "unexpected .RE"
	    return
	}
	.br {
	    man-puts <BR>
	    return
	}
	.DE {
	    manerror "unexpected .DE"
	    return
	}
	.DS {
	    if {[next-op-is .ta rest]} {
		# skip the leading .ta directive if it is there
	    }
	    if {[match-text @stuff .DE]} {
		set td "<td><p class=\"tablecell\">"
		set bodyText [string map [list \n <tr>$td \t $td] \n$stuff]
		man-puts "<dl><dd><table border=\"0\">$bodyText</table></dl>"
		#man-puts <PRE>$stuff</PRE>
	    } elseif {[match-text .fi @ul1 @ul2 .nf @stuff .DE]} {
		man-puts "<PRE>[lindex $ul1 1][lindex $ul2 1]\n$stuff</PRE>"
	    } else {
		manerror "unexpected .DS format:\n[expand-next-text 2]"
	    }
	    return
	}
	.CS {
	    if {[next-op-is .ta rest]} {
		# ???
	    }
	    if {[match-text @stuff .CE]} {
		man-puts <PRE>$stuff</PRE>
	    } else {
		manerror "unexpected .CS format:\n[expand-next-text 2]"
	    }
	    return
	}
	.CE {
	    manerror "unexpected .CE"
	    return
	}
	.sp {
	    man-puts <P>
	}
	.ta {
	    manerror "ignoring $line"
	}
	.nf {
	    if {[match-text @more .fi]} {
		foreach more [split $more \n] {
		    man-puts $more<BR>
		}
	    } elseif {[match-text .RS @more .RE .fi]} {
		man-puts <DL><DD>
		foreach more [split $more \n] {
		    man-puts $more<BR>
		}
		man-puts </DL>
	    } elseif {[match-text .RS @more .RS @more2 .RE .RE .fi]} {
		man-puts <DL><DD>
		foreach more [split $more \n] {
		    man-puts $more<BR>
		}
		man-puts <DL><DD>
		foreach more2 [split $more2 \n] {
		    man-puts $more2<BR>
		}
		man-puts </DL></DL>
	    } elseif {[match-text .RS @more .RS @more2 .RE @more3 .RE .fi]} {
		man-puts <DL><DD>
		foreach more [split $more \n] {
		    man-puts $more<BR>
		}
		man-puts <DL><DD>
		foreach more2 [split $more2 \n] {
		    man-puts $more2<BR>
		}
		man-puts </DL><DD>
		foreach more3 [split $more3 \n] {
		    man-puts $more3<BR>
		}
		man-puts </DL>
	    } elseif {[match-text .sp .RS @more .RS @more2 .sp .RE .RE .fi]} {
		man-puts <P><DL><DD>
		foreach more [split $more \n] {
		    man-puts $more<BR>
		}
		man-puts <DL><DD>
		foreach more2 [split $more2 \n] {
		    man-puts $more2<BR>
		}
		man-puts </DL></DL><P>
	    } elseif {[match-text .RS .sp @more .sp .RE .fi]} {
		man-puts <P><DL><DD>
		foreach more [split $more \n] {
		    man-puts $more<BR>
		}
		man-puts </DL><P>
	    } else {
		manerror "ignoring $line"
	    }
	}
	.fi {
	    manerror "ignoring $line"
	}
	.na -
	.ad -
	.UL -
	.ne {
	    manerror "ignoring $line"
	}
	default {
	    manerror "unrecognized format directive: $line"
	}
    }
}
##
## merge copyright listings
##
proc merge-copyrights {l1 l2} {
    set merge {}
    set re1 {^Copyright +(?:\(c\)|\\\(co|&copy;) +(\w.*?)(?:all rights reserved)?(?:\. )*$}
    set re2 {^(\d+) +(?:by +)?(\w.*)$}         ;# date who
    set re3 {^(\d+)-(\d+) +(?:by +)?(\w.*)$}   ;# from to who
    set re4 {^(\d+), *(\d+) +(?:by +)?(\w.*)$} ;# date1 date2 who
    foreach copyright [concat $l1 $l2] {
	if {[regexp -nocase -- $re1 $copyright -> info]} {
	    set info [string trimright $info ". "] ; # remove extra period
	    if {[regexp -- $re2 $info -> date who]} {
		lappend dates($who) $date
		continue
	    } elseif {[regexp -- $re3 $info -> from to who]} {
		for {set date $from} {$date <= $to} {incr date} {
		    lappend dates($who) $date
		}
		continue
	    } elseif {[regexp -- $re3 $info -> date1 date2 who]} {
		lappend dates($who) $date1 $date2
		continue
	    }
	}
	puts "oops: $copyright"
    }
    foreach who [array names dates] {
	set list [lsort -dictionary $dates($who)]
	if {[llength $list] == 1 || [lindex $list 0] eq [lrange $list end end]} {
	    lappend merge "Copyright &copy; [lindex $list 0] $who"
	} else {
	    lappend merge "Copyright &copy; [lindex $list 0]-[lrange $list end end] $who"
	}
    }
    return [lsort -dictionary $merge]
}

proc makedirhier {dir} {
    try {
	if {![file isdirectory $dir]} {
	    file mkdir $dir
	}
    } on error msg {
	return -code error "cannot create directory $dir: $msg"
    }
}

proc addbuffer {args} {
    global manual
    if {$manual(partial-text) ne ""} {
	append manual(partial-text) \n
    }
    append manual(partial-text) [join $args ""]
}
proc flushbuffer {} {
    global manual
    if {$manual(partial-text) ne ""} {
	lappend manual(text) [process-text $manual(partial-text)]
	set manual(partial-text) ""
    }
}

return
