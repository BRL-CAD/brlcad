# Simple HTML display library by Stephen Uhler (stephen.uhler@sun.com)
# Copyright (c) 1995 by Sun Microsystems
# Version 0.1 Thu Jul 20 09:06:28 PDT 1995
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# To use this package,  create a text widget (say, .text)
# and set a variable full of html, (say $html), and issue:
#	HMinit_win .text
#	HMparse_html $html "HMrender .text"
# You also need to supply the routine:
#   proc HMlink_callback {win href} { ...}
#      win:  The name of the text widget
#      href  The name of the link
# which will be called anytime the user "clicks" on a link.
# The supplied version just prints the link to stdout.
# In addition, if you wish to use embedded images, you will need to write
#   proc HMset_image {handle src}
#      handle  an arbitrary handle (not really)
#      src     The name of the image
# Which calls
#	HMgot_image $handle $image
# with the TK image.
#
# To return a "used" text widget to its initialized state, call:
#   HMreset_win .text
# See "sample.tcl" for sample usage
##################################################################
############################################
# mapping of html tags to text tag properties
# properties beginning with "T" map directly to text tags

# These are Defined in HTML 2.0

array set HMtag_map {
	a	   {Tlink link}
	b      {weight bold}
	blockquote	{style i indent 1 Trindent rindent}
	bq		{style i indent 1 Trindent rindent}
	cite   {style i}
	code   {family courier}
	dfn    {style i}	
	dir    {indent 1}
	dl     {indent 1}
	em     {style i}
	h1     {size 24 weight bold}
	h2     {size 22}		
	h3     {size 20}	
	h4     {size 18}
	h5     {size 16}
	h6     {style i}
	i      {style i}
	kbd    {family courier weight bold}
	menu     {indent 1}
	ol     {indent 1}
	pre    {fill 0 family courier Tnowrap nowrap}
	samp   {family courier}		
	strong {weight bold}		
	tt     {family courier}
	u	 {Tunderline underline}
	ul     {indent 1}
	var    {style i}	
}

# These are in common(?) use, but not defined in html2.0

array set HMtag_map {
	center {Tcenter center}
	strike {Tstrike strike}
	u	   {Tunderline underline}
}

# initial values

set HMtag_map(hmstart) {
	family times   weight medium   style r   size 14
	Tcenter ""   Tlink ""   Tnowrap ""   Tunderline ""   list list
	fill 1   indent "" counter 0 adjust 0
}

# html tags that insert white space

array set HMinsert_map {
	blockquote "\n\n" /blockquote "\n"
	br	"\n"
	dd	"\n" /dd	"\n"
	dl	"\n" /dl	"\n"
	dt	"\n"
	form "\n"	/form "\n"
	h1	"\n\n"	/h1	"\n"
	h2	"\n\n"	/h2	"\n"
	h3	"\n\n"	/h3	"\n"
	h4	"\n"	/h4	"\n"
	h5	"\n"	/h5	"\n"
	h6	"\n"	/h6	"\n"
	li   "\n"
	/dir "\n"
	/ul "\n"
	/ol "\n"
	/menu "\n"
	p	"\n\n"
	pre "\n"	/pre "\n"
}

# tags that are list elements, that support "compact" rendering

array set HMlist_elements {
	ol 1   ul 1   menu 1   dl 1   dir 1
}
############################################
# initialize the window and stack state

proc HMinit_win {win} {
	upvar #0 HM$win var
	
	HMinit_state $win
	$win tag configure underline -underline 1
	$win tag configure center -justify center
	$win tag configure nowrap -wrap none
	$win tag configure rindent -rmargin $var(S_tab)c
	$win tag configure strike -overstrike 1
	$win tag configure mark -foreground red		;# list markers
	$win tag configure list -spacing1 3p -spacing3 3p		;# regular lists
	$win tag configure compact -spacing1 0p		;# compact lists
	$win tag configure link -foreground blue	;# hypertext links
	HMset_indent $win $var(S_tab)
	$win configure -wrap word

	# for horizontal rules
	$win tag configure thin -font [HMx_font Times 14 Medium R]
	$win tag configure hr -relief sunken -borderwidth 2 -wrap none \
		-tabs [winfo width $win]
	bind $win <Configure> {%W tag configure hr -tabs %w}

	# generic link enter callback

	$win tag bind link <1> "HMlink_hit $win %x %y"
}

