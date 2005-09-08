#
# Script for installation of BLT under Windows
#

namespace eval Installer {
    variable commandList {}
    variable cmdLog {}
    variable component
    array set component {
	binaries	1
	headers		1
	html		1
	scripts		1
	demos		1
    }
    variable totalBytes 0
    variable totalFiles 0
    variable panel 0
    variable panelList {
	Welcome Directory Components Ready Finish
    }
}

proc Installer::DoInstall { package version } {
    global prefix srcdir

    regsub {\.} $version {} v2
    variable scriptdir 
    set scriptdir $prefix/lib/blt${version}
    variable component
    global tcl_platform
    if { $component(binaries) } {
	if { $tcl_platform(platform) == "unix" } {
	    Add ${srcdir}/src -perm 0755 \
		-file bltwish \
		-file bltsh \
		-rename "bltwish bltwish${v2}" \
		-rename "bltsh bltsh${v2}" \
		$prefix/bin
	    set ext [info sharedlibextension]
	    Add ${srcdir}/src -perm 0755 \
		-file libBLT${v2}.a \
		-file libBLTlite${v2}.a \
		-file shared/libBLT${v2}${ext} \
		-file shared/libBLTlite${v2}${ext} \
		$prefix/lib
	} else {
	    Add ${srcdir}/src -perm 0755 \
		-file bltwish.exe \
		-file bltsh.exe \
		-file BLT${v2}.dll \
		-file BLTlite${v2}.dll \
		-rename "bltwish.exe bltwish${v2}.exe" \
		-rename "bltsh.exe bltsh${v2}.exe" \
		$prefix/bin
	    Add ${srcdir}/src -perm 0755 \
		-file BLT${v2}.lib \
		-file BLTlite${v2}.lib \
		$prefix/lib
	}
    }
    if { $component(headers) } {
	Add ${srcdir}/src \
	    -file blt.h \
	    -file bltChain.h \
	    -file bltVector.h \
	    -file bltTree.h \
	    $prefix/include
    }
    if { $component(html) } {
	Add ${srcdir}/html -pattern *.html $scriptdir/html
    }
    if { $component(scripts) } {
	Add ${srcdir} -file README -file PROBLEMS $scriptdir
	Add ${srcdir}/library \
	    -pattern *.cur \
	    -pattern *.tcl \
	    -pattern *.pro \
	    -file tclIndex \
	    $scriptdir 
	Add ${srcdir}/library/dd_protocols \
	    -pattern *.tcl \
	    -file tclIndex \
	    $scriptdir/dd_protocols
    }
    if { $component(demos) } {
	Add ${srcdir}/demos \
	    -pattern *.tcl \
	    -file htext.txt \
	    $scriptdir/demos
	Add ${srcdir}/demos/bitmaps -pattern *.xbm $scriptdir/bitmaps
	Add ${srcdir}/demos/bitmaps/hand -pattern *.xbm $scriptdir/bitmaps/hand
	Add ${srcdir}/demos/bitmaps/fish -pattern *.xbm $scriptdir/bitmaps/fish
	Add ${srcdir}/demos/images \
	    -pattern *.gif \
	    -file out.ps \
	    $scriptdir/images
	Add ${srcdir}/demos/scripts -pattern *.tcl $scriptdir/scripts
    }
    Install $package $version
}

proc Installer::InstallDirectory { dest } {
    variable commandList
    lappend commandList [list CheckPath $dest]
}

proc Installer::Update { src dest size perm } {
    variable currentBytes
    variable totalBytes

    .install.text insert end "file copy -force $src $dest\n"
    if { [catch {file copy -force $src $dest} results] != 0 } {
	.install.text insert end "Error: $results\n" fail
    } else {
	incr currentBytes $size
    }
    global tcl_platform
    if { $tcl_platform(platform) == "unix" } {
	.install.text insert end "file attributes $dest -permissions $perm\n"
	if { [catch {file attributes $dest -permissions $perm} results] != 0 } {
	    .install.text insert end "Error: $results\n" fail
	}
    }
    set percent [expr round(double($currentBytes)/$totalBytes * 100.0)]
    .install.current configure -text "$percent% complete"
    update
}

