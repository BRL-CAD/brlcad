##                 D R A W A B L E . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Copyright Notice -
#       This software is Copyright (C) 1998 by the United States Army
#       in all countries except the USA.  All rights reserved.
#
# Description -
#	The Drawable class wraps LIBRT's drawable geometry object.
#
class Drawable {
    protected variable dg ""

    constructor {db} {}
    destructor {}

    public method assoc {args}
    public method blast {args}
    public method clear {}
    public method draw {args}
    public method erase {args}
    public method erase_all {args}
    public method ev {args}
    public method get_autoview {}
    public method get_dgname {}
    public method illum {obj}
    public method label {obj}
    public method observer {args}
    public method overlay {args}
    public method report {args}
    public method rt {args}
    public method rtcheck {args}
    public method vdraw {args}
    public method who {args}
    public method zap {}

    public method ? {}
    public method apropos {key}
    public method help {args}
    public method getUserCmds {}

    private method help_init {}

    private variable help
}

body Drawable::constructor {db} {
    set dg [subst $this]_dg
    dg_open $dg $db
    Drawable::help_init
}

body Drawable::destructor {} {
    $dg close
    delete object $help
}

body Drawable::observer {args} {
    eval $dg observer $args
}

body Drawable::assoc {args} {
    eval $dg assoc $args
}

body Drawable::draw {args} {
    eval $dg draw $args
}

body Drawable::erase {args} {
    eval $dg erase $args
}

body Drawable::zap {} {
    $dg zap
}

body Drawable::who {args} {
    eval $dg who $args
}

body Drawable::blast {args} {
    eval $dg blast $args
}

body Drawable::clear {} {
    $dg clear
}

body Drawable::ev {args} {
    eval $dg ev $args
}

body Drawable::erase_all {args} {
    eval $dg erase_all $args
}

body Drawable::overlay {args} {
    eval $dg overlay $args
}

body Drawable::report {args} {
    eval $dg report $args
}

body Drawable::rt {args} {
    eval $dg rt $args
}

body Drawable::rtcheck {args} {
    eval $dg rtcheck $args
}

body Drawable::vdraw {args} {
    eval $dg vdraw $args
}

body Drawable::get_autoview {} {
    $dg get_autoview
}

body Drawable::get_dgname {} {
    return $dg
}

body Drawable::illum {args} {
    eval $dg illum $args
}

body Drawable::label {args} {
    eval $dg label $args
}

body Drawable::help {args} {
    return [eval $help get $args]
}

body Drawable::? {} {
    return [$help ? 20 4]
}

body Drawable::apropos {key} {
    return [$help apropos $key]
}

body Drawable::getUserCmds {} {
    return [$help getCmds]
}

body Drawable::help_init {} {
    set help [cadwidgets::Help #auto]

    $help add blast		{{-C#/#/# <objects>} {clear screen, draw objects}}
    $help add clear		{{} {clear screen}}
    $help add draw		{{-C#/#/# <objects>} {draw objects}}
    $help add erase		{{<objects>} {remove objects from the screen}}
    $help add erase_all		{{<objects>} {remove all occurrences of object(s) from the screen}}
    $help add ev		{{[-dfnqstuvwT] [-P #] <objects>} {evaluate objects via NMG tessellation}}
    $help add get_autoview	{{} {get view parameters that shows drawn geometry}}
    $help add illum		{{name} {illuminate object}}
    $help add label		{{} {}}
    $help add overlay		{{file.plot [name]} {read UNIX-Plot as named overlay}}
    $help add report		{{} {}}
    $help add rt		{{[options] [-- objects]} {do raytrace of view or specified objects}}
    $help add rtcheck		{{[options]} {check for overlaps in current view}}
    $help add vdraw		{{write|insert|delete|read|length|show [args]} {vector drawing (cnuzman)}}
    $help add who		{{[r(eal)|p(hony)|b(oth)]} {list the top-level objects currently being displayed}}
    $help add zap		{{} {clear screen}}
}
