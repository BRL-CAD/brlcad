
proc CopyOptions { cmd orig new } {
    set all [eval $orig $cmd]
    set configLine $new
    foreach arg $cmd {
	lappend configLine $arg
    }
    foreach option $all {
	if { [llength $option] != 5 } {
	    continue
	}
	set switch [lindex $option 0]
	set initial [lindex $option 3]
	set current [lindex $option 4]
	if { [string compare $initial $current] == 0 } {
	    continue
	}
	lappend configLine $switch $current
    }
    eval $configLine
}

proc CopyBindings { oper orig new args } {
    set tags [$orig $oper bind]
    if { [llength $args] > 0 } {
	lappend tags [lindex $args 0]
    }
    foreach tag $tags {
	foreach binding [$orig $oper bind $tag] {
	    set cmd [$orig $oper bind $tag $binding]
	    $new $oper bind $tag $binding $cmd
	}
    }
}

proc CloneGraph { orig new } {
    graph $new
    CopyOptions "configure" $orig $new 
    # Axis component
    foreach axis [$orig axis names] {
	if { [$new axis name $axis] == "" } {
	    $new axis create $axis
	}
	CopyOptions [list axis configure $axis] $orig $new
    }
    foreach axis { x y x2 y2 } {
	$new ${axis}axis use [$orig ${axis}axis use]
    }
    # Pen component
    foreach pen [$orig pen names] {
	if { [$new pen name $pen] == "" } {
	    $new pen create $pen
	}
	CopyOptions [list pen configure $pen] $orig $new
    }
    # Marker component
    foreach marker [$orig marker names] {
	$new marker create [$orig marker type $marker] -name $marker
	CopyBindings marker $orig $new $marker
	CopyOptions [list marker configure $marker] $orig $new
    }
    # Element component
    foreach elem [$orig element names] {
	$new element create $elem
	CopyBindings element $orig $new $elem
	CopyOptions [list element configure $elem] $orig $new
    }
    # Fix element display list
    $new element show [$orig element show]
    # Legend component
    CopyOptions {legend configure} $orig $new
    CopyBindings legend $orig $new
    # Postscript component
    CopyOptions {postscript configure} $orig $new
    # Grid component
    CopyOptions {grid configure} $orig $new
    # Grid component
    CopyOptions {crosshairs configure} $orig $new
    # Graph bindings
    foreach binding [bind $orig] {
	set cmd [bind $orig $binding]
	bind $new $binding $cmd
    }
    return $new
}
    
toplevel .top
pack [CloneGraph $graph .top.graph]
