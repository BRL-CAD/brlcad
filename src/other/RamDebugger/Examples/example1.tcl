
toplevel .t
pack [frame .t.f]
pack [label .t.f.l -text rrrrr]
pack [label .t.f.l2 -text rrrrr]

pack [label .l -text rrrrr]


proc pp1 { string } {
    puts pp1
    for { set i 0 } { $i < [string length $string] } { incr i } {
	set bb [string index $string $i]
    }
}

proc pp2 { string } {
    puts pp2
    foreach i [split $string ""] {
	set bb $i
	set cc $bb--qq
    }
}

set string [string repeat "aa" 10]
pp1 $string
pp2 $string
