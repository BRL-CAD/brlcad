##                 M G E D . T C L
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
#	The Mged class inherits from QuadDisplay and contains
#       a Database object.
#

option add *Mged.width 400 widgetDefault
option add *Mged.height 400 widgetDefault

itcl::class Mged {
    inherit QuadDisplay

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
	method find {args}
	method form {args}
	method g {args}
	method get {args}
	method get_eyemodel {viewObj}
	method hide {args}
	method i {args}
	method illum {obj}
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
	method rt_gettrees {args}
	method shells {args}
	method showmats {args}
	method summary {args}
	method title {args}
	method tol {args}
	method tops {args}
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

itcl::body Mged::constructor {file args} {
    set db [Database #auto $file]
    set dg [$db Drawable::get_dgname]
    addall $dg

    catch {eval itk_initialize $args}
}

itcl::body Mged::destructor {} {
    ::delete object $db
}

#----------------------------------#
# Commands related to the Database #
#----------------------------------#
#
itcl::body Mged::opendb {args} {
    eval $db open $args
}

itcl::body Mged::match {args} {
    eval $db match $args
}

itcl::body Mged::get {args} {
    eval $db get $args
}

itcl::body Mged::put {args} {
    eval $db put $args
}

itcl::body Mged::adjust {args} {
    eval $db adjust $args
}

itcl::body Mged::attr {args} {
    eval $db attr $args
}

itcl::body Mged::form {args} {
    eval $db form $args
}

itcl::body Mged::tops {args} {
    eval $db tops $args
}

itcl::body Mged::shells {args} {
    eval $db shells $args
}

itcl::body Mged::showmats {args} {
    eval $db showmats $args
}

itcl::body Mged::summary {args} {
    eval $db summary $args
}

itcl::body Mged::rt_gettrees {args} {
    eval $db rt_gettrees $args
}

itcl::body Mged::dump {args} {
    eval $db dump $args
}

itcl::body Mged::dbip {args} {
    eval $db dbip $args
}

itcl::body Mged::l {args} {
    eval $db l $args
}

itcl::body Mged::listeval {args} {
    eval $db listeval $args
}

itcl::body Mged::ls {args} {
    eval $db ls $args
}

itcl::body Mged::lt {args} {
    eval $db lt $args
}

itcl::body Mged::pathlist {args} {
    eval $db pathlist $args
}

itcl::body Mged::paths {args} {
    eval $db paths $args
}

itcl::body Mged::expand {args} {
    eval $db expand $args
}

itcl::body Mged::kill {args} {
    eval $db kill $args
}

itcl::body Mged::killall {args} {
    eval $db killall $args
}

itcl::body Mged::killtree {args} {
    eval $db killtree $args
}

itcl::body Mged::cp {args} {
    eval $db cp $args
}

itcl::body Mged::mv {args} {
    eval $db mv $args
}

itcl::body Mged::mvall {args} {
    eval $db mvall $args
}

itcl::body Mged::nmg_collapse {args} {
    eval $db nmg_collapse $args
}

itcl::body Mged::nmg_simplify {args} {
    eval $db nmg_simplify $args
}

itcl::body Mged::copyeval {args} {
    eval $db copyeval $args
}

itcl::body Mged::concat {args} {
    eval $db concat $args
}

itcl::body Mged::dup {args} {
    eval $db dup $args
}

itcl::body Mged::g {args} {
    eval $db g $args
}

itcl::body Mged::rm {args} {
    eval $db rm $args
}

itcl::body Mged::c {args} {
    eval $db c $args
}

itcl::body Mged::comb {args} {
    eval $db comb $args
}

itcl::body Mged::find {args} {
    eval $db find $args
}

itcl::body Mged::whichair {args} {
    eval $db whichair $args
}

itcl::body Mged::whichid {args} {
    eval $db whichid $args
}

itcl::body Mged::title {args} {
    eval $db title $args
}

itcl::body Mged::tree {args} {
    eval $db tree $args
}

itcl::body Mged::unhide {args} {
    eval $db unhide $args
}

itcl::body Mged::color {args} {
    eval $db color $args
}

itcl::body Mged::prcolor {args} {
    eval $db prcolor $args
}

itcl::body Mged::tol {args} {
    eval $db tol $args
}

itcl::body Mged::push {args} {
    eval $db push $args
}

itcl::body Mged::whatid {args} {
    eval $db whatid $args
}

itcl::body Mged::keep {args} {
    eval $db keep $args
}

itcl::body Mged::cat {args} {
    eval $db cat $args
}

itcl::body Mged::hide {args} {
    eval $db hide $args
}

itcl::body Mged::i {args} {
    eval $db i $args
}

itcl::body Mged::make_bb {name args} {
    eval $db make_bb $name $args
}

itcl::body Mged::make_name {args} {
    eval $db make_name $args
}

itcl::body Mged::units {args} {
    eval $db units $args
}

#
# get_eyemodel - returns a list containing the viewsize, orientation,
#                and eye_pt strings. Useful for constructing Rt scripts
#
itcl::body Mged::get_eyemodel {viewObj} {
    return [eval $db get_eyemodel $viewObj]
}

itcl::body Mged::draw {args} {
    set who [who]

    if {$who == ""} {
	set blank 1
    } else {
	set blank 0
    }

    if {$blank} {
	# stop observing the Drawable
	detach_drawableall $dg
	set result [eval $db draw $args]
	# resume observing the Drawable
	attach_drawableall $dg

	# stop observing the View
	detach_viewall
	autoviewall
	# resume observing the View
	attach_viewall

	# We need to refresh here because nobody was observing
	# during the changes to the Drawable and the View. This
	# was done in order to prevent multiple refreshes.
	refreshall
    } else {
	set result [eval $db draw $args]
    }

    return $result
}

itcl::body Mged::E {args} {
    eval $db E $args
}

itcl::body Mged::erase {args} {
    eval $db erase $args
}

itcl::body Mged::who {args} {
    eval $db who $args
}

itcl::body Mged::xpush {args} {
    eval $db xpush $args
}

itcl::body Mged::zap {args} {
    eval $db zap $args
}

itcl::body Mged::binary {args} {
    eval $db binary $args
}

itcl::body Mged::blast {args} {
    eval $db blast $args
}

itcl::body Mged::clear {args} {
    eval $db clear $args
}

itcl::body Mged::ev {args} {
    eval $db ev $args
}

itcl::body Mged::erase_all {args} {
    eval $db erase_all $args
}

itcl::body Mged::overlay {args} {
    eval $db overlay $args
}

itcl::body Mged::vdraw {args} {
    eval $db vdraw $args
}

itcl::body Mged::illum {args} {
    eval $db illum $args
}

itcl::body Mged::label {args} {
    eval $db label $args
}

itcl::body Mged::r {args} {
    eval $db r $args
}

itcl::body Mged::report {args} {
    eval $db report $args
}

itcl::body Mged::? {} {
    return "[QuadDisplay::?]\n[$db ?]"
}

itcl::body Mged::apropos {key} {
    return "[QuadDisplay::apropos $key] [$db apropos $key]"
}

itcl::body Mged::help {args} {
    if {[llength $args] && [lindex $args 0] != {}} {
	if {[catch {eval QuadDisplay::help $args} result]} {
	    set result [eval $db help $args]
	}

	return $result
    }

    # list all help messages for QuadDisplay and Db
    return "[QuadDisplay::help][$db help]"
}

itcl::body Mged::getUserCmds {} {
    return "? apropos help [QuadDisplay::getUserCmds] [$db getUserCmds]"
}
