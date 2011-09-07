#                          D B . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
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

#####################################################################
# DEPRECATED: This widget is deprecated and should no longer be used.
# Use the Ged widget instead.
#####################################################################

#
# Description -
#	The Db class wraps LIBRT's database object.
#
::itcl::class Db {
    constructor {dbOrFile} {}
    destructor {}

    public {
	variable dbfile ""

	method adjust {args}
	method arced {args}
	method attr {args}
	method binary {args}
	method c {args}
	method cat {args}
	method color {args}
	method comb {args}
	method comb_color {args}
	method concat {args}
	method copyeval {args}
	method cp {args}
	method dbip {args}
	method dump {args}
	method dup {args}
	method edcomb {args}
	method edit {args}
	method edmater {args}
	method expand {args}
	method facetize {args}
	method find {args}
	method form {args}
	method g {args}
	method get {args}
	method get_type {args}
	method get_dbname {}
	method hide {args}
	method i {args}
	method importFg4Section {args}
	method item {args}
	method keep {args}
	method kill {args}
	method killall {args}
	method killtree {args}
	method l {args}
	method listeval {args}
	method log {args}
	method ls {args}
	method lt {args}
	method make {args}
	method make_bb {name args}
	method make_name {args}
	method match {args}
	method mater {args}
	method mirror {args}
	method move_arb_edge {args}
	method move_arb_face {args}
	method mv {args}
	method mvall {args}
	method nmg_collapse {args}
	method nmg_simplify {args}
	method observer {args}
	method ocenter {args}
	method orotate {args}
	method oscale {args}
	method otranslate {args}
	method open {args}
	method pathlist {args}
	method paths {args}
	method prcolor {args}
	method push {args}
	method put {args}
	method r {args}
	method rm {args}
	method rmap {args}
	method rmater {args}
	method rotate_arb_face {args}
	method rt_gettrees {args}
	method shader {args}
	method shareDb {_db}
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
	method version {args}
	method whatid {args}
	method whichair {args}
	method whichid {args}
	method wmater {args}
	method xpush {args}

	method ? {}
	method apropos {key}
	method help {args}
	method getUserCmds {}
    }

    protected {
	variable db ""
	variable sharedDb 0
    }

    private {
	variable help

	method help_init {}
    }
}

::itcl::configbody Db::dbfile {
    Db::open $dbfile
}

::itcl::body Db::constructor {dbOrFile} {

    puts "DEPRECATION WARNING: The Db widget should no longer be used.  Use the Ged widget instead."

    if {[catch {$dbOrFile ls}]} {
	set dbfile $dbOrFile
	set db [subst $this]_db
	wdb_open $db $dbfile
    } else {
	set dbfile ""
	set db $dbOrFile
	set sharedDb 1
    }

    Db::help_init
}

::itcl::body Db::destructor {} {
    if {!$sharedDb} {
	rename $db ""
    }

    catch {delete object $help}
}

::itcl::body Db::open {args} {
    set dbfile [eval $db open $args]
}

::itcl::body Db::observer {args} {
    eval $db observer $args
}

::itcl::body Db::ocenter {args} {
    eval $db ocenter $args
}

::itcl::body Db::orotate {args} {
    eval $db orotate $args
}

::itcl::body Db::oscale {args} {
    eval $db oscale $args
}

::itcl::body Db::otranslate {args} {
    eval $db otranslate $args
}

::itcl::body Db::match {args} {
    eval $db match $args
}

::itcl::body Db::get {args} {
    eval $db get $args
}

::itcl::body Db::get_type {args} {
    eval $db get_type $args
}

::itcl::body Db::put {args} {
    eval $db put $args
}

::itcl::body Db::adjust {args} {
    eval $db adjust $args
}

::itcl::body Db::form {args} {
    eval $db form $args
}

::itcl::body Db::rt_gettrees {args} {
    eval $db rt_gettrees $args
}

::itcl::body Db::shader {args} {
    eval $db shader $args
}

::itcl::body Db::shareDb {_db} {
    if {!$sharedDb} {
	rename $db ""
    }

    set db $_db
    set sharedDb 1
}

::itcl::body Db::shells {args} {
    eval $db shells $args
}

::itcl::body Db::showmats {args} {
    eval $db showmats $args
}

