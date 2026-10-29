#                N A V I G A T I O N _ T E S T . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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

# Developer check requiring a usable Tk desktop (native or virtual).
# Run with: cmake --build <build-directory> --target brlman-navigation-test
# This check is excluded from the default build and CTest suites.

if {[catch {package require Tk 8.6} message]} {
    puts "Tk tests skipped: $message"
    exit 77
}

proc assert_equal {description actual expected} {
    if {$actual ne $expected} {
	error "$description: got {$actual}, expected {$expected}"
    }
}

proc write_page {root section name body} {
    set directory [file join $root $section]
    file mkdir $directory
    set channel [open [file join $directory $name.html] w]
    try {
	puts $channel "<title>$name $section</title>"
	puts $channel "<h2 id=\"_name\">NAME</h2><p>$name - fixture manual</p>"
	puts $channel "<h2 id=\"_description\">DESCRIPTION</h2>$body"
    } finally {
	close $channel
    }
}

proc choose_section {viewer label} {
    set selector [$viewer component section_selector]
    $selector set $label
    event generate $selector <<ComboboxSelected>>
    update
}

proc choose_page {viewer label} {
    set listbox [$viewer component manpagelistbox]
    set index [lsearch -exact [$listbox get 0 end] $label]
    if {$index < 0} {
	error "Page '$label' is missing from the index"
    }
    $listbox selection clear 0 end
    $listbox selection set $index
    event generate $listbox <<ListboxSelect>>
    update
}

set root [file join [pwd] man-browser-test-[pid]]
if {[catch {
    load [lindex $argv 0] Tklitehtml
    package require Iwidgets
    set scripts [file join [file dirname [info script]] .. tclscripts]
    source [file join $scripts helpcomm.tcl]
    # Browser search and section selection must use the canonical helpers.
    foreach command [info commands ::manpage_search_*] {
	rename $command {}
    }
    source [file join $scripts man_browser.tcl]
    wm withdraw .
    set ::background_errors {}
    proc bgerror {message} {
	lappend ::background_errors $message
    }

    foreach {section name body} {
	man1 alpha {<p>Alpha manual.</p>}
	man1 shared {<div style="height: 1400px">Program manual.</div>}
	man3 beta {<p>Beta manual.</p>}
	man3 shared {<p>Library manual.</p>}
	mann gamma {<p>deepneedle</p>}
	mann shared {<p>Command manual.</p>}
    } {
	write_page $root $section $name $body
    }

    set viewer [ManBrowser .viewer -path $root]
    $viewer activate
    update
    set listbox [$viewer component manpagelistbox]
    set browser [$viewer childsite].browser.htmlview
    set selector [$viewer component section_selector]
    set search [$viewer component search_frame]
    set all_pages [list {alpha (1)} {beta (3)} {gamma (n)} \
	{shared (1)} {shared (3)} {shared (n)}]
    assert_equal "default section" [$selector get] "All sections"
    assert_equal "combined alphabetical index" [$listbox get 0 end] $all_pages

    foreach section {1 3 n} directory {man1 man3 mann} {
	choose_page $viewer "shared ($section)"
	assert_equal "duplicate page in section $section" \
	    [$browser title] "shared $directory"
    }
    choose_page $viewer "shared (1)"
    [$browser html] yview moveto 0.5
    set view [[$browser html] yview]
    choose_section $viewer "3 - Libraries"
    assert_equal "library index" [$listbox get 0 end] [list {beta (3)} {shared (3)}]
    assert_equal "section filter preserves document" [$browser title] "shared man1"
    assert_equal "section filter preserves scroll position" [[$browser html] yview] $view
    assert_equal "hidden document is not selected" [$listbox curselection] {}
    choose_section $viewer "5 - Formats"
    assert_equal "missing section has an empty index" [$listbox size] 0
    choose_section $viewer "All sections"
    assert_equal "visible document selection is restored" \
	[$listbox get [$listbox curselection]] "shared (1)"

    choose_page $viewer "shared (n)"
    $viewer search shared short
    assert_equal "search across duplicate names" [$listbox get 0 end] \
	[list {shared (1)} {shared (3)} {shared (n)}]
    assert_equal "programmatic search selects one result" [$listbox curselection] 0
    choose_page $viewer "shared (3)"
    assert_equal "filtered selection uses its own section" [$browser title] "shared man3"
    $search.controls.clear invoke
    assert_equal "clear restores the combined index" [$listbox get 0 end] $all_pages
    assert_equal "clear preserves the displayed manual" [$browser title] "shared man3"

    focus -force $search.entry
    update
    $search.entry insert 0 beta
    event generate $search.entry <KeyRelease> -keysym a
    update
    assert_equal "typing filters the index" [$listbox get 0 end] [list {beta (3)}]
    assert_equal "typing preserves the displayed manual" [$browser title] "shared man3"
    $search.controls.clear invoke

    $viewer search deepneedle short
    assert_equal "short search excludes body text" [$listbox size] 0
    $viewer search deepneedle full
    assert_equal "full text search includes body text" [$listbox get 0 end] [list {gamma (n)}]
    assert_equal "full text result opens the correct section" [$browser title] "gamma mann"
    assert_equal "select a page hidden by the query" [$viewer select alpha] 1
    assert_equal "selection clears a conflicting query" [$search.entry get] ""
    assert_equal "programmatic selection opens the page" [$browser title] "alpha man1"
    assert_equal "programmatic selection restores the index" [$listbox get 0 end] $all_pages
    assert_equal "unknown page" [$viewer select missing] 0

    $browser findbar Alpha
    update
    assert_equal "find searches the displayed manual" \
	[$browser.find.count cget -text] "1 of 2"
    set first [string first {Alpha manual} [$browser text]]
    $browser selection set $first [expr {$first + [string length Alpha]}]
    update
    $browser.menu invoke Copy
    assert_equal "manual selection copies through the shared menu" \
	[clipboard get -displayof $browser] Alpha
    choose_page $viewer "beta (3)"
    assert_equal "find follows manual navigation" \
	[$browser.find.count cget -text] "No matches"
    $browser.find.close invoke
    assert_equal "closing find clears manual highlights" [$browser find current] {}

    set mged [ManBrowser .mged -path $root -defaultDir n -parentName MGED]
    assert_equal "MGED starts in its requested section" \
	[[$mged component section_selector] get] "n - MGED commands"
    assert_equal "MGED duplicate selection" [$mged select shared] 1
    assert_equal "MGED opens the command manual" \
	[[$mged childsite].browser.htmlview title] "shared mann"

    set other_root [file join $root other]
    write_page $other_root man3 zeta {<p>Independent manual collection.</p>}
    set other [ManBrowser .other -path $other_root]
    assert_equal "separate viewer collection" \
	[[$other component manpagelistbox] get 0 end] [list {zeta (3)}]
    $viewer search shared short
    assert_equal "viewer collections stay independent" [$listbox size] 3

    set plain [ManBrowser .plain -path $root -useToC 0]
    assert_equal "selection without an index" [$plain select gamma] 1
    assert_equal "document without an index" \
	[[$plain childsite].browser.htmlview title] "gamma mann"
    update
    assert_equal "background errors" $::background_errors {}
} message options]} {
    puts stderr [dict get $options -errorinfo]
    file delete -force $root
    exit 1
}
foreach viewer {.viewer .mged .other .plain} {
    destroy $viewer
}
file delete -force $root
puts "Manual page navigation passed"
exit 0

# Local Variables:
# mode: tcl
# tab-width: 8
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