proc Installer::InstallFile { src dest perm } {
    variable commandList
    variable totalBytes
    variable totalFiles

    if { [catch { file size $src } size ] != 0 } {
	set size 0
    }
    lappend commandList [list Update $src $dest $size $perm]
    incr totalBytes $size
    incr totalFiles
}

proc Installer::Add { dir args } {
    variable commandList 
    variable totalBytes

    if { ![file exists $dir] }  {
	error "can't find directory \"$dir\"" 
    }
    set argc [llength $args]
    set destDir [lindex $args end]
    incr argc -2
    set perm 0644

    InstallDirectory $destDir
    foreach { option value } [lrange $args 0 $argc] {
	switch -- $option {
	    "-pattern" {  
		foreach f [lsort [glob $dir/$value]] { 
		    InstallFile $f $destDir/[file tail $f] $perm
		}
	    }
	    "-rename" {
		set src [lindex $value 0]
		set dest [lindex $value 1]
		InstallFile $dir/$src $destDir/$dest $perm
	    }
    	    "-perm" {  
		set perm $value
	    }
    	    "-file" {  
		InstallFile $dir/$value $destDir/$value $perm
	    }
	    default {
		error "Unknown option \"$option\""
	    }
	}
    }
}

proc Installer::CheckPath { dest } {
    set save [pwd]
    if { [file pathtype $dest] == "absolute" } {
	if { [string match  {[a-zA-Z]:*} $dest] } {
	    cd [string range $dest 0 2]
	    set dest [string range $dest 3 end]
	} else {
	    cd /
	    set dest [string range $dest 1 end]
	}
    }	
    set dirs [file split $dest]
    foreach d $dirs {
	if { ![file exists $d] } {
	    .install.text insert end "file mkdir $d\n"
	    if { [catch { file mkdir $d } result] != 0 } {
		.install.text insert end "Error: $result\n" fail
		break
	    }
	}
	if { ![file isdirectory $d] } {
	    .install.text insert end "Error: Not a directory: \"$d\"" fail
	    break
	}
	update
	cd $d 
    }
    cd $save
}

proc Installer::MakePackageIndex { package version file } {
    global prefix
    set suffix [info sharedlibextension]
    regsub {\.} $version {} version_no_dots
    set libName "${package}${version_no_dots}${suffix}"
    set libPath [file join ${prefix}/bin $libName]
    set cmd [list load $libPath $package]

    if { [file exists $file] } {
	file delete $file
    }
    set cmd {
	set fid [open $file "w"] 
	puts $fid "# Package Index for $package"
	puts $fid "# generated on [clock format [clock seconds]]"
	puts $fid ""
	puts $fid [list package ifneeded $package $version $cmd]
	close $fid
    }
    if { [catch $cmd result] != 0 } {
	.install.text insert end "Error: $result\n" fail
    }
}

proc Installer::SetRegistryKey { package version valueName } {
    variable scriptdir 
    global tcl_version
    
    package require registry
    set key HKEY_LOCAL_MACHINE\\Software\\$package\\$version\\$tcl_version 
    registry set $key $valueName $scriptdir
}

proc Installer::Install { package version } {
    variable commandList 
    variable totalBytes 
    variable currentBytes 0
    variable totalFiles
    variable scriptdir 

    .install.totals configure -text "Files: $totalFiles Size: $totalBytes"
    foreach cmd $commandList {
	if { ![winfo exists .install] } {
	    return 
	}
	if { [catch $cmd result] != 0 } {
	    .install.text insert end "Error: $result\n" fail
	} 
	update
    }
    global tcl_version tcl_platform prefix
    set name [string tolower $package]
    MakePackageIndex $package $version $scriptdir/pkgIndex.tcl
    MakePackageIndex $package $version \
	$prefix/lib/tcl${tcl_version}/${name}${version}/pkgIndex.tcl
    if { $tcl_platform(platform) == "windows" } {
	SetRegistryKey $package $version ${package}_LIBRARY
    }
    .install.cancel configure -text "Done"
}

proc Installer::Next {} {
    variable panel
    variable continue
    variable panelList

    incr panel
    set max [llength $panelList]
    if { $panel >= $max } {
	exit 0
    }
    if { ($panel + 1) == $max } {
	.next configure -text "Finish"
	.cancel configure -state disabled
    } else {
	.next configure -text "Next"
    }
    if { $panel > 0 } {
	.back configure -state normal
    }
    set continue 1
}

