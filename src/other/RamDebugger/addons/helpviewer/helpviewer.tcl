
# lappend auto_path /c/compasser/addons
# lappend auto_path /c/TclTk/RamDebugger/addons
# lappend auto_path /compasser/addons
lappend auto_path [file dirname [file dirname [info script]]]

################################################################################
#  This software is copyrighted by Ramon Ribó (RAMSAN) ramsan@cimne.upc.es.
#  (http://gid.cimne.upc.es/ramsan) The following terms apply to all files
#  associated with the software unless explicitly disclaimed in individual files.

#  The authors hereby grant permission to use, copy, modify, distribute,
#  and license this software and its documentation for any purpose, provided
#  that existing copyright notices are retained in all copies and that this
#  notice is included verbatim in any distributions. No written agreement,
#  license, or royalty fee is required for any of the authorized uses.
#  Modifications to this software may be copyrighted by their authors
#  and need not follow the licensing terms described here, provided that
#  the new terms are clearly indicated on the first page of each file where
#  they apply.

#  IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
#  FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
#  ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
#  DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.

#  THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
#  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
#  IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
#  NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
#  MODIFICATIONS.
################################################################################



# requiring exactly 2.1 to avoid getting the one from Activestate
#catch { package require -exact Tkhtml 2.1 }
#package require -exact Tkhtml 2.0
package require Tkhtml;#de momento que funcione si tiene solo la version 2

package require BWidget
package require supergrid
package require dialogwin
package require fileutil
package require htmlparse
package require ncgi
package require struct 2.1

catch { package require img::jpeg }
catch { package require img::png }
catch { package require resizer }
package require msgcat

if { [info commands _] eq "" } {
    proc ::_ { args } {
	if { [regexp {(.*)#C#(.*)} [lindex $args 0] {} str comm] } {
	    set args [lreplace $args 0 0 $str]
	}
	return [uplevel 1 ::msgcat::mc $args]
    }
}

proc filenormalize { file } {

    if { $file == "New file" } { return $file }
    if { $file == "" } { return "" }

    set pwd [pwd]
    catch {
	cd [file dirname $file]
	set file [file join [pwd] [file tail $file]]
    }
    cd $pwd

    if { $::tcl_platform(platform) == "windows" } {
	catch { set file [file attributes $file -longname] }
    }

    return $file
}

if { [info commands tkTabToWindow] == "" } {
    proc tkTabToWindow {w} {
	focus $w
	after 100 {
	    set w [focus]
	    if {[string equal [winfo class $w] Entry]} {
		$w selection range 0 end
		$w icursor end
	    }
	}
    }
}

set comms [list tkButtonInvoke tkTextSelectTo tkEntryInsert tkEntryBackspace \
    tk_textCut tk_textCopy tk_textPaste tk_focusNext tk_focusPrev tkTextClosestGap \
    tkTextAutoScan tkCancelRepeat]

foreach i $comms {
    auto_load $i
    if {![llength [info commands $i]]} {
	#tk::unsupported::ExposePrivateCommand $i
	regsub {^tk} $i {::tk::} new
	interp alias "" $i "" $new
    }
}

namespace eval History {
    variable list ""
    variable pos 0
    variable menu

    proc Add { name } {
	variable list
	variable pos
	variable menu

	lappend list $name
	set pos [expr [llength $list]-1]
	if { $pos == 0 } {
	    if { [info exists menu] && [winfo exists $menu] } {
		$menu entryconf Backward -state disabled
	    }
	} else {
	    if { [info exists menu] && [winfo exists $menu] } {
		$menu entryconf Backward -state normal
	    }
	}
	if { [info exists menu] && [winfo exists $menu] } {
	    $menu entryconf Forward -state disabled
	}
    }
    proc GoHome { w } {
	variable list
	variable pos
	variable menu

	set pos 0
	if { [info exists menu] && [winfo exists $menu] } {
	    $menu entryconf Backward -state disabled
	}
	if { [info exists menu] && [winfo exists $menu] } {
	    $menu entryconf Forward -state normal
	}
	HelpViewer::LoadRef $w [lindex $list $pos] 0
    }
    proc GoBackward { w } {
	variable list
	variable pos
	variable menu

	if { ![info exists pos] } {
	    bell
	    return
	}
	incr pos -1
	if { $pos == 0 } {
	    if { [info exists menu] && [winfo exists $menu] } {
		$menu entryconf Backward -state disabled
	    }
	}
	if { [info exists menu] && [winfo exists $menu] } {
	    $menu entryconf Forward -state normal
	}
	HelpViewer::LoadRef $w [lindex $list $pos] 0
    }
    proc GoForward { w } {
	variable list
	variable pos
	variable menu
	
	if { $pos >= [expr [llength $list]-1] } {
	    bell
	    return
	}
	incr pos 1
	if { $pos == [expr [llength $list]-1] } {
	    if { [info exists menu] && [winfo exists $menu] } {
		$menu entryconf Forward -state disabled
	    }
	}
	if { [info exists menu] && [winfo exists $menu] } {
	    $menu entryconf Backward -state normal
	}
	HelpViewer::LoadRef $w [lindex $list $pos] 0
    }
}

namespace eval HelpPrefs {
    variable tttrick 0
    variable RunningAlone 0
}

namespace eval HelpViewer {

    variable HelpBaseDir
    variable LastFileList

    variable showImages 1
    variable OldImages
    variable Images
    variable BigImages
    variable SearchPos ""
    variable selection_state ""

    variable searchcase 0
    variable searchmode -exact
    variable searchcase 0
    variable searchFromBegin 0
    variable SearchType -forwards
    ::msgcat::mcload [file join [file dirname [info script]] msgs]
}

proc HelpViewer::GiveLastFile { w } {
    variable LastFileList
    set retval ""
    catch {
	set w [winfo toplevel $w]
	set retval $LastFileList($w)
    }
    return $retval
}
proc HelpViewer::EnterLastFile { w file } {
    variable LastFileList
    set w [winfo toplevel $w]
    set LastFileList($w) $file
}

#A font chooser routine.
#
#  $base.h.h config -fontcommand pickFont
proc HelpViewer::pickFont {size attrs} {
    global tcl_platform

    if { [lsearch [font names] HelpFont] != -1 } {
	set family [font conf HelpFont -family]
	set fsize [font conf HelpFont -size]
    } else {
	set family Helvetica
	switch $::tcl_platform(platform) {
	    windows { set fsize 11 }
	    default { set fsize 15 }
	}
    }

    if { $HelpPrefs::tttrick } {
	set a [expr {-1<[lsearch $attrs fixed]?{Symbol}:"$family"}]
    } else {
	set a [expr {-1<[lsearch $attrs fixed]?{Courier}:"$family"}]
    }
    set b [expr {-1<[lsearch $attrs italic]?{italic}:{roman}}]
    set c [expr {-1<[lsearch $attrs bold]?{bold}:{normal}}]
    set d [expr int($fsize*pow(1.2,$size-4))]
#     if { $tcl_platform(platform) != "windows"} {
#         set d [expr {3+int(12*pow(1.2,$size-4))}]
#     } else {
#         set d [expr {3+int(12*pow(1.2,$size-6))}]
#     }

    list $a $d $b $c
}

proc HelpViewer::CreateFrame { frame { hscroll 1 } {vscroll 1 }  } {
    frame $frame
    frame $frame.f1
    frame $frame.f2
    if { $hscroll } {
	set hscrollcommand "$frame.f2hsb set"
    } else { set xscrollcommand "" }
    if { $vscroll } {
	set yscrollcommand "$frame.f1vsb set"
    } else { set yscrollcommand "" }
    html $frame.f1.h \
    -xscrollcommand $hscrollcommand \
    -yscrollcommand $yscrollcommand \
    -padx 5 \
    -pady 9 \
	-formcommand FormCmd \
	-imagecommand [list ImageCmd $frame.f1.h] \
    -scriptcommand ScriptCmd \
    -appletcommand AppletCmd \
    -underlinehyperlinks 0 \
    -bg white -tablerelief raised \
    -resolvercommand ResolveUri \
    -exportselection 1 \
    -takefocus 1

    bind $frame.f1.h.x <1> "HelpViewer::HrefBinding $frame.h %x %y"
    if { $hscroll } {
	frame $frame.f2.sp -width [winfo reqwidth $base.h.vsb] -bd 2 -relief raised
	scrollbar $frame.f2.hsb -orient horizontal -command "$frame.f1.h xview"
	pack $base.f2.sp -side right -fill y
	pack $frame.f2.hsb -side left -fill x -expand 1
    }
    if { $vscroll } {
	scrollbar $frame.f1.vsb -orient vertical -command "$frame.f1.h yview"
	pack $frame.f1.vsb -side right -fill y -expand 1
    }
    pack $frame.f1.h -side left -fill both -expand 1
    pack $frame.f1 -side top -fill both -expand 1
    pack $frame.f2 -side bottom -fill x -expand 1
}

proc HelpViewer::FrameCmd { base type arglist } {
    global HelpPriv
    switch $type {
	frameset {
	    set HelpPriv(frameset) $arglist
	    foreach i [array names frame*] {
		uset HelpPriv($i)
	    }
	}
	frame {
	    set i 0
	    while { [info exists HelpPriv(frame$i)] } { incr i }
	    set HelpPriv(frame$i) $arglist
	}
	/frameset {
	    foreach "name args" "$HelpPriv(frameset)" {
		switch $name {
		    rows {
		    }
		}
	    }
	    set i 0
	    while { [info exists HelpPriv(frame$i)] } {
		foreach "name args" "$HelpPriv(frame$i)" {


		}
		incr i
	    }
	}
    }
    tk_messageBox -icon error -message $type----$arglist-- -type ok
}

# This routine is called for each form element
#
proc HelpViewer::FormCmd {n cmd args} {
    # puts "FormCmd: $n $cmd $args"
    switch $cmd {
	select -
	textarea -
	input {
	    set w [lindex $args 1]
	    label $w -image nogifsm
	}
    }
}

proc HelpViewer::do_not_delete_image { args } {
    set a 1
}

proc HelpViewer::ImageCmd { htmlwidget args } {
    variable showImages
    variable OldImages
    variable Images
    variable BigImages
    variable html
    global HelpPriv

    if {!$showImages} {
	if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
	    return smgray
	} else {
	    set img [image create photo]
	    $img copy smgray
	    return $img
	}
    }
    set fn [lindex $args 0]

    set list [file split $fn]
    if { [lindex $list 0] == "." } {
	set list [lrange $list 1 end]
    }
    set fn [eval [concat "file join $list"]]

    if { [file dirname $fn] == "." } {
	set fn [file tail $fn]
    }
    if {[info exists OldImages($fn)]} {
	set Images($fn) $OldImages($fn)
	unset OldImages($fn)
	return $Images($fn)
    }
    set fn [file join $HelpPriv(basedir) $fn]
    if {[catch {image create photo -file $fn} img]} {
	if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
	    return smgray
	} else {
	    set img [image create photo]
	    $img copy smgray
	    return $img
	}
    }
    set userwidth [lindex $args 1]
    set userheight [lindex $args 2]

    set widget_width [winfo width $html]
    if { $widget_width <= 1 } { set widget_width [winfo reqwidth $html] }

    if { $userwidth eq "" && [image width $img] > .9*$widget_width } {
	set userwidth [expr {int(.9*$widget_width)}]
    }
    if { $userwidth ne "" && [info commands img_zoom] ne "" } {
	if { [regexp {([\d.]+)%} $userwidth {} width_percent] } {
	    set pwidth [winfo width $htmlwidget]
	    if { $pwidth <= 1 } { set pwidth [winfo reqwidth $htmlwidget] }
	    set userwidth [expr {int($pwidth*$width_percent/100.0)}]
	}
	set userheight [expr {$userwidth*[image height $img]/[image width $img]}]
	set realimg [image create photo -width $userwidth -height $userheight]
	img_zoom $realimg $img Lanczos3
	image delete $img
	set img $realimg
    }
    if {[image width $img]*[image height $img]>20000} {
	set b [image create photo -width [image width $img] \
	    -height [image height $img]]
	set BigImages($b) $img
	set img $b
	after idle "HelpViewer::MoveBigImage $b"
    }
    set Images($fn) $img
    return $img
}

