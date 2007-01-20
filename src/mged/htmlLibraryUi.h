/*                 H T M L L I B R A R Y U I . H
 * BRL-CAD
 *
 * Copyright (c) 1995-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file htmlLibraryUi.h
# Simple HTML display library by Stephen Uhler (stephen.uhler@sun.com)
# Copyright (c) 1995 by Sun Microsystems
# Version 0.1 Thu Jul 20 09:06:28 PDT 1995
*/
char *html_library_ui_str = "\
array set HMtag_map {\
	a	   {Tlink link}\
	b      {weight bold}\
	blockquote	{style i indent 1 Trindent rindent}\
	bq		{style i indent 1 Trindent rindent}\
	cite   {style i}\
	code   {family courier}\
	dfn    {style i}	\
	dir    {indent 1}\
	dl     {indent 1}\
	em     {style i}\
	h1     {size 24 weight bold}\
	h2     {size 22}		\
	h3     {size 20}	\
	h4     {size 18}\
	h5     {size 16}\
	h6     {style i}\
	i      {style i}\
	kbd    {family courier weight bold}\
	menu     {indent 1}\
	ol     {indent 1}\
	pre    {fill 0 family courier Tnowrap nowrap}\
	samp   {family courier}		\
	strong {weight bold}		\
	tt     {family courier}\
	u	 {Tunderline underline}\
	ul     {indent 1}\
	var    {style i}	\
};\
\
array set HMtag_map {\
	center {Tcenter center}\
	strike {Tstrike strike}\
	u	   {Tunderline underline}\
};\
\
set HMtag_map(hmstart) {\
	family times   weight medium   style r   size 14\
	Tcenter \"\"   Tlink \"\"   Tnowrap \"\"   Tunderline \"\"   list list\
	fill 1   indent \"\" counter 0 adjust 0\
};\
\
array set HMinsert_map {\
	blockquote \"\\n\\n\" /blockquote \"\\n\"\
	br	\"\\n\"\
	dd	\"\\n\" /dd	\"\\n\"\
	dl	\"\\n\" /dl	\"\\n\"\
	dt	\"\\n\"\
	form \"\\n\"	/form \"\\n\"\
	h1	\"\\n\\n\"	/h1	\"\\n\"\
	h2	\"\\n\\n\"	/h2	\"\\n\"\
	h3	\"\\n\\n\"	/h3	\"\\n\"\
	h4	\"\\n\"	/h4	\"\\n\"\
	h5	\"\\n\"	/h5	\"\\n\"\
	h6	\"\\n\"	/h6	\"\\n\"\
	li   \"\\n\"\
	/dir \"\\n\"\
	/ul \"\\n\"\
	/ol \"\\n\"\
	/menu \"\\n\"\
	p	\"\\n\\n\"\
	pre \"\\n\"	/pre \"\\n\"\
};\
\
array set HMlist_elements {\
	ol 1   ul 1   menu 1   dl 1   dir 1\
};\
\
proc HMinit_win {win} {\
	upvar #0 HM$win var;\
	\
	HMinit_state $win;\
	$win tag configure underline -underline 1;\
	$win tag configure center -justify center;\
	$win tag configure nowrap -wrap none;\
	$win tag configure rindent -rmargin $var(S_tab)c;\
	$win tag configure strike -overstrike 1;\
	$win tag configure mark -foreground red;\
	$win tag configure list -spacing1 3p -spacing3 3p;\
	$win tag configure compact -spacing1 0p;\
	$win tag configure link -foreground blue;\
	HMset_indent $win $var(S_tab);\
	$win configure -wrap word;\
\
	$win tag configure thin -font [HMx_font Times 14 Medium R];\
	$win tag configure hr -relief sunken -borderwidth 2 -wrap none \
		-tabs [winfo width $win];\
	bind $win <Configure> {%W tag configure hr -tabs %w};\
\
	$win tag bind link <1> \"HMlink_hit $win %x %y\";\
};\
\
proc HMset_indent {win cm} {\
	set tabs [expr $cm / 2.0];\
	$win configure -tabs ${tabs}c;\
	foreach i {1 2 3 4 5 6 7 8 9} {\
		set tab [expr $i * $cm];\
		$win tag configure indent$i -lmargin1 ${tab}c -lmargin2 ${tab}c \
			-tabs \"[expr $tab + $tabs]c [expr $tab + 2*$tabs]c\";\
	};\
};\
\
proc HMreset_win {win} {\
	regsub -all { +[^L ][^ ]*} \" [$win tag names] \" {} tags;\
	catch \"$win tag delete $tags\";\
	eval $win mark unset [$win mark names];\
	$win delete 0.0 end;\
	$win tag configure hr -tabs [winfo width $win];\
	HMinit_state $win;\
};\
\
proc HMinit_state {win} {\
	upvar #0 HM$win var;\
	array set tmp [array get var S_*];\
	catch {unset var};\
	array set var {\
		stop 0\
		tags 0\
		fill 0\
		list list\
		S_adjust_size 0\
		S_tab 1.0\
		S_unknown \\xb7\
		S_update 10\
		S_symbols O*=+-o\\xd7\\xb0>:\\xb7\
	};\
	array set var [array get tmp];\
};\
\
array set HMparam_map {\
	-update S_update\
	-tab S_tab\
	-unknown S_unknown\
	-stop S_stop\
	-size S_adjust_size\
	-symbols S_symbols\
};\
\
proc HMset_state {win args} {\
	upvar #0 HM$win var;\
	global HMparam_map;\
	set bad 0;\
	if {[catch {array set params $args}]} {return 0;};\
	foreach i [array names params] {\
		incr bad [catch {set var($HMparam_map($i)) $params($i)}];\
	};\
	return [expr $bad == 0];\
};\
\
proc HMrender {win tag not param text} {\
	upvar #0 HM$win var;\
	if {$var(stop)} return;\
	global HMtag_map HMinsert_map HMlist_elements;\
	set tag [string tolower $tag];\
	set text [HMmap_esc $text];\
\
	if {[info exists HMlist_elements($tag)]} {\
		set list \"list [expr {[HMextract_param $param compact] ? \"compact\" : \"list\"}]\";\
	} else {\
		set list \"\";\
	};\
\
	catch {HMstack $win $not \"$HMtag_map($tag) $list\"};\
\
	set bad [catch {$win insert end $HMinsert_map($not$tag) \"space $var(font)\"}];\
	if {!$bad && [llength $var(fill)]>0 && [lindex $var(fill) end]} {\
		set text [string trimleft $text];\
	};\
\
	if {[llength $var(fill)]>0 && [lindex $var(fill) end]} {\
		set text [HMzap_white $text];\
	};\
\
	catch {HMtag_$not$tag $win $param text} msg;\
\
	set tags [HMcurrent_tags $win];\
	$win insert end $text $tags;\
\
	if {!([incr var(tags)] % $var(S_update))} {\
		update;\
	};\
};\
\
proc HMtag_title {win param text} {\
	upvar $text data;\
	wm title [winfo toplevel $win] $data;\
	set data \"\";\
};\
\
proc HMtag_hr {win param text} {\
	$win insert end \"\\n\" space \"\\n\" thin \"\\t\" \"thin hr\" \"\\n\" thin;\
};\
\
proc HMtag_ol {win param text} {\
	upvar #0 HM$win var;\
	set var(count$var(level)) 0;\
};\
\
proc HMtag_ul {win param text} {\
	upvar #0 HM$win var;\
	catch {unset var(count$var(level))};\
};\
\
proc HMtag_menu {win param text} {\
	upvar #0 HM$win var;\
	set var(menu) ->;\
	set var(compact) 1;\
};\
\
proc HMtag_/menu {win param text} {\
	upvar #0 HM$win var;\
	catch {unset var(menu)};\
	catch {unset var(compact)};\
};\
	\
proc HMtag_dt {win param text} {\
	upvar #0 HM$win var;\
	upvar $text data;\
	set level $var(level);\
	incr level -1;\
	$win insert end \"$data\" \"hi [lindex $var(list) end] indent$level $var(font)\";\
	set data {};\
};\
\
proc HMtag_li {win param text} {\
	upvar #0 HM$win var;\
	set level $var(level);\
	incr level -1;\
	set x [string index $var(S_symbols)+-+-+-+-\" $level];\
	catch {set x [incr var(count$level)]};\
	catch {set x $var(menu)};\
	$win insert end \\t$x\\t \"mark [lindex $var(list) end] indent$level $var(font)\";\
};\
\
proc HMtag_a {win param text} {\
	upvar #0 HM$win var;\
\
	if {[HMextract_param $param href]} {\
		set var(Tref) L:$href;\
		HMlink_setup $win $href;\
	};\
};\
\
proc HMtag_/a {win param text} {\
	upvar #0 HM$win var;\
	catch {unset var(Tref)};\
};\
\
proc HMtag_img {win param text} {\
	upvar #0 HM$win var;\
\
	array set align_map {top top    middle center    bottom bottom};\
	set align bottom		;\
	HMextract_param $param align;\
	catch {set align $align_map([string tolower $align])};\
\
	set alt \"<image>\";\
	HMextract_param $param alt;\
	set alt [HMmap_esc $alt];\
\
	set border 1;\
	HMextract_param $param border;\
\
	set item $win.$var(tags);\
	catch {destroy $item};\
	if {[HMextract_param $param width] && [HMextract_param $param height]} {\
		frame $item -width $width -height $height;\
		pack propagate $item 0;\
		set label $item.label;\
		label $label;\
		pack $label -expand 1 -fill both;\
	} else {\
		set label $item;\
		label $label ;\
	};\
\
	$label configure -relief ridge -fg orange -text $alt;\
	catch {$label configure -bd $border};\
	$win window create end -align $align -window $item -pady 2;\
\
	set tags [HMcurrent_tags $win];\
	foreach tag $tags {\
		$win tag add $tag $item;\
	};\
\
	if {[HMextract_param $param ismap]} {\
		set link \"\";\
		regsub -all {[^L]*L:([^ ]*).*}  $tags {\\1} link;\
		global HMevents;\
		foreach i [array names HMevents] {\
			bind $label <$i> \"%W configure $HMevents($i)\";\
		};\
		bind ismap <ButtonRelease-1> \"HMlink_callback $win $link?%x,%y\";\
		bindtags $label \"ismap [bindtags $label]\";\
	} ;\
\
	set src \"\";\
	HMextract_param $param src;\
	HMset_image $label $src;\
};\
\
proc HMset_image {handle src} {\
	puts \"Found an image <$src> to put in $handle\";\
	HMgot_image $handle \"can't get\\n$src\";\
};\
\
proc HMgot_image {win image_error} {\
	if {[winfo name $win] == \"label\"} {\
		pack propagate [winfo parent $win] 1;\
	};\
	if {[catch {$win configure -image $image_error}]} {\
		$win configure -image {};\
		$win configure -text $image_error;\
	};\
};\
\
array set HMevents {\
	Enter	{-borderwidth 2 -relief raised }\
	Leave	{-borderwidth 0 -relief flat }\
	1		{-borderwidth 2 -relief sunken}\
	ButtonRelease-1	{-relief raised}\
};\
\
proc HMlink_setup {win href} {\
	global HMevents;\
	foreach i [array names HMevents] {\
		eval $win tag bind L:$href <$i> \
			\\{$win tag configure L:$href $HMevents($i)\\};\
	};\
};\
\
proc HMlink_hit {win x y} {\
	set tags [$win tag names @$x,$y];\
	regsub -all {[^L]*L:([^ ]*).*}  $tags {\\1} link;\
	HMlink_callback $win $link;\
};\
\
proc HMlink_callback {win href} {\
	puts \"Got hit on $win, link $href\";\
};\
\
proc HMextract_param {param key {val \"\"}} {\
\
	if {$val == \"\"} {\
		upvar $key result;\
	} else {\
		upvar $val result;\
	};\
\
	if {\
	  [regsub -nocase [format {.*%s *= *\"([^\"]*).*} $key] $param {\\1} value] ||\
	  [regsub -nocase [format {.*%s *= *'([^']*).*} $key] $param {\\1} value] ||\
	  [regsub -nocase [format {.*%s *= *([^ ]+).*} $key] $param {\\1} value] } {\
		set result $value;\
		return 1;\
	};\
\
	set bad \\[^a-zA-Z\\]+;\
	if {[regexp -nocase  \"$bad$key$bad\" -$param-]} {\
		return 1;\
	} else {\
		return 0;\
	};\
};\
\
proc HMstack {win push list} {\
	upvar #0 HM$win var;\
	array set tags $list;\
	if {$push == \"\"} {\
		foreach tag [array names tags] {\
			lappend var($tag) $tags($tag);\
		};\
	} else {\
		foreach tag [array names tags] {\
			set cnt [regsub { *[^ ]+$} $var($tag) {} var($tag)];\
		};\
	};\
};\
\
proc HMcurrent_tags {win} {\
	upvar #0 HM$win var;\
	set font font;\
	foreach i {family size weight style} {\
		set $i [lindex $var($i) end];\
		append font :[set $i];\
	};\
	set xfont [HMx_font $family $size $weight $style $var(S_adjust_size)];\
	catch {$win tag configure $font -font $xfont} msg;\
	set indent [llength $var(indent)];\
	incr indent -1;\
	lappend tags $font indent$indent;\
	foreach tag [array names var T*] {\
		append tags \" [lindex $var($tag) end]\";\
	};\
	set var(font) $font;\
	set var(level) $indent;\
	return $tags;\
};\
\
proc HMx_font {family size weight style {adjust_size 0}} {\
	catch {incr size $adjust_size};\
	return \"-*-$family-$weight-$style-normal--${size}-*-*-*-*-*-*-*\";\
};\
\
proc HMparse_html {html {cmd HMtest_parse}} {\
	regsub -all \\{ $html {\\&ob;} html;\
	regsub -all \\} $html {\\&cb;} html;\
	set w \" \\t\\r\\n\"	;\
	proc HMcl x {return \"\\[$x\\]\";};\
	set exp <(/?)([HMcl ^$w>]+)[HMcl $w]*([HMcl ^>]*)>;\
	set sub \"\\}\\n$cmd {\\\\2} {\\\\1} {\\\\3} \\{\";\
	regsub -all $exp $html $sub html;\
	eval \"$cmd hmstart {} {} \\{ $html \\}\";\
	eval \"$cmd hmstart / {} {}\";\
};\
\
proc HMtest_parse {command tag slash text_after_tag} {\
	puts \"==> $command $tag $slash $text_after_tag\";\
};\
\
proc HMzap_white {data} {\
	regsub -all \"\\[ \\t\\r\\n\\]+\" $data \" \" data;\
	return $data;\
};\
\
proc HMmap_esc {text} {\
	if {![regexp & $text]} {return $text;};\
	regsub -all {([][$\\\\])} $text {\\\\\\1} new;\
	regsub -all {&#([0-9][0-9][0-9]?);?} \
		$new {[format %c \\1]} new ;\
	regsub -all {&([^ ;]+);?} $new {[HMdo_map \\1]} new;\
	return [subst $new];\
};\
\
proc HMdo_map {text {unknown ?}} {\
	global HMesc_map;\
	set result $unknown;\
	catch {set result $HMesc_map($text)};\
	return $result;\
};\
\
array set HMesc_map {\
   lt <   gt >   amp &   quot \\\"   copy \\xa9\
   reg \\xae   ob \\x7b   cb \\x7d   nbsp \\xa0\
};\
\
array set HMesc_map {\
	nbsp \\xa0 iexcl \\xa1 cent \\xa2 pound \\xa3 curren \\xa4\
	yen \\xa5 brvbar \\xa6 sect \\xa7 uml \\xa8 copy \\xa9\
	ordf \\xaa laquo \\xab not \\xac shy \\xad reg \\xae\
	hibar \\xaf deg \\xb0 plusmn \\xb1 sup2 \\xb2 sup3 \\xb3\
	acute \\xb4 micro \\xb5 para \\xb6 middot \\xb7 cedil \\xb8\
	sup1 \\xb9 ordm \\xba raquo \\xbb frac14 \\xbc frac12 \\xbd\
	frac34 \\xbe iquest \\xbf Agrave \\xc0 Aacute \\xc1 Acirc \\xc2\
	Atilde \\xc3 Auml \\xc4 Aring \\xc5 AElig \\xc6 Ccedil \\xc7\
	Egrave \\xc8 Eacute \\xc9 Ecirc \\xca Euml \\xcb Igrave \\xcc\
	Iacute \\xcd Icirc \\xce Iuml \\xcf ETH \\xd0 Ntilde \\xd1\
	Ograve \\xd2 Oacute \\xd3 Ocirc \\xd4 Otilde \\xd5 Ouml \\xd6\
	times \\xd7 Oslash \\xd8 Ugrave \\xd9 Uacute \\xda Ucirc \\xdb\
	Uuml \\xdc Yacute \\xdd THORN \\xde szlig \\xdf agrave \\xe0\
	aacute \\xe1 acirc \\xe2 atilde \\xe3 auml \\xe4 aring \\xe5\
	aelig \\xe6 ccedil \\xe7 egrave \\xe8 eacute \\xe9 ecirc \\xea\
	euml \\xeb igrave \\xec iacute \\xed icirc \\xee iuml \\xef\
	eth \\xf0 ntilde \\xf1 ograve \\xf2 oacute \\xf3 ocirc \\xf4\
	otilde \\xf5 ouml \\xf6 divide \\xf7 oslash \\xf8 ugrave \\xf9\
	uacute \\xfa ucirc \\xfb uuml \\xfc yacute \\xfd thorn \\xfe\
	yuml \\xff\
};\
";

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
