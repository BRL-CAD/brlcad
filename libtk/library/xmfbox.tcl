# xmfbox.tcl --
#
#	Implements the "Motif" style file selection dialog for the
#	Unix platform. This implementation is used only if the
#	"tk_strictMotif" flag is set.
#
# SCCS: @(#) xmfbox.tcl 1.6 97/10/01 15:06:07
#
# Copyright (c) 1996 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#


# tkMotifFDialog --
#
#	Implements a file dialog similar to the standard Motif file
#	selection box.
#
# Return value:
#
#	A list of two members. The first member is the absolute
#	pathname of the selected file or "" if user hits cancel. The
#	second member is the name of the selected file type, or ""
#	which stands for "default file type"
#
proc tkMotifFDialog {args} {
    global tkPriv
    set w __tk_filedialog
    upvar #0 $w data

    if ![string compare [lindex [info level 0] 0] tk_getOpenFile] {
	set type open
    } else {
	set type save
    }

    tkMotifFDialog_Config $w $type $args

    if {![string compare $data(-parent) .]} {
        set w .$w
    } else {
        set w $data(-parent).$w
    }

    # (re)create the dialog box if necessary
    #
    if {![winfo exists $w]} {
	tkMotifFDialog_Create $w
    } elseif {[string compare [winfo class $w] TkMotifFDialog]} {
	destroy $w
	tkMotifFDialog_Create $w
    }
    wm transient $w $data(-parent)

    tkMotifFDialog_Update $w

    # 5. Withdraw the window, then update all the geometry information
    # so we know how big it wants to be, then center the window in the
    # display and de-iconify it.

    wm withdraw $w
    update idletasks
    set x [expr [winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
	    - [winfo vrootx [winfo parent $w]]]
    set y [expr [winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
	    - [winfo vrooty [winfo parent $w]]]
    wm geom $w +$x+$y
    wm deiconify $w
    wm title $w $data(-title)

    # 6. Set a grab and claim the focus too.

    set oldFocus [focus]
    set oldGrab [grab current $w]
    if {$oldGrab != ""} {
	set grabStatus [grab status $oldGrab]
    }
    grab $w
    focus $data(sEnt)
    $data(sEnt) select from 0
    $data(sEnt) select to   end

    # 7. Wait for the user to respond, then restore the focus and
    # return the index of the selected button.  Restore the focus
    # before deleting the window, since otherwise the window manager
    # may take the focus away so we can't redirect it.  Finally,
    # restore any grab that was in effect.

    tkwait variable tkPriv(selectFilePath)
    catch {focus $oldFocus}
    grab release $w
    wm withdraw $w
    if {$oldGrab != ""} {
	if {$grabStatus == "global"} {
	    grab -global $oldGrab
	} else {
	    grab $oldGrab
	}
    }
    return $tkPriv(selectFilePath)
}

proc tkMotifFDialog_Config {w type argList} {
    upvar #0 $w data

    set data(type) $type

    # 1: the configuration specs
    #
    set specs {
	{-defaultextension "" "" ""}
	{-filetypes "" "" ""}
	{-initialdir "" "" ""}
	{-initialfile "" "" ""}
	{-parent "" "" "."}
	{-title "" "" ""}
    }

    # 2: default values depending on the type of the dialog
    #
    if ![info exists data(selectPath)] {
	# first time the dialog has been popped up
	set data(selectPath) [pwd]
	set data(selectFile) ""
    }

    # 3: parse the arguments
    #
    tclParseConfigSpec $w $specs "" $argList

    if ![string compare $data(-title) ""] {
	if ![string compare $type "open"] {
	    set data(-title) "Open"
	} else {
	    set data(-title) "Save As"
	}
    }

    # 4: set the default directory and selection according to the -initial
    #    settings
    #
    if [string compare $data(-initialdir) ""] {
	if [file isdirectory $data(-initialdir)] {
	    set data(selectPath) [glob $data(-initialdir)]
	} else {
	    error "\"$data(-initialdir)\" is not a valid directory"
	}
    }
    set data(selectFile) $data(-initialfile)

    # 5. Parse the -filetypes option. It is not used by the motif
    #    file dialog, but we check for validity of the value to make sure
    #    the application code also runs fine with the TK file dialog.
    #
    set data(-filetypes) [tkFDGetFileTypes $data(-filetypes)]

    if ![info exists data(filter)] {
	set data(filter) *
    }
    if ![winfo exists $data(-parent)] {
	error "bad window path name \"$data(-parent)\""
    }
}

proc tkMotifFDialog_Create {w} {
    set dataName [lindex [split $w .] end]
    upvar #0 $dataName data

    # 1: Create the dialog ...
    #
    toplevel $w -class TkMotifFDialog
    set top [frame $w.top -relief raised -bd 1]
    set bot [frame $w.bot -relief raised -bd 1]

    pack $w.bot -side bottom -fill x
    pack $w.top -side top -expand yes -fill both

    set f1 [frame $top.f1]
    set f2 [frame $top.f2]
    set f3 [frame $top.f3]

    pack $f1 -side top    -fill x
    pack $f3 -side bottom -fill x
    pack $f2 -expand yes -fill both

    set f2a [frame $f2.a]
    set f2b [frame $f2.b]

    grid $f2a -row 0 -column 0 -rowspan 1 -columnspan 1 -padx 4 -pady 4 \
	-sticky news
    grid $f2b -row 0 -column 1 -rowspan 1 -columnspan 1 -padx 4 -pady 4 \
	-sticky news
    grid rowconfig $f2 0    -minsize 0   -weight 1
    grid columnconfig $f2 0 -minsize 0   -weight 1
    grid columnconfig $f2 1 -minsize 150 -weight 2

    # The Filter box
    #
    label $f1.lab -text "Filter:" -under 3 -anchor w
    entry $f1.ent
    pack $f1.lab -side top -fill x -padx 6 -pady 4
    pack $f1.ent -side top -fill x -padx 4 -pady 0
    set data(fEnt) $f1.ent

    # The file and directory lists
    #
    set data(dList) [tkMotifFDialog_MakeSList $w $f2a Directory: 0 DList]
    set data(fList) [tkMotifFDialog_MakeSList $w $f2b Files:     2 FList]

    # The Selection box
    #
    label $f3.lab -text "Selection:" -under 0 -anchor w
    entry $f3.ent
    pack $f3.lab -side top -fill x -padx 6 -pady 0
    pack $f3.ent -side top -fill x -padx 4 -pady 4
    set data(sEnt) $f3.ent

    # The buttons
    #
    set data(okBtn) [button $bot.ok     -text OK     -width 6 -under 0 \
	-command "tkMotifFDialog_OkCmd $w"]
    set data(filterBtn) [button $bot.filter -text Filter -width 6 -under 0 \
	-command "tkMotifFDialog_FilterCmd $w"]
    set data(cancelBtn) [button $bot.cancel -text Cancel -width 6 -under 0 \
	-command "tkMotifFDialog_CancelCmd $w"]

    pack $bot.ok $bot.filter $bot.cancel -padx 10 -pady 10 -expand yes \
	-side left

    # Create the bindings:
    #
    bind $w <Alt-t> "focus $data(fEnt)"
    bind $w <Alt-d> "focus $data(dList)"
    bind $w <Alt-l> "focus $data(fList)"
    bind $w <Alt-s> "focus $data(sEnt)"

    bind $w <Alt-o> "tkButtonInvoke $bot.ok    "
    bind $w <Alt-f> "tkButtonInvoke $bot.filter"
    bind $w <Alt-c> "tkButtonInvoke $bot.cancel"

    bind $data(fEnt) <Return> "tkMotifFDialog_ActivateFEnt $w"
    bind $data(sEnt) <Return> "tkMotifFDialog_ActivateSEnt $w"

    wm protocol $w WM_DELETE_WINDOW "tkMotifFDialog_CancelCmd $w"
}

proc tkMotifFDialog_MakeSList {w f label under cmd} {
    label $f.lab -text $label -under $under -anchor w
    listbox $f.l -width 12 -height 5 -selectmode browse -exportselection 0\
	-xscrollcommand "$f.h set" \
	-yscrollcommand "$f.v set" 
    scrollbar $f.v -orient vertical   -takefocus 0 \
	-command "$f.l yview"
    scrollbar $f.h -orient horizontal -takefocus 0 \
	-command "$f.l xview"
    grid $f.lab -row 0 -column 0 -sticky news -rowspan 1 -columnspan 2 \
	-padx 2 -pady 2
    grid $f.l -row 1 -column 0 -rowspan 1 -columnspan 1 -sticky news
    grid $f.v -row 1 -column 1 -rowspan 1 -columnspan 1 -sticky news
    grid $f.h -row 2 -column 0 -rowspan 1 -columnspan 1 -sticky news

    grid rowconfig    $f 0 -weight 0 -minsize 0
    grid rowconfig    $f 1 -weight 1 -minsize 0
    grid columnconfig $f 0 -weight 1 -minsize 0

    # bindings for the listboxes
    #
    set list $f.l
    bind $list <Up>        "tkMotifFDialog_Browse$cmd $w"
    bind $list <Down>      "tkMotifFDialog_Browse$cmd $w"
    bind $list <space>     "tkMotifFDialog_Browse$cmd $w"
    bind $list <1>         "tkMotifFDialog_Browse$cmd $w"
    bind $list <B1-Motion> "tkMotifFDialog_Browse$cmd $w"
    bind $list <Double-1>  "tkMotifFDialog_Activate$cmd $w"
    bind $list <Return>    "tkMotifFDialog_Browse$cmd $w; tkMotifFDialog_Activate$cmd $w"

    bindtags $list "Listbox $list [winfo toplevel $list] all"
    tkListBoxKeyAccel_Set $list

    return $f.l
}

proc tkMotifFDialog_BrowseDList {w} {
    upvar #0 [winfo name $w] data

    focus $data(dList)
    if ![string compare [$data(dList) curselection] ""] {
	return
    }
    set subdir [$data(dList) get [$data(dList) curselection]]
    if ![string compare $subdir ""] {
	return
    }

    $data(fList) selection clear 0 end

    set list [tkMotifFDialog_InterpFilter $w]
    set data(filter) [lindex $list 1]

    case $subdir {
	. {
	    set newSpec [file join $data(selectPath) $data(filter)]
	}
	.. {
	    set newSpec [file join [file dirname $data(selectPath)] \
		$data(filter)]
	}
	default {
	    set newSpec [file join $data(selectPath) $subdir $data(filter)]
	}
    }

    $data(fEnt) delete 0 end
    $data(fEnt) insert 0 $newSpec
}

proc tkMotifFDialog_ActivateDList {w} {
    upvar #0 [winfo name $w] data

    if ![string compare [$data(dList) curselection] ""] {
	return
    }
    set subdir [$data(dList) get [$data(dList) curselection]]
    if ![string compare $subdir ""] {
	return
    }

    $data(fList) selection clear 0 end

    case $subdir {
	. {
	    set newDir $data(selectPath)
	}
	.. {
	    set newDir [file dirname $data(selectPath)]
	}
	default {
	    set newDir [file join $data(selectPath) $subdir]
	}
    }

    set data(selectPath) $newDir
    tkMotifFDialog_Update $w

    if [string compare $subdir ..] {
	$data(dList) selection set 0
	$data(dList) activate 0
    } else {
	$data(dList) selection set 1
	$data(dList) activate 1
    }
}

proc tkMotifFDialog_BrowseFList {w} {
    upvar #0 [winfo name $w] data

    focus $data(fList)
    if ![string compare [$data(fList) curselection] ""] {
	return
    }
    set data(selectFile) [$data(fList) get [$data(fList) curselection]]
    if ![string compare $data(selectFile) ""] {
	return
    }

    $data(dList) selection clear 0 end

    $data(fEnt) delete 0 end
    $data(fEnt) insert 0 [file join $data(selectPath) $data(filter)]
    $data(fEnt) xview end
 
    $data(sEnt) delete 0 end
    $data(sEnt) insert 0 [file join $data(selectPath) $data(selectFile)]
    $data(sEnt) xview end
}

proc tkMotifFDialog_ActivateFList {w} {
    upvar #0 [winfo name $w] data

    if ![string compare [$data(fList) curselection] ""] {
	return
    }
    set data(selectFile) [$data(fList) get [$data(fList) curselection]]
    if ![string compare $data(selectFile) ""] {
	return
    } else {
	tkMotifFDialog_ActivateSEnt $w
    }
}

proc tkMotifFDialog_ActivateFEnt {w} {
    upvar #0 [winfo name $w] data

    set list [tkMotifFDialog_InterpFilter $w]
    set data(selectPath) [lindex $list 0]
    set data(filter)    [lindex $list 1]

    tkMotifFDialog_Update $w
}

proc tkMotifFDialog_InterpFilter {w} {
    upvar #0 [winfo name $w] data

    set text [string trim [$data(fEnt) get]]
    # Perform tilde substitution
    #
    if ![string compare [string index $text 0] ~] {
	set list [file split $text]
	set tilde [lindex $list 0]
	catch {
	    set tilde [glob $tilde]
	}
	set text [eval file join [concat $tilde [lrange $list 1 end]]]
    }

    set resolved [file join [file dirname $text] [file tail $text]]

    if [file isdirectory $resolved] {
	set dir $resolved
	set fil $data(filter)
    } else {
	set dir [file dirname $resolved]
	set fil [file tail    $resolved]
    }

    return [list $dir $fil]
}


proc tkMotifFDialog_ActivateSEnt {w} {
    global tkPriv
    upvar #0 [winfo name $w] data

    set selectFilePath [string trim [$data(sEnt) get]]
    set selectFile     [file tail    $selectFilePath]
    set selectPath     [file dirname $selectFilePath]


    if {![string compare $selectFilePath ""]} {
	tkMotifFDialog_FilterCmd $w
	return
    }

    if {[file isdirectory $selectFilePath]} {
	set data(selectPath) [glob $selectFilePath]
	set data(selectFile) ""
	tkMotifFDialog_Update $w
	return
    }

    if [string compare [file pathtype $selectFilePath] "absolute"] {
	tk_messageBox -icon warning -type ok \
	    -message "\"$selectFilePath\" must be an absolute pathname"
	return
    }

    if ![file exists $selectPath] {
	tk_messageBox -icon warning -type ok \
	    -message "Directory \"$selectPath\" does not exist."
	return
    }

    if ![file exists $selectFilePath] {
	if ![string compare $data(type) open] {
	    tk_messageBox -icon warning -type ok \
		-message "File \"$selectFilePath\" does not exist."
	    return
	}
    } else {
	if ![string compare $data(type) save] {
	    set message [format %s%s \
		"File \"$selectFilePath\" already exists.\n\n" \
		"Replace existing file?"]
	    set answer [tk_messageBox -icon warning -type yesno \
		-message $message]
	    if ![string compare $answer "no"] {
		return
	    }
	}
    }

    set tkPriv(selectFilePath) $selectFilePath
    set tkPriv(selectFile)     $selectFile
    set tkPriv(selectPath)     $selectPath
}


proc tkMotifFDialog_OkCmd {w} {
    upvar #0 [winfo name $w] data

    tkMotifFDialog_ActivateSEnt $w
}

proc tkMotifFDialog_FilterCmd {w} {
    upvar #0 [winfo name $w] data

    tkMotifFDialog_ActivateFEnt $w
}

proc tkMotifFDialog_CancelCmd {w} {
    global tkPriv

    set tkPriv(selectFilePath) ""
    set tkPriv(selectFile)     ""
    set tkPriv(selectPath)     ""
}

# tkMotifFDialog_Update
#
#	Load the files and synchronize the "filter" and "selection" fields
#	boxes.
#
# popup:
#	If this is true, then update the selection field according to the
#	"-selection" flag
#
proc tkMotifFDialog_Update {w} {
    upvar #0 [winfo name $w] data

    $data(fEnt) delete 0 end
    $data(fEnt) insert 0 [file join $data(selectPath) $data(filter)]
    $data(sEnt) delete 0 end
    $data(sEnt) insert 0 [file join $data(selectPath) $data(selectFile)]
 
    tkMotifFDialog_LoadFiles $w
}

proc tkMotifFDialog_LoadFiles {w} {
    upvar #0 [winfo name $w] data

    $data(dList) delete 0 end
    $data(fList) delete 0 end

    set appPWD [pwd]
    if [catch {
	cd $data(selectPath)
    }] {
	cd $appPWD

	$data(dList) insert end ".."
	return
    }

    # Make the dir list
    #
    foreach f [lsort -command tclSortNoCase [glob -nocomplain .* *]] {
	if [file isdir $f] {
	    $data(dList) insert end $f
	}
    }
    # Make the file list
    #
    if ![string compare $data(filter) *] {
	set files [lsort -command tclSortNoCase [glob -nocomplain .* *]]
    } else {
	set files [lsort -command tclSortNoCase \
	    [glob -nocomplain $data(filter)]]
    }

    set top 0
    foreach f $files {
	if ![file isdir $f] {
	    $data(fList) insert end $f
	    if [string match .* $f] {
		incr top
	    }
	}
    }

    # The user probably doesn't want to see the . files. We adjust the view
    # so that the listbox displays all the non-dot files
    $data(fList) yview $top

    cd $appPWD
}

proc tkListBoxKeyAccel_Set {w} {
    bind Listbox <Any-KeyPress> ""
    bind $w <Destroy> "tkListBoxKeyAccel_Unset $w"
    bind $w <Any-KeyPress> "tkListBoxKeyAccel_Key $w %A"
}

proc tkListBoxKeyAccel_Unset {w} {
    global tkPriv

    catch {after cancel $tkPriv(lbAccel,$w,afterId)}
    catch {unset tkPriv(lbAccel,$w)}
    catch {unset tkPriv(lbAccel,$w,afterId)}
}

proc tkListBoxKeyAccel_Key {w key} {
    global tkPriv

    append tkPriv(lbAccel,$w) $key
    tkListBoxKeyAccel_Goto $w $tkPriv(lbAccel,$w)
    catch {
	after cancel $tkPriv(lbAccel,$w,afterId)
    }
    set tkPriv(lbAccel,$w,afterId) [after 500 tkListBoxKeyAccel_Reset $w]
}

proc tkListBoxKeyAccel_Goto {w string} {
    global tkPriv

    set string [string tolower $string]
    set end [$w index end]
    set theIndex -1

    for {set i 0} {$i < $end} {incr i} {
	set item [string tolower [$w get $i]]
	if {[string compare $string $item] >= 0} {
	    set theIndex $i
	}
	if {[string compare $string $item] <= 0} {
	    set theIndex $i
	    break
	}
    }

    if {$theIndex >= 0} {
	$w selection clear 0 end
	$w selection set $theIndex $theIndex
	$w activate $theIndex
	$w see $theIndex
    }
}

proc tkListBoxKeyAccel_Reset {w} {
    global tkPriv

    catch {unset tkPriv(lbAccel,$w)}
}