proc HelpViewer::MoveBigImage b {
    variable BigImages

    if {![info exists BigImages($b)]} return
    $b copy $BigImages($b)
    image delete $BigImages($b)
    unset BigImages($b)
    update
}


# This routine is called for every <SCRIPT> markup
#
proc HelpViewer::ScriptCmd {args} {
    # puts "ScriptCmd: $args"
}

# This routine is called for every <APPLET> markup
#
proc HelpViewer::AppletCmd {w arglist} {
    # puts "AppletCmd: w=$w arglist=$arglist"
    label $w -text "The Applet $w" -bd 2 -relief raised
}

proc HelpViewer::Motion { w x y } {
    variable selection_state

    set txt ""

    set PATH [winfo parent [winfo parent $w]]
    $PATH configure -cursor ""

    set node ""
    foreach node [$w node $x $y] {
	for {set n $node} {$n!=""} {set n [$n parent]} {
	    set tag [$n tag]
	    if {$tag=="a" && [$n attr -default "" href]!=""} {
		$PATH configure -cursor hand2
		set txt "hyper-link: [$n attr href]"
		break
	    }
	}
    }
    if {$txt == "" && $node != ""} {
	for {set n $node} {$n!=""} {set n [$n parent]} {
	    set tag [$n tag]
	    if {$tag==""} {
		$PATH configure -cursor xterm
		set txt [string range [$n text] 0 20]
	    } else {
		set txt "<[$n tag]>$txt"
	    }
	}
    }
    if { $selection_state eq "press" } {
	set to [$w node -index $x $y]
	if {[llength $to]==2} {
	    foreach {node index} $to {}
	    catch { $w select to $node $index }
	}
	selection own $w
    }
}

proc HelpViewer::ButtonRelease1 { w x y } {
    variable selection_state

    set selection_state ""
}

proc HelpViewer::ButtonPress1 { w x y } {
    variable selection_state

    set err [catch {
	    $w select clear
	    set from [$w node -index $x $y]
	    if {[llength $from]==2} {
		foreach {node index} $from {}
		$w select from $node $index
		$w select to $node $index
		set selection_state press
	    }
	} ret]
    if { $err } { return }
    set node [lindex [$w node $x $y] 0]
    set href ""
    while { $node ne "" } {
	if { [$node tag] eq "a" && [$node att -default "" href] ne "" } {
	    set href [$node att href]
	    break
	}
	set node [$node parent]
    }
    if { $href ne "" } { LoadRef $w $href }
}

proc HelpViewer::DoubleButtonPress1 { w x y } {
    variable selection_state

    set err [catch { $w select from } ret]
    if { $err } { return }
    lassign $ret node index
    if { $node eq "" } { return }
    set txt [$node text]
    foreach "ini end" [list $index $index] break
    for { set i $index } { $i >= 0 } { incr i -1 } {
	if { ![regexp {\w} [string index $txt $i]] } { break }
	set ini $i
    }
    for { set i $index } { $i < [string length $txt] } { incr i } {
	if { ![regexp {\w} [string index $txt $i]] } { break }
	set end $i
    }
    $w select from $node $ini
    $w select to $node [expr {$end+1}]
    set selection_state press
}

# This procedure is called when the user clicks on a hyperlink.
# See the "bind $base.h.h.x" below for the binding that invokes this
# procedure
#
proc HelpViewer::HrefBinding {w x y} {

    set new [$w href $x $y]
    catch {
	if { [llength $new] == 1 } {
	    set new [lindex $new 0]
	    if { [llength $new] == 1 } {
		set new [lindex $new 0]
	    }
	}
    }
    LoadRef $w $new
}

