#                    H E L P C O M M . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Description -
#	Routines common to the BRL-CAD help system.
#

proc help_comm {data args} {
    global $data

    if {[llength $args] > 0} {
	foreach cmd [join $args] {
	    if [info exists [subst $data]($cmd)] {
		append info "Usage: $cmd [lindex [subst $[subst $data]($cmd)] 0]\n\nDescription: [lindex [subst $[subst $data]($cmd)] 1]\n"
	    } else {
		append info "No help found for $cmd\n"
	    }
	}
    } else {
	foreach cmd [lsort [array names [subst $data]]] {
	    append info "$cmd [lindex [subst $[subst $data]($cmd)] 0]\n\t[lindex [subst $[subst $data]($cmd)] 1]\n"
	}
    }
    return $info
}


proc ?_comm {data min ncol} {
    global $data

    set i 1
    foreach cmd [lsort [array names [subst $data]]] {
	append info [format "%-[subst $min]s" $cmd]
	if { ![expr $i % [subst $ncol]] } {
	    append info "\n"
	}
	incr i
    }
    return $info
}


proc apropos_comm {data key} {
    global $data

    set info ""
    foreach cmd [lsort [array names [subst $data]]] {
	if {[string first $key $cmd] != -1} {
	    append info "$cmd "
	} elseif {[string first $key [lindex [subst $[subst $data]($cmd)] 0]] != -1} {
	    append info "$cmd "
	} elseif {[string first $key [lindex [subst $[subst $data]($cmd)] 1]] != -1} {
	    append info "$cmd "
	}
    }

    return $info
}


proc manpage_search_terms {query} {
    set terms {}
    set term ""
    foreach c [split [string tolower $query] ""] {
	if {[string is alnum -strict $c]} {
	    append term $c
	} elseif {$term != ""} {
	    lappend terms $term
	    set term ""
	}
    }
    if {$term != ""} {
	lappend terms $term
    }
    return $terms
}


proc manpage_search_count {haystack needle} {
    set count 0
    set start 0
    while 1 {
	set idx [string first $needle $haystack $start]
	if {$idx < 0} {
	    break
	}
	incr count
	set start [expr {$idx + [string length $needle]}]
    }
    return $count
}


proc manpage_search_score {query fields} {
    set terms [manpage_search_terms $query]
    if {[llength $terms] == 0} {
	return -1
    }

    set score 0
    foreach term $terms {
	set term_score 0
	foreach {weight text} $fields {
	    set cnt [manpage_search_count [string tolower $text] $term]
	    if {$cnt > 0} {
		if {$cnt > 20} {
		    set cnt 20
		}
		incr term_score [expr {$weight * $cnt}]
	    }
	}
	if {$term_score == 0} {
	    return -1
	}
	incr score $term_score
    }
    return $score
}


proc manpage_search_normalize {text} {
    regsub -all {[[:space:]]+} $text { } text
    return [string trim $text]
}


proc manpage_search_html_to_text {html} {
    regsub -all -nocase {<style[^>]*>.*?</style>} $html { } html
    regsub -all -nocase {<script[^>]*>.*?</script>} $html { } html
    regsub -all {<[^>]+>} $html { } text
    set text [string map [list "&lt;" "<" "&gt;" ">" "&amp;" "&" "&quot;" "\"" "&#34;" "\"" "&#39;" "'" "&apos;" "'" "&nbsp;" " "] $text]
    return [manpage_search_normalize $text]
}


proc manpage_search_html_section {html section} {
    set lhtml [string tolower $html]
    set marker "id=\"_[string tolower $section]\""
    set pos [string first $marker $lhtml]
    if {$pos < 0} {
	return ""
    }
    set start [string first "</h2>" $lhtml $pos]
    if {$start < 0} {
	return ""
    }
    incr start 5
    set end [string first "<h2" $lhtml $start]
    if {$end < 0} {
	set sect [string range $html $start end]
    } else {
	set sect [string range $html $start [expr {$end - 1}]]
    }
    return [manpage_search_html_to_text $sect]
}


proc manpage_search_roff_inline {line} {
    set line [string map [list "\\-" "-" "\\(em" "--" "\\(en" "-" "\\(aq" "'" "\\&" ""] $line]
    regsub -all {\\f.} $line {} line
    regsub -all {\\[a-zA-Z]+} $line {} line
    return $line
}


proc manpage_search_roff_macro_text {line} {
    if {[string index $line 0] != "."} {
	return [manpage_search_roff_inline $line]
    }
    if {[regexp {^\.(SH|SS)[[:space:]]+(.+)$} $line -> macro text]} {
	return [manpage_search_roff_inline $text]
    }
    if {[regexp {^\.(B|I|BR|IR|BI|IB)[[:space:]]+(.+)$} $line -> macro text]} {
	return [manpage_search_roff_inline $text]
    }
    return ""
}


proc manpage_search_roff_to_text {roff} {
    set text ""
    foreach line [split $roff "\n"] {
	set ltxt [manpage_search_roff_macro_text $line]
	if {$ltxt != ""} {
	    append text $ltxt "\n"
	}
    }
    return [manpage_search_normalize $text]
}