# set the indent spacing (in cm) for lists
# TK uses a "weird" tabbing model that causes \t to insert a single
# space if the current line position is past the tab setting

proc HMset_indent {win cm} {
	set tabs [expr $cm / 2.0]
	$win configure -tabs ${tabs}c
	foreach i {1 2 3 4 5 6 7 8 9} {
		set tab [expr $i * $cm]
		$win tag configure indent$i -lmargin1 ${tab}c -lmargin2 ${tab}c \
			-tabs "[expr $tab + $tabs]c [expr $tab + 2*$tabs]c"
	}
}

# reset the state of window - get ready for the next page
# remove all but the font tags

proc HMreset_win {win} {
	regsub -all { +[^L ][^ ]*} " [$win tag names] " {} tags
	catch "$win tag delete $tags"
	eval $win mark unset [$win mark names]
	$win delete 0.0 end
	$win tag configure hr -tabs [winfo width $win]
	HMinit_state $win
}

# initialize the window's state array
# Parameters beginning with S_ are NOT reset
#  adjust_size:		global font size adjuster
#  unknown:		character to use for unknown entities
#  tab:			tab stop (in cm)
#  stop:		enabled to stop processing
#  update:		how many tags between update calls
#  tags:		number of tags processed so far
#  symbols:		Symbols to use on un-ordered lists

proc HMinit_state {win} {
	upvar #0 HM$win var
	array set tmp [array get var S_*]
	catch {unset var}
	array set var {
		stop 0
		tags 0
		fill 0
		list list
		S_adjust_size 0
		S_tab 1.0
		S_unknown \xb7
		S_update 10
		S_symbols O*=+-o\xd7\xb0>:\xb7
	}
	array set var [array get tmp]
}

# alter the parameters of the text state
# this allows an application to over-ride the default settings
# it is called as: HMset_state -param value -param value ...

array set HMparam_map {
	-update S_update
	-tab S_tab
	-unknown S_unknown
	-stop S_stop
	-size S_adjust_size
	-symbols S_symbols
}

proc HMset_state {win args} {
	upvar #0 HM$win var
	global HMparam_map
	set bad 0
	if {[catch {array set params $args}]} {return 0}
	foreach i [array names params] {
		incr bad [catch {set var($HMparam_map($i)) $params($i)}]
	}
	return [expr $bad == 0]
}

############################################
# manage the display of html

# this gets called for every html tag
#   win:   The name of the text widget to render into
#   tag:   The html tag (in arbitrary case)
#   not:   a "/" or the empty string
#   param: The un-interpreted parameter list
#   text:  The plain text until the next html tag

proc HMrender {win tag not param text} {
	upvar #0 HM$win var
	if {$var(stop)} return
	global HMtag_map HMinsert_map HMlist_elements
	set tag [string tolower $tag]
	set text [HMmap_esc $text]

	# manage compact rendering of lists
	if {[info exists HMlist_elements($tag)]} {
		set list "list [expr {[HMextract_param $param compact] ? "compact" : "list"}]"
	} else {
		set list ""
	}

	# adjust tag state
	catch {HMstack $win $not "$HMtag_map($tag) $list"}

	# insert white space (with current font)
	# adding white space can get a bit tricky.  This isn't quite right
	set bad [catch {$win insert end $HMinsert_map($not$tag) "space $var(font)"}]
	if {!$bad && [llength $var(fill)]>0 && [lindex $var(fill) end]} {
		set text [string trimleft $text]
	}

	# to fill or not to fill
	if {[llength $var(fill)]>0 && [lindex $var(fill) end]} {
		set text [HMzap_white $text]
	}

	# do any special tag processing
	catch {HMtag_$not$tag $win $param text} msg

	# debugging only (code will not be here if no debugging was set)

	# add the text with proper tags
	set tags [HMcurrent_tags $win]
	$win insert end $text $tags

	# We need to do an update every so often to insure interactive response.
	# This can cause us to re-enter the event loop, and cause recursive
	# invocations of HMrender, so we need to be careful.
	if {!([incr var(tags)] % $var(S_update))} {
		update
	}
}