proc HelpViewer::xml { args } {
    set map [list > "&gt;" < "&lt;" & "&amp;" \" "&quot;" ' "&apos;"]
    return [string map $map [join $args ""]]
}

proc HelpViewer::xml_inv { args } {
    set map [list "&gt;" > "&lt;" < "&amp;" & "&quot;" \" "&apos;" ']
    return [string map $map [join $args ""]]
}

proc HelpViewer::LoadRef { w new { enterinhistory 1 } } {
    global tcl_platform

    if { [regexp {http:/localhost/cgi-bin/man/man2html\?(\w+)\+(\w+)} $new {} sec word] } {
	SearchManHelpFor $w $word $sec
	return
    } elseif { $new != "" && [regexp {[a-zA-Z]+[a-zA-Z]:.*} $new] } {
	regexp {[^/\\]*:.*} $new url
	if { [regexp {:/[^/]} $url] } {
	    regsub {:/} $url {://} url
	}
	if { $tcl_platform(platform) != "windows"} {
	    set comm [auto_execok konqueror]
	    if { $comm == "" } {
		set comm [auto_execok netscape]
	    }
	    if { $comm == "" } {
		tk_messageBox -icon warning -message \
		"Check url: $url in your web browser." -type ok
	    } else {
		exec $comm $url &
	    }
	} else {
	    if { [info commands freewrap::shell_getCmd_imp] ne "" } {
		set comm [freewrap::shell_getCmd_imp .html open]
	    } else {
		set comm ""
		catch {
		    package require registry
		    set class [registry get {HKEY_CLASSES_ROOT\.html} ""]
		    set comm [registry get "HKEY_CLASSES_ROOT\\$class\\shell\\open\\command" ""]
		}
	    }
	    if { $comm == "" } {
		tk_messageBox -icon warning -message \
		"Check url: $url in your web browser." -type ok
	    } else {
		regsub -all {\\} $comm {\\\\} comm
		eval [concat "exec $comm $url &"]
	    }
	}
	return
    }

    if {$new!=""} {
	set LastFile [GiveLastFile $w]
	if { [string match \#* [file tail $new]] } {
	    set new $LastFile[file tail $new]
	}
	set pattern $LastFile#
	set len [string length $pattern]
	incr len -1
	if {[string range $new 0 $len]==$pattern} {
	    incr len
	    set tag [string range $new $len end]
	    if { [package vcompare [package present Tkhtml] 3.0] >= 0 } {
		set tag [$w search "a\[name='[xml_inv $tag]'\]"]
	    }
	    $w yview $tag
	    if { $enterinhistory } {
		History::Add $new
	    }
	} elseif { [regexp {(.*)\#(.*)} $new {} file tag] } {
	    LoadFile $w $file $enterinhistory $tag
	} else {
	    LoadFile $w $new $enterinhistory
	}
    }
}

proc HelpViewer::Load { w } {
    set filetypes [list \
	    [list [_ "Html Files"] {.html .htm}] \
	    [list [_ "All files"] *]]
    global lastDir htmltext
    set f [tk_getOpenFile -initialdir $lastDir -filetypes $filetypes]

    if {$f!=""} {
	LoadFile $w $f
	set lastDir [file dirname $f]
    }
}

# Clear the screen.
#
# Clear the screen.
#
proc HelpViewer::Clear { w } {
    variable OldImages
    variable Images

    global hotkey
    if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
	$w clear
    } else {
	$w reset
	$w style {
	    /* This is a CSS style sheet */
	    body {
		font-family: Verdana, helvetica, sans-serif;
		font-size: 10pt;
		margin: 1em;
		text-align: justify;
	    }
	    h1,h2,h3 {
		background-color: #d7e4ef;
		font-weight: bold;
		font-size: 12pt;
	    }
	    p {
		text-align: justify;
		margin-top: 0cm;
		margin-bottom: 4pt;
	    }
	    blockquote {
		margin-top: 0cm;
		margin-bottom: 0cm;
		margin-right: 0cm;
	    }
	    ul {
		margin-top: 0cm;
		margin-bottom: 0cm;
	    }
	    td {
		vertical-align: top
	    }
	    li {
		margin-bottom: 4pt;
	    }
	}
    }
    catch {unset hotkey}
    ClearBigImages
    ClearOldImages
    foreach fn [array names Images] {
	set OldImages($fn) $Images($fn)
    }
    array unset Images
}

proc HelpViewer::ClearOldImages {} {
    variable OldImages

   foreach fn [array names OldImages] {
	catch { image delete $OldImages($fn) }
    }
    array unset OldImages
}
proc HelpViewer::ClearBigImages {} {
    variable BigImages

    foreach b [array names BigImages] {
	image delete $BigImages($b)
    }
    array unset BigImages
}

# Read a file
#
proc HelpViewer::ReadFile {name} {
    if { [file dirname $name] == "." } {
	set name [file tail $name]
    }
    if {[catch {open $name r} fp]} {
	tk_messageBox -icon error -message $fp -type ok
	return {}
    } else {
	#fconfigure $fp -translation binary
	set r [read $fp]
	close $fp
	if { [regexp {(?i)<meta\s+[^>]*charset=utf-8[^>]*>} $r] } {
	    set fp [open $name r]
	    fconfigure $fp -encoding utf-8
	    set r [read $fp]
	    close $fp
	}
	if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
	    #some libTkHtml.dll show incorrecty this letter,
	    #and other dll's like ActiveState 8.4.9.0 or 8.5.0.0 crash
	    regsub -all {\u00E9} $r {\&eacute;} r
	} else {
	    set map {
		á &aacute;
		é &eacute;
		í &iacute;
		ó &oacute;
		ú &uacute;
		à &agrave;
		è &egrave;
		ì &igrave;
		ò &ograve;
		ù &ugrave;
		ñ &ntilde;
		ä &auml;
		ö &ouml;
	    }
	    set r [string map $map $r]
	    
	    regsub {<style.*?</style>} $r {} r
	    
	}
	return $r
    }
}

# Load a file into the HTML widget
#
proc HelpViewer::LoadFile {w name { enterinhistory 1 } { tag "" } } {
    global HelpPriv
    variable SearchPos

    if { $name == "" } { return }

    if { [file isdir $name] } {
	set files [glob -dir $name *]
	if { [llength $files] == 1 && ![file isdirectory [lindex $files 0]] } {
	    set name [lindex $files 0]
	} else {
	    set rex {(?i)(index|contents|_toc)\.(htm|html)$}
	    set name ""
	    foreach file [lsearch -inline -all -regexp $files $rex] {
		if { $name eq "" } {
		    set name $file
		} elseif { [regexp {(?i)^(index|contents)\.(htm|html)} [file tail $file]] } {
		    set name $file
		}
	    }
	    if { $name eq "" } { return }
	}
    }

    if { ![file readable $name] } {
	set dir [file dirname [GiveLastFile $w]]
	set name [file join $dir $name]
    }

    set html [ReadFile $name]
    if {$html==""} return
    Clear $w
    EnterLastFile $w $name

    if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
	$w configure -base $name
    }
    set HelpPriv(basedir) [file dirname $name]
    if { $enterinhistory } {
	if { $tag == "" } {
	    History::Add $name
	} else {
	    History::Add $name#$tag
	}
    }
    catch { $w select clear }
    $w parse $html
    ClearOldImages
    if { $tag != "" } {
	update idletasks
	if { [package vcompare [package present Tkhtml] 3.0] >= 0 } {
	    set tag [$w search "a\[name='[xml_inv $tag]'\]"]
	}
	if { $tag ne "" } { $w yview $tag }
    }
    if { $tag == "" } {
	TryToSelect $name noload
    } else {
	TryToSelect $name#$tag noload
    }
    set SearchPos ""
}

proc HelpViewer::GiveManHelpNames { word } {

    if { [auto_execok man2html] == "" } { return "" }

    set err [catch { exec man -aw $word } file]
    if { $err } { return "" }

    set words ""
    foreach i [split $file \n] {
	set ext [string trimleft [file ext $i] .]
	if { $ext == "gz" } { set ext [string trimleft [file ext [file root $i]] .] }
	if { [lsearch $words "$word (man $ext)"] == -1 } {
	    lappend words "$word (man $ext)"
	}
    }
    return $words
}

proc HelpViewer::SearchManHelpFor { w word { mansection "" } } {

    if { $mansection == "" } {
	regexp {(\S*)\s+\(man\s+(.*)\)} $word {} word mansection
    }

    set err [catch { exec man -aw $word } file]
    if { $err } { return }

    set files [split $file \n]

    if { [llength $files] > 1 } {
	set found 0
	foreach i $files {
	    set ext [string trimleft [file ext $i] .]
	    if { $ext == "gz" } { set ext [string trimleft [file ext [file root $i]] .] }
	    if { $mansection == $ext } {
		set found 1
		set file $i
		break
	    }
	}
	if { !$found } { set file [lindex $files 0] }
    } else { set file [lindex $files 0] }

    if { [file ext $file] == ".gz" } {
	set comm [list exec gunzip -c $file | man2html]
    } else { set comm [list exec man2html $file] }

    set err [catch { eval $comm } html]
    #if { $err } { return }

    Clear $w

    catch { $w select clear }
    $w parse $html
    ClearOldImages
    variable SearchPos
    set SearchPos ""
}

# Refresh the current file.
#
proc HelpViewer::Refresh {w args} {
    set LastFile [GiveLastFile $w]
    if {![info exists LastFile] || ![winfo exists $w] } return
    LoadFile $w $LastFile 0
}

proc HelpViewer::ResolveUri { args } {
    foreach "basename uri" $args break
    if { ![regexp {^http://} $uri] } {
	return [file join [file dirname $basename] $uri]
    } else {
	return $uri
    }
}

proc HelpViewer::ManageSel { htmlw w x y type } {
    global HelpPriv tcl_platform
    if { ![winfo exists  $w] } { return }
    if { [winfo parent $w] != $htmlw } { return }
    if { [info exists HelpPriv(lastafter)] } {
	after cancel $HelpPriv(lastafter)
    }
    switch -glob $type {
	press {
	    update idletasks
	    catch { $htmlw select clear }
	    set idx [$htmlw index @$x,$y]
	    if { $idx == "" } { return }
	    $htmlw insert $idx
	    catch { $htmlw select set $idx $idx }
	}
	motion* {
	    if { $type == "motion" } {
		#set ini [$htmlw index sel.first]
		set ini [$htmlw index insert]
		if { $ini == "" } {
		    catch { $htmlw select clear }
		} else {
		    catch {
		        set idx [$htmlw index @$x,$y]
		        if { $idx <= $ini } {
		            $htmlw select set $idx $ini
		        } else {
		            $htmlw select set $ini $idx
		        }
		    }
		}
	    }
	    set isout 0
	    if { $x > [winfo width $htmlw] } {
		$htmlw xview scroll 1 units
		set isout 1
	    } elseif { $x <0 } {
		$htmlw xview scroll -1 units
		set isout 1
	    }
	    if { $y > [winfo height $htmlw] } {
		$htmlw yview scroll 1 units
		set isout 1
	    } elseif { $y <0 } {
		$htmlw yview scroll -1 units
		set isout 1
	    }
	    if { $isout } {
		set HelpPriv(lastafter) [after 100 HelpViewer::ManageSel \
		    $htmlw $w $x $y motionout]
	    }
	}
	release {
	    #set ini [$htmlw index sel.first]
	    set ini [$htmlw index insert]
	    if { $ini == "" } {
		catch { $htmlw select clear }
		return
	    }
	    set idx [$htmlw index @$x,$y]
	    if { $idx == $ini } {
		catch { $htmlw select clear } 
	    } else {
		if { $idx <= $ini } {
		    catch { $htmlw select set $idx $ini }
		} else {
		    catch { $htmlw select set $ini $idx }
		}
	    }
	    if { $tcl_platform(platform) != "windows"} {
		selection own -command "HelpViewer::LooseSelection $htmlw" $htmlw
	    }
	}
	
    }
}


proc HelpViewer::LooseSelection { w } {
    catch { $w select clear }
}

proc HelpViewer::CopySelected { w { offset 0 } { maxBytes 0} } {
    global tcl_platform

    set ini [$w index sel.first]
    set end [$w index sel.last]
    if { $ini == "" || $end == "" } { return }

    regexp {([0-9]+)[.]([0-9]+)} $ini {} initoc inipos
    regexp {([0-9]+)[.]([0-9]+)} $end {} endtoc endpos
    #puts [$w token list $initoc $endtoc]
    set rettext ""
    set iposlast [expr $endtoc-$initoc]
    set ipos 0

    foreach i [$w token list $initoc $endtoc] {
	set type [lindex $i 1]
	set contents [join [lrange $i 2 end]]
	switch -- $type {
	    Text {
		set inichar 0
		set endchar [string length $contents]
		if { $ipos == 0 } {
		    set inichar $inipos
		}
		if { $ipos == $iposlast } {
		    set endchar [expr $endpos-1]
		}
		append rettext [string range $contents $inichar $endchar]
	    }
	    Space {
		if { [lindex $contents 1] == 0 } {
		    append rettext " "
		} else {
		    append rettext "\n"
		}
	    }
	}
	incr ipos
    }
    if { $tcl_platform(platform) == "windows"} {
	clipboard clear -displayof $w
	clipboard append -displayof $w $rettext
    } else {
	return [string range $rettext $offset [expr $offset+$maxBytes-1]]
    }
}

proc guiGetSelection { w { offset 0 } { maxChars 0 } } {
    catch {
	set span [$w select span]
	if {[llength $span] != 4} {
	    return ""
	}
	foreach {n1 i1 n2 i2} $span {}
	
	set not_empty 0
	set T ""
	set N $n1
	while {1} {
	
	    if {[$N tag] eq ""} {
		set index1 0
		set index2 end
		if {$N == $n1} {set index1 $i1}
		if {$N == $n2} {set index2 [expr {$i2-1}]}
		
		set text [string range [$N text] $index1 $index2]
		append T $text
		if {[string trim $text] ne ""} {set not_empty 1}
	    } else {
		array set prop [$N prop]
		if {$prop(display) ne "inline" && $not_empty} {
		    append T "\n"
		    set not_empty 0
		}
	    }
	
	    if {$N eq $n2} break
	
	    if {[$N nChild] > 0} {
		set N [$N child 0]
	    } else {
		while {[set next_node [$N right_sibling]] eq ""} {
		    set N [$N parent]
		}
		set N $next_node
	    }
	
	    if {$N eq ""} {error "End of tree!"}
	}
	if { $::tcl_platform(platform) == "windows"} {
	    clipboard clear -displayof $w
	    clipboard append -displayof $w $T
	} else {
	    set T [string range $T $offset [expr $offset + $maxChars]]
	}
    } msg
    return $msg
}


# what can be: HTML or Word or CSV
proc HelpViewer::SaveHTMLAs { w what } {

    set fromfile [GiveLastFile $w]
    switch $what {
	HTML {
	    set types [list \
		    [list [_ "Html Files"] {.html .htm}] \
		    [list [_ "All files"] *]]

	    set ext ".html"
	    set initial [file tail $fromfile]
	}
	Word {
	    set types [list \
		    [list [_ "Word file"] {.doc}] \
		    [list [_ "All files"] *]]

	    set ext ".doc"
	    set initial [file root [file tail $fromfile]].doc
	}
	CSV {
	    set types [list \
		    [list [_ "CSV file"] {.csv .txt}] \
		    [list [_ "All files"] *]]

	    set ext ".csv"
	    set initial [file root [file tail $fromfile]].csv
	}
    }
    if { [file exists ConcretePrefs::reportexportdir] && \
	[file isdir $ConcretePrefs::reportexportdir] } {
	set defaultdir $ConcretePrefs::reportexportdir
    } else { set defaultdir "" }

    if { $::tcl_platform(platform) == "windows" } {
	set tofile [tk_getSaveFile -defaultextension $ext -filetypes $types \
	    -initialfile $initial -parent $w -initialdir $defaultdir \
	    -title "Save Results"]
    } else {
	set tofile [Browser-ramR file save]
    }


    if { $tofile == "" } { return }

    catch {
	set ConcretePrefs::reportexportdir [file dirname $filename]
    }

    if { [file ext $tofile] == ".html" || [file ext $tofile] == ".htm" } {
	set reportexportdir [file dirname $tofile]
	set imgdir [file join $reportexportdir timages]
	if { [file exists $imgdir] } {
	    set retval [tk_dialogRAM  $w._tmp Warning \
		[concat "Are you sure to delete directory "\
		    " '$imgdir' and all its contents?"]\
		warning 0 OK Cancel]
	    if { $retval == 1 } { return }
	    if { [catch {
		file delete -force $imgdir
	    } err] } {
		WarnWin "error: Could not delete directory '$imgdir' ($err)"
		return
	    }
	}

	if { [catch {
	    file copy -force [file join [file dirname $fromfile] timages] \
	    $imgdir
	    file copy -force $fromfile $tofile
	} err] } {
	    WarnWin [concat "Problems exporting report to '$tofile' ($err). "\
		"Check write permissions and disk space"]
	    return
	}
    } elseif { [file ext $tofile] == ".doc" || [file ext $tofile] == ".rtf" } {
	rtf:HTML2RTF $fromfile $tofile "Ram Series"
    } elseif { [file ext $tofile] == ".csv" || [file ext $tofile] == ".txt" } {
	rtf:HTML2CSV $fromfile $tofile "Ram Series"
    } else {
	WarnWin "Unknown extension for file '$tofile'"
	return
    }
    set comm [FindApplicationForOpening $tofile]
    # do not visualize
    set comm ""
    if { $comm == "" } {
	WarnWin "Report exported OK to file '$tofile'"
    } else {
	set text "Report exported OK to file '$tofile' Do you want to visualize it?"
	set retval [tk_messageBox -default no -icon question -message $text \
	    -parent $w -title "Report exported" -type yesno]
	if { $retval == "yes" } {
	    eval exec $comm [list [file native $tofile]] &
	}
    }
}
proc HelpViewer::HTMLToClipBoardCSV { w } {
    set fromfile [GiveLastFile $w]
    rtf:HTML2CSV $fromfile "" "Ram Series"
    WarnWin Done
}
proc HelpViewer::FindApplicationForOpening { file } {
    if { $::tcl_platform(platform) == "windows" } {
	return [auto_execok start]
    } else {
	set comm ""
	switch [string tolower [file extension $file]] {
	    ".htm" - ".html" {
		set comm [auto_execok konqueror]
		if { $comm == "" } {
		    set comm [auto_execok netscape]
		}
	    }
	    ".csv" - ".txt" {
		set comm [auto_execok kspread]
	    }
	}
    }
    return $comm
}

proc HelpViewer::HelpWindow { file { base .help} { geom "" } { title "" } { tocfile "" } } {
    variable HelpBaseDir
    variable html
    variable tree
    variable searchlistbox1
    variable searchlistbox2
    variable notebook

    if { [info commands html] == "" } {
	set text "Package tkhtml could not be load, and so, it is not possible to see the help.\n"
	append text "The RamDebugger distribution only contains tkhtml libraries "
	append text "for Windows and Linux.\n"
	append text "You must get Tkhtml for other OS separately and install it in order to use the help"
	WarnWin $text
	return
    }

    global lastDir tcl_platform argv0

    set imagesdir [file join [file dirname [info script]] images]

    if { $tcl_platform(platform) != "windows" } {
	option add *Scrollbar*Width 10
	option add *Scrollbar*BorderWidth 1
	option add *Button*BorderWidth 1
    }
    option add *Menu*TearOff 0

    if { $tcl_platform(platform) != "windows" } {
#          option add *background AntiqueWhite3
#          option add *Button*background bisque3
#          option add *Menu*background bisque3
#          option add *Button*foreground black
#          option add *Entry*background thistle
#          option add *DisabledForeground grey60
#          option add *HighlightBackground AntiqueWhite3
    }

    catch { destroy $base }

    if { $title == "" } { set title Help }

    if { [file isdir $file] } {
	set HelpBaseDir $file
    } else {
	set HelpBaseDir [file dirname $file]
    }
    set HelpBaseDir [filenormalize $HelpBaseDir]

    #          if { $geom == "" } {
    #              set width 400
    #              set height 500
    #              set x [expr [winfo screenwidth $base]/2-$width/2]
    #              set y [expr [winfo screenheight $base]/2-$height/2]
    #              wm geom $base ${width}x${height}+${x}+${y}
    #          } else {
    #              wm geom $base $geom
    #          }

    if { [info procs InitWindow] != "" } {
	InitWindow $base $title PostHelpViewerWindowGeom
	if { ![winfo exists $base] } return ;# windows disabled || usemorewindows == 0
    } else {
	toplevel $base
	if { $::tcl_platform(platform) == "windows" } {	   
	    wm attributes $base -toolwindow 1
	}
	wm title $base $title
    }
    wm withdraw $base

    # These images are used in place of GIFs or of form elements
    #

    if { [lsearch [image names] biggray] == -1 } {
	
	image create photo biggray -data {
	    R0lGODdhPAA+APAAALi4uAAAACwAAAAAPAA+AAACQISPqcvtD6OctNqLs968+w+G4kiW5omm
	    6sq27gvH8kzX9o3n+s73/g8MCofEovGITCqXzKbzCY1Kp9Sq9YrNFgsAO///
	}
	image create photo smgray -data {
	    R0lGODdhOAAYAPAAALi4uAAAACwAAAAAOAAYAAACI4SPqcvtD6OctNqLs968+w+G4kiW5omm
	    6sq27gvH8kzX9m0VADv/
	}
	image create photo nogifbig -data {
	    R0lGODdhJAAkAPEAAACQkADQ0PgAAAAAACwAAAAAJAAkAAACmISPqcsQD6OcdJqKM71PeK15
	    AsSJH0iZY1CqqKSurfsGsex08XuTuU7L9HywHWZILAaVJssvgoREk5PolFo1XrHZ29IZ8oo0
	    HKEYVDYbyc/jFhz2otvdcyZdF68qeKh2DZd3AtS0QWcDSDgWKJXY+MXS9qY4+JA2+Vho+YPp
	    FzSjiTIEWslDQ1rDhPOY2sXVOgeb2kBbu1AAADv/
	}
	image create photo nogifsm -data {
	    R0lGODdhEAAQAPEAAACQkADQ0PgAAAAAACwAAAAAEAAQAAACNISPacHtD4IQz80QJ60as25d
	    3idKZdR0IIOm2ta0Lhw/Lz2S1JqvK8ozbTKlEIVYceWSjwIAO///
	}
    }

    if { [lsearch [image names] imatge_fletxa_e] == -1 } {

	image create photo imatge_fletxa_e -data {
	    R0lGODlhJwAeAJECAE5qmoWh0f///wAAACH5BAEAAAIALAAAAAAnAB4AAAJb
	    lI+py+0Po5y00mBzCCDf3XnRBooQWZoNmqoK27oHTNc2Y+c69+4+HJr9hqSg
	    gIjkIZJDo4Hpcz6hOemUSrNeUYCu9wsOdx1AGY5rPsfSyzVbqH0r34s4/Y4v
	    AAA7
	}
	image create photo imatge_fletxa_d -data {
	    R0lGODlhJwAeAJECAE5qmoWh0f///wAAACH5BAEAAAIALAAAAAAnAB4AAAJa
	    lI+py+0Po5y0WhfuDVlT3nkRGIoYaT4klzYr677yPMf0fTP4Ti/8/1IAh6AE
	    cQgAII6/pJGJcz6hM6kiic1qtwCZtWV4fcGC1ZhMOqM56nUb/SYLlPK63VAA
	    ADs=
	}
	image create photo imatge_quit -data {
	    R0lGODlhIAAeAJECAE5qmoWh0f///wAAACH5BAEAAAIALAAAAAAgAB4AAAJm
	    lI+py+0Powq02kulwNwC2YVfVAHmiaYgNWpP6QYvG8sNrFU3nesLHgr+eEFh
	    AlgMHYlJ5QHZxJiezKintalaA1METpAKm64T3oqLpXJr3fI613anIXPvtmet
	    D/MasT/uEig4eFAAADs=
	}
	image create photo imatge_save -data {
	    R0lGODlhIAAeAOcAAEdjk05qmktnl1VxoWuHt2yIuUhklUlllVJunmSAsHuX
	    x4ik1Iml1YSg0EhklE9rm197q3aSwoai0oej04Wh0YSg0V57q01pmVt3p3GN
	    vYGez3yazpuy2YGe0XWRwVdzo2yIuIGdzYKf0IGe0KC229Lc7vz9/v///4Og
	    0FFtnUpmllNvn2eDs32ZyX2bzn+cz5ev2MfU6vT3+6W63YSh02N/r1BsnGJ+
	    rnmVxYSh0X6cz4+o1bvL5uzw+Njh8HqWxkpnl4Cdz4ij0rDC4eLp9P7+/1Rw
	    oExomIKezoKg0miEtEdklGmFtY2o1Pj6/H6by4ql04+p1Yej1Fh0pNvj8evw
	    +L/N54mk022JuWSAsYOh06i83vT2+8bT6pau14Kez/r7/fv8/dHc7qC123iY
	    zVx4qHaTw36c0NDb7dzk8qq933mYzXuZz4ah0aK11ZSs04Kf0XKOvmB8rIWi
	    1J602ufs9rXG44ul03qZzXmYzoOf0Zmv1LvH2Nbb3OPk3b/K2X+d0Pn6/fH1
	    +sDP55Kr1niYzoCd0JKq07PC19HX297g3d3f3djc3Nfb3Iai0WF9rXGOvoOg
	    0pCq1X+dz3uZzn2bz42n07LD3MvT2tzf3d3g3dnd3KG11XiUxMXP2tXY16Kp
	    p8vPz9vf39ve3MPN2X+c0MnT4H+Kiic5PomTluLl5drd3Iul0mWBsaK32b7D
	    wDlKTlRjZt3h4eXl3qm61oqm1tne4FNiZTpKT7rAwd3h4Nre3MTO2aO21Yei
	    0V56qsLO4Y6Xlig7P3mFh+Pl5Kq71oum0oCe0YSi1Jmw1svOzIaPj8fLyLLB
	    15Gq04Oh1Iek1H6aylNvoM3U2sPP4Jyy14Kg04ej1YOg0YCe0oaj1XeTw3yY
	    yIWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh
	    0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh0YWh
	    0YWh0YWh0YWh0YWh0YWh0SH5BAEAAP8ALAAAAAAgAB4AAAj+AP8JHEiwoEEA
	    AQwqXHhQwAACBQwwnFjwAIIEChYwaJCQIkMHDyBEkMBgAoUKDCx4NOjgAoYM
	    DUpS0LCBAocOHg6s/AdAwAcQIRgsoCBiwwgSJUycQCEhhUcVK1i0WLBAAgoX
	    L2DEkHGi64kZNGowPGDjBo4JDCTk0OFiB48eXuP60PEDiMIUESjIDLJByBAi
	    cQOfKCJkghGFR5BIKIpiho8igiMPSaJkCUsmDJqUcBK58wkiOp4cUThgAhTP
	    qKNImaJQgAIdVFALrmLlCgMsDg5m0bJFdlcuXbywrUDhywWFCCRUAOM5jJgx
	    GsiIoEBdbxmFB8ycQRM5jRoKa9j+tHHzBk71BXF0GpQzh07cOnbu4MmjZw+f
	    Pn7+AKpO3YbCBw1oEIgggxCyQSGGHIJIIooswggjjTjCHwOPYAdJJJJMQkkl
	    llyCSSaabPLgg4pwMkJ1E3SigkIYMECdI558Akoooow44iik7IciAgpdEIIE
	    FJRiyimopKKKjSOuwgpx1DHQCgAsFTBUB668AksssiDJyCiz0NIBdRPUosBo
	    Bn0wFAU52HILLrno8uAuiuzCSy++oCWBB78EcJxBR7QAJCDABCPMMMRokgkm
	    xRhzDDIT/FBDCuotBAALLsKRjDLLMJNIM8508Aw00ShhxIorSWMSBXBMQ001
	    elhzDRI1WExxBJQ7/aPCWRRgQ0E22lAQRxkX5FYrQTekVdI2cjwQ6bAEpbAA
	    N60gQCqziK0gAK3UBgQAOw==
	}

	image create photo appbook16 -data {
	    R0lGODlhEAAQAIQAAPwCBAQCBDyKhDSChGSinFSWlEySjCx+fHSqrGSipESO
	    jCR6dKTGxISytIy6vFSalBxydAQeHHyurAxubARmZCR+fBx2dDyKjPz+/MzK
	    zLTS1IyOjAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAQABAAAAVkICCOZGmK
	    QXCWqTCoa0oUxnDAZIrsSaEMCxwgwGggHI3E47eA4AKRogQxcy0mFFhgEW3M
	    CoOKBZsdUrhFxSUMyT7P3bAlhcnk4BoHvb4RBuABGHwpJn+BGX1CLAGJKzmK
	    jpF+IQAh/mhDcmVhdGVkIGJ5IEJNUFRvR0lGIFBybyB2ZXJzaW9uIDIuNQ0K
	    qSBEZXZlbENvciAxOTk3LDE5OTguIEFsbCByaWdodHMgcmVzZXJ2ZWQuDQpo
	    dHRwOi8vd3d3LmRldmVsY29yLmNvbQA7
	}
	image create photo appbookopen16 -data {
	    R0lGODlhEAAQAIUAAPwCBAQCBExCNGSenHRmVCwqJPTq1GxeTHRqXPz+/Dwy
	    JPTq3Ny+lOzexPzy5HRuVFSWlNzClPTexIR2ZOzevPz29AxqbPz6/IR+ZDyK
	    jPTy5IyCZPz27ESOjJySfDSGhPTm1PTizJSKdDSChNzWxMS2nIR6ZKyijNzO
	    rOzWtIx+bLSifNTGrMy6lIx+ZCRWRAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	    AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAQABAAAAae
	    QEAAQCwWBYJiYEAoGAFIw0E5QCScAIVikUgQqNargtFwdB9KSDhxiEjMiUlg
	    HlB3E48IpdKdLCxzEAQJFxUTblwJGH9zGQgVGhUbbhxdG4wBHQQaCwaTb10e
	    mB8EBiAhInp8CSKYIw8kDRSfDiUmJ4xCIxMoKSoRJRMrJyy5uhMtLisTLCQk
	    C8bHGBMj1daARgEjLyN03kPZc09FfkEAIf5oQ3JlYXRlZCBieSBCTVBUb0dJ
	    RiBQcm8gdmVyc2lvbiAyLjUNCqkgRGV2ZWxDb3IgMTk5NywxOTk4LiBBbGwg
	    cmlnaHRzIHJlc2VydmVkLg0KaHR0cDovL3d3dy5kZXZlbGNvci5jb20AOw==
	}
	image create photo filedocument16 -data {
	    R0lGODlhEAAQAIUAAPwCBFxaXNze3Ly2rJSWjPz+/Ozq7GxqbJyanPT29HRy
	    dMzOzDQyNIyKjERCROTi3Pz69PTy7Pzy7PTu5Ozm3LyqlJyWlJSSjJSOhOzi
	    1LyulPz27PTq3PTm1OzezLyqjIyKhJSKfOzaxPz29OzizLyidIyGdIyCdOTO
	    pLymhOzavOTStMTCtMS+rMS6pMSynMSulLyedAAAAAAAAAAAAAAAAAAAAAAA
	    AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAQABAAAAaQ
	    QIAQECgajcNkQMBkDgKEQFK4LFgLhkMBIVUKroWEYlEgMLxbBKLQUBwc52Hg
	    AQ4LBo049atWQyIPA3pEdFcQEhMUFYNVagQWFxgZGoxfYRsTHB0eH5UJCJAY
	    ICEinUoPIxIcHCQkIiIllQYEGCEhJicoKYwPmiQeKisrKLFKLCwtLi8wHyUl
	    MYwM0tPUDH5BACH+aENyZWF0ZWQgYnkgQk1QVG9HSUYgUHJvIHZlcnNpb24g
	    Mi41DQqpIERldmVsQ29yIDE5OTcsMTk5OC4gQWxsIHJpZ2h0cyByZXNlcnZl
	    ZC4NCmh0dHA6Ly93d3cuZGV2ZWxjb3IuY29tADs=
	}
	image create photo viewmag16 -data {
	    R0lGODlhEAAQAIUAAPwCBCQmJDw+PAwODAQCBMza3NTm5MTW1HyChOTy9Mzq
	    7Kze5Kzm7OT29Oz6/Nzy9Lzu7JTW3GTCzLza3NTy9Nz29Ize7HTGzHzK1AwK
	    DMTq7Kzq9JTi7HTW5HzGzMzu9KzS1IzW5Iza5FTK1ESyvLTa3HTK1GzGzGzG
	    1DyqtIzK1AT+/AQGBATCxHRydMTCxAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	    AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAQABAAAAZ8
	    QIAQEBAMhkikgFAwHAiC5FCASCQUCwYiKiU0HA9IRAIhSAcTSuXBsFwwk0wy
	    YNBANpyOxPMxIzMgCyEiHSMkGCV+SAQQJicoJCllUgBUECEeKhAIBCuUSxMK
	    IFArBIpJBCxmLQQuL6eUAFCusJSzr7GLArS5Q7O1tmZ+QQAh/mhDcmVhdGVk
	    IGJ5IEJNUFRvR0lGIFBybyB2ZXJzaW9uIDIuNQ0KqSBEZXZlbENvciAxOTk3
	    LDE5OTguIEFsbCByaWdodHMgcmVzZXJ2ZWQuDQpodHRwOi8vd3d3LmRldmVs
	    Y29yLmNvbQA7
	}

    }

    ################################################################################
    # menu is not mapped in this version
    ################################################################################

#     frame $base.mbar -bd 2 -relief raised
#     menubutton $base.mbar.file -text File -underline 0 -menu $base.mbar.file.m
#     pack $base.mbar.file -side left -padx 5
#     set m [menu $base.mbar.file.m]
#     $m add command -label Open -underline 0 -command "HelpViewer::Load $base.h.h"
#     $m add command -label Refresh -underline 0 -command "HelpViewer::Refresh $base.h.h"
#     $m add separator
#     $m add command -label Index -acc Ctrl-i -command "\
#                              HelpViewer::LoadFile $base.h.h [list $file] ;\
#                              set HelpViewer::lastDir [list [file dirname $file]]"
#     $m add command -label Refresh -acc Ctrl-r -command "HelpViewer::Refresh $base.h.h"
#     $m add separator
#     $m add command -label Close -acc Ctrl-q -command "destroy $base"
#     bind $base <Control-o> "HelpViewer::Load $base.h.h"
#     bind $base <Control-r> "$m invoke Refresh"
#     bind $base <Control-i> "$m invoke Index"
#     bind $base <Control-q> "$m invoke Close"

#     menubutton $base.mbar.edit -text Edit -underline 0 -menu $base.mbar.edit.m
#     pack $base.mbar.edit -side left -padx 5
#     set m [menu $base.mbar.edit.m]
#     $m add command -label Copy -acc Ctrl-c -command "HelpViewer::CopySelected $base.h.h"
#     bind $base <Control-c> "$m invoke Copy"

#     menubutton $base.mbar.go -text Go -underline 0 -menu $base.mbar.go.m
#     pack $base.mbar.go -side left -padx 5
#     set History::menu [menu $base.mbar.go.m]
#     $History::menu add command -label Backward -acc Alt-<- -state disabled -command \
#     "History::GoBackward $base.h.h"
#     $History::menu add command -label Forward -acc Alt--> -state disabled -command \
#     "History::GoForward $base.h.h"
#     bind $base <Alt-Left> "$History::menu invoke Backward"
#     bind $base <Alt-Right> "$History::menu invoke Forward"


    ################################################################################
    # the HTML viewer
    ################################################################################

    set pw [panedwindow $base.pw -orient horizontal -grid 0]

    if { [catch {
	foreach "weight1 weight2" [RamDebugger::ManagePanes $pw h "5 12"] break
    }]} {
	set weight1 180
	set weight2 400
    }
#     set pane1 [$pw add -weight $weight1]
    set pane1 [frame $pw.pane1]
    $pw add $pane1 -width $weight1

    $pane1 configure -bg \#add8d8
    NoteBook $pane1.nb -homogeneous 1 -bd 1 -internalborderwidth 3 -bg \#add8d8 \
	    -activebackground \#add8d8  -disabledforeground \#add8d8 -grid "0 py3"

    set notebook $pane1.nb

    set f1 [$pane1.nb insert end tree -text [_ "Contents"] -image appbook16]

    set sw [ScrolledWindow $f1.lf -relief sunken -borderwidth 1 -grid "0"]
    set tree [Tree $sw.tree -bg white\
	    -relief flat -borderwidth 2 -width 15 -highlightthickness 0\
	    -redraw 1 -deltay 18 -bg #add8d8\
	-opencmd   "HelpViewer::moddir 1 $sw.tree" \
	-closecmd  "HelpViewer::moddir 0 $sw.tree" \
	-width 5 \
	]
    $sw setwidget $tree

    set font [option get $tree font Font]
    if { $font != "" } {
	$tree configure -deltay [expr [font metrics $font -linespace]]
    }

    if { $::tcl_platform(platform) != "windows" } {
	$tree configure  -selectbackground \#48c96f -selectforeground white
    }
    $pane1.nb itemconfigure tree -raisecmd "focus $tree"

    set f2 [$pane1.nb insert end search -text [_ "Search"] -image viewmag16]

    label $f2.l1 -text "S:" -bg #add8d8 -grid 0
    ComboBox $f2.e1 -textvariable HelpViewer::searchstring -entrybg #add8d8 \
	-bd 1 -grid "1 py3"

    $pane1.nb itemconfigure search -raisecmd "tkTabToWindow $f2.e1"
    $f2.e1 bind <Return> "focus $f2.lf1.lb; HelpViewer::SearchInAllHelp $f2.e1"


    ScrolledWindow $f2.lf1 -relief sunken -borderwidth 1 -grid "0 2"
    set searchlistbox1 [listbox $f2.lf1.lb -listvar HelpViewer::SearchFound -bg white \
	    -exportselection 0]
    $f2.lf1 setwidget $f2.lf1.lb

    ScrolledWindow $f2.lf2 -relief sunken -borderwidth 1 -grid "0 2"
    set searchlistbox2 [listbox $f2.lf2.lb -listvar HelpViewer::SearchFound2 -grid "0 2" \
	    -bg white -exportselection 0]
    $f2.lf2 setwidget $f2.lf2.lb

    bind $f2.lf1.lb <FocusIn> "if { \[%W curselection] == {} } { %W selection set 0 }"
    bind $f2.lf1.lb <Double-1> "focus $f2.lf2.lb; HelpViewer::SearchInAllHelpL1"
    bind $f2.lf1.lb <Return> "focus $f2.lf2.lb; HelpViewer::SearchInAllHelpL1"
    bind $f2.lf2.lb <FocusIn> "if { \[%W curselection] == {} } { %W selection set 0 }"
    bind $f2.lf2.lb <Double-1> "HelpViewer::SearchInAllHelpL2"
    bind $f2.lf2.lb <Return> "HelpViewer::SearchInAllHelpL2"

    set HelpViewer::SearchFound ""
    set HelpViewer::SearchFound2 ""

    supergrid::go $f1
    supergrid::go $f2
    $pane1.nb compute_size
    $pane1.nb raise tree

#     set pane2 [$pw add -weight $weight2]
    set pane2 [frame $pw.pane2]
    $pw add $pane2 -width $weight2

    set sw [ScrolledWindow $pane2.lf -relief sunken -borderwidth 0 -grid "0 2"]
    
    if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
	set html [html $sw.h \
		-padx 5 \
		-pady 9 \
		-formcommand HelpViewer::FormCmd \
		-imagecommand [list HelpViewer::ImageCmd $sw.h] \
		-scriptcommand HelpViewer::ScriptCmd \
		-appletcommand HelpViewer::AppletCmd \
		-underlinehyperlinks 0 \
		-bg white -tablerelief raised \
		-resolvercommand HelpViewer::ResolveUri \
		-exportselection 1 \
		-takefocus 1 \
		-fontcommand HelpViewer::pickFont \
		-width 550 \
		-height 450 \
		-borderwidth 0 \
		]
    } else {
	set html [html $sw.h \
		-imagecmd [list HelpViewer::ImageCmd $sw.h] \
		-width 550 \
		-height 450 \
		]
    }
    $sw setwidget $html

    if { [package vcompare [package present Tkhtml] 3.0] >= 0 } {
	$html configure -imagecmd [list HelpViewer::ImageCmd $sw.h]
    }

    frame $base.buts -bg grey93 -grid "0 ew"

    button $base.buts.b1 -image imatge_fletxa_e -bg grey93 -relief flat \
	    -command "History::GoBackward $html" -height 50 -grid "0 e" \
	    -highlightthickness 0
    button $base.buts.b2 -image imatge_fletxa_d -bg grey93 -relief flat \
	    -command "History::GoForward $html" -height 50 -grid "1 w" \
	    -highlightthickness 0

    menubutton $base.buts.b3 -text [_ "More"]... -bg grey93 -fg \#add8d8 -relief flat \
	    -menu $base.buts.b3.m -grid "2 e" -activebackground grey93 -width 6

    menu $base.buts.b3.m
    $base.buts.b3.m add command -label [_ "Home"] -acc "" -command \
	     "History::GoHome $html"
    $base.buts.b3.m add command -label [_ "Previus"] -acc "Alt-Left" -command \
	     "History::GoBackward $html"
    $base.buts.b3.m add command -label [_ "Next"] -acc "Alt-Right" -command \
	     "History::GoForward $html"
    $base.buts.b3.m add separator
    $base.buts.b3.m add command -label [_ "Search in page..."] -acc "Ctrl+F" -command \
	     "focus $html; HelpViewer::SearchWindow"
    $base.buts.b3.m add command -label [_ "Search more"] -acc "F3" -command \
	     "focus $html; HelpViewer::Search"
    $base.buts.b3.m add separator
    $base.buts.b3.m add command -label [_ "Close"] -acc "ESC" -command \
	     "destroy [winfo toplevel $html]"

    if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
	bind $html.x <ButtonRelease-3> [list tk_popup $base.buts.b3.m %X %Y]
    } else {
	bind $html <ButtonRelease-3> [list tk_popup $base.buts.b3.m %X %Y]
    }

#     menubutton $base.buts.b3 -image imatge_save -bg grey93 -relief flat \
#             -menu $base.buts.b3.m -height 50 -grid "2 e" -activebackground grey93

#     menu $base.buts.b3.m
#     $base.buts.b3.m add command -label "As HTML" -command \
#             "HelpViewer::SaveHTMLAs $base HTML"
#     $base.buts.b3.m add command -label "As RTF file (Word)" -command \
#             "HelpViewer::SaveHTMLAs $base Word"
#     $base.buts.b3.m add command -label "As CSV file (Excel)" -command \
#             "HelpViewer::SaveHTMLAs $base CSV"
#     $base.buts.b3.m add command -label "Copy to clipboard (Excel)" -command \
#             "HelpViewer::HTMLToClipBoardCSV $base"

    if { $HelpPrefs::RunningAlone } {
	button $base.buts.b4 -image imatge_quit -bg grey93 -relief flat \
		-command "exit" -height 50 -grid "3 e" \
	    -highlightthickness 0
    } else {
	button $base.buts.b4 -image imatge_quit -bg grey93 -relief flat \
		-command "destroy $base" -height 50 -grid "3 e" \
	    -highlightthickness 0
    }

    supergrid::go $pane1
    supergrid::go $pane2
    supergrid::go $base
    
    grid configure $base.buts.b4 -padx "2 14"

    grid columnconf $base.buts "0 1" -weight 1
    grid columnconf $base.buts "2 3 4" -weight 0

#     if { $HelpPrefs::RunningAlone } {
#         grid $base.mbar -col 0 -row 0 -columnspan 3 -sticky ew
#     }

    # This procedure is called when the user selects the File/Open
    # menu option.
    #
    set lastDir [pwd]

    #      $base.h.h token handler Frameset "FrameCmd $base"
    #      $base.h.h token handler Frame "FrameCmd $base"
    #      $base.h.h token handler /Frameset "FrameCmd $base"


    # This binding changes the cursor when the mouse move over
    # top of a hyperlink.
    #
    if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
	bind HtmlClip <Motion> {
	    set parent [winfo parent %W]
	    set url [$parent href %x %y]
	    if {[string length $url] > 0} {
		$parent configure -cursor hand2
	    } else {
		$parent configure -cursor {}
	    }
	}
	bind $html.x <1> "focus $html; HelpViewer::HrefBinding $html %x %y"
	bind $base <Control-c> "HelpViewer::CopySelected $html"
	if {[string equal "unix" $::tcl_platform(platform)]} {
	    bind $html.x <4> { %W yview scroll -1 units; break }
	    bind $html.x <5> { %W yview scroll 1 units; break }
	    bind $tree.c <4> { %W yview scroll -5 units; break }
	    bind $tree.c <5> { %W yview scroll 5 units; break }
	}
    } else {
	bind $html <Motion> [list HelpViewer::Motion %W %x %y]
	set f [winfo parent [winfo parent $html]]
	bind $html <Leave> [list $f configure -cursor ""]
	bind $html <1> "focus $html; HelpViewer::ButtonPress1 $html %x %y"
	bind $html <Double-1> "focus $html; HelpViewer::DoubleButtonPress1 $html %x %y"
	bind $html <ButtonRelease-1> "focus $html; HelpViewer::ButtonRelease1 $html %x %y"
	bind $base <Control-c> [list guiGetSelection $html]
    if {[string equal "unix" $::tcl_platform(platform)]} {
	    bind $html <4> { %W yview scroll -1 units; break }
	    bind $html <5> { %W yview scroll 1 units; break }
	    bind $tree.c <4> { %W yview scroll -5 units; break }
	    bind $tree.c <5> { %W yview scroll 5 units; break }
    }
    }
    focus $html

    bind $html
    bind $html <Prior> {
	%W yview scroll -1 pages
    }

    bind $html <Next> {
	%W yview scroll 1 pages
    }
    bind $html <Home> {
	%W yview moveto 0
    }

    bind $html <End> {
	%W yview moveto 1
    }

    bind $html <Control-o> "HelpViewer::Load $html"

    bind $base <1> "HelpViewer::ManageSel $html %W %x %y press"
    bind $base <B1-Motion> "HelpViewer::ManageSel $html %W %x %y motion"
    bind $base <ButtonRelease-1> "HelpViewer::ManageSel $html %W %x %y release"
    if { $tcl_platform(platform) != "windows"} {
	selection handle $html "HelpViewer::CopySelected $html"
    }

    $tree bindText  <ButtonPress-1>        "HelpViewer::Select $tree 1"
    $tree bindText  <Double-ButtonPress-1> "HelpViewer::Select $tree 2"
    $tree bindImage  <ButtonPress-1>        "HelpViewer::Select $tree 1"
    $tree bindImage  <Double-ButtonPress-1> "HelpViewer::Select $tree 2"
    $tree bindText <Control-ButtonPress-1>        "HelpViewer::Select $tree 3"
    $tree bindImage  <Control-ButtonPress-1>        "HelpViewer::Select $tree 3"
    $tree bindText <Shift-ButtonPress-1>        "HelpViewer::Select $tree 4"
    $tree bindImage  <Shift-ButtonPress-1>        "HelpViewer::Select $tree 4"
    # dirty trick
    foreach i [bind $tree.c] {
	bind $tree.c $i "+ [list after idle [list HelpViewer::Select $tree 0 {}]]"
    }
    bind $tree.c <Return> "HelpViewer::Select $tree 1 {}"
    bind $tree.c <KeyPress> "if \[string is wordchar -strict {%A}] {HelpViewer::KeyPress %A}"
    bind $tree.c <Alt-KeyPress-Left> ""
    bind $tree.c <Alt-KeyPress-Right> ""
    bind $tree.c <Alt-KeyPress> { break }

    bind [winfo toplevel $html] <Alt-Left> "History::GoBackward $html; break"
    bind [winfo toplevel $html] <Alt-Right> "History::GoForward $html; break"

    if { $tocfile == "" } {
	FillDir $tree root
    } else {
	if { [catch { FillTreeWithToc $tree root $tocfile } ] } {
	     FillDir $tree root
	}
    }

    bind [winfo toplevel $html] <Control-f> "focus $html; HelpViewer::SearchWindow ; break"
    bind [winfo toplevel $html] <F3> "focus $html; HelpViewer::Search ; break"
    if { [info script] == $::argv0 } {
	bind [winfo toplevel $html] <Escape> "exit"
    } else {
	bind [winfo toplevel $html] <Escape> "destroy [winfo toplevel $html]"
    }

    bind [winfo toplevel $html] <Control-f> "focus $html; HelpViewer::SearchWindow ; break"

    set cmd_tree "[list $notebook raise tree]"
    set cmd_search "[list $notebook raise search] ; [list tkTabToWindow $f2.e1]"
    bind [winfo toplevel $html] <Alt-KeyPress-c> $cmd_tree
    bind [winfo toplevel $html] <Control-KeyPress-i> $cmd_search
    bind [winfo toplevel $html] <Alt-KeyPress-i> $cmd_search
    bind [winfo toplevel $html] <Control-KeyPress-s> $cmd_search
    bind [winfo toplevel $html] <Alt-KeyPress-s> $cmd_search

    addtag_to_all_widgets $base mousewheel
    bind mousewheel <MouseWheel> {
	set w [winfo containing %X %Y]
	while { $w != [winfo toplevel $w] } {
	    set err [catch {
		    set ycomm [$w cget -yscrollcommand]
		    if { $ycomm != "" } {
		        if { [tk windowingsystem] eq "aqua" } {
		            $w yview scroll [expr {-1*%D}] units
		        } else {
		            $w yview scroll [expr int(-1*%D/36)] units
		        }
		        break
		    }
		}]
	    if { !$err } { break }
	    set w [winfo parent $w]
	}
	break
    }
    gestures::init [winfo toplevel $html] [list HelpViewer::gestures_do]


    # If an argument was specified, read it into the HTML widget.
    #
    update

    if {$file!=""} {
	LoadFile $html $file
    }
    set x [expr [winfo screenwidth $base]/2-400]
    set y [expr [winfo screenheight $base]/2-300]
    wm geometry $base 700x460+${x}+$y
    update idletasks
    wm deiconify $base

    return $html
}

proc HelpViewer::gestures_do { gesture } {
    variable html

    switch $gesture {
	GestureLeft {
	    History::GoBackward $html
	}
	GestureRight {
	    History::GoForward $html
	}
	GestureUp,GestureDown {
	    wm withdraw [winfo toplevel $html]
	}
    }
}

proc HelpViewer::addtag_to_all_widgets { w tag } {
    bindtags $w [concat [list $tag] [bindtags $w]]
    foreach wc [winfo children $w] {
	addtag_to_all_widgets $wc $tag
    }
}


proc HelpViewer::HelpSearchWord { word } {
    variable notebook

    set HelpViewer::searchstring $word
    $notebook raise search
    SearchInAllHelp
}

# proc HelpViewer::FillTreeWithToc { tree parent tocfile } {
#
#     package require tdom
#     set fin [open $tocfile r]
#     set htmldata [read $fin]
#     close $fin
#     if { [regexp {(?i)<meta\s+[^>]*charset=utf-8[^>]*>} $htmldata] } {
#         set fin [open $tocfile r]
#         fconfigure $fin -encoding utf-8
#         set htmldata [read $fin]
#         close $fin
#     }
#     dom parse -html $htmldata doc
#     set root [$doc documentElement]
#
#     set bodynode [$root selectNodes //body|BODY]
#     foreach node [$bodynode selectNodes ul|UL|(p/ul)|(P/UL)] {
#         _FillTreeWithToc $tree $parent $node
#     }
# }
#
# proc HelpViewer::_FillTreeWithToc { tree treeparentnode ulnode } {
#
#     set lasttreenode ""
#     foreach node [$ulnode selectNodes li|LI|ul|UL] {
#         switch [string tolower [$node nodeName]] {
#             li {
#                 set anode [$node selectNodes a|A]
#                 set href [$anode selectNodes @href|@HREF]
#                 regsub -all {\[|\]} [$node toXPath] {*} name
#                 $tree insert end $treeparentnode $name -image filedocument16 \
#                     -text [$anode text] -drawcross never \
#                     -data [list file $href]
#                 set lasttreenode $name
#             }
#             ul {
#                 if { $lasttreenode != "" } {
#                     set data [$tree itemcget $lasttreenode -data]
#                     $tree itemconfigure $lasttreenode -image appbook16 -drawcross \
#                         allways -data [list folder [lindex $data 1]]
#                 }
#                 _FillTreeWithToc $tree $lasttreenode $node
#             }
#         }
#     }
# }

proc HelpViewer::FillTreeWithToc { tree parent tocfile } {

    set fin [open $tocfile r]
    set html [read $fin]
    close $fin
    if { [regexp {(?i)<meta\s+[^>]*charset=utf-8[^>]*>} $html] } {
	set fin [open $tocfile r]
	fconfigure $fin -encoding utf-8
	set html [read $fin]
	close $fin
    }
    set htmltree [::struct::tree]
    htmlparse::2tree $html $htmltree
    _FillTreeWithToc $tree $parent $htmltree root [file dirname $tocfile]
    $htmltree destroy
}

proc HelpViewer::_FillTreeWithToc { tree treeparentnode htmltree parentNode dir } {

    set lasttreenode ""
    foreach node [$htmltree children $parentNode] {
	set type [string tolower [$htmltree get $node type]]
	switch $type {
	    li {
		set found 0
		foreach childNode [$htmltree children $node] {
		    if { [string tolower [$htmltree get $childNode type]] eq "a" } {
		        set found 1 ; break
		    }
		}
		if { !$found } { continue }
		set data [$htmltree get $childNode data]
		if { ![regexp {(?i)href\s*=\s*\"([^\"]*)\"} $data {} href] && \
		    ![regexp {(?i)href\s*=\s*\'([^\']*)\'} $data {} href] && \
		        ![regexp {(?i)href\s*=\s*(\S*)} $data {} href] } {
		        continue
		}
		set found 0
		foreach childchildNode [$htmltree children $childNode] {
		    if { [$htmltree get $childchildNode type] eq "PCDATA" } {
		        set found 1 ; break
		    }
		}
		if { !$found } { continue }
		set name ht_$childNode
		set txt [decode [$htmltree get $childchildNode data]]
		set data [decode [file join $dir $href]]
		$tree insert end $treeparentnode $name -image filedocument16 \
		    -text $txt -drawcross never \
		    -data [list file $data]

		if { $treeparentnode eq "root" } {
		    set parentdata ""
		} else {
		    set parentdata [$tree itemcget $treeparentnode -data]
		}
		if { [lindex $data 0] ne "folder" && $treeparentnode ne "root" } {
		    $tree itemconfigure $treeparentnode -image appbook16 -drawcross \
		        allways -data [list folder [lindex $parentdata 1]]
		}
		set lasttreenode $name
		_FillTreeWithToc $tree $lasttreenode $htmltree $node $dir
	    }
	    ul {
		if { $lasttreenode != "" } {
		    set data [$tree itemcget $lasttreenode -data]
		    $tree itemconfigure $lasttreenode -image appbook16 -drawcross \
		        allways -data [list folder [lindex $data 1]]
		    _FillTreeWithToc $tree $lasttreenode $htmltree $node $dir
		} else {
		    _FillTreeWithToc $tree $treeparentnode $htmltree $node $dir
		}
	    }
	    default {
		_FillTreeWithToc $tree $treeparentnode $htmltree $node $dir
	    }
	}
    }
}

# regmap re string var script
# substitute all the occurrences of 're' in 'string' with
# the result of the evaluation of 'script'. Script is called
# for every match with 'var' variable set to the matching string.
#
# Example: regmap {[0-9]+} "1 2 hello 3 4 world" x {expr {$x+1}}
# Result:  "2 3 hello 4 5 world"
proc regmap {re string var script} {
    set submatches [lindex [regexp -about $re] 0]
    lappend varlist idx
    while {[incr submatches -1] >= 0} {
	lappend varlist _
    }
    set res $string
    set indices [regexp -all -inline -indices $re $string]
    set delta 0
    foreach $varlist $indices {
	foreach {start end} $idx break
	set substr [string range $string $start $end]
	uplevel [list set $var $substr]
	set subresult [uplevel $script]
	incr start $delta
	incr end $delta
	set res [string replace $res $start $end $subresult]
	incr delta [expr {[string length $subresult]-[string length $substr]}]
    }
    return $res
}


proc HelpViewer::decode {str} {

    return [regmap {&#\d+;} $str d {format %c [string range $d 2 end-1]}]
}


proc HelpViewer::FillDir { tree node } {
    variable HelpBaseDir

    if { $node == "root" } {
	set dir $HelpBaseDir
    } else {
	set dir [lindex [$tree itemcget $node -data] 1]
    }

    set idxfolder 0
    set files ""
    foreach i [glob -nocomplain -dir $dir *] {
	lappend files [file tail $i]
    }
    if { [set ipos [lsearch -regexp $files {_toc\.html?$}]] != -1 } {
	set err [catch {
		FillTreeWithToc $tree $node [file join $dir [lindex $files $ipos]]
	    } out]
	if { !$err } { return $out }
    }
    foreach i [lsort -dictionary $files] {
	set fullpath [file join $dir $i]
	regsub {^[0-9]+} $i {} name
	regsub -all {\s} $fullpath _ item
	if { [file isdir $fullpath] } {
	    if { [string equal -nocase $i "images"] } { continue }
	    $tree insert $idxfolder $node $item -image appbook16 -text $name \
		-data [list folder $fullpath] -drawcross allways
	    incr idxfolder
	} elseif { [string match .htm* [file ext $i]] } {
	    set name [file root $i]
	    $tree insert end $node $item -image filedocument16 -text $name \
		-data [list file $fullpath]
	}
    }
}

proc HelpViewer::moddir { idx tree node } {
    variable HelpBaseDir

    if { $idx && [$tree itemcget $node -drawcross] == "allways" } {
	FillDir $tree $node
	$tree itemconfigure $node -drawcross auto

	if { [llength [$tree nodes $node]] } {
	    $tree itemconfigure $node -image appbookopen16
	} else {
	    $tree itemconfigure $node -image appbook16
	}
    } else {
	if { [lindex [$tree itemcget $node -data] 0] == "folder" } {
	    switch $idx {
		0 { set img appbook16 }
		1 { set img appbookopen16 }
	    }
	    $tree itemconfigure $node -image $img
	}
    }
}

proc HelpViewer::KeyPress { a } {
    variable tree
    variable searchstring

    set node [$tree selection get]
    if { [llength $node] != 1 } { return }

    append searchstring $a
    after 300 [list set HelpViewer::searchstring ""]

    if { [$tree itemcget $node -open] == 1 && [llength [$tree nodes $node]] > 0 } {
	set parent $node
	set after 1
    } else {
	set parent [$tree parent $node]
	set after 0
    }

    foreach i [$tree nodes $parent] {
	if { !$after } {
	    if { $i == $node } {
		if { [string length $HelpViewer::searchstring] > 1 } {
		    set after 2
		} else {
		    set after 1
		}
	    }
	}
	if { $after == 2 && [string match -nocase $HelpViewer::searchstring* \
		[$tree itemcget $i -text]] } {
	    $tree selection clear
	    $tree selection set $i
	    $tree see $i
	    return
	}
	if { $after == 1 } { set after 2 }
    }
    foreach i [$tree nodes [$tree parent $node]] {
	if { $i == $node } { return }
	if { [string match -nocase $HelpViewer::searchstring* [$tree itemcget $i -text]] } {
	    $tree selection clear
	    $tree selection set $i
	    $tree see $i
	    return
	}
    }
}

proc HelpViewer::Select { tree num node } {
    variable dblclick
    variable html

    if { $node == "" } {
	set node [$tree selection get]
	if { [llength $node] != 1 } { return }
    } elseif { ![$tree exists $node] } {
	return
    }
    set dblclick 1


    if { $num eq "" || $num eq "noload" || $num >= 1 } {
	if { [$tree itemcget $node -drawcross] != "never" } {
	    if { $num eq "" || $num eq "noload" || [$tree itemcget $node -open] == 0 } {
		$tree itemconfigure $node -open 1
		set idx 1
	    } else {
		$tree itemconfigure $node -open 0
		set idx 0
	    }
	    moddir $idx $tree $node
	    if { $num == 1 && $idx == 0 } {
		return
	    }
	}
	$tree selection set $node
	if { [llength [$tree selection get]] == 1 } {
	    set data [$tree itemcget [$tree selection get] -data]
	    if { $num eq "" || ($num >= 1 && $num <= 2) } {
		LoadRef $html [lindex $data 1]
		#LoadFile $html [lindex $data 1]
	    }
	}
	return
    }
}

proc HelpViewer::SelectInTree { name } {
    variable HelpBaseDir
    variable tree

    set nameL [file split $name]

    set level 0
    set node root
    while 1 {
	set found 0
	foreach i [$tree nodes $node] {
	    if { [$tree itemcget $i -text] eq [lindex $nameL $level] } {
		set found 1
		break
	    }
	}
	if { !$found } { return }
	if { [lindex [$tree itemcget $i -data] 0] == "folder" } {
	    if { [$tree itemcget $i -open] == 0 } {
		$tree itemconfigure $i -open 1
	    }
	    moddir 1 $tree $i
	}

	if { $level == [llength $nameL]-1 } {
	    Select $tree "" $i
	    $tree see $i
	    return
	}
	set node $i
	incr level
    }
}

proc HelpViewer::_give_tree_nodes { tree parent } {

    set nodes ""
    foreach i [$tree nodes $parent] {
	lappend nodes $i
	eval lappend nodes [_give_tree_nodes $tree $i]
    }
    return $nodes
}

proc HelpViewer::TryToSelect { name { noload "" } } {
    variable HelpBaseDir
    variable tree

    set nameL [file split $name]

    set level [llength [file split $HelpBaseDir]]
    set node root
    while 1 {
	set found 0
	foreach i [$tree nodes $node] {
	    if { [lindex [$tree itemcget $i -data] 1] == [eval file join [lrange $nameL 0 $level]] } {
		set found 1
		break
	    }
	}
	if { !$found } {
	    foreach i [_give_tree_nodes $tree root] {
		if { [lindex [$tree itemcget $i -data] 1] == [eval file join [lrange $nameL 0 $level]] } {
		    set found 1
		    break
		}
	    }
	    if { !$found } { return }
	}
	set n $i
	while { $n ne "root" } {
	    if { [lindex [$tree itemcget $n -data] 0] == "folder" } {
		if { [$tree itemcget $n -open] == 0 } {
		    $tree itemconfigure $n -open 1
		}
		moddir 1 $tree $n
	    }
	    set n [$tree parent $n]
	}

	if { $level == [llength $nameL]-1 } {
	    Select $tree $noload $i
	    $tree see $i
	    return
	}
	set node $i
	incr level
    }
}

proc HelpViewer::Search3 {} {
    variable html
    variable searchstring
    variable SearchPos
    variable searchcase
    variable searchFromBegin

    if { ![info exists searchstring] } {
	WarnWin "Before using 'Continue search', use 'Search'" [winfo toplevel $html]
	return
    }
    if { $searchstring eq "" } { return }

    set bnode ""
    if { $SearchPos ne "" } {
	foreach "bnode idx" $SearchPos break
    } elseif { !$searchFromBegin } {
	foreach "bnode idx" [$html select from] break
    }
    foreach node [$html search *] {
	if { $bnode ne "" && $node ne $bnode } { continue }
	if { $searchcase } {
	    set rex "(?q)$searchstring"
	} else {
	    set rex "(?iq)$searchstring"
	}
	set ret [regexp -inline -all -indices $rex [$node text]]
	if { $node eq $bnode } {
	    foreach i $ret {
		foreach "ini end" $i break
		if { $idx == $ini } {
		    set idx ""
		} elseif { $idx eq "" } {
		    set bnode $node
		    break
		}
	    }
	    set bnode ""
	} elseif { $bnode eq "" && $ret ne "" } {
	    foreach "ini end" [lindex $ret 0] break
	    set bnode $node
	    break
	}
    }
    if { $bnode eq "" && $SearchPos ne "" } {
	bell
	set SearchPos ""
	Search3
	return
    }
    if { $bnode eq "" } {
	bell
	return
    }

    catch { $html select clear }
    $html select from $bnode $ini
    $html select to $bnode [expr {$end+1}]
    $html yview $bnode
    $html yview scroll -1 unit
    set SearchPos [list $bnode $ini]
}


proc HelpViewer::Search {} {
    variable html

    if { [package vcompare [package present Tkhtml] 3.0] >= 0 } {
	return [Search3]
    }

    if { ![info exists ::HelpViewer::searchstring] } {
	WarnWin "Before using 'Continue search', use 'Search'" [winfo toplevel $html]
	return
    }
    if { $HelpViewer::searchstring != "" } {
	
	set comm [list $html text find $HelpViewer::searchstring]
	if { $::HelpViewer::searchcase == 0 } {
	    lappend comm nocase
	}
	
	if { $HelpViewer::SearchType == "-forwards" } {
	    if { $HelpViewer::SearchPos != "" } {
		lappend comm after $HelpViewer::SearchPos
	    } else {
		lappend comm after 1.0
	    }
	} else {
	    if { $HelpViewer::SearchPos != "" } {
		lappend comm before $HelpViewer::SearchPos
	    } else {
		lappend comm before end
	    }
	}
	set idx1 ""
	foreach "idx1 idx2" [eval $comm] break

	if { $idx1 == "" } {
	    bell
	} else {
	    scan $idx2 "%d.%d" line char
	    set idx2 $line.[expr $char+1]
	    $html selection set $idx1 [$html index $idx2]

	    set y [lindex [$html coords $idx1] 1]
	    if { $y == "" } { set y 0 }
	    set height [lindex [$html coords end] 1]
	
	    foreach "f1 f2" [$html yview] break
	    set ys [expr $y/double($height)-($f2-$f1)/2.0]
	    if { $ys < 0 } { set ys 0 }
	    $html yview moveto $ys
	    $html refresh

	    set HelpViewer::SearchPos $idx1
	}
    }
}

proc HelpViewer::SearchWindow {} {
    variable html
    variable searchmode
    variable searchcase
    variable searchFromBegin
    variable SearchType

    set f [DialogWin::Init $html [_ "Search"] separator "" [_ "Ok"] [_ "Cancel"]]
    set w [winfo toplevel $f]

    label $f.l1 -text [_ "Text"]: -grid 0
    entry $f.e1 -textvariable ::HelpViewer::searchstring -grid "1 px3 py3"

    set f25 [frame $f.f25 -bd 1 -relief ridge -grid "0 2 w px3"]
    radiobutton $f25.r1 -text [_ "Forward"] -variable ::HelpViewer::SearchType \
	-value -forwards -grid "0 w"
    radiobutton $f25.r2 -text [_ "Backward"] -variable ::HelpViewer::SearchType \
	-value -backwards -grid "0 w"

    set f3 [frame $f.f3 -grid "0 2 w"]
    checkbutton $f3.cb1 -text [_ "Consider case"] -variable ::HelpViewer::searchcase \
	-grid 0
    checkbutton $f3.cb2 -text [_ "From beginning"] -variable ::HelpViewer::searchFromBegin \
	-grid 1

    supergrid::go $f

    if { ![info exists ::HelpViewer::searchstring] } {
	set ::HelpViewer::searchstring ""
    }

    set searchmode -exact
    set searchcase 0
    set searchFromBegin 1
    set SearchType -forwards

    tkTabToWindow $f.e1
    bind $w <Return> "DialogWin::InvokeOK"

    set action [DialogWin::CreateWindow]
    switch $action {
	0 {
	    DialogWin::DestroyWindow
	    return
	}
	1 {
	    if { $::HelpViewer::searchstring == "" } {
		DialogWin::DestroyWindow
		return
	    }
	    set ::HelpViewer::SearchPos ""
	    Search
	    DialogWin::DestroyWindow
	}
    }
}

proc HelpViewer::EnterDirForIndex { dir } {
    variable IndexDirectory

    set IndexDirectory $dir
}

proc HelpViewer::CreateIndex {} {
    variable HelpBaseDir
    variable IndexDirectory
    variable Index
    variable IndexFilesTitles
    variable progressbar
    variable progressbarStop
    variable html

    if { ![info exists IndexDirectory] } {
	set IndexDirectory $HelpBaseDir
    }

    if { [array exists Index] } { return }
    if { [file exists [file join $IndexDirectory wordindex]] && \
	[file mtime [file join $IndexDirectory wordindex]] >= [file mtime $HelpBaseDir] } {
	set fin [open [file join $IndexDirectory wordindex] r]
	fconfigure $fin -encoding utf-8
	foreach "IndexFilesTitles aa" [read $fin] break
	array set Index $aa
	close $fin
	return
    }

    WaitState 1

    ProgressDlg $html.prdg -textvariable HelpViewer::progressbarT -variable \
	    HelpViewer::progressbar -title "Creating search index" \
	     -troughcolor \#48c96f -stop Stop -command "set HelpViewer::progressbarStop 1"

    set progressbar 0
    set progressbarStop 0

    catch { unset Index }

    set files [::fileutil::findByPattern $HelpBaseDir "*.htm *.html"]

    set len [llength [file split $HelpBaseDir]]
    set ipos 0
    set numfiles [llength $files]

    set IndexFilesTitles ""

    foreach i $files {
	set HelpViewer::progressbar [expr int($ipos*50/$numfiles)]
	set HelpViewer::progressbarT $HelpViewer::progressbar%
	if { $HelpViewer::progressbarStop } {
	    destroy .prdg
	    return
	}

	set fin [open $i r]
	set aa [read $fin]
	close $fin
	if { [regexp {(?i)<meta\s+[^>]*charset=utf-8[^>]*>} $aa] } {
	    set fin [open $i r]
	    fconfigure $fin -encoding utf-8
	    set aa [read $fin]
	    close $fin
	}
	set file [eval file join [lrange [file split $i] $len end]]
	set title ""
	regexp {(?i)<title>(.*?)</title>} $aa {} title
	if { $title == "" } {
	    regexp {(?i)<h([1234])>(.*?)</h\1>} $aa {} {} title
	}
	lappend IndexFilesTitles [list $file $title]
	set IndexPos [expr [llength $IndexFilesTitles]-1]

	foreach j [regexp -inline -all -- {-?\w{3,}} $aa] {
	    if { [string is integer $j] || [string length $j] > 25 || [regexp {_[0-9]+$} $j] } {
		continue
	    }
	    lappend Index([string tolower $j]) $IndexPos
	}
	incr ipos
    }

    proc IndexesSortCommand { e1 e2 } {
	upvar freqs freqsL
	if { $freqsL($e1) > $freqsL($e2) } { return -1 }
	if { $freqsL($e1) < $freqsL($e2) } { return 1 }
	return 0
    }

    set names [array names Index]
    set len [llength $names]
    set ipos 0
    foreach i $names {
	set HelpViewer::progressbar [expr 50+int($ipos*50/$len)]
	set HelpViewer::progressbarT $HelpViewer::progressbar%
	if { $HelpViewer::progressbarStop } {
	    destroy .prdg
	    return
	}
	foreach j $Index($i) {
	    set title [lindex [lindex $IndexFilesTitles $j] 1]
	    if { [string match -nocase *$i* $title] } {
		set icr 10
	    } else { set icr 1 }
	    if { ![info exists freqs($j)] } {
		set freqs($j) $icr
	    } else { incr freqs($j) $icr }
	}
#          if { $i == "variable" } {
#              puts "-----variable-----"
#              foreach j $Index($i) {
#                  puts [lindex $IndexFilesTitles $j]-----$j
#              }
#              parray freqs
#          }
	set Index($i) [lrange [lsort -command HelpViewer::IndexesSortCommand [array names freqs]] \
		0 4]

#          if { $i == "variable" } {
#              puts "-----variable-----"
#              foreach j [lsort -command HelpViewer::IndexesSortCommand [array names freqs]] {
#                  puts [lindex $IndexFilesTitles $j]-----$j
#              }
#          }
	unset freqs
	incr ipos
    }

    set HelpViewer::progressbar 100
    set HelpViewer::progressbarT $HelpViewer::progressbar%
    destroy $html.prdg
    set fout [open [file join $IndexDirectory wordindex] w]
    fconfigure $fout -encoding utf-8
    puts -nonewline $fout [list $IndexFilesTitles [array get Index]]
    close $fout
    WaitState 0
}

proc HelpViewer::IsWordGood { word otherwords } {
    variable Index
    variable IndexFilesTitles

    if { $otherwords == "" } { return 1 }

    if { ![info exists Index($word)] } { return 0 }

    foreach i $Index($word) {
	set file [lindex [lindex $IndexFilesTitles $i] 0]
	if { [HasFileTheWord $file $otherwords] } { return 1 }
    }
    return 0
}

proc HelpViewer::HasFileTheWord { file otherwords } {
    variable HelpBaseDir
    variable Index
    variable IndexFilesTitles
    variable FindWordInFileCache

    set fullfile [file join $HelpBaseDir $file]

    foreach word $otherwords {
	if { [info exists FindWordInFileCache($file,$word)] } {
	    if { !$FindWordInFileCache($file,$word) } { return 0 }
	    continue
	}
	set fin [open $fullfile r]
	set aa [read $fin]
	close $fin
	if { [regexp {(?i)<meta\s+[^>]*charset=utf-8[^>]*>} $aa] } {
	    set fin [open $fullfile r]
	    fconfigure $fin -encoding utf-8
	    set aa [read $fin]
	    close $fin
	}
	if { [string match -nocase *$word* $aa] } {
	    set FindWordInFileCache($file,$word) 1
	} else {
	    set FindWordInFileCache($file,$word) 0
	    return 0
	}
    }
    return 1
}

proc HelpViewer::SearchInAllHelp { { combo "" } } {
    variable HelpBaseDir
    variable Index
    variable searchlistbox1

    set word [string tolower $HelpViewer::searchstring]

    if { $combo ne "" } {
	set values [$combo cget -values]
	if { [set ipos [lsearch -exact $values $word]] != -1 } {
	    set values [lreplace $values $ipos $ipos]
	}
	set values [linsert $values 0 $word]
	$combo configure -values [lrange $values 0 8]
    }

    CreateIndex

    set HelpViewer::SearchFound ""
    set HelpViewer::SearchFound2 ""

    if { [string trim $word] == "" } { return }

    set words [regexp -all -inline {\S+} $word]
    if { [llength $words] > 1 } {
	set word [lindex $words 0]
	set otherwords [lrange $words 1 end]
    } else { set otherwords "" }

    set ipos 0
    set iposgood -1
    foreach i [array names Index *$word*] {
	if { ![IsWordGood $i $otherwords] } { continue }

	lappend HelpViewer::SearchFound $i
	if { [string equal $word [lindex $i 0]] } { set iposgood $ipos }
	incr ipos
    }
    if { $iposgood == -1 && [llength [GiveManHelpNames $HelpViewer::searchstring]] > 0 } {
	lappend HelpViewer::SearchFound $HelpViewer::searchstring
	set iposgood $ipos
    }

    if { $iposgood >= 0 } {
	$searchlistbox1 selection clear 0 end
	$searchlistbox1 selection set $iposgood
	$searchlistbox1 see $iposgood
	SearchInAllHelpL1
    }
}

proc HelpViewer::SearchInAllHelpL1 {} {
    variable Index
    variable IndexFilesTitles
    variable SearchFound2
    variable SearchFound2data
    variable searchlistbox1
    variable searchlistbox2

    set SearchFound2 ""
    set SearchFound2data ""

    set sels [$searchlistbox1 curselection]
    if { $sels == "" } {
	bell
	return
    }

    set words [regexp -all -inline {\S+} $HelpViewer::searchstring]
    if { [llength $words] > 1 } {
	set otherwords [lrange $words 1 end]
    } else { set otherwords "" }

    set ipos 0
    set iposgood -1
    set iposgoodW -1
    foreach i $sels {
	set word [$searchlistbox1 get $i]
	if { [info exists Index($word)] } {
	    foreach i $Index($word) {
		foreach "file title" [lindex $IndexFilesTitles $i] break

		if { ![HasFileTheWord $file $otherwords] } { continue }

		if { [lsearch $HelpViewer::SearchFound2 $title] != -1 } { continue }

		lappend SearchFound2 $title
		lappend SearchFound2data $i
		if { [string match -nocase *$word* $title] } {
		    set W 1
		    foreach i $otherwords {
		        if { [string match -nocase *$i* $title] } { incr W }
		    }
		    if { [string match -nocase *$HelpViewer::searchstring* $title] } { incr W }
		    if { [string match *$HelpViewer::searchstring* $title] } { incr W }
		    if { [string equal -nocase $HelpViewer::searchstring $title] } { incr W }
		    if { [string equal $HelpViewer::searchstring $title] } { incr W }
		
		    if { $W > $iposgoodW } {
		        set iposgood $ipos
		        set iposgoodW $W
		    }
		}
		incr ipos
	    }
	}
	foreach i [GiveManHelpNames $word] {
	    lappend SearchFound2 $i
	    if { $iposgood == -1 } {
		set iposgood $ipos
	    } else { set iposgood -2 }
	    incr ipos
	}
    }
    if { $iposgood < 0 && $ipos > 0 } { set iposgood 0 }
    if { $iposgood >= 0 } {
	focus $searchlistbox2
	$searchlistbox2 selection clear 0 end
	$searchlistbox2 selection set $iposgood
	$searchlistbox2 see $iposgood
	SearchInAllHelpL2
    }
}

proc HelpViewer::SearchInAllHelpL2 {} {
    variable HelpBaseDir
    variable SearchFound2data
    variable IndexFilesTitles
    variable SearchFound2
    variable SearchFound2data
    variable html
    variable searchlistbox2

    set sels [$searchlistbox2 curselection]
    if { [llength $sels] != 1 } {
	bell
	return
    }
    if { [regexp {(.*)\(man (.*)\)} [lindex $SearchFound2 $sels]] } {
	SearchManHelpFor $html [lindex $SearchFound2 $sels]
    } else {
	set i [lindex $SearchFound2data $sels]
	set file [file join $HelpBaseDir [lindex [lindex $IndexFilesTitles $i] 0]]
	
	LoadFile $html $file 1
    }
}

proc HelpViewer::WaitState { what } {
    variable tree
    variable html

    switch $what {
	1 {
	    $tree configure -cursor watch
	    if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
		$html configure -cursor watch
	    } else {
		[winfo parent [winfo parent $html]] configure -cursor watch
	    }
	}
	0 {
	    $tree configure -cursor ""
	    if { [package vcompare [package present Tkhtml] 3.0] < 0 } {
		$html configure -cursor ""
	    } else {
		[winfo parent [winfo parent $html]] configure -cursor ""
	    }
	}
    }
    update
}

################################################################################
#    gestures
#
#
################################################################################

namespace eval gestures {
    variable path
    variable callback
}

proc gestures::init { w _callback } {
    variable callback

    set callback($w) $_callback

    _prepare_for_gestures $w gestures_$w
    bind gestures_$w <ButtonPress-3> [list gestures::press %X %Y]
    bind gestures_$w <B3-Motion> [list gestures::motion %X %Y]
    bind gestures_$w <ButtonRelease-3> [list gestures::release $w %X %Y]
}

proc gestures::_prepare_for_gestures { w bindtag } {
    bindtags $w [linsert [bindtags $w] 0 $bindtag]
    foreach i [winfo children $w] { _prepare_for_gestures $i $bindtag }
}

proc gestures::press { x y } {
    variable path

    set path [list $x $y]
}

proc gestures::motion { x y } {
    variable path

    lappend path $x $y
}

proc gestures::release { w x y } {
    variable path
    variable callback

    lappend path $x $y

    set t 10
    foreach "x y" $path {
	if { ![info exists box(minx)] || $x < $box(minx) } { set box(minx) $x }
	if { ![info exists box(miny)] || $y < $box(miny) } { set box(miny) $y }
	if { ![info exists box(maxx)] || $x > $box(maxx) } { set box(maxx) $x }
	if { ![info exists box(maxy)] || $y > $box(maxy) } { set box(maxy) $y }
    }

    if { $box(maxx)-$box(minx) < $t && $box(maxy)-$box(miny) < $t } { return }

    foreach "x0 y0" [lrange $path 0 1] break
    foreach "xend yend" [lrange $path end-1 end] break

    if { $x0-$xend >= $t } {
	set gesture GestureLeft
    } elseif { $xend-$x0 >= $t } {
	set gesture GestureRight
    } elseif { $x0-$box(miny) >= $t && $xend-$box(miny) >= $t } {
	set gesture GestureUp,GestureDown
    } else { return }

    uplevel #0 $callback($w) $gesture
    return -code break
}


if { [info exists argv0] && [info script] == $argv0 } {
    set cmd [string map [list %FILE [list [info script]]] {
	package require RamDebugger
	ramdebugger eval [list RamDebugger::OpenFileF %FILE]
    }]
    bind all <F12> [list catch $cmd]

    wm withdraw .
    package require Tkhtml
    package require Img
    if { [llength $argv] && [file readable [lindex $argv 0]] } {
	set file [lindex $argv 0]
    } elseif { $::tcl_platform(platform) != "windows"} {
	set file {/c/TclTk/RamDebugger/help}
    } else {
	set file {C:\Users\ramsan\Documents\myTclTk\RamDebugger\help}
    }
    HelpViewer::HelpWindow $file
}

package provide helpviewer 1.4
