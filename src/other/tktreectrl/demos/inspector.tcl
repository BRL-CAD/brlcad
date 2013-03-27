namespace eval Inspector {
}

proc Inspector::InitWindow {} {
    variable Priv

    InitPics *checked

    set w .inspector
    catch {destroy $w}
    toplevel $w
    wm title $w "TkTreeCtrl Inspector"

    #
    # Component
    #

    panedwindow $w.splitterH -orient horizontal
    TreePlusScrollbarsInAFrame $w.splitterH.f1 1 1
    $w.splitterH add $w.splitterH.f1

    set T $w.splitterH.f1.t
    $T configure -showroot no
    $T element create e1 text -fill [list $::SystemHighlightText {selected focus}]
    $T element create e2 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	-showfocus yes
    $T style create s1
    $T style elements s1 {e2 e1}
    # Tk listbox has linespace + 1 height
    $T style layout s1 e2 -union [list e1] -ipadx 2 -ipady {0 1} -iexpand nes
    $T column create -text "Component" -button no -itemstyle s1 -tags C0
    foreach label {Column Header Item Widget} {
	set I [$T item create -parent root]
	$T item text $I C0 $label
    }

    $T notify bind $T <Selection> {
	Inspector::SelectComponent
    }

    #
    # Instance
    #

    panedwindow $w.splitterV -orient vertical
    TreePlusScrollbarsInAFrame $w.splitterV.f1 1 1
set f $w.splitterV.f1
grid rowconfigure $f 0 -weight 0
grid rowconfigure $f 1 -weight 1
grid configure $f.sh -row 2
grid configure $f.t -row 1
grid configure $f.sv -row 1
grid [label $f.label -text "Headers" -bg {light blue}] -row 0 -column 0 -columnspan 2 -sticky we
    $w.splitterV add $w.splitterV.f1

    set T $w.splitterV.f1.t
    $T configure -showroot no
    $T element create e1 text -fill [list $::SystemHighlightText {selected focus}]
    $T element create e2 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	-showfocus yes
    $T style create s1
    $T style elements s1 {e2 e1}
    # Tk listbox has linespace + 1 height
    $T style layout s1 e2 -union [list e1] -ipadx 2 -ipady {0 1} -iexpand nes

    $T item state define CHECK
    $T element create imgCheck image -image {checked CHECK unchecked {}}
    set S [$T style create styCheck]
    $T style elements $S [list imgCheck]
    $T style layout $S imgCheck -expand news

    $w.splitterH add $w.splitterV

    #
    # Sub-Instance
    #

    TreePlusScrollbarsInAFrame $w.splitterV.f2 1 1
set f $w.splitterV.f2
grid rowconfigure $f 0 -weight 0
grid rowconfigure $f 1 -weight 1
grid configure $f.sh -row 2
grid configure $f.t -row 1
grid configure $f.sv -row 1
grid [label $f.label -text "Header columns" -bg {light blue}] -row 0 -column 0 -columnspan 2 -sticky we
    $w.splitterV add $w.splitterV.f2

    set T $w.splitterV.f2.t
    $T configure -showroot no
    $T element create e1 text -fill [list $::SystemHighlightText {selected focus}]
    $T element create e2 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	-showfocus yes
    $T style create s1
    $T style elements s1 {e2 e1}
    # Tk listbox has linespace + 1 height
    $T style layout s1 e2 -union [list e1] -ipadx 2 -ipady {0 1} -iexpand nes

    $T item state define CHECK
    $T element create imgCheck image -image {checked CHECK unchecked {}}
    set S [$T style create styCheck]
    $T style elements $S [list imgCheck]
    $T style layout $S imgCheck -expand news

    ###

    pack $w.splitterH -expand yes -fill both

    set Priv(win) $w
    set Priv(tree1) $w.splitterH.f1.t
    set Priv(tree2) $w.splitterV.f1.t
    set Priv(tree3) $w.splitterV.f2.t
    set Priv(label2) $w.splitterV.f1.label
    set Priv(label3) $w.splitterV.f2.label

    return
}

