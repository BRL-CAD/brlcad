##                 D A T A B A S E . T C L
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
#	The Database class is comprised of a database object and a
#       drawable geometry object. When a database object is opened
#       a drawable geometry object is created and associated with it.
#
class Database {
    protected variable db ""
    protected variable dg ""
    public variable name ""

    constructor {args} {
	set db [subst $this]_db
	set dg [subst $this]_dg

	# process options
	eval configure $args
    }

    destructor {
	# close the current drawable geometry object
	if {[info commands $dg] != ""} {
	    $dg close
	}

	# close the current database
	if {[info commands $db] != ""} {
	    $db close
	}
    }

    # database methods
    method open {args}
    method match {args}
    method get {args}
    method put {args}
    method adjust {args}
    method form {args}
    method tops {args}
    method rt_gettrees {args}
    method dump {args}
    method dbip {args}
    method ls {args}
    method l {args}
    method listeval {args}
    method paths {args}
    method expand {args}
    method kill {args}
    method killall {args}
    method killtree {args}
    method cp {args}
    method mv {args}
    method mvall {args}
    method concat {args}
    method dup {args}
    method g {args}
    method rm {args}
    method r {args}
    method c {args}
    method comb {args}
    method find {args}
    method whichair {args}
    method whichid {args}
    method title {args}
    method tree {args}
    method color {args}
    method prcolor {args}
    method tol {args}
    method push {args}
    method whatid {args}
    method keep {args}
    method cat {args}
    method i {args}

    # drawable geometry methods
    method draw {args}
    method erase {args}
    method zap {}
    method who {args}

    method blast {args}
    method clear {}
    method ev {args}
    method erase_all {args}
    method overlay {args}
    method rt {args}
    method rtcheck {args}
    method vdraw {args}
}

configbody Database::name {
    open $name
}

body Database::draw {args} {
    eval $dg draw $args
}

body Database::erase {args} {
    eval $dg erase $args
}

body Database::zap {} {
    $dg zap
}

body Database::who {args} {
    eval $dg who $args
}

body Database::blast {args} {
    eval $dg blast $args
}

body Database::clear {} {
    eval $dg clear
}

body Database::ev {args} {
    eval $dg ev $args
}

body Database::erase_all {args} {
    eval $dg erase_all $args
}

body Database::overlay {args} {
    eval $dg overlay $args
}

body Database::rt {args} {
    eval $dg rt $args
}

body Database::rtcheck {args} {
    eval $dg rtcheck $args
}

body Database::vdraw {args} {
    eval $dg vdraw $args
}

body Database::open {args} {
    set len [llength $args]

    if {$len > 1} {
	return -code error "open:: too many arguments"
    }

    # get the current database name
    if {$len == 0} {
	return $name
    }

    # close the current drawable geometry object
    if {[info commands $dg] != ""} {
	$dg close
    }

    # close the current database
    if {[info commands $db] != ""} {
	$db close
    }

    # open a database
    set result [catch {wdb_open $db db $args} msg]
    dg_open $dg $db

    if {$result} {
	return -code $result $msg
    }

    set name $args
    return $name
}

body Database::match {args} {
    eval $db match $args
}

body Database::get {args} {
    eval $db get $args
}

body Database::put {args} {
    eval $db put $args
}

body Database::adjust {args} {
    eval $db adjust $args
}

body Database::form {args} {
    eval $db form $args
}

body Database::tops {args} {
    eval $db tops $args
}

body Database::rt_gettrees {args} {
    eval $db rt_gettrees $args
}

body Database::dump {args} {
    eval $db dump $args
}

body Database::dbip {} {
    eval $db dbip
}

body Database::ls {args} {
    eval $db ls $args
}

body Database::l {args} {
    eval $db l $args
}

body Database::listeval {args} {
    eval $db listeval $args
}

body Database::paths {args} {
    eval $db paths $args
}

body Database::expand {args} {
    eval $db expand $args
}

body Database::kill {args} {
    eval $db kill $args
}

body Database::killall {args} {
    eval $db killall $args
}

body Database::killtree {args} {
    eval $db killtree $args
}

body Database::cp {args} {
    eval $db cp $args
}

body Database::mv {args} {
    eval $db mvall $args
}

body Database::mvall {args} {
    eval $db mvall $args
}

body Database::concat {args} {
    eval $db concat $args
}

body Database::dup {args} {
    eval $db dup $args
}

body Database::g {args} {
    eval $db g $args
}

body Database::rm {args} {
    eval $db rm $args
}

body Database::r {args} {
    eval $db r $args
}

body Database::c {args} {
    eval $db c $args
}

body Database::comb {args} {
    eval $db comb $args
}

body Database::find {args} {
    eval $db find $args
}

body Database::whichair {args} {
    eval $db whichair $args
}

body Database::whichid {args} {
    eval $db whichid $args
}

body Database::title {args} {
    eval $db title $args
}

body Database::tree {args} {
    eval $db tree $args
}

body Database::color {args} {
    eval $db color $args
}

body Database::prcolor {args} {
    eval $db prcolor $args
}

body Database::tol {args} {
    eval $db tol $args
}

body Database::push {args} {
    eval $db push $args
}

body Database::whatid {args} {
    eval $db whatid $args
}

body Database::keep {args} {
    eval $db keep $args
}

body Database::cat {args} {
    eval $db cat $args
}

body Database::i {args} {
    eval $db i $args
}
