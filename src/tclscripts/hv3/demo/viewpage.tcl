#!/usr/brlcad/bin/bwish
package require hv3

#source snit.tcl
#source hv3.tcl

proc mkTkImage {file} {
     set name [image create photo -file $file]
     return [list $name [list image delete $name]]
}

set fd [open index.html]
set data [read $fd]
close $fd

set myHv3 [::hv3::hv3 .hv3]
#$myHv3 Subscribe motion [list .hv3 motion]
bind .hv3 <3> [list .hv3 rightclick %x %y %X %Y]
bind .hv3 <2> [list .hv3 middleclick %x %y]
set html [$myHv3 html]
$html configure -parsemode html
$html configure -imagecmd mkTkImage
$html parse $data
pack .hv3 -side left -expand yes -fill y

#scrollbar .helpviewer_yscroll -command {.helpviewer yview} -orient vertical
#html .helpviewer -yscrollcommand {.helpviewer_yscroll set}
#grid .helpviewer .helpviewer_yscroll -sticky news
#grid rowconfigure . 0 -weight 1
#grid columnconfigure . 0 -weight 1
#.helpviewer configure -parsemode html 
#.helpviewer configure -imagecmd mkTkImage
#.helpviewer parse $data
