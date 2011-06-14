# RCS: @(#) $Id$

#
# Demo: Outlook Express folder list
#
proc DemoOutlookFolders {} {

    InitPics outlook-*

    set T [DemoList]

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -itemheight $height -selectmode browse \
	-showroot yes -showrootbutton no -showbuttons yes \
	-showlines [ShouldShowLines $T]

    $T configure -canvaspadx {4 0} -canvaspady {2 0}

    #
    # Create columns
    #

    $T column create -text Folders -tags C0
    $T configure -treecolumn C0

    #
    # Create custom item states.
    # When an item has the custom "unread" state, the elemTxtName element
    # uses a bold font and the elemTxtCount element is visible.
    #

    $T state define unread

    #
    # Create elements
    #

    $T element create elemImg image
    $T element create elemTxtName text -fill [list $::SystemHighlightText {selected focus}] \
	-font [list DemoFontBold unread] -lines 1
    $T element create elemTxtCount text -fill blue
    $T element create elemRectSel rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	-showfocus yes

    #
    # Create styles using the elements
    #

    # image + text + text
    set S [$T style create styFolder]
    $T style elements $S {elemRectSel elemImg elemTxtName elemTxtCount}
    $T style layout $S elemImg -expand ns
    $T style layout $S elemTxtName -padx 4 -expand ns -squeeze x
    $T style layout $S elemTxtCount -expand ns -visible {yes unread no {}}
    $T style layout $S elemRectSel -union [list elemTxtName] -iexpand ns -ipadx 2

    #
    # Create items and assign styles
    #

    $T item style set root C0 $S
    $T item element configure root C0 \
	elemImg -image outlook-main + \
	elemTxtName -text "Outlook Express"

    set parentList [list root {} {} {} {} {} {}]
    set parent root
    foreach {depth img text button unread} {
	0 local "Local Folders" yes 0
	    1 inbox Inbox no 5
	    1 outbox Outbox no 0
	    1 sent "Sent Items" no 0
	    1 deleted "Deleted Items" no 50
	    1 draft Drafts no 0
	    1 folder "Messages to Dad" no 0
	    1 folder "Messages to Sis" no 0
	    1 folder "Messages to Me" yes 5
		2 folder "2001" no 0
		2 folder "2000" no 0
		2 folder "1999" no 0
	0 server "news.gmane.org" yes 0
	    1 group "gmane.comp.lang.lua.general" no 498
    } {
	set item [$T item create -button $button]
	$T item style set $item C0 $S
	$T item element configure $item C0 \
	    elemImg -image outlook-$img + \
	    elemTxtName -text $text

	if {$unread} {
	    $T item element configure $item C0 \
		elemTxtCount -text "($unread)"
	    $T item state set $item unread
	}

	$T item lastchild [lindex $parentList $depth] $item
	incr depth
	set parentList [lreplace $parentList $depth $depth $item]
    }

    return
}

#
# Here is the original implementation which doesn't use custom states.
# It has 4 different item styles and 6 different elements.
#
proc DemoOutlookFolders.orig {} {

    InitPics outlook-*

    set T [DemoList]

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -itemheight $height -selectmode browse \
	-showroot yes -showrootbutton no -showbuttons yes \
	-showlines [ShouldShowLines $T]

    $T configure -canvaspadx {4 0} -canvaspady {2 0}

    #
    # Create columns
    #

    $T column create -text Folders -tags C0
    $T configure -treecolumn C0

    #
    # Create elements
    #

    $T element create elemImgAny image
    $T element create elemTxtRead text -fill [list $::SystemHighlightText {selected focus}] \
	-lines 1
    $T element create elemTxtUnread text -fill [list $::SystemHighlightText {selected focus}] \
	-font [list DemoFontBold] -lines 1
    $T element create elemTxtCount text -fill blue
    $T element create elemImgFolder image -image outlook-folder
    $T element create elemRectSel rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	-showfocus yes

    #
    # Create styles using the elements
    #

    # image + text
    set S [$T style create styAnyRead]
    $T style elements $S {elemRectSel elemImgAny elemTxtRead}
    $T style layout $S elemImgAny -expand ns
    $T style layout $S elemTxtRead -padx {4 0} -expand ns -squeeze x
    $T style layout $S elemRectSel -union [list elemTxtRead] -iexpand ns -ipadx 2

    # image + text + text
    set S [$T style create styAnyUnread]
    $T style elements $S {elemRectSel elemImgAny elemTxtUnread elemTxtCount}
    $T style layout $S elemImgAny -expand ns
    $T style layout $S elemTxtUnread -padx 4 -expand ns -squeeze x
    $T style layout $S elemTxtCount -expand ns
    $T style layout $S elemRectSel -union [list elemTxtUnread] -iexpand ns -ipadx 2

    # folder + text
    set S [$T style create styFolderRead]
    $T style elements $S {elemRectSel elemImgFolder elemTxtRead}
    $T style layout $S elemImgFolder -expand ns
    $T style layout $S elemTxtRead -padx {4 0} -expand ns -squeeze x
    $T style layout $S elemRectSel -union [list elemTxtRead] -iexpand ns -ipadx 2

    # folder + text + text
    set S [$T style create styFolderUnread]
    $T style elements $S {elemRectSel elemImgFolder elemTxtUnread elemTxtCount}
    $T style layout $S elemImgFolder -expand ns
    $T style layout $S elemTxtUnread -padx 4 -expand ns -squeeze x
    $T style layout $S elemTxtCount -expand ns
    $T style layout $S elemRectSel -union [list elemTxtUnread] -iexpand ns -ipadx 2

    #
    # Create items and assign styles
    #

    $T item style set root C0 styAnyRead
    $T item element configure root C0 \
	elemImgAny -image outlook-main + \
	elemTxtRead -text "Outlook Express"

    set parentList [list root {} {} {} {} {} {}]
    set parent root
    foreach {depth img text button unread} {
	0 local "Local Folders" yes 0
	    1 inbox Inbox no 5
	    1 outbox Outbox no 0
	    1 sent "Sent Items" no 0
	    1 deleted "Deleted Items" no 50
	    1 draft Drafts no 0
	    1 folder "Messages to Dad" no 0
	    1 folder "Messages to Sis" no 0
	    1 folder "Messages to Me" yes 5
		2 folder "2001" no 0
		2 folder "2000" no 0
		2 folder "1999" no 0
	0 server "news.gmane.org" yes 0
	    1 group "gmane.comp.lang.lua.general" no 498
    } {
	set item [$T item create -button $button]
	if {[string equal $img folder]} {
	    if {$unread} {
		$T item style set $item C0 styFolderUnread
		$T item element configure $item C0 \
		    elemTxtUnread -text $text + \
		    elemTxtCount -text "($unread)"
	    } else {
		$T item style set $item C0 styFolderRead
		$T item element configure $item C0 elemTxtRead -text $text
	    }
	} else {
	    if {$unread} {
		$T item style set $item C0 styAnyUnread
		$T item element configure $item C0 \
		    elemImgAny -image outlook-$img + \
		    elemTxtUnread -text $text + \
		    elemTxtCount -text "($unread)"
	    } else {
		$T item style set $item C0 styAnyRead
		$T item element configure $item C0 \
		    elemImgAny -image outlook-$img + \
		    elemTxtRead -text $text
	    }
	}
	$T item lastchild [lindex $parentList $depth] $item
	incr depth
	set parentList [lreplace $parentList $depth $depth $item]
    }

    return
}