# html tags requiring special processing
# Procs of the form HMtag_<tag> or HMtag_</tag> get called just before
# the text for this tag is displayed.
#   win:   The name of the text widget to render into
#   param: The un-interpreted parameter list
#   text:  A pass-by-reference name of the plain text until the next html tag
#          Tag commands may change this to affect what text will be inserted
#          next.

proc HMtag_title {win param text} {
	upvar $text data
	wm title [winfo toplevel $win] $data
	set data ""
}

proc HMtag_hr {win param text} {
	$win insert end "\n" space "\n" thin "\t" "thin hr" "\n" thin
}

# list element tags

proc HMtag_ol {win param text} {
	upvar #0 HM$win var
	set var(count$var(level)) 0
}

proc HMtag_ul {win param text} {
	upvar #0 HM$win var
	catch {unset var(count$var(level))}
}

proc HMtag_menu {win param text} {
	upvar #0 HM$win var
	set var(menu) ->
	set var(compact) 1
}

proc HMtag_/menu {win param text} {
	upvar #0 HM$win var
	catch {unset var(menu)}
	catch {unset var(compact)}
}
	
proc HMtag_dt {win param text} {
	upvar #0 HM$win var
	upvar $text data
	set level $var(level)
	incr level -1
	$win insert end "$data" "hi [lindex $var(list) end] indent$level $var(font)"
	set data {}
}

