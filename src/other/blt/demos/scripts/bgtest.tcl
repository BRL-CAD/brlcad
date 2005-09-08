
set fid [open "../README" "r"]
set data [read $fid]
close $fid

regsub -all "\r|\n|\t" $data " " data 
set data [split $data " "]

set count 0
set maxWords 500
foreach word $data {
    if { $word == "" }  {
	continue
    }
    if { $count & 0x1 } {
	puts -nonewline stderr "($word)"
	flush stderr
    } else {
	puts -nonewline stdout "($word)"
	flush stdout
    }
    incr count
    if { ($count % 10) == 0 } {
	puts stdout ""
	puts stderr ""
	flush stdout
	flush stderr
    }
    if { $count > $maxWords } {
	break
    }
    after 500
}
exit 0

