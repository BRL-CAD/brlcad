#                        M G E D . T C L
# BRL-CAD
#
# Copyright (C) 1998-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
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
#	The Mged class inherits from QuadDisplay and contains
#       a Database object.
#

option add *Mged.width 400 widgetDefault
option add *Mged.height 400 widgetDefault

::itcl::class Mged {
    inherit QuadDisplay

    itk_option define -unitsCallback unitsCallback UnitsCallback ""
    itk_option define -autoViewEnable autoViewEnable AutoViewEnable 1

    constructor {file args} {
	eval QuadDisplay::constructor
    } {}
    destructor {}

    #----------------------------------#
    # Commands related to the Database #
    #----------------------------------#
    #
    public {
	method adjust {args}
	method attr {args}
	method binary {args}
	method blast {args}
	method c {args}
	method cat {args}
	method clear {args}
	method color {args}
	method comb {args}
	method concat {args}
	method copyeval {args}
	method cp {args}
	method dbip {args}
	method draw {args}
	method dump {args}
	method dup {args}
	method E {args}
	method erase {args}
	method erase_all {args}
	method ev {args}
	method expand {args}
	method facetize {args}
	method find {args}
	method form {args}
	method g {args}
	method get {args}
	method get_type {args}
	method get_eyemodel {viewObj}
	method hide {args}
	method how {args}
	method i {args}
	method illum {obj}
	method importFg4Section {args}
	method keep {args}
	method kill {args}
	method killall {args}
	method killtree {args}
	method l {args}
	method label {args}
	method listeval {args}
	method ls {args}
	method lt {args}
	method make_bb {name args}
	method make_name {args}
	method match {args}
	method move_arb_edge {args}
	method move_arb_face {args}
	method mv {args}
	method mvall {args}
	method nmg_collapse {args}
	method nmg_simplify {args}
	method opendb {args}
	method overlay {args}
	method pathlist {args}
	method paths {args}
	method prcolor {args}
	method push {args}
	method put {args}
	method r {args}
	method report {args}
	method rm {args}
	method rmap {args}
	method rotate_arb_face {args}
	method rt_gettrees {args}
	method set_outputHandler {args}
	method set_transparency {args}
	method shaded_mode {args}
	method shells {args}
	method showmats {args}
	method summary {args}
	method title {args}
	method tol {args}
	method tops {args}
	method track {args}
	method tree {args}
	method unhide {args}
	method units {args}
	method vdraw {args}
	method whatid {args}
	method whichair {args}
	method whichid {args}
	method who {args}
	method xpush {args}
	method zap {args}

	method attachObservers {}
	method detachObservers {}

	method ? {}
	method apropos {key}
	method help {args}
	method getUserCmds {}
    }

    private {
	variable db
	variable dg
    }
}