::itcl::body Db::summary {args} {
    eval $db summary $args
}

::itcl::body Db::tops {args} {
    eval $db tops $args
}

::itcl::body Db::dump {args} {
    eval $db dump $args
}

::itcl::body Db::dbip {args} {
    eval $db dbip $args
}

::itcl::body Db::l {args} {
    eval $db l $args
}

::itcl::body Db::listeval {args} {
    eval $db listeval $args
}

::itcl::body Db::log {args} {
    eval $db log $args
}

::itcl::body Db::ls {args} {
    eval $db ls $args
}

::itcl::body Db::lt {args} {
    eval $db lt $args
}

::itcl::body Db::pathlist {args} {
    eval $db pathlist $args
}

::itcl::body Db::paths {args} {
    eval $db paths $args
}

::itcl::body Db::expand {args} {
    eval $db expand $args
}

::itcl::body Db::facetize {args} {
    eval $db facetize $args
}

::itcl::body Db::kill {args} {
    eval $db kill $args
}

::itcl::body Db::killall {args} {
    eval $db killall $args
}

::itcl::body Db::killtree {args} {
    eval $db killtree $args
}

::itcl::body Db::cp {args} {
    eval $db cp $args
}

::itcl::body Db::nmg_collapse {args} {
    eval $db nmg_collapse $args
}

::itcl::body Db::nmg_simplify {args} {
    eval $db nmg_simplify $args
}

::itcl::body Db::concat {args} {
    eval $db concat $args
}

::itcl::body Db::copyeval {args} {
    eval $db copyeval $args
}

::itcl::body Db::dup {args} {
    eval $db dup $args
}

::itcl::body Db::edcomb {args} {
    eval $db edcomb $args
}

::itcl::body Db::edit {args} {
    eval $db edit $args
}

::itcl::body Db::edmater {args} {
    eval $db edmater $args
}

::itcl::body Db::g {args} {
    eval $db g $args
}

::itcl::body Db::rm {args} {
    eval $db rm $args
}

::itcl::body Db::rmap {args} {
    eval $db rmap $args
}

::itcl::body Db::rmater {args} {
    eval $db rmater $args
}

::itcl::body Db::rotate_arb_face {args} {
    eval $db rotate_arb_face $args
}

::itcl::body Db::r {args} {
    eval $db r $args
}

::itcl::body Db::c {args} {
    eval $db c $args
}

::itcl::body Db::comb {args} {
    eval $db comb $args
}

::itcl::body Db::comb_color {args} {
    eval $db comb_color $args
}

::itcl::body Db::find {args} {
    eval $db find $args
}

::itcl::body Db::whichair {args} {
    eval $db whichair $args
}

::itcl::body Db::whichid {args} {
    eval $db whichid $args
}

::itcl::body Db::wmater {args} {
    eval $db wmater $args
}

::itcl::body Db::xpush {args} {
    eval $db xpush $args
}

::itcl::body Db::title {args} {
    eval $db title $args
}

::itcl::body Db::track {args} {
    eval $db track $args
}

::itcl::body Db::tree {args} {
    eval $db tree $args
}

::itcl::body Db::unhide {args} {
    eval $db unhide $args
}

::itcl::body Db::units {args} {
    eval $db units $args
}

::itcl::body Db::color {args} {
    eval $db color $args
}

::itcl::body Db::prcolor {args} {
    eval $db prcolor $args
}

::itcl::body Db::tol {args} {
    eval $db tol $args
}

::itcl::body Db::push {args} {
    eval $db push $args
}

::itcl::body Db::whatid {args} {
    eval $db whatid $args
}

::itcl::body Db::keep {args} {
    eval $db keep $args
}

::itcl::body Db::cat {args} {
    eval $db cat $args
}

::itcl::body Db::hide {args} {
    eval $db hide $args
}

::itcl::body Db::i {args} {
    eval $db i $args
}

::itcl::body Db::importFg4Section {args} {
    eval $db importFg4Section $args
}

::itcl::body Db::item {args} {
    eval $db item $args
}

::itcl::body Db::get_dbname {} {
    return $db
}

::itcl::body Db::make {args} {
    eval $db make $args
}

::itcl::body Db::make_bb {name args} {
    eval $db make_bb $name $args
}