proc Installer::Back {} {
    variable panel
    variable continue
    
    incr panel -1 
    if { $panel <= 0 } {
	.back configure -state disabled
	set panel 0
    } else {
	.back configure -state normal
    }
    .next configure -text "Next"
    .cancel configure -state normal
    set continue 0
}

proc Installer::Cancel {} {
    exit 0
}

if { $tcl_platform(platform) == "unix" } {
    font create textFont -family Helvetica -weight normal -size 11
    font create titleFont -family Helvetica -weight normal -size 14
} else {
    font create titleFont -family Arial -weight bold -size 12
    font create textFont -family Arial -weight normal -size 9
}
font create hugeFont -family {Times New Roman} -size 18 -slant italic \
    -weight bold

proc Installer::MakeLink { widget tag command } {
    $widget tag configure $tag -foreground blue -underline yes 
    $widget tag bind $tag <ButtonRelease-1> \
	"$command; $widget tag configure $tag -foreground blue"
    $widget tag bind $tag <ButtonPress-1> \
	[list $widget tag configure $tag -foreground red]
}

proc Installer::Welcome { package version }  {
    global tcl_version 
    if { [winfo exists .panel] } {
	destroy .panel
    }
    text .panel -wrap word -width 10 -height 18 \
	-relief flat -padx 4 -pady 4 -cursor arrow \
	-background [. cget -bg]
    .panel tag configure welcome -font titleFont -justify center \
	-foreground navyblue
    .panel tag configure package -font hugeFont -foreground red \
	-justify center
     MakeLink .panel next "Installer::Next"
     MakeLink .panel cancel "Installer::Cancel"
    .panel insert end "Welcome!\n" welcome
    .panel insert end "\n"
    .panel insert end \
	"This installs the compiled \n" "" \
	"${package} ${version}\n" package \
	"binaries and components from the source directories to " "" \
	"where Tcl/Tk is currently installed.\n\nThe compiled binaries " "" \
	"require Tcl/Tk $tcl_version.\n\n" 
    .panel insert end \
	"Press the " "" \
	"Next" next \
	" button to continue.  Press the " "" \
	"Cancel" cancel \
	" button if you do not wish to install $package at this time."
    .panel configure -state disabled
    blt::table . \
	0,1 .panel -columnspan 3 -pady 10 -padx { 0 10 } -fill x -anchor n
    tkwait variable Installer::continue
}

option add *Hierbox.openCommand	{Installer::OpenDir %W "%P" %n}