::itcl::body Mged::constructor {file args} {
    set db [Database #auto $file]
    set dg [$db Drawable::get_dgname]
    addAll $dg

    # sync up the units between the Database and QuadDisplay
    QuadDisplay::units [$db units -s]

    catch {eval itk_initialize $args}
}

::itcl::body Mged::destructor {} {
    ::itcl::delete object $db
}

#----------------------------------#
# Commands related to the Database #
#----------------------------------#
#
::itcl::body Mged::opendb {args} {
    set rval [eval $db open $args]

    # sync up the units between the Database and QuadDisplay
    QuadDisplay::units [$db units -s]

    return $rval
}

::itcl::body Mged::match {args} {
    eval $db match $args
}

::itcl::body Mged::get {args} {
    eval $db get $args
}

::itcl::body Mged::get_type {args} {
    eval $db get_type $args
}

::itcl::body Mged::put {args} {
    eval $db put $args
}

::itcl::body Mged::adjust {args} {
    eval $db adjust $args
}

::itcl::body Mged::attr {args} {
    eval $db attr $args
}

::itcl::body Mged::form {args} {
    eval $db form $args
}

::itcl::body Mged::tops {args} {
    eval $db tops $args
}

::itcl::body Mged::shells {args} {
    eval $db shells $args
}

::itcl::body Mged::showmats {args} {
    eval $db showmats $args
}

::itcl::body Mged::summary {args} {
    eval $db summary $args
}

::itcl::body Mged::rt_gettrees {args} {
    eval $db rt_gettrees $args
}

::itcl::body Mged::set_outputHandler {args} {
    eval $db set_outputHandler $args
}

::itcl::body Mged::set_transparency {args} {
    eval $db set_transparency $args
}

::itcl::body Mged::shaded_mode {args} {
    eval $db shaded_mode $args
}

::itcl::body Mged::dump {args} {
    eval $db dump $args
}

::itcl::body Mged::dbip {args} {
    eval $db dbip $args
}

::itcl::body Mged::l {args} {
    eval $db l $args
}

::itcl::body Mged::listeval {args} {
    eval $db listeval $args
}

::itcl::body Mged::ls {args} {
    eval $db ls $args
}

::itcl::body Mged::lt {args} {
    eval $db lt $args
}

::itcl::body Mged::pathlist {args} {
    eval $db pathlist $args
}

::itcl::body Mged::paths {args} {
    eval $db paths $args
}

::itcl::body Mged::expand {args} {
    eval $db expand $args
}

::itcl::body Mged::facetize {args} {
    eval $db facetize $args
}

::itcl::body Mged::kill {args} {
    eval $db kill $args
}

::itcl::body Mged::killall {args} {
    eval $db killall $args
}

::itcl::body Mged::killtree {args} {
    eval $db killtree $args
}

::itcl::body Mged::cp {args} {
    eval $db cp $args
}

::itcl::body Mged::move_arb_edge {args} {
    eval $db move_arb_edge $args
}

::itcl::body Mged::move_arb_face {args} {
    eval $db move_arb_face $args
}

::itcl::body Mged::mv {args} {
    eval $db mv $args
}

::itcl::body Mged::mvall {args} {
    eval $db mvall $args
}

::itcl::body Mged::nmg_collapse {args} {
    eval $db nmg_collapse $args
}

::itcl::body Mged::nmg_simplify {args} {
    eval $db nmg_simplify $args
}

::itcl::body Mged::copyeval {args} {
    eval $db copyeval $args
}

::itcl::body Mged::concat {args} {
    eval $db concat $args
}

::itcl::body Mged::dup {args} {
    eval $db dup $args
}

::itcl::body Mged::g {args} {
    eval $db g $args
}

::itcl::body Mged::rm {args} {
    eval $db rm $args
}

::itcl::body Mged::rmap {args} {
    eval $db rmap $args
}

::itcl::body Mged::rotate_arb_face {args} {
    eval $db rotate_arb_face $args
}

::itcl::body Mged::c {args} {
    eval $db c $args
}

::itcl::body Mged::comb {args} {
    eval $db comb $args
}

::itcl::body Mged::find {args} {
    eval $db find $args
}

::itcl::body Mged::whichair {args} {
    eval $db whichair $args
}

::itcl::body Mged::whichid {args} {
    eval $db whichid $args
}

::itcl::body Mged::title {args} {
    eval $db title $args
}

::itcl::body Mged::track {args} {
    eval $db track $args
}

::itcl::body Mged::tree {args} {
    eval $db tree $args
}

::itcl::body Mged::unhide {args} {
    eval $db unhide $args
}

::itcl::body Mged::color {args} {
    eval $db color $args
}

::itcl::body Mged::prcolor {args} {
    eval $db prcolor $args
}

::itcl::body Mged::tol {args} {
    eval $db tol $args
}

::itcl::body Mged::push {args} {
    eval $db push $args
}

::itcl::body Mged::whatid {args} {
    eval $db whatid $args
}

::itcl::body Mged::keep {args} {
    eval $db keep $args
}

::itcl::body Mged::cat {args} {
    eval $db cat $args
}

::itcl::body Mged::hide {args} {
    eval $db hide $args
}

::itcl::body Mged::how {args} {
    eval $db how $args
}

::itcl::body Mged::i {args} {
    eval $db i $args
}

::itcl::body Mged::importFg4Section {args} {
    eval $db importFg4Section $args
}

::itcl::body Mged::make_bb {name args} {
    eval $db make_bb $name $args
}

::itcl::body Mged::make_name {args} {
    eval $db make_name $args
}

::itcl::body Mged::units {args} {
    set rval [eval QuadDisplay::units $args]

    # must be a "get"
    if {[llength $args] == 0} {
	return $rval
    }

    if {$itk_option(-unitsCallback) != ""} {
	catch {eval $itk_option(-unitsCallback) $args}
    }

    eval $db units $args
}

#
# get_eyemodel - returns a list containing the viewsize, orientation,
#                and eye_pt strings. Useful for constructing Rt scripts
#
::itcl::body Mged::get_eyemodel {viewObj} {
    return [eval $db get_eyemodel $viewObj]
}

::itcl::body Mged::draw {args} {
    set who [who]

    if {$who == ""} {
	set blank 1
    } else {
	set blank 0
    }

    if {$blank && $itk_option(-autoViewEnable)} {
	# stop observing the Drawable
	detach_drawableAll $dg

	catch {eval $db draw $args} result

	# resume observing the Drawable
	attach_drawableAll $dg

	# stop observing the View
	detach_viewAll
	autoviewAll
	# resume observing the View
	attach_viewAll

	# We need to refresh here because nobody was observing
	# during the changes to the Drawable and the View. This
	# was done in order to prevent multiple refreshes.
	refreshAll
    } else {
	catch {eval $db draw $args} result
    }

    return $result
}

::itcl::body Mged::E {args} {
    set who [who]

    if {$who == ""} {
	set blank 1
    } else {
	set blank 0
    }

    if {$blank} {
	# stop observing the Drawable
	detach_drawableAll $dg
	set result [eval $db E $args]
	# resume observing the Drawable
	attach_drawableAll $dg

	# stop observing the View
	detach_viewAll
	autoviewAll
	# resume observing the View
	attach_viewAll

	# We need to refresh here because nobody was observing
	# during the changes to the Drawable and the View. This
	# was done in order to prevent multiple refreshes.
	refreshAll
    } else {
	set result [eval $db E $args]

	#XXX This is a temporary hack. I need to look at why
	#    the Drawable's observers are not being notified.
	refreshAll
    }

    return $result
}

::itcl::body Mged::erase {args} {
    eval $db erase $args
}

::itcl::body Mged::who {args} {
    eval $db who $args
}

::itcl::body Mged::xpush {args} {
    eval $db xpush $args
}

::itcl::body Mged::zap {args} {
    eval $db zap $args
}

::itcl::body Mged::binary {args} {
    eval $db binary $args
}

::itcl::body Mged::blast {args} {
    eval $db blast $args
}

::itcl::body Mged::clear {args} {
    eval $db clear $args
}

::itcl::body Mged::ev {args} {
    eval $db ev $args
}

::itcl::body Mged::erase_all {args} {
    eval $db erase_all $args
}

::itcl::body Mged::overlay {args} {
    eval $db overlay $args
}

::itcl::body Mged::vdraw {args} {
    eval $db vdraw $args
}

::itcl::body Mged::illum {args} {
    eval $db illum $args
}

::itcl::body Mged::label {args} {
    eval $db label $args
}

::itcl::body Mged::r {args} {
    eval $db r $args
}

::itcl::body Mged::report {args} {
    eval $db report $args
}

::itcl::body Mged::attachObservers {} {
    attach_drawableAll $dg
    attach_viewAll
}

::itcl::body Mged::detachObservers {} {
    detach_drawableAll $dg
    detach_viewAll
}

::itcl::body Mged::? {} {
    return "[QuadDisplay::?]\n[$db ?]"
}

::itcl::body Mged::apropos {key} {
    return "[QuadDisplay::apropos $key] [$db apropos $key]"
}

::itcl::body Mged::help {args} {
    if {[llength $args] && [lindex $args 0] != {}} {
	if {[catch {eval QuadDisplay::help $args} result]} {
	    set result [eval $db help $args]
	}

	return $result
    }

    # list all help messages for QuadDisplay and Db
    return "[QuadDisplay::help][$db help]"
}

::itcl::body Mged::getUserCmds {} {
    return "? apropos help [QuadDisplay::getUserCmds] [$db getUserCmds]"
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