proc HMtag_li {win param text} {
	upvar #0 HM$win var
	set level $var(level)
	incr level -1
	set x [string index $var(S_symbols)+-+-+-+-" $level]
	catch {set x [incr var(count$level)]}
	catch {set x $var(menu)}
	$win insert end \t$x\t "mark [lindex $var(list) end] indent$level $var(font)"
}

# hypertext links.

proc HMtag_a {win param text} {
	upvar #0 HM$win var

	if {[HMextract_param $param href]} {
		set var(Tref) L:$href
		HMlink_setup $win $href
	}
}

proc HMtag_/a {win param text} {
	upvar #0 HM$win var
	catch {unset var(Tref)}
}

# This interface is subject to change
# Most of the work is getting around a limitation of TK that prevents
# setting the size of a label to a widthxheight in pixels
#
# Images have the following parameters:
#    align:  top,middle,bottom
#    alt:    alternate text
#    ismap:  A clickable image map
#    src:    The URL link
# Netscape supports
#    width:  A width hint (in pixels)
#    height:  A height hint (in pixels)
#    border: The size of the window border

proc HMtag_img {win param text} {
	upvar #0 HM$win var

	# get alignment
	array set align_map {top top    middle center    bottom bottom}
	set align bottom		;# The spec isn't clear what the default should be
	HMextract_param $param align
	catch {set align $align_map([string tolower $align])}

	# get alternate text
	set alt "<image>"
	HMextract_param $param alt
	set alt [HMmap_esc $alt]

	# get the border width
	set border 1
	HMextract_param $param border

	# see if we have an image size hint
	# If so, make a frame the "hint" size to put the label in
	# otherwise just make the label
	set item $win.$var(tags)
	catch {destroy $item}
	if {[HMextract_param $param width] && [HMextract_param $param height]} {
		frame $item -width $width -height $height
		pack propagate $item 0
		set label $item.label
		label $label
		pack $label -expand 1 -fill both
	} else {
		set label $item
		label $label 
	}

	$label configure -relief ridge -fg orange -text $alt
	catch {$label configure -bd $border}
	$win window create end -align $align -window $item -pady 2

	# add in all the current tags (this is overkill)
	set tags [HMcurrent_tags $win]
	foreach tag $tags {
		$win tag add $tag $item
	}

	# set imagemap callbacks
	if {[HMextract_param $param ismap]} {
		set link ""
		regsub -all {[^L]*L:([^ ]*).*}  $tags {\1} link
		global HMevents
		foreach i [array names HMevents] {
			bind $label <$i> "%W configure $HMevents($i)"
		}
		bind ismap <ButtonRelease-1> "HMlink_callback $win $link?%x,%y"
		bindtags $label "ismap [bindtags $label]"
	} 

	# now callback to the application
	set src ""
	HMextract_param $param src
	HMset_image $label $src
}

# The app needs to supply one of these
#proc HMset_image {handle src} {
#	puts "Found an image <$src> to put in $handle"
#	HMgot_image $handle "can't get\n$src"
#}

# When the image is available, the application should call back here.
# If we have the image, put it in the label, otherwise display the error
# message.  If we don't get a callback, the "alt" text remains.
# if we have a clickable image, arrange for a callback

proc HMgot_image {win image_error} {
	# if we're in a frame turn on geometry propogation
	if {[winfo name $win] == "label"} {
		pack propagate [winfo parent $win] 1
	}
	if {[catch {$win configure -image $image_error}]} {
		$win configure -image {}
		$win configure -text $image_error
	}
}

# Sample hypertext link callback routine - should be replaced by app
# This proc is called once for each <A> tag.
# Applications can overwrite this procedure, as required, or
# replace the HMevents array
#   win:   The name of the text widget to render into
#   href:  The HREF link for this <a> tag.

array set HMevents {
	Enter	{-borderwidth 2 -relief raised }
	Leave	{-borderwidth 0 -relief flat }
	1		{-borderwidth 2 -relief sunken}
	ButtonRelease-1	{-relief raised}
}

proc HMlink_setup {win href} {
	global HMevents
	foreach i [array names HMevents] {
		eval $win tag bind L:$href <$i> \
			\{$win tag configure L:$href $HMevents($i)\}
	}
}

# generic link-hit callback
# This gets called upon button hits on hypertext links
# Applications are expected to supply ther own HMlink_callback routine
#   win:   The name of the text widget to render into
#   x,y:   The cursor position at the "click"

proc HMlink_hit {win x y} {
	set tags [$win tag names @$x,$y]
	regsub -all {[^L]*L:([^ ]*).*}  $tags {\1} link
	HMlink_callback $win $link
}

# replace this!
#   win:   The name of the text widget to render into
#   href:  The HREF link for this <a> tag.

#proc HMlink_callback {win href} {
#	puts "Got hit on $win, link $href"
#}

# extract a value from parameter list (this needs a re-do)
# returns "1" if the keyword is found, "0" otherwise
#   param:  A parameter list.  It should alredy have been processed to
#           remove any entity references
#   key:    The parameter name
#   val:    The variable to put the value into (use key as default)

proc HMextract_param {param key {val ""}} {

	if {$val == ""} {
		upvar $key result
	} else {
		upvar $val result
	}

	# look for name=value combinations.  Either (') or (") are valid delimeters
	if {
	  [regsub -nocase [format {.*%s *= *"([^"]*).*} $key] $param {\1} value] ||
	  [regsub -nocase [format {.*%s *= *'([^']*).*} $key] $param {\1} value] ||
	  [regsub -nocase [format {.*%s *= *([^ ]+).*} $key] $param {\1} value] } {
		set result $value
		return 1
	}

	# now look for valueless names
	# I should strip out name=value pairs, so we don't end up with "name"
	# inside the "value" part of some other key word - some day
	
	set bad \[^a-zA-Z\]+
	if {[regexp -nocase  "$bad$key$bad" -$param-]} {
		return 1
	} else {
		return 0
	}
}

# These next two routines manage the display state of the page.

# push or pop tags to/from stack
# the current tag is the last item on the list

proc HMstack {win push list} {
	upvar #0 HM$win var
	array set tags $list
	if {$push == ""} {
		foreach tag [array names tags] {
			lappend var($tag) $tags($tag)
		}
	} else {
		foreach tag [array names tags] {
			set cnt [regsub { *[^ ]+$} $var($tag) {} var($tag)]
		}
	}
}

# extract set of current text tags
# tags starting with T map directly to text tags

proc HMcurrent_tags {win} {
	upvar #0 HM$win var
	set font font
	foreach i {family size weight style} {
		set $i [lindex $var($i) end]
		append font :[set $i]
	}
	set xfont [HMx_font $family $size $weight $style $var(S_adjust_size)]
	catch {$win tag configure $font -font $xfont} msg
	set indent [llength $var(indent)]
	incr indent -1
	lappend tags $font indent$indent
	foreach tag [array names var T*] {
		append tags " [lindex $var($tag) end]"
	}
	set var(font) $font
	set var(level) $indent
	return $tags
}

