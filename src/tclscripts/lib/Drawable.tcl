#                    D R A W A B L E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#	The Drawable class wraps LIBRT's drawable geometry object.
#
::itcl::class Drawable {
    constructor {db} {}
    destructor {}

    public {
	method assoc {args}
	method autoview {args}
	method blast {args}
	method clear {args}
	method draw {args}
	method E {args}
	method erase {args}
	method erase_all {args}
	method ev {args}
	method get_autoview {}
	method get_eyemodel {viewObj}
	method get_dgname {}
	method how {args}
	method illum {args}
	method label {args}
	method nirt {args}
	method observer {args}
	method overlay {args}
	method report {args}
	method qray {args}
	method rt {args}
	method rtabort {args}
	method rtarea {args}
	method rtcheck {args}
	method rtedge {args}
	method rtweight {args}
	method set_outputHandler {args}
	method set_transparency {args}
	method shaded_mode {args}
	method vdraw {args}
	method who {args}
	method zap {args}

	method ? {}
	method apropos {key}
	method help {args}
	method getUserCmds {}
    }

    protected {
	variable dg ""
    }

    private {
	method help_init {}
	variable help
    }
}

::itcl::body Drawable::constructor {db} {
    global tcl_platform

    set dg [subst $this]_dg
    dg_open $dg $db

    Drawable::help_init
}

::itcl::body Drawable::destructor {} {
    rename $dg ""
    catch {delete object $help}
}

::itcl::body Drawable::assoc {args} {
    eval $dg assoc $args
}

::itcl::body Drawable::autoview {args} {
    eval $dg autoview $args
}

::itcl::body Drawable::blast {args} {
    eval $dg blast $args
}

::itcl::body Drawable::clear {args} {
    eval $dg clear $args
}

::itcl::body Drawable::draw {args} {
    eval $dg draw $args
}

::itcl::body Drawable::E {args} {
    eval $dg E $args
}

::itcl::body Drawable::erase {args} {
    eval $dg erase $args
}

::itcl::body Drawable::erase_all {args} {
    eval $dg erase_all $args
}

::itcl::body Drawable::ev {args} {
    eval $dg ev $args
}

::itcl::body Drawable::get_autoview {} {
    $dg get_autoview
}

::itcl::body Drawable::get_dgname {} {
    return $dg
}

::itcl::body Drawable::get_eyemodel {viewObj} {
    return [$dg get_eyemodel $viewObj]
}

::itcl::body Drawable::how {args} {
    eval $dg how $args
}

::itcl::body Drawable::illum {args} {
    eval $dg illum $args
}

::itcl::body Drawable::label {args} {
    eval $dg label $args
}

::itcl::body Drawable::nirt {args} {
    eval $dg nirt $args
}

::itcl::body Drawable::observer {args} {
    eval $dg observer $args
}

::itcl::body Drawable::overlay {args} {
    eval $dg overlay $args
}

::itcl::body Drawable::report {args} {
    eval $dg report $args
}

::itcl::body Drawable::qray {args} {
    eval $dg qray $args
}

::itcl::body Drawable::rt {args} {
    eval $dg rt $args
}

::itcl::body Drawable::rtabort {} {
    $dg rtabort
}

::itcl::body Drawable::rtarea {args} {
    eval $dg rtarea $args
}

::itcl::body Drawable::rtcheck {args} {
    eval $dg rtcheck $args
}

::itcl::body Drawable::rtedge {args} {
    eval $dg rtedge $args
}

::itcl::body Drawable::rtweight {args} {
    eval $dg rweight $args
}

::itcl::body Drawable::set_outputHandler {args} {
    eval $dg set_outputHandler $args
}

::itcl::body Drawable::set_transparency {args} {
    eval $dg set_transparency $args
}

::itcl::body Drawable::shaded_mode {args} {
    eval $dg shaded_mode $args
}

::itcl::body Drawable::vdraw {args} {
    eval $dg vdraw $args
}

::itcl::body Drawable::who {args} {
    eval $dg who $args
}

::itcl::body Drawable::zap {args} {
    eval $dg zap $args
}

::itcl::body Drawable::help {args} {
    return [eval $help get $args]
}

::itcl::body Drawable::? {} {
    return [$help ? 20 4]
}

::itcl::body Drawable::apropos {key} {
    return [$help apropos $key]
}

::itcl::body Drawable::getUserCmds {} {
    return [$help getCmds]
}

::itcl::body Drawable::help_init {} {
    set help [cadwidgets::Help #auto]

    $help add autoview		{{view_obj} {set the view object's size and center}}
    $help add E			{{[-s] <objects>} {evaluated edit of objects. Option 's' provides a slower,
	but better fidelity evaluation}}
    $help add blast		{{-C#/#/# <objects>} {clear screen, draw objects}}
    $help add clear		{{} {clear screen}}
    $help add draw		{{-C#/#/# <objects>} {draw objects}}
    $help add erase		{{<objects>} {remove objects from the screen}}
    $help add erase_all		{{<objects>} {remove all occurrences of object(s) from the screen}}
    $help add ev		{{[-dfnqstuvwT] [-P #] <objects>} {evaluate objects via NMG tessellation}}
    $help add get_autoview	{{} {get view parameters that shows drawn geometry}}
    $help add how		{{obj} {returns how an object is being displayed}}
    $help add illum		{{name} {illuminate object}}
    $help add label		{{[-n] obj} {label objects}}
    $help add nirt		{{[nirt(1) options] [x y z]}	{trace a single ray from current view}}
    $help add overlay		{{file.pl [name]} {overlay the specified 2D/3D UNIX plot file}}
    $help add qray		{{subcommand}	{get/set query_ray characteristics}}
    $help add report		{{[lvl]} {print solid table & vector list}}
    $help add rt		{{[options] [-- objects]} {do raytrace of view or specified objects}}
    $help add rtabort		{{} {abort the associated raytraces}}
    $help add rtcheck		{{[options]} {check for overlaps in current view}}
    $help add rtarea		{{[options] [-- objects]} {calculate area of specified objects}}
    $help add rtedge		{{[options] [-- objects]} {do raytrace of view or specified objects yielding only edges}}
    $help add rtweight		{{[options] [-- objects]} {calculate weight of specified objects}}
    $help add shaded_mode	{{[0|1|2]}	{get/set shaded mode}}
    $help add vdraw		{{write|insert|delete|read|length|show [args]} {vector drawing (cnuzman)}}
    $help add who		{{[r(eal)|p(hony)|b(oth)]} {list the top-level objects currently being displayed}}
    $help add zap		{{} {clear screen}}
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