proc manpage_search_roff_section {roff section} {
    set active 0
    set text ""
    set section [string tolower $section]
    foreach line [split $roff "\n"] {
	if {[regexp {^\.SH[[:space:]]+(.+)$} $line -> heading]} {
	    set heading [string trim [string tolower [manpage_search_roff_inline $heading]] "\""]
	    set active [expr {$heading == $section}]
	    continue
	}
	if {$active} {
	    set ltxt [manpage_search_roff_macro_text $line]
	    if {$ltxt != ""} {
		append text $ltxt "\n"
	    }
	}
    }
    return [manpage_search_normalize $text]
}


proc manpage_search_section_text {raw format section} {
    if {$format == "html"} {
	return [manpage_search_html_section $raw $section]
    }
    return [manpage_search_roff_section $raw $section]
}


proc manpage_search_plain_text {raw format} {
    if {$format == "html"} {
	return [manpage_search_html_to_text $raw]
    }
    return [manpage_search_roff_to_text $raw]
}


proc manpage_search_summary {name_section} {
    set name_section [manpage_search_normalize $name_section]
    if {[regexp {^[^-]+[[:space:]]+-[[:space:]]+(.+)$} $name_section -> desc]} {
	return [string trim $desc]
    }
    if {[regexp {^[^-]+-(.+)$} $name_section -> desc]} {
	return [string trim $desc]
    }
    return $name_section
}


proc manpage_search_section_dir {section} {
    switch -- $section {
	1 - man1 { return man1 }
	3 - man3 { return man3 }
	5 - man5 { return man5 }
	n - mann { return mann }
	default { return $section }
    }
}


proc manpage_search_section_label {section_dir} {
    switch -- $section_dir {
	man1 { return 1 }
	man3 { return 3 }
	man5 { return 5 }
	mann { return n }
	default { return $section_dir }
    }
}


proc manpage_search_ext {section_dir format} {
    if {$format == "html"} {
	return html
    }
    if {$section_dir == "mann"} {
	return nged
    }
    return [string range $section_dir 3 end]
}


proc manpage_search_files {sections} {
    set files {}
    foreach section $sections {
	set section_dir [manpage_search_section_dir $section]
	set section_label [manpage_search_section_label $section_dir]
	set found 0

	foreach spec [list [list [file join [bu_dir doc] html] html] [list [bu_dir man] roff]] {
	    set root [lindex $spec 0]
	    set format [lindex $spec 1]
	    set dir [file join $root $section_dir]
	    if {![file isdirectory $dir]} {
		continue
	    }

	    set ext [manpage_search_ext $section_dir $format]
	    foreach path [glob -nocomplain -directory $dir *.$ext] {
		set name [file rootname [file tail $path]]
		lappend files [list $name $section_label $path $format]
	    }
	    set found 1
	    break
	}

	if {!$found} {
	    continue
	}
    }
    return $files
}


proc manpage_search_index_file {name section path format} {
    global manpage_search_cache

    set key "$path|$format"
    if {[info exists manpage_search_cache($key)]} {
	return $manpage_search_cache($key)
    }

    if {[catch {
	set fp [open $path r]
	fconfigure $fp -encoding utf-8 -translation lf
	set raw [read $fp]
	close $fp
    }]} {
	catch {close $fp}
	return ""
    }

    set name_section [manpage_search_section_text $raw $format name]
    set summary [manpage_search_summary $name_section]
    set synopsis [manpage_search_section_text $raw $format synopsis]
    set body [manpage_search_plain_text $raw $format]

    set manpage_search_cache($key) [list $name $section $summary $synopsis $body]
    return $manpage_search_cache($key)
}


proc manpage_search_rank_record {query mode record} {
    set name [lindex $record 0]
    set summary [lindex $record 2]
    set synopsis [lindex $record 3]
    set body [lindex $record 4]

    set fields [list 200 $name 80 $summary 40 $synopsis]
    if {$mode == "full"} {
	lappend fields 8 $body
    }

    return [manpage_search_score $query $fields]
}


proc manpage_search {query args} {
    set mode short
    set sections {mann}
    set format names

    set i 0
    while {$i < [llength $args]} {
	set arg [lindex $args $i]
	switch -- $arg {
	    -mode {
		incr i
		set mode [lindex $args $i]
	    }
	    -sections {
		incr i
		set sections [lindex $args $i]
	    }
	    -format {
		incr i
		set format [lindex $args $i]
	    }
	}
	incr i
    }

    set ranked {}
    foreach fileinfo [manpage_search_files $sections] {
	set record [manpage_search_index_file [lindex $fileinfo 0] [lindex $fileinfo 1] [lindex $fileinfo 2] [lindex $fileinfo 3]]
	if {$record == ""} {
	    continue
	}
	set score [manpage_search_rank_record $query $mode $record]
	if {$score >= 0} {
	    lappend ranked [list $score [lindex $record 0] [lindex $record 1] [lindex $record 2]]
	}
    }

    set ranked [lsort -decreasing -integer -index 0 $ranked]
    set info ""
    foreach result $ranked {
	set name [lindex $result 1]
	set section [lindex $result 2]
	set summary [lindex $result 3]
	if {$format == "lines"} {
	    if {$summary != ""} {
		append info [format "%s(%s) - %s\n" $name $section $summary]
	    } else {
		append info [format "%s(%s)\n" $name $section]
	    }
	} else {
	    append info "$name "
	}
    }

    return [string trimright $info]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