# generate an X font name
proc HMx_font {family size weight style {adjust_size 0}} {
	catch {incr size $adjust_size}
	return "-*-$family-$weight-$style-normal--${size}-*-*-*-*-*-*-*"
}
############################################
# Turn HTML into TCL commands
#   html    A string containing an html document
#   cmd		A command to run for each html tag found

proc HMparse_html {html {cmd HMtest_parse}} {
	regsub -all \{ $html {\&ob;} html
	regsub -all \} $html {\&cb;} html
	set w " \t\r\n"	;# white space
	proc HMcl x {return "\[$x\]"}
	set exp <(/?)([HMcl ^$w>]+)[HMcl $w]*([HMcl ^>]*)>
	set sub "\}\n$cmd {\\2} {\\1} {\\3} \{"
	regsub -all $exp $html $sub html
	eval "$cmd hmstart {} {} \{ $html \}"
	eval "$cmd hmstart / {} {}"
}

proc HMtest_parse {command tag slash text_after_tag} {
	puts "==> $command $tag $slash $text_after_tag"
}

# Convert multiple white space into a single space

proc HMzap_white {data} {
	regsub -all "\[ \t\r\n\]+" $data " " data
	return $data
}

# find HTML escape characters of the form &xxx;

proc HMmap_esc {text} {
	if {![regexp & $text]} {return $text}
	regsub -all {([][$\\])} $text {\\\1} new
	regsub -all {&#([0-9][0-9][0-9]?);?} \
		$new {[format %c \1]} new 
	regsub -all {&([^ ;]+);?} $new {[HMdo_map \1]} new
	return [subst $new]
}

# convert an HTML escape sequence into character

proc HMdo_map {text {unknown ?}} {
	global HMesc_map
	set result $unknown
	catch {set result $HMesc_map($text)}
	return $result
}

# table of escape characters (ISO latin-1 esc's are in a different table)

array set HMesc_map {
   lt <   gt >   amp &   quot \"   copy \xa9
   reg \xae   ob \x7b   cb \x7d   nbsp \xa0
}
#############################################################
# ISO Latin-1 escape codes

array set HMesc_map {
	nbsp \xa0 iexcl \xa1 cent \xa2 pound \xa3 curren \xa4
	yen \xa5 brvbar \xa6 sect \xa7 uml \xa8 copy \xa9
	ordf \xaa laquo \xab not \xac shy \xad reg \xae
	hibar \xaf deg \xb0 plusmn \xb1 sup2 \xb2 sup3 \xb3
	acute \xb4 micro \xb5 para \xb6 middot \xb7 cedil \xb8
	sup1 \xb9 ordm \xba raquo \xbb frac14 \xbc frac12 \xbd
	frac34 \xbe iquest \xbf Agrave \xc0 Aacute \xc1 Acirc \xc2
	Atilde \xc3 Auml \xc4 Aring \xc5 AElig \xc6 Ccedil \xc7
	Egrave \xc8 Eacute \xc9 Ecirc \xca Euml \xcb Igrave \xcc
	Iacute \xcd Icirc \xce Iuml \xcf ETH \xd0 Ntilde \xd1
	Ograve \xd2 Oacute \xd3 Ocirc \xd4 Otilde \xd5 Ouml \xd6
	times \xd7 Oslash \xd8 Ugrave \xd9 Uacute \xda Ucirc \xdb
	Uuml \xdc Yacute \xdd THORN \xde szlig \xdf agrave \xe0
	aacute \xe1 acirc \xe2 atilde \xe3 auml \xe4 aring \xe5
	aelig \xe6 ccedil \xe7 egrave \xe8 eacute \xe9 ecirc \xea
	euml \xeb igrave \xec iacute \xed icirc \xee iuml \xef
	eth \xf0 ntilde \xf1 ograve \xf2 oacute \xf3 ocirc \xf4
	otilde \xf5 ouml \xf6 divide \xf7 oslash \xf8 ugrave \xf9
	uacute \xfa ucirc \xfb uuml \xfc yacute \xfd thorn \xfe
	yuml \xff
}