proc Installer::OpenDir { widget path atnode } {
    puts path=$path
    set save [pwd]
    global tcl_platform
	
    if { $tcl_platform(platform) == "windows" } {
	if { $path == "/" } {
	    foreach v [file volumes] {
		if { ![string match {[A-B]:/} $v] } {
		    .browser.h insert end $v -button yes
		}
	    }
	    return
	}
	set path [string trimleft $path /]
    } 
    cd $path/
    foreach dir [lsort [glob -nocomplain */ ]] { 
	set node [$widget insert -at $atnode end $dir]
	# Does the directory have subdirectories?
	set subdirs [glob -nocomplain $dir/*/ ]
	if { $subdirs != "" } {
	    $widget entry configure $node -button yes
	} else {
	    $widget entry configure $node -button no
	}	
    }
    cd $save
}

proc Installer::CenterPanel { panel }  {
    update idletasks
    set x [expr ([winfo width .] - [winfo reqwidth $panel]) / 2]
    set y [expr ([winfo height .] - [winfo reqheight $panel]) / 2]
    incr x [winfo rootx .]
    incr y [winfo rooty .]
    wm geometry $panel +$x+$y
    wm deiconify $panel
}

proc Installer::Browse { }  {
    if { [winfo exists .browser] } {
	raise .browser
	wm deiconify .browser
	return
    }
    toplevel .browser
    entry .browser.entry -bg white -borderwidth 2 -relief sunken \
	-textvariable Installer::selection
    bind .browser.entry <KeyPress-Return> {
	.browser.ok flash
	.browser.ok invoke
    }
    blt::hierbox .browser.h -hideroot yes -separator / -height 1i -width 3i \
	-borderwidth 2 -relief sunken -autocreate yes -trim / \
	-yscrollcommand { .browser.sbar set } \
	-selectcommand { 
	    set index [.browser.h curselection]
	    set path [lindex [.browser.h get -full $index] 0]
	    if { $tcl_platform(platform) == "windows" } {
		set path [string trimleft $path /]
	    } 
	    puts $path
	    set Installer::selection $path
	}
    scrollbar .browser.sbar -command { .browser.h yview }
    wm protocol .browser WM_DELETE_WINDOW { .browser.cancel invoke }
    wm transient .browser .
    frame .browser.sep -height 2 -borderwidth 1 -relief sunken
    button .browser.ok -text "Continue" -command {
	set prefix $Installer::selection
	grab release .browser
	destroy .browser
    }
    button .browser.cancel -text "Cancel" -command {
	grab release .browser
	destroy .browser
    }
    global tcl_platform
    global prefix
    if { $tcl_platform(platform) == "windows" } {
	set root "C:/"
	.browser.h open "C:/"
	.browser.h open "C:/Program Files"
	if { [file exists $prefix] } {
	   .browser.h see $prefix/
	}
    } else {
	set root /usr/local
	#.browser.h open $root
    }
    set Installer::selection $prefix
    wm title .browser "Installer: Select Install Directory"
    blt::table .browser \
	0,0 .browser.entry -fill x -columnspan 2  -padx 4 -pady 4 \
	1,0 .browser.h -fill both -columnspan 2 -padx 4 -pady { 0 4 } \
	1,2 .browser.sbar -fill y \
	2,0 .browser.sep -fill x -padx 10 -cspan 3 -pady { 4 0 } \
	3,0 .browser.ok  -width .75i -padx 4 -pady 4 \
	3,1 .browser.cancel -width .75i -padx 4 -pady 4
    blt::table configure .browser c2 r0 r2 r3 -resize none
    wm withdraw .browser
    after idle { 
	Installer::CenterPanel .browser 
	grab set .browser
    } 
}

proc Installer::Directory { package version }  {
    global tcl_version 
    if { [winfo exists .panel] } {
	destroy .panel
    }
    frame .panel
    text .panel.text -wrap word -width 10 \
	-height 10 -borderwidth 0 -padx 4 -pady 4 -cursor arrow \
	-background [.panel cget -background]
    .panel.text tag configure title -font titleFont -justify center \
	-foreground navyblue
    .panel.text insert end "Select Destination Directory\n" title
    .panel.text insert end "\n\n"
    .panel.text insert end "Please select the directory where Tcl/Tk \
$tcl_version is installed. This is also where $package $version will be \
installed.\n" 
    .panel.text configure -state disabled
    frame .panel.frame -relief groove -borderwidth 2
    label .panel.frame.label -textvariable ::prefix
    button .panel.frame.button -text "Browse..." -command Installer::Browse
    blt::table .panel.frame \
	0,0 .panel.frame.label -padx 4 -pady 4 -anchor w \
	0,1 .panel.frame.button -padx 4 -pady 4 -anchor e 
    blt::table .panel \
	0,0 .panel.text -padx 4 -pady 4 -fill both \
	1,0 .panel.frame -padx 4 -fill x
    blt::table . \
	0,1 .panel -columnspan 3 -pady 10 -padx { 0 10 } -fill both
    tkwait variable Installer::continue
}

proc Installer::Components { package version }  {
    global tcl_version 
    if { [winfo exists .panel] } {
	destroy .panel
    }
    regsub {\.} $version {} v2
    frame .panel
    text .panel.text -wrap word -width 10 \
	-height 8 -borderwidth 0 -padx 4 -pady 4 -cursor arrow \
	-background [.panel cget -background]
    .panel.text tag configure title -font titleFont -justify center \
	-foreground navyblue
    .panel.text insert end "Select Components\n" title
    .panel.text insert end "\n\n"
    .panel.text insert end "Please select the components you wish to install. \
You should install all components.\n" 
    .panel.text configure -state disabled
    frame .panel.frame -relief groove -borderwidth 2
    variable component
    global tcl_platform
    if { $tcl_platform(platform) == "unix" } {
	set ext [info sharedlibextension]
	set sharedlib lib${package}${v2}${ext}
	set lib lib${package}${v2}.a
	set exe ""
    } else {
	set sharedlib ${package}${v2}.dll
	set lib ${package}${v2}.lib
	set exe ".exe"
    }
    checkbutton .panel.frame.binaries \
	-text "bltwish${exe} and Shared Library" \
	-variable Installer::component(binaries)
    checkbutton .panel.frame.scripts \
	-text "Script Library" \
	-variable Installer::component(scripts)
    checkbutton .panel.frame.headers \
	-text "Include Files and Static Library" \
	-variable Installer::component(headers)
    checkbutton .panel.frame.html -text "HTML Manual Pages" \
	-variable Installer::component(html)
    checkbutton .panel.frame.demos -text "Demos" \
	-variable Installer::component(demos)
    blt::table .panel.frame \
	0,0 .panel.frame.binaries -padx 4 -pady 4 -anchor w \
	1,0 .panel.frame.scripts -padx 4 -pady 4 -anchor w \
	2,0 .panel.frame.headers -padx 4 -pady 4 -anchor w \
	3,0 .panel.frame.html -padx 4 -pady 4 -anchor w \
	4,0 .panel.frame.demos -padx 4 -pady 4 -anchor w 
    blt::table .panel \
	0,0 .panel.text -padx 4 -pady 4 -fill both \
	1,0 .panel.frame -padx 4 -fill both
    blt::table . \
	0,1 .panel -columnspan 3 -pady 10 -padx { 0 10 } -fill both
    tkwait variable Installer::continue
}

proc Installer::Ready { package version }  {
    global tcl_version 
    if { [winfo exists .panel] } {
	destroy .panel
    }
    text .panel -wrap word -width 10 -height 18 \
	-relief flat -padx 4 -pady 4 -cursor arrow \
	-background [. cget -bg]
    .panel tag configure welcome -font titleFont -justify center \
	-foreground navyblue
    .panel tag configure package -font hugeFont -foreground red \
	-justify center
     MakeLink .panel next "Installer::Next"
     MakeLink .panel cancel "Installer::Cancel"
     MakeLink .panel back "Installer::Back"
    .panel insert end "Ready To Install!\n" welcome
    .panel insert end "\n"
    .panel insert end "We're now ready to install ${package} ${version} \
and its components.\n\n"
    .panel insert end \
	"Press the " "" \
	"Next" next \
	" button to install all selected components.\n\n" "" 
    .panel insert end \
	"To reselect components, click on the " "" \
	"Back" back \
	" button.\n\n"
    .panel insert end \
	"Press the " "" \
	"Cancel" cancel \
	" button if you do not wish to install $package at this time."
    .panel configure -state disabled
    blt::table . \
	0,1 .panel -columnspan 3 -pady 10 -padx { 0 10 } -fill x -anchor n
    tkwait variable Installer::continue
    if { $Installer::continue } {
	Results
	update
	DoInstall $package $version
    }
}

proc Installer::Results { }  {
    if { [winfo exists .install] } {
	destroy .install
    }
    toplevel .install
    text .install.text -height 10 -width 50 -wrap none -bg white \
	-yscrollcommand { .install.ybar set } \
	-xscrollcommand { .install.xbar set } 
    .install.text tag configure fail -foreground red
    label .install.totals -text "Files: 0  Bytes: 0" -width 50
    label .install.current -text "Installing:\n" -height 2 -width 50
    scrollbar .install.ybar -command { .install.text yview }
    scrollbar .install.xbar -command { .install.text xview } -orient horizontal
    wm protocol .install WM_DELETE_WINDOW { .install.cancel invoke }
    wm transient .install .
    button .install.cancel -text "Cancel" -command {
	grab release .install
	destroy .install
    }
    blt::table .install \
	0,0 .install.totals -anchor w -columnspan 2  \
	1,0 .install.current -anchor w -cspan 2 \
	2,0 .install.text -fill both \
	2,1 .install.ybar -fill y \
	3,0 .install.xbar -fill x \
	4,0 .install.cancel -width .75i -padx 4 -pady 4 -cspan 2 
    blt::table configure .install c1 r0 r1 r3  -resize none
    wm withdraw .install
    after idle { 
	Installer::CenterPanel .install 
	grab set .install
    } 
}

proc Installer::Finish { package version }  {
    global tcl_version 
    if { [winfo exists .panel] } {
	destroy .panel
    }
    text .panel -wrap word -width 10 -height 18 \
	-relief flat -padx 4 -pady 4 -cursor arrow \
	-background [. cget -bg]
    .panel tag configure welcome -font titleFont -justify center \
	-foreground navyblue
    .panel tag configure package -font hugeFont -foreground red \
	-justify center
    .panel insert end "Installation Completed\n" welcome
    .panel insert end "\n"
    .panel insert end "${package} ${version} is now installed.\n\n"
     MakeLink .panel finish "Installer::Next"
    .panel insert end \
	"Press the " "" \
	"Finish" finish \
	" button to exit this installation"
    .panel configure -state disabled
    blt::table . \
	0,1 .panel -columnspan 3 -pady 10 -padx { 0 10 } -fill x -anchor n
    tkwait variable Installer::continue
}


set prefix [lindex $argv 2]
set srcdir [lindex $argv 1]
set version [lindex $argv 0]

set version 2.4
set package BLT
regsub {\.} $version {} v2
set ext [info sharedlibextension]

if { $tcl_platform(platform) == "unix" } {
    set ext [info sharedlibextension]
    set sharedlib shared/lib${package}${v2}${ext}
    set prefix "/usr/local/blt"
} else {
    set sharedlib ${package}${v2}.dll
    set prefix "C:/Program Files/Tcl"
}
if { [file exists ./src/$sharedlib] } {
    load ./src/$sharedlib $package
} else {
    error "Can't find library \"$sharedlib\" to load"
}
set blt_library $srcdir/library

image create photo openFolder -format gif -data {
R0lGODlhEAANAPIAAAAAAH9/f7+/v///////AAAAAAAAAAAAACH+JEZpbGUgd3JpdHRlbiBi
eSBBZG9iZSBQaG90b3Nob3CoIDUuMAAsAAAAABAADQAAAzk4Gsz6cIQ44xqCZCGbk4MmclAA
gNs4ml7rEaxVAkKc3gTAnBO+sbyQT6M7gVQpk9HlAhgHzqhUmgAAOw==
}
image create photo closeFolder -format gif -data {
R0lGODlhEAANAPIAAAAAAH9/f7+/v///AP///wAAAAAAAAAAACH+JEZpbGUgd3JpdHRlbiBi
eSBBZG9iZSBQaG90b3Nob3CoIDUuMAAsAAAAABAADQAAAzNIGsz6kAQxqAjxzcpvc1KWBUDY
nRQZWmilYi37EmztlrAt43R8mzrO60P8lAiApHK5TAAAOw==
}
image create photo blt -file ${srcdir}/demos/images/blt98.gif
option add *Text.font textFont
option add *HighlightThickness 0
option add *Hierbox.icons "closeFolder openFolder"
option add *Hierbox.button yes

set color \#accaff
option add *Frame.background $color
option add *Toplevel.background $color
#option add *Button.background $color
option add *Checkbutton.background $color
option add *Label.background $color
option add *Text.background $color
. configure -bg $color
wm title . "$package $version Installer"
label .image -image blt -borderwidth 2 -relief groove
button .back -text "Back" -state disabled -command Installer::Back -underline 0
button .next -text "Next" -command Installer::Next -underline 0
button .cancel -text "Cancel" -command Installer::Cancel -underline 0
frame .sep -height 2 -borderwidth 1 -relief sunken
blt::table . \
    0,0 .image -fill both -padx 10 -pady 10 \
    1,0 .sep -fill x -padx 4 -pady 4 -columnspan 4 -padx 5 \
    2,1 .back -anchor e -padx 4 -pady 4 \
    2,2 .next -anchor w -padx 4 -pady 4 \
    2,3 .cancel -padx 20 -pady 4

blt::table configure . .back .next .cancel -width .75i
blt::table configure . r1 r2 -resize none
blt::table configure . r3 -height 0.125i

while { 1 } {
    namespace eval Installer {
	variable panel
	set cmd [lindex $panelList $panel]
	eval [list $cmd $package $version]
    }	
    update
} 
