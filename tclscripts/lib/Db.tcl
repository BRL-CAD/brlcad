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
#       This software is Copyright (C) 1998-2004 by the United States Army
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
    public method attr {args}
    public method binary {args}
    public method c {args}
    public method cat {args}
    public method color {args}
    public method comb {args}
    public method concat {args}
    public method copyeval {args}
    public method cp {args}
    public method dbip {args}
    public method dump {args}
    public method dup {args}
    public method expand {args}
    public method find {args}
    public method form {args}
    public method g {args}
    public method get {args}
    public method get_dbname {}
    public method hide {args}
    public method i {args}
    public method keep {args}
    public method kill {args}
    public method killall {args}
    public method killtree {args}
    public method l {args}
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
    public method observer {args}
    public method open {args}
    public method pathlist {args}
    public method paths {args}
    public method prcolor {args}
    public method push {args}
    public method put {args}
    public method r {args}
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
    public method version {args}
    public method whatid {args}
    public method whichair {args}
    public method whichid {args}
    public method xpush {args}

    public method ? {}
    public method apropos {key}
    public method help {args}
    public method getUserCmds {}

    private method help_init {}

    private variable help
}

configbody Db::dbfile {
    Db::open $dbfile
}

body Db::constructor {filename} {
    set dbfile $filename
    set db [subst $this]_db
    wdb_open $db $dbfile
    Db::help_init
}