::itcl::body Db::make_name {args} {
    eval $db make_name $args
}

::itcl::body Db::mater {args} {
    eval $db mater $args
}

::itcl::body Db::mirror {args} {
    eval $db mirror $args
}

::itcl::body Db::move_arb_edge {args} {
    eval $db move_arb_edge $args
}

::itcl::body Db::move_arb_face {args} {
    eval $db move_arb_face $args
}

::itcl::body Db::mv {args} {
    eval $db mv $args
}

::itcl::body Db::mvall {args} {
    eval $db mvall $args
}

::itcl::body Db::arced {args} {
    eval $db arced $args
}

::itcl::body Db::attr {args} {
    eval $db attr $args
}

::itcl::body Db::version {args} {
    eval $db version $args
}

::itcl::body Db::binary {args} {
    eval $db binary $args
}

::itcl::body Db::help {args} {
    return [eval $help get $args]
}

::itcl::body Db::? {} {
    return [$help ? 20 4]
}

::itcl::body Db::apropos {key} {
    return [$help apropos $key]
}

::itcl::body Db::getUserCmds {} {
    return [$help getCmds]
}

::itcl::body Db::help_init {} {
    set help [cadwidgets::Help \#auto]

    $help add adjust	{{} {adjust database object parameters}}
    $help add arced     {{a/b anim_cmd ...} {edit the matrix, etc., along an arc}}
    $help add attr      {{ {set|get|rm|append} object [args]}
	{set, get, remove or append to attribute values for the specified object.
	    for the "set" subcommand, the arguments are attribute name/value pairs
	    for the "get" subcommand, the arguments are attribute names
	    for the "rm" subcommand, the arguments are attribute names
	    for the "append" subcommand, the arguments are attribute name/value pairs } }
    $help add binary	{{(-i|-o) major_type minor_type dest source}
	{manipulate opaque objects.
	    Must specify one of -i (for creating or adjusting objects (input))
	    or -o for extracting objects (output).
	    If the major type is "u" the minor type must be one of:
	    "f" -> float
	    "d" -> double
	    "c" -> char (8 bit)
	    "s" -> short (16 bit)
	    "i" -> int (32 bit)
	    "l" -> long (64 bit)
	    "C" -> unsigned char (8 bit)
	    "S" -> unsigned short (16 bit)
	    "I" -> unsigned int (32 bit)
	    "L" -> unsigned long (64 bit)
	    For input, source is a file name and dest is an object name.
	    For output source is an object name and dest is a file name.
	    Only uniform array binary objects (major_type=u) are currently supported}}
    $help add c		{{[-gr] comb_name <boolean_expr>} {create or extend a combination using standard notation}}
    $help add cat	{{<objects>} {list attributes (brief)}}
    $help add color	{{low high r g b str} {make color entry}}
    $help add comb	{{comb_name <operation solid>} {create or extend combination w/booleans}}
    $help add comb_color {{comb R G B} {set combination's color}}
    $help add concat	{{[-t] [-u] [-c] [-s|-p] file [prefix]} {concatenate 'file' onto end of present database.  Run 'dup file' first.}}
    $help add copyeval	{{new_solid path_to_old_solid}	{copy an 'evaluated' path solid}}
    $help add cp	{{from to} {copy [duplicate] object}}
    $help add dbip	{{} {get dbip}}
    $help add dump	{{file} {write current state of database object to file}}
    $help add dup	{{file [prefix]} {check for dup names in 'file'}}
    $help add edcomb	{{comb rflag rid air los mid} {modify combination record information}}
    $help add edit      {{[help] subcmd args} {edit objects via subcommands}}
    $help add edmater	{{comb1 [comb2 ...]} {edit combination materials}}
    $help add expand	{{expression} {globs expression against database objects}}
    $help add find	{{[-s] <objects>} {find all references to objects}}
    $help add form	{{objType} {returns form of objType}}
    $help add g		{{groupname <objects>} {group objects}}
    $help add get	{{obj ?attr?} {get obj attributes}}
    $help add hide	{{[objects]} {set the "hidden" flag for the specified objects so they do not appear in a "t" or "ls" command output}}
    $help add i		{{obj combination [operation]} {add instance of obj to comb}}
    $help add item	{{region ident [air [material [los]]]} {set region ident codes}}
    $help add keep	{{keep_file object(s)} {save named objects in specified file}}
    $help add kill	{{[-f] <objects>} {delete object[s] from file}}
    $help add killall	{{<objects>} {kill object[s] and all references}}
    $help add killtree	{{<object>} {kill complete tree[s] - BE CAREFUL}}
    $help add l		{{[-r] <object(s)>} {list attributes (verbose). Objects may be paths}}
    $help add listeval	{{} {lists 'evaluated' path solids}}
    $help add log	{{get|start|stop} {used to control logging}}
    $help add ls	{{[-a -c -r -s]} {table of contents}}
    $help add lt	{{object} {return first level tree as list of operator/member pairs}}
    $help add make	{{-t | object type} {make an object/primitive of the specified type}}
    $help add make_bb	{{bbname object(s)} {make a bounding box (rpp) around the specified objects}}
    $help add match	{{exp} {returns all database objects matching the given expression}}
    $help add mater	{{region shader R G B inherit} {modify region's material information}}
    $help add mirror	{{[-p point] [-d dir] [-x] [-y] [-z] [-o offset] old new}	{mirror object along the specified axis}}
    $help add move_arb_edge	{{arb edge pt} {move an arb's edge through pt}}
    $help add move_arb_face	{{arb face pt} {move an arb's face through pt}}
    $help add mv	{{old new} {rename object}}
    $help add mvall	{{old new} {rename object everywhere}}
    $help add nmg_collapse    {{nmg_solid new_solid maximum_error_distance [minimum_allowed_angle]}	{decimate NMG solid via edge collapse}}
    $help add nmg_simplify    {{[arb|tgc|ell|poly] new_solid nmg_solid}	{simplify nmg_solid, if possible}}
    $help add open	{{?dbfile?} {open a database}}
    $help add ocenter 	{{obj [x y z]} {get/set center for obj}}
    $help add ocrotate	{{obj x y z} {rotate object about its center by x, y, z degrees}}
    $help add orotate	{{x y z} {rotate object}}
    $help add oscale	{{sf} {scale object}}
    $help add otranslate {{x y z} {translate object}}
    $help add pathlist	{{name(s)}	{list all paths from name(s) to leaves}}
    $help add paths	{{pattern} {lists all paths matching input path}}
    $help add prcolor	{{} {print color&material table}}
    $help add push	{{object[s]} {pushes object's path transformations to solids}}
    $help add put	{{object data} {creates an object}}
    $help add r		{{region <operation solid>} {create or extend a Region combination}}
    $help add rm	{{comb <members>} {remove members from comb}}
    $help add rmap	{{} {returns a region ids to region(s) mapping}}
    $help add rmater	{{file} {read material properties from a file}}
    $help add rt_gettrees      {{} {}}
    $help add rotate_arb_face	{{arb face pt} {rotate an arb's face through pt}}
    $help add shader	{{comb shader_material [shader_args]} {command line version of the mater command}}
    $help add shells	{{nmg_model}	{breaks model into seperate shells}}
    $help add showmats	{{path}	{show xform matrices along path}}
    $help add summary	{{[s r g]}	{count/list solid/reg/groups}}
    $help add title	{{?string?} {print or change the title}}
    $help add tol	{{"[abs #] [rel #] [norm #] [dist #] [perp #]"} {show/set tessellation and calculation tolerances}}
    $help add tops	{{} {find all top level objects}}
    $help add track	{{args} {create a track}}
    $help add tree	{{[-c] [-i n] [-d n] [-o outfile] object(s)} {print out a tree of all members of an object, or all members to depth n in the tree if n -d option is supplied}}
    $help add unhide	{{[objects]} {unset the "hidden" flag for the specified objects so they will appear in a "t" or "ls" command output}}
    $help add units	{{[mm|cm|m|in|ft|...]}	{change units}}
    $help add version	{{} {return the database version}}
    $help add whatid	{{region_name} {display ident number for region}}
    $help add whichair	{{air_codes(s)} {lists all regions with given air code}}
    $help add whichid	{{[-s] ident(s)} {lists all regions with given ident code}}
    $help add wmater	{{file comb1 [comb2 ...]} {write material properties to a file for the specified combinations}}
    $help add xpush	{{object} {Experimental Push Command}}
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
