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
class Mged {
    inherit QuadDisplay

    constructor {file {type X} args} {
	eval QuadDisplay::constructor $type
    } {}
    destructor {}

    ######################### Commands related to the Database #########################
    public method opendb {args}
    public method match {args}
    public method get {args}
    public method put {args}
    public method adjust {args}
    public method form {args}
    public method tops {args}
    public method rt_gettrees {args}
    public method dump {args}
    public method dbip {}
    public method ls {args}
    public method l {args}
    public method listeval {args}
    public method paths {args}
    public method expand {args}
    public method kill {args}
    public method killall {args}
    public method killtree {args}
    public method cp {args}
    public method mv {args}
    public method mvall {args}
    public method concat {args}
    public method dup {args}
    public method g {args}
    public method rm {args}
    public method r {args}
    public method c {args}
    public method comb {args}
    public method find {args}
    public method whichair {args}
    public method whichid {args}
    public method title {args}
    public method tree {args}
    public method color {args}
    public method prcolor {args}
    public method tol {args}
    public method push {args}
    public method whatid {args}
    public method keep {args}
    public method cat {args}
    public method i {args}
    public method draw {args}
    public method erase {args}
    public method zap {}
    public method who {args}
    public method blast {args}
    public method clear {}
    public method ev {args}
    public method erase_all {args}
    public method overlay {args}
    public method vdraw {args}
    public method illum {obj}
    public method label {obj}

    private variable db ""
    private variable dg ""
}

body Mged::constructor {file {type X} args} {
    set db [Database #auto $file]
    set dg [$db Drawable::get_name]
    addall $dg
    eval itk_initialize $args
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

body Mged::form {args} {
    eval $db form $args
}

body Mged::tops {args} {
    eval $db tops $args
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

body Mged::ls {args} {
    eval $db ls $args
}

body Mged::l {args} {
    eval $db l $args
}

body Mged::listeval {args} {
    eval $db listeval $args
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

body Mged::i {args} {
    eval $db i $args
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

body Mged::erase {args} {
    eval $db erase $args
}

body Mged::zap {} {
    $db zap
}

body Mged::who {} {
    $db who
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