body Db::destructor {} {
    rename $db ""
    catch {delete object $help}
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

body Db::rt_gettrees {args} {
    eval $db rt_gettrees $args
}

body Db::shells {args} {
    eval $db shells $args
}

body Db::showmats {args} {
    eval $db showmats $args
}

body Db::summary {args} {
    eval $db summary $args
}

body Db::tops {args} {
    eval $db tops $args
}

body Db::dump {args} {
    eval $db dump $args
}

body Db::dbip {args} {
    eval $db dbip $args
}

body Db::l {args} {
    eval $db l $args
}

body Db::listeval {args} {
    eval $db listeval $args
}

body Db::ls {args} {
    eval $db ls $args
}

body Db::lt {args} {
    eval $db lt $args
}

body Db::pathlist {args} {
    eval $db pathlist $args
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
    eval $db mv $args
}

body Db::mvall {args} {
    eval $db mvall $args
}

body Db::nmg_collapse {args} {
    eval $db nmg_collapse $args
}

body Db::nmg_simplify {args} {
    eval $db nmg_simplify $args
}

body Db::concat {args} {
    eval $db concat $args
}

body Db::copyeval {args} {
    eval $db copyeval $args
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

body Db::xpush {args} {
    eval $db xpush $args
}

body Db::title {args} {
    eval $db title $args
}

body Db::tree {args} {
    eval $db tree $args
}

body Db::unhide {args} {
    eval $db unhide $args
}

body Db::units {args} {
    eval $db units $args
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

body Db::hide {args} {
    eval $db hide $args
}

body Db::i {args} {
    eval $db i $args
}

body Db::get_dbname {} {
    return $db
}

body Db::make_bb {name args} {
    eval $db make_bb $name $args
}

body Db::make_name {args} {
    eval $db make_name $args
}

body Db::attr {args} {
    eval $db attr $args
}

body Db::version {args} {
    eval $db version $args
}

body Db::binary {args} {
    eval $db binary $args
}

body Db::help {args} {
    return [eval $help get $args]
}

body Db::? {} {
    return [$help ? 20 4]
}

body Db::apropos {key} {
    return [$help apropos $key]
}

body Db::getUserCmds {} {
    return [$help getCmds]
}

body Db::help_init {} {
    set help [cadwidgets::Help #auto]

    $help add adjust	{{} {adjust database object parameters}}
    $help add attr      {{ {set|get|rm|append} object [args]}
	      {set, get, remove or append to attribute values for the specified object.
         for the "set" subcommand, the arguments are attribute name/value pairs
         for the "get" subcommand, the arguments are attribute names
         for the "rm" subcommand, the arguments are attribute names
         for the "append" subcommand, the arguments are attribute name/value pairs } }

    $help add c		{{[-gr] comb_name [boolean_expr]} {create or extend a combination using standard notation}}
    $help add cat	{{<objects>} {list attributes (brief)}}
    $help add color	{{low high r g b str} {make color entry}}
    $help add comb	{{comb_name <operation solid>} {create or extend combination w/booleans}}
    $help add concat	{{file [prefix]} {concatenate 'file' onto end of present database.  Run 'dup file' first.}}
    $help add copyeval	{{new_solid path_to_old_solid}	{copy an 'evaluated' path solid}}
    $help add cp	{{from to} {copy [duplicate] object}}
    $help add dbip	{{} {get dbip}}
    $help add dump	{{file} {write current state of database object to file}}
    $help add dup	{{file [prefix]} {check for dup names in 'file'}}
    $help add expand	{{expression} {globs expression against database objects}}
    $help add find	{{[-s] <objects>} {find all references to objects}}
    $help add form	{{objType} {returns form of objType}}
    $help add g		{{groupname <objects>} {group objects}}
    $help add get	{{obj ?attr?} {get obj attributes}}
    $help add hide	{{[objects]} {set the "hidden" flag for the specified objects so they do not appear in a "t" or "ls" command output}}
    $help add i		{{obj combination [operation]} {add instance of obj to comb}}
    $help add keep	{{keep_file object(s)} {save named objects in specified file}}
    $help add kill	{{[-f] <objects>} {delete object[s] from file}}
    $help add killall	{{<objects>} {kill object[s] and all references}}
    $help add killtree	{{<object>} {kill complete tree[s] - BE CAREFUL}}
    $help add l		{{[-r] <object(s)>} {list attributes (verbose). Objects may be paths}}
    $help add listeval	{{} {lists 'evaluated' path solids}}
    $help add ls	{{[-a -c -r -s]} {table of contents}}
    $help add lt	{{object} {return first level tree as list of operator/member pairs}}
    $help add match	{{exp} {returns all database objects matching the given expression}}
    $help add mv	{{old new} {rename object}}
    $help add mvall	{{old new} {rename object everywhere}}
    $help add nmg_collapse    {{nmg_solid new_solid maximum_error_distance [minimum_allowed_angle]}	{decimate NMG solid via edge collapse}}
    $help add nmg_simplify    {{[arb|tgc|ell|poly] new_solid nmg_solid}	{simplify nmg_solid, if possible}}
    $help add open	{{?dbfile?} {open a database}}
    $help add pathlist	{{name(s)}	{list all paths from name(s) to leaves}}
    $help add paths	{{pattern} {lists all paths matching input path}}
    $help add prcolor	{{} {print color&material table}}
    $help add push	{{object[s]} {pushes object's path transformations to solids}}
    $help add put	{{object data} {creates an object}}
    $help add r		{{region <operation solid>} {create or extend a Region combination}}
    $help add rm	{{comb <members>} {remove members from comb}}
    $help add rt_gettrees      {{} {}}
    $help add shells	{{nmg_model}	{breaks model into seperate shells}}
    $help add showmats	{{path}	{show xform matrices along path}}
    $help add summary	{{[s r g]}	{count/list solid/reg/groups}}
    $help add title	{{?string?} {print or change the title}}
    $help add tol	{{[abs #] [rel #] [norm #] [dist #] [perp #]} {show/set tessellation and calculation tolerances}}
    $help add tops	{{} {find all top level objects}}
    $help add tree	{{[-c] [-i n] [-o outfile] object(s)} {print out a tree of all members of an object}}
    $help add unhide	{{[objects]} {unset the "hidden" flag for the specified objects so they will appear in a "t" or "ls" command output}}
    $help add units	{{[mm|cm|m|in|ft|...]}	{change units}}
    $help add version	{{} {return the database version}}
    $help add whatid	{{region_name} {display ident number for region}}
    $help add whichair	{{air_codes(s)} {lists all regions with given air code}}
    $help add whichid	{{[-s] ident(s)} {lists all regions with given ident code}}
    $help add xpush	{{object} {Experimental Push Command}}
}
