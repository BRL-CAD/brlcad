# RCS: @(#) $Id$

#
# Demo: MailWasher
#
proc DemoMailWasher {} {

    set T [DemoList]

    InitPics *checked

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showrootbutton no -showbuttons no \
	-showlines no -itemheight $height -selectmode browse \
	-xscrollincrement 20 -xscrollsmoothing yes

    #
    # Create columns
    #

    set pad 4
    $T column create -text Delete -textpadx $pad -justify center -tags delete
    $T column create -text Bounce -textpadx $pad -justify center -tags bounce
    $T column create -text Status -width 80 -textpadx $pad -tags status
    $T column create -text Size -width 40 -textpadx $pad -justify right -tags size
    $T column create -text From -width 140 -textpadx $pad -tags from
    $T column create -text Subject -width 240 -textpadx $pad -tags subject
    $T column create -text Received -textpadx $pad -arrow up -tags received
    $T column create -text Attachments -textpadx $pad -tags attachments

    $T state define CHECK

    #
    # Create elements
    #

    $T element create border rect -open nw -outline gray -outlinewidth 1 \
	-fill [list $::SystemHighlight {selected}]
    $T element create imgCheck image -image {checked CHECK unchecked {}}
    $T element create txtAny text \
	-fill [list $::SystemHighlightText {selected}] -lines 1
    $T element create txtNone text -text "none" \
	-fill [list $::SystemHighlightText {selected}] -lines 1
    $T element create txtYes text -text "yes" \
	-fill [list $::SystemHighlightText {selected}] -lines 1
    $T element create txtNormal text -text "Normal" \
	-fill [list $::SystemHighlightText {selected} #006800 {}] -lines 1
    $T element create txtPossSpam text -text "Possible Spam"  \
	-fill [list $::SystemHighlightText {selected} #787800 {}] -lines 1
    $T element create txtProbSpam text -text "Probably Spam" \
	-fill [list $::SystemHighlightText {selected} #FF9000 {}] -lines 1
    $T element create txtBlacklist text -text "Blacklisted" \
	-fill [list $::SystemHighlightText {selected} #FF5800 {}] -lines 1

    #
    # Create styles using the elements
    #

    set S [$T style create styCheck]
    $T style elements $S [list border imgCheck]
    $T style layout $S border -detach yes -iexpand xy
    $T style layout $S imgCheck -expand news

    set pad 4

    foreach name {Any None Yes Normal PossSpam ProbSpam Blacklist} {
	set S [$T style create sty$name]
	$T style elements $S [list border txt$name]
	$T style layout $S border -detach yes -iexpand xy
	$T style layout $S txt$name -padx $pad -squeeze x -expand ns
    }

    #
    # Create items and assign styles
    #

    for {set i 0} {$i < 1} {incr i} {
	foreach {from subject} {
	    baldy@spammer.com "Your hair is thinning"
	    flat@spammer.com "Your breasts are too small"
	    tiny@spammer.com "Your penis is too small"
	    dumbass@spammer.com "You are not very smart"
	    bankrobber@spammer.com "You need more money"
	    loser@spammer.com "You need better friends"
	    gossip@spammer.com "Find out what your coworkers think about you"
	    whoami@spammer.com "Find out what you think about yourself"
	    downsized@spammer.com "You need a better job"
	    poorhouse@spammer.com "Your mortgage is a joke"
	    spam4ever@spammer.com "You need more spam"
	} {
	    set item [$T item create]
	    set status [lindex [list styNormal styPossSpam styProbSpam styBlacklist] [expr int(rand() * 4)]]
	    set delete [expr int(rand() * 2)]
	    set bounce [expr int(rand() * 2)]
	    set attachments [lindex [list styNone styYes] [expr int(rand() * 2)]]
	    $T item style set $item delete styCheck bounce styCheck \
		status $status size styAny \
		from styAny subject styAny received styAny \
		attachments $attachments
	    if {$delete} {
		$T item state forcolumn $item delete CHECK
	    }
	    if {$bounce} {
		$T item state forcolumn $item bounce CHECK
	    }
	    set bytes [expr {512 + int(rand() * 1024 * 12)}]
	    set size [expr {$bytes / 1024 + 1}]KB
	    set seconds [expr {[clock seconds] - int(rand() * 100000)}]
	    set received [clock format $seconds -format "%d/%m/%y %I:%M %p"]
	    $T item text $item size $size from $from subject $subject received $received
	    $T item lastchild root $item
	}
    }
    if 0 {
	$T notify bind MailWasher <Button1-ElementPress-imgOn> {
	    %T item style set %I %C styOff
	}
	$T notify bind MailWasher <Button1-ElementPress-imgOff> {
	    %T item style set %I %C styOn
	}
    }

    set ::SortColumn received
    $T notify bind $T <Header-invoke> {
	if {[%T column compare %C == $SortColumn]} {
	    if {[%T column cget $SortColumn -arrow] eq "down"} {
		set order -increasing
		set arrow up
	    } else {
		set order -decreasing
		set arrow down
	    }
	} else {
	    if {[%T column cget $SortColumn -arrow] eq "down"} {
		set order -decreasing
		set arrow down
	    } else {
		set order -increasing
		set arrow up
	    }
	    %T column configure $SortColumn -arrow none
	    set SortColumn %C
	}
	%T column configure %C -arrow $arrow
	switch [%T column cget %C -tags] {
	    bounce -
	    delete {
		%T item sort root $order -column %C -command [list CompareOnOff %T %C] -column subject -dictionary
	    }
	    status {
		%T item sort root $order -column %C -dictionary
	    }
	    from {
		%T item sort root $order -column %C -dictionary -column subject -dictionary
	    }
	    subject {
		%T item sort root $order -column %C -dictionary
	    }
	    size {
		%T item sort root $order -column %C -dictionary -column subject -dictionary
	    }
	    received {
		%T item sort root $order -column %C -dictionary -column subject -dictionary
	    }
	    attachments {
		%T item sort root $order -column %C -dictionary -column subject -dictionary
	    }
	}
    }

    bind DemoMailWasher <ButtonPress-1> {
	set id [%W identify %x %y]
	if {$id eq ""} {
	} elseif {[lindex $id 0] eq "header"} {
	} else {
	    lassign $id what item where arg1 arg2 arg3
	    if {$where eq "column"} {
		if {[%W column tag expr $arg1 {delete || bounce}]} {
		    %W item state forcolumn $item $arg1 ~CHECK
#					return -code break
		}
	    }
	}
    }

    bindtags $T [list $T DemoMailWasher TreeCtrl [winfo toplevel $T] all]

    return
}

proc CompareOnOff {T C item1 item2} {
    set s1 [$T item state forcolumn $item1 $C]
    set s2 [$T item state forcolumn $item2 $C]
    if {$s1 eq $s2} { return 0 }
    if {[lsearch -exact $s1 CHECK] == -1} { return -1 }
    return 1
}