proc Inspector::InspectColumns {inspectMe} {
    variable Priv
    $Priv(label2) configure -text Columns
    set T $Priv(tree2)
    $T item delete all
    $T column delete all
    foreach title {ID -lock -width -expand -resize -squeeze -visible} {
	$T column create -text $title -tags C$title -itembackground {gray90 ""}
    }
    foreach C [$inspectMe column list] {
	set I [$T item create -parent root]
	$T item style set $I CID s1
	$T item text $I CID $C
	foreach title {-lock -width} {
	    $T item style set $I C$title s1
	    $T item text $I C$title [$inspectMe column cget $C $title]
	}
	foreach title {-expand -resize -squeeze -visible} {
	    $T item style set $I C$title styCheck
	    if {[$inspectMe column cget $C $title]} {
		$T item state forcolumn $I C$title CHECK
	    }
	}
    }

    $T notify bind $T <Selection> {}

    set T $Priv(tree3)
    $T item delete all
    $T column delete all
    $Priv(label3) configure -text <nothing>

    return
}

proc Inspector::InspectHeaders {inspectMe} {
    variable Priv
    $Priv(label2) configure -text Headers
    set T $Priv(tree2)
    $T item delete all
    $T column delete all
    foreach title {ID -height -tags -visible} {
	$T column create -text $title -tags C$title -itembackground {gray90 ""}
    }
    foreach H [$inspectMe header id all] {
	set I [$T item create -parent root]
	$T item style set $I CID s1
	$T item text $I CID $H
	foreach title {-height -tags} {
	    $T item style set $I C$title s1
	    $T item text $I C$title [$inspectMe header cget $H $title]
	}
	foreach title {-visible} {
	    $T item style set $I C$title styCheck
	    if {[$inspectMe header cget $H $title]} {
		$T item state forcolumn $I C$title CHECK
	    }
	}
	set Priv(item2header,$I) $H
    }

    $T notify bind $T <Selection> {
	if {%c > 0} {
	    Inspector::SelectHeader
	}
    }

    set T $Priv(tree3)
    $T item delete all
    $T column delete all
    $Priv(label3) configure -text <nothing>

    return
}

proc Inspector::InspectHeaderColumns {inspectMe H} {
    variable Priv
    $Priv(label3) configure -text "Columns in Header #$H"
    set T $Priv(tree3)
    $T item delete all
    $T column delete all
    foreach title {ID -text -image -arrow -arrowside -arrowgravity -button -justify span} {
	$T column create -text $title -tags C$title -itembackground {gray90 ""}
    }
    foreach C [$inspectMe column list] {
	set I [$T item create -parent root]
	$T item style set $I CID s1
	$T item text $I CID $C
	foreach title {-text -image -arrow -arrowside -arrowgravity -justify} {
	    $T item style set $I C$title s1
	    $T item text $I C$title [$inspectMe header cget $H $C $title]
	}
	foreach title {-button} {
	    $T item style set $I C$title styCheck
	    if {[$inspectMe header cget $H $C $title]} {
		$T item state forcolumn $I C$title CHECK
	    }
	}

	$T item style set $I Cspan s1
	$T item text $I Cspan [$inspectMe header span $H $C]
    }
    return
}

proc Inspector::InspectItems {inspectMe} {
    variable Priv
    $Priv(label2) configure -text "Items in the demo list"
    set T $Priv(tree2)
    $T item delete all
    $T column delete all
    foreach title {ID -button -height -tags -visible -wrap} {
	$T column create -text $title -tags C$title -itembackground {gray90 ""}
    }
    foreach iI [$inspectMe item range first last] {
	set I [$T item create -parent root]
	$T item style set $I CID s1
	$T item text $I CID $iI
	foreach title {-button -height -tags} {
	    $T item style set $I C$title s1
	    $T item text $I C$title [$inspectMe item cget $iI $title]
	}
	foreach title {-visible -wrap} {
	    $T item style set $I C$title styCheck
	    if {[$inspectMe item cget $iI $title]} {
		$T item state forcolumn $I C$title CHECK
	    }
	}
    }

    $T notify bind $T <Selection> {}

    set T $Priv(tree3)
    $T item delete all
    $T column delete all
    $Priv(label3) configure -text <nothing>

    return
}

proc Inspector::SelectComponent {} {
    variable Priv
    set T $Priv(tree1)
    set I [$T selection get 0]
    if {[$T item text $I C0] eq "Column"} {
	InspectColumns [DemoList]
    }
    if {[$T item text $I C0] eq "Header"} {
	InspectHeaders [DemoList]
    }
    if {[$T item text $I C0] eq "Item"} {
	InspectItems [DemoList]
    }
    return
}

proc Inspector::SelectHeader {} {
    variable Priv
    set T $Priv(tree2)
    set I [$T selection get 0]
    InspectHeaderColumns [DemoList] $Priv(item2header,$I)
    return
}

Inspector::InitWindow
Inspector::InspectHeaders [DemoList]
Inspector::InspectHeaderColumns [DemoList] 0


