##                 D B . T C L
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
#	The Db class wraps LIBRT's database object.
#
class Db {
    protected variable db ""
    public variable dbfile ""

    constructor {filename} {}
    destructor {}

    public method adjust {args}
    public method c {args}
    public method cat {args}
    public method color {args}
    public method comb {args}
    public method concat {args}
    public method cp {args}
    public method dbip {}
    public method dump {args}
    public method dup {args}
    public method expand {args}
    public method find {args}
    public method form {args}
    public method g {args}
    public method get {args}
    public method get_dbname {}
    public method i {args}
    public method keep {args}
    public method kill {args}
    public method killall {args}
    public method killtree {args}
    public method l {args}
    public method listeval {args}
    public method ls {args}
    public method match {args}
    public method mv {args}
    public method mvall {args}
    public method open {args}
    public method observer {args}
    public method paths {args}
    public method prcolor {args}
    public method push {args}
    public method put {args}
    public method r {args}
    public method rm {args}
    public method rt_gettrees {args}
    public method title {args}
    public method tol {args}
    public method tops {args}
    public method tree {args}
    public method whatid {args}
    public method whichair {args}
    public method whichid {args}
}

configbody Db::dbfile {
    Db::open $dbfile
}

body Db::constructor {filename} {
    set dbfile $filename
    set db [subst $this]_db
    wdb_open $db db $dbfile
}

body Db::destructor {} {
    $db close
}

body Db::open {args} {
    set dbfile [eval $db open $args]
}

body Db::observer {args} {
    eval $db observer $args
}

body Db::match {args} {
    eval $db match $args
}

body Db::get {args} {
    eval $db get $args
}

body Db::put {args} {
    eval $db put $args
}

body Db::adjust {args} {
    eval $db adjust $args
}

body Db::form {args} {
    eval $db form $args
}

body Db::tops {args} {
    eval $db tops $args
}

body Db::rt_gettrees {args} {
    eval $db rt_gettrees $args
}

body Db::dump {args} {
    eval $db dump $args
}

body Db::dbip {} {
    eval $db dbip
}

body Db::ls {args} {
    eval $db ls $args
}

body Db::l {args} {
    eval $db l $args
}

body Db::listeval {args} {
    eval $db listeval $args
}

body Db::paths {args} {
    eval $db paths $args
}

body Db::expand {args} {
    eval $db expand $args
}

body Db::kill {args} {
    eval $db kill $args
}

body Db::killall {args} {
    eval $db killall $args
}

body Db::killtree {args} {
    eval $db killtree $args
}

body Db::cp {args} {
    eval $db cp $args
}

body Db::mv {args} {
    eval $db mvall $args
}

body Db::mvall {args} {
    eval $db mvall $args
}

body Db::concat {args} {
    eval $db concat $args
}

body Db::dup {args} {
    eval $db dup $args
}

body Db::g {args} {
    eval $db g $args
}

body Db::rm {args} {
    eval $db rm $args
}

body Db::r {args} {
    eval $db r $args
}

body Db::c {args} {
    eval $db c $args
}

body Db::comb {args} {
    eval $db comb $args
}

body Db::find {args} {
    eval $db find $args
}

body Db::whichair {args} {
    eval $db whichair $args
}

body Db::whichid {args} {
    eval $db whichid $args
}

body Db::title {args} {
    eval $db title $args
}

body Db::tree {args} {
    eval $db tree $args
}

body Db::color {args} {
    eval $db color $args
}

body Db::prcolor {args} {
    eval $db prcolor $args
}

body Db::tol {args} {
    eval $db tol $args
}

body Db::push {args} {
    eval $db push $args
}

body Db::whatid {args} {
    eval $db whatid $args
}

body Db::keep {args} {
    eval $db keep $args
}

body Db::cat {args} {
    eval $db cat $args
}

body Db::i {args} {
    eval $db i $args
}

body Db::get_dbname {} {
    return $db
}
