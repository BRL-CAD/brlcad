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

class Mged {
    inherit QuadDisplay

    constructor {file args} {
	eval QuadDisplay::constructor
    } {}
    destructor {}

    ######################### Commands related to the Database #########################
    public method adjust {args}
    public method attr {args}
    public method attr_rm {args}
    public method blast {args}
    public method c {args}
    public method cat {args}
    public method clear {}
    public method color {args}
    public method comb {args}
    public method concat {args}
    public method copyeval {args}
    public method cp {args}
    public method dbip {}
    public method draw {args}
    public method dump {args}
    public method dup {args}
    public method E {args}
    public method erase {args}
    public method erase_all {args}
    public method ev {args}
    public method expand {args}
    public method find {args}
    public method form {args}
    public method g {args}
    public method get {args}
    public method hide {args}
    public method i {args}
    public method illum {obj}
    public method keep {args}
    public method kill {args}
    public method killall {args}
    public method killtree {args}
    public method l {args}
    public method label {obj}
    public method listeval {args}
    public method ls {args}
    public method lt {args}
    public method make_bb {name args}
    public method make_name {args}
    public method match {args}
    public method mv {args}
    public method mvall {args}
    public method nmg_collapse {args}
    public method nmg_simplify {args}
    public method opendb {args}
    public method overlay {args}
    public method pathlist {args}
    public method paths {args}
    public method prcolor {args}
    public method push {args}
    public method put {args}
    public method r {args}
    public method report {args}
    public method rm {args}
    public method rt_gettrees {args}
    public method shells {args}
    public method showmats {args}
    public method summary {args}
    public method title {args}
    public method tol {args}
    public method tops {args}
    public method tree {args}
    public method unhide {args}
    public method units {args}
    public method vdraw {args}
    public method whatid {args}
    public method whichair {args}
    public method whichid {args}
    public method who {args}
    public method xpush {args}
    public method zap {args}

    public method ? {}
    public method apropos {key}
    public method help {args}
    public method getUserCmds {}

    private variable db
    private variable dg
}

body Mged::constructor {file args} {
    set db [Database #auto $file]
    set dg [$db Drawable::get_dgname]
    addall $dg

    catch {eval itk_initialize $args}
}

body Mged::destructor {} {
    ::delete object $db
}

######################### Commands related to the Database #########################
body Mged::opendb {args} {
    eval $db open $args
}

body Mged::match {args} {
    eval $db match $args
}

body Mged::get {args} {
    eval $db get $args
}

body Mged::put {args} {
    eval $db put $args
}

body Mged::adjust {args} {
    eval $db adjust $args
}

body Mged::attr {args} {
    eval $db attr $args
}

body Mged::attr_rm {args} {
    eval $db attr_rm $args
}

body Mged::form {args} {
    eval $db form $args
}

body Mged::tops {args} {
    eval $db tops $args
}

body Mged::shells {args} {
    eval $db shells $args
}

body Mged::showmats {args} {
    eval $db showmats $args
}

body Mged::summary {args} {
    eval $db summary $args
}

body Mged::rt_gettrees {args} {
    eval $db rt_gettrees $args
}

body Mged::dump {args} {
    eval $db dump $args
}

body Mged::dbip {} {
    $db dbip
}

body Mged::l {args} {
    eval $db l $args
}

body Mged::listeval {args} {
    eval $db listeval $args
}

body Mged::ls {args} {
    eval $db ls $args
}

body Mged::lt {args} {
    eval $db lt $args
}

body Mged::pathlist {args} {
    eval $db pathlist $args
}

body Mged::paths {args} {
    eval $db paths $args
}

body Mged::expand {args} {
    eval $db expand $args
}

body Mged::kill {args} {
    eval $db kill $args
}

body Mged::killall {args} {
    eval $db killall $args
}

body Mged::killtree {args} {
    eval $db killtree $args
}

body Mged::cp {args} {
    eval $db cp $args
}

body Mged::mv {args} {
    eval $db mv $args
}

body Mged::mvall {args} {
    eval $db mvall $args
}

body Mged::nmg_collapse {args} {
    eval $db nmg_collapse $args
}

body Mged::nmg_simplify {args} {
    eval $db nmg_simplify $args
}

body Mged::copyeval {args} {
    eval $db copyeval $args
}

body Mged::concat {args} {
    eval $db concat $args
}

body Mged::dup {args} {
    eval $db dup $args
}

body Mged::g {args} {
    eval $db g $args
}

body Mged::rm {args} {
    eval $db rm $args
}

body Mged::c {args} {
    eval $db c $args
}

body Mged::comb {args} {
    eval $db comb $args
}

body Mged::find {args} {
    eval $db find $args
}

body Mged::whichair {args} {
    eval $db whichair $args
}

body Mged::whichid {args} {
    eval $db whichid $args
}

body Mged::title {args} {
    eval $db title $args
}

body Mged::tree {args} {
    eval $db tree $args
}

body Mged::unhide {args} {
    eval $db unhide $args
}

body Mged::color {args} {
    eval $db color $args
}

body Mged::prcolor {args} {
    eval $db prcolor $args
}

body Mged::tol {args} {
    eval $db tol $args
}

body Mged::push {args} {
    eval $db push $args
}

body Mged::whatid {args} {
    eval $db whatid $args
}

body Mged::keep {args} {
    eval $db keep $args
}

body Mged::cat {args} {
    eval $db cat $args
}

body Mged::hide {args} {
    eval $db hide $args
}

body Mged::i {args} {
    eval $db i $args
}

body Mged::make_bb {name args} {
    eval $db make_bb $name $args
}

body Mged::make_name {args} {
    eval $db make_name $args
}

body Mged::units {args} {
    eval $db units $args
}

body Mged::draw {args} {
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

body Mged::E {args} {
    eval $db E $args
}

body Mged::erase {args} {
    eval $db erase $args
}

body Mged::who {} {
    $db who
}

body Mged::xpush {args} {
    eval $db xpush $args
}

body Mged::zap {args} {
    $db zap $args
}

body Mged::blast {args} {
    eval $db blast $args
}

body Mged::clear {} {
    $db clear
}

body Mged::ev {args} {
    eval $db ev $args
}

body Mged::erase_all {args} {
    eval $db erase_all $args
}

body Mged::overlay {args} {
    eval $db overlay $args
}

body Mged::vdraw {args} {
    eval $db vdraw $args
}

body Mged::illum {args} {
    eval $db illum $args
}

body Mged::label {args} {
    eval $db label $args
}

body Mged::r {args} {
    eval $db r $args
}

body Mged::report {args} {
    eval $db report $args
}

body Mged::? {} {
    return "[QuadDisplay::?]\n[$db ?]"
}

body Mged::apropos {key} {
    return "[QuadDisplay::apropos $key] [$db apropos $key]"
}

body Mged::help {args} {
    if {[llength $args] && [lindex $args 0] != {}} {
	if {[catch {eval QuadDisplay::help $args} result]} {
	    set result [eval $db help $args]
	}

	return $result
    }

    # list all help messages for QuadDisplay and Db
    return "[QuadDisplay::help][$db help]"
}

body Mged::getUserCmds {} {
    return "[QuadDisplay::getUserCmds] [$db getUserCmds]"
}
