#                        H E L P . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
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
#==============================================================================
#
# TCL versions of MGED "help", "?", and "apropos" commands
#
#==============================================================================

# This command causes helplib.tcl to get read in.
helplib

set mged_help_data(?)		{{}	{summary of available mged commands}}
set mged_help_data(?lib)	{{}	{summary of available library commands}}
set mged_help_data(?devel)	{{}	{summary of available mged developer commands}}
set mged_help_data(%)		{{}	{Escape to an interactive shell. Note - This only works in a
	command window associated with a tty (i.e. the window used
	to start MGED in classic mode). }}
set mged_help_data(3ptarb)	{{}	{makes arb given 3 pts, 2 coord of 4th pt, and thickness}}
set mged_help_data(adc)		{{[<a1|a2|dst|dh|dv|hv|dx|dy|dz|xyz|reset|help> [value(s)]]}	{control the angle/distance cursor}}
set mged_help_data(adjust)	$helplib_data(wdb_adjust)
set mged_help_data(ae)		$helplib_data(vo_aet)
set mged_help_data(analyze)	{{[arbname]}	{analyze faces of ARB}}
set mged_help_data(animmate)	{{[parent]}	{tool for building animation scripts}}
set mged_help_data(apropos)	{{keyword}	{finds commands whose descriptions contain the given keyword}}
set mged_help_data(aproposlib)	{{keyword}	{finds library commands whose descriptions contain the given keyword}}
set mged_help_data(aproposdevel)	{{keyword}	{finds commands used for development whose descriptions
	contain the given keyword}}
set mged_help_data(arb)		{{name rot fb}	{make arb8, rotation + fallback}}
set mged_help_data(arced)	{{a/b ...anim_command...}	{edit matrix or materials on combination's arc}}
set mged_help_data(area)	{{[endpoint_tolerance]}	{calculate presented area of view (use ev -wT)}}
set mged_help_data(arot)        $helplib_data(vo_arot)
set mged_help_data(attach)	{{[-d display_string] [-i init_script] [-n name]
	      [-t is_toplevel] [-W width] [-N height]
	      [-S square_size] win_type}	{attach to a display manager}}
set mged_help_data(attr)        $helplib_data(wdb_attr)
set mged_help_data(autoview)	{{}	{set view size and center so that all displayed solids are in view}}
set mged_help_data(B)		$helplib_data(dgo_blast)
set mged_help_data(bev)		{{[-t] [-P#] new_obj obj1 op obj2 op obj3 op ...}	{boolean evaluation of objects via NMG's}}
set mged_help_data(bot_condense) {{new_bot_solid old_bot_solid} {remove unreferenced vertices in a BOT solid}}
set mged_help_data(bot_decimate)  $helplib_data(wdb_bot_decimate)
set mged_help_data(bot_face_fuse) {{new_bot_solid old_bot_solid} {eliminate duplicate faces in a BOT solid}}
set mged_help_data(bot_face_sort) $helplib_data(wdb_bot_face_sort)
set mged_help_data(bot_smooth)  $helplib_data(wdb_bot_smooth)
set mged_help_data(bot_vertex_fuse) {{new_bot_solid old_bot_solid} {fuse duplicate vertices in a BOT solid}}
set mged_help_data(build_region) {{[-a region_number] tag start end} {build a region from solids matching RE "tag.s*"}}
set mged_help_data(c)		$helplib_data(wdb_comb_std)
set mged_help_data(cat)		$helplib_data(wdb_cat)
set mged_help_data(center)	$helplib_data(vo_center)
set mged_help_data(closedb)	{{}	{close any open database}}
set mged_help_data(color)	$helplib_data(wdb_color)
set mged_help_data(comb)	$helplib_data(wdb_comb)
set mged_help_data(comb_color)	{{comb R G B}	{assign a color to a combination (like 'mater')}}
set mged_help_data(copyeval)	$helplib_data(wdb_copyeval)
set mged_help_data(copymat)	{{a/b c/d}	{copy matrix from one combination's arc to another's}}
set mged_help_data(cp)		$helplib_data(wdb_copy)
set mged_help_data(cpi)		{{from to}	{copy cylinder and position at end of original cylinder}}
set mged_help_data(d)		$helplib_data(dgo_erase)
set mged_help_data(dall)	$helplib_data(dgo_erase_all)
set mged_help_data(db)		{{command}	{database manipulation routines}}
set mged_help_data(db_glob)	{{cmd_string}	{globs cmd_string against the MGED database
	 resulting in an expanded command string}}
set mged_help_data(dbconcat)	$helplib_data(wdb_concat)
set mged_help_data(dbfind)	$helplib_data(wdb_find)
set mged_help_data(dbupgrade)	{{[-f|-help]}	{upgrade your database to the current format}}
set mged_help_data(dbversion)	{{}	{return the database version}}
set mged_help_data(debugbu)	{{[hex_code]}	{show/set debugging bit vector for libbu}}
set mged_help_data(debugdir)	{{}	{Print in-memory directory, for debugging}}
set mged_help_data(debuglib)	{{[hex_code]}	{show/set debugging bit vector for librt}}
set mged_help_data(debugmem)	{{}	{Print librt memory use map}}
set mged_help_data(debugnmg)	{{[hex code]}	{show/set debugging bit vector for NMG}}
set mged_help_data(decompose)	{{nmg_solid [prefix]}	{decompose nmg_solid into maximally connected shells}}
set mged_help_data(delay)	{{sec usec}	{delay for the specified amount of time}}
set mged_help_data(dm)		{{set var [val]}	{do display-manager specific command}}
set mged_help_data(draw)	$helplib_data(dgo_draw)
set mged_help_data(dump)	$helplib_data(wdb_dump)
set mged_help_data(dup)		$helplib_data(wdb_dup)
set mged_help_data(E)		$helplib_data(dgo_E)
set mged_help_data(e)		$helplib_data(dgo_draw)
set mged_help_data(em)          {{[-C#/#/#] value [value value...]} {display all regions with attribute "MUVES_Component" set to any of the specified values}}
set mged_help_data(eac)		{{air_code(s)}	{display all regions with given air code}}
set mged_help_data(echo)	{{[text]}	{echo arguments back}}
set mged_help_data(edcodes)	{{object(s)}	{edit region ident codes}}
set mged_help_data(edcolor)	{{}	{text edit color table}}
set mged_help_data(edcomb)	{{combname Regionflag regionid air los [material]}	{edit combination record info}}
set mged_help_data(edgedir)	{{[delta_x delta_y delta_z]|[rot fb]}	{define direction of ARB edge being moved}}
set mged_help_data(edmater)	{{comb(s)}	{edit combination materials}}
set mged_help_data(erase)	{{<objects>}	{remove objects from the screen}}
set mged_help_data(erase_all)	{{<objects>}	{remove all occurrences of object(s) from the screen}}
set mged_help_data(ev)		$helplib_data(dgo_ev)
set mged_help_data(eqn)		{{A B C}	{planar equation coefficients}}
set mged_help_data(exit)	{{}	{exit}}
set mged_help_data(extrude)	{{#### distance}	{extrude dist from face}}
set mged_help_data(expand)	$helplib_data(wdb_expand)
set mged_help_data(eye_pt)	$helplib_data(vo_eye)
set mged_help_data(e_muves)	{{MUVES_component_1 MUVES_component2 ...}	{display listed MUVES components/systems}}
set mged_help_data(facedef)	{{####}	{define new face for an arb}}
set mged_help_data(facetize)	{{[-ntT] [-P#] new_obj old_obj(s)}	{convert objects to faceted BOT objects (or NMG for -n option) at current tol}}
set mged_help_data(form)	$helplib_data(wdb_form)
set mged_help_data(fracture)	{{NMGsolid [prefix]}	{fracture an NMG solid into many NMG solids, each containing one face}}
set mged_help_data(g)		$helplib_data(wdb_group)
set mged_help_data(garbage_collect)	{{}	{eliminate unused space in database file}}
set mged_help_data(get)		$helplib_data(wdb_get)
set mged_help_data(get_regions)	{{combination}	{returns the names of all regions under a given combination/assembly}}
set mged_help_data(gui)	{{[-config b|c|g] [-d display_string]
	[-gd graphics_display_string] [-dt graphics_type]
	[-id name] [-c -h -j -s]}	{create display/command window pair}}
set mged_help_data(help)	{{[command(s)]}	{give usage message for given command(s)}}
set mged_help_data(helplib)	{{[library_command(s)]}	{give usage message for given library command(s)}}
set mged_help_data(helpdevel)	{{[command(s)]}	{give usage message for given developer command(s)}}
set mged_help_data(hide)        $helplib_data(wdb_hide)
set mged_help_data(history)	{{[-delays]}	{list command history}}
set mged_help_data(i)		$helplib_data(wdb_instance)
set mged_help_data(idents)		{{file object(s)}	{make ascii summary of region idents}}
set mged_help_data(ill)		{{name}	{illuminate object}}
set mged_help_data(in)		{{[-f] [-s] parameters...}	{keyboard entry of solids.  -f for no drawing, -s to enter solid edit}}
set mged_help_data(inside)	{{[outside_solid new_inside_solid thicknesses]}	{finds inside solid per specified thicknesses. Note that in an edit mode the edited solid is used for the outside_solid and should not appear on the command line }}
set mged_help_data(item)	{{region ident [air [material [los]]]}	{set region ident codes}}
set mged_help_data(joint)	{{command [options]}	{articulation/animation commands}}
set mged_help_data(journal)	{{[-d] fileName}	{record all commands and timings to journal}}
set mged_help_data(keep)	$helplib_data(wdb_keep)
set mged_help_data(keypoint)	{{[x y z | reset]}	{set/see center of editing transformations}}
set mged_help_data(kill)	$helplib_data(wdb_kill)
set mged_help_data(killall)	$helplib_data(wdb_killall)
set mged_help_data(killtree)	$helplib_data(wdb_killtree)
set mged_help_data(knob)	{{[-e -i -m -v] [-o v/m/e/k] [zap|zero|(x|y|z|X|Y|Z|S|ax|ay|az|aX|aY|aZ|aS|xadc|yadc|ang1|ang2|distadc [val])]}	{emulate knob twist (e.g. knob x 1)}}
set mged_help_data(l)		$helplib_data(wdb_list)
set mged_help_data(l_muves)	{{MUVES_component1 MUVES_component2 ...} {list the MGED components that make up the specified MUVES components/systems}}
set mged_help_data(labelvert)	{{object[s]}	{label vertices of wireframes of objects}}
set mged_help_data(listeval)	$helplib_data(wdb_listeval)
set mged_help_data(loadtk)	{{[DISPLAY]}	{initializes the Tk window library}}
set mged_help_data(loadview)	{{file}	{load view from raytrace script file}}
set mged_help_data(lookat)	$helplib_data(vo_lookat)
set mged_help_data(ls)		$helplib_data(wdb_ls)
set mged_help_data(lm)          {{[-l] value [value value...]} {list all regions that have a MUVES_Component attribute with any of the listed values}}
set mged_help_data(lt)		$helplib_data(wdb_lt)
set mged_help_data(M)		{{1|0 xpos ypos}	{invoke a traditional MGED mouse event}}
set mged_help_data(make)	{{-t | name <arb8|arb7|arb6|arb5|arb4|arbn|ars|bot|ehy|ell|ell1|epa|eto|extrude|grip|half|nmg|part|pipe|rcc|rec|rhc|rpc|rpp|sketch|sph|tec|tgc|tor|trc>}	{create a primitive}}
set mged_help_data(make_bb)	$helplib_data(wdb_make_bb)
set mged_help_data(match)	$helplib_data(wdb_match)
set mged_help_data(mater)	{{comb [material]}	{assign/delete material to combination}}
set mged_help_data(matpick)	{{# or a/b}	{select arc which has matrix to be edited, in O_PATH state}}
set mged_help_data(memprint)	{{}	{print memory maps}}
set mged_help_data(mirface)	{{#### axis}	{mirror an ARB face}}
set mged_help_data(mirror)	{{old new axis}	{mirror solid or combination around axis}}
set mged_help_data(mrot)	$helplib_data(vo_mrot)
set mged_help_data(mv)		$helplib_data(wdb_move)
set mged_help_data(mvall)	$helplib_data(wdb_moveall)
set mged_help_data(nirt)	$helplib_data(dgo_nirt)
set mged_help_data(nmg_collapse)	$helplib_data(wdb_nmg_collapse)
set mged_help_data(nmg_simplify)	$helplib_data(wdb_nmg_simplify)
set mged_help_data(oed)		{{path_lhs path_rhs}	{go from view to object_edit of path_lhs/path_rhs}}
set mged_help_data(opendb)	{{[database.g]}	{close current .g file, and open new .g file}}
set mged_help_data(orientation)	$helplib_data(vo_orient)
set mged_help_data(orot)	{{[-i] xdeg ydeg zdeg}	{rotate object being edited}}
set mged_help_data(oscale)	{{factor}	{scale object by factor}}
set mged_help_data(overlay)	$helplib_data(dgo_overlay)
set mged_help_data(p)		{{dx [dy dz]}	{set parameters}}
set mged_help_data(pathlist)	$helplib_data(wdb_pathlist)
set mged_help_data(paths)	$helplib_data(wdb_paths)
set mged_help_data(permute)	{{tuple}	{permute vertices of an ARB}}
set mged_help_data(plot)	{{[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]}	{make UNIX-plot of view}}
set mged_help_data(pl)		{{[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]}	{Experimental - uses dm-plot:make UNIX-plot of view}}
set mged_help_data(polybinout)	{{file}	{store vlist polygons into polygon file (experimental)}}
set mged_help_data(pov)		$helplib_data(vo_pov)
set mged_help_data(prcolor)	$helplib_data(wdb_prcolor)
set mged_help_data(prefix)	{{new_prefix object(s)}	{prefix each occurrence of object name(s)}}
set mged_help_data(press)	{{button_label}	{emulate button press}}
set mged_help_data(prj_add)	{{ [-t] [-b] [-n] shaderfile [image_file] [image_width] [image_height]} {Appends image filename + current view parameters to shaderfile}}
set mged_help_data(preview)	{{[-v] [-d sec_delay] [-D start frame] [-K last frame] rt_script_file}	{preview new style RT animation script}}
set mged_help_data(ps)		{{[-f font] [-t title] [-c creator] [-s size in inches] [-l linewidth] file}	{creates a postscript file of the current view}}
set mged_help_data(push)	$helplib_data(wdb_push)
set mged_help_data(put)		$helplib_data(wdb_put)
set mged_help_data(putmat)	{{a/b {I | m0 m1 ... m16}}	{replace matrix on combination's arc}}
set mged_help_data(q)		{{}	{quit}}
set mged_help_data(qray)	$helplib_data(dgo_qray)
set mged_help_data(query_ray)	$helplib_data(dgo_nirt)
set mged_help_data(quit)	{{}	{quit}}
set mged_help_data(qorot)	{{x y z dx dy dz theta}	{rotate object being edited about specified vector}}
set mged_help_data(qvrot)	{{dx dy dz theta}	{set view from direction vector and twist angle}}
set mged_help_data(r)		$helplib_data(wdb_region)
set mged_help_data(rcc-blend)	{{rccname newname thickness [b|t]}	{create a blend at an end of an rcc}}
set mged_help_data(rcc-cap)     {{rccname newname [height] [b|t]}      {create a cap (ell) at an end of an rcc}}
set mged_help_data(rcc-tgc)     {{rccname newname x y z [b|t]}     {create a tgc with the specified apex at an end of an rcc}}
set mged_help_data(rcc-tor)     {{rccname newname}     {create a tor from an rcc}}
set mged_help_data(rcodes)	{{filename}	{read region ident codes from filename}}
set mged_help_data(read_muves)	{{MUVES_regionmap_file [sysdef_file]}	{read the MUVES region_map file and optionally the sysdef file}}
set mged_help_data(red)		{{object}	{edit a group or region using a text editor}}
set mged_help_data(redraw_vlist)	{{object(s)}	{given the name(s) of database objects, re-generate the vlist
	associated with every solid in view which references the
	named object(s), either solids or regions. Particularly
	useful with outboard .inmem database modifications.}}
set mged_help_data(refresh)	{{}	{send new control list}}
set mged_help_data(regdebug)	{{[number]}	{toggle display manager debugging or set debug level}}
set mged_help_data(regdef)	{{ident [air [los [material]]]}	{change next region default codes}}
set mged_help_data(regions)	{{file object(s)}	{make ascii summary of regions}}
set mged_help_data(reid)	{{assembly regionID}	{assign region IDs to all regions under some assembly/combination starting with the given region ID number}}
set mged_help_data(release)	{{[name]}	{release display processor}}
set mged_help_data(remat)	{{assembly GIFTmater}	{assign a given material ID number to all regions under some assembly/combination}}
set mged_help_data(reset)	{{}	{Reset view such that all solids can be seen}}
set mged_help_data(rfarb)	{{}	{makes arb given point, 2 coord of 3 pts, rot, fb, thickness}}
set mged_help_data(rm)		$helplib_data(wdb_remove)
set mged_help_data(rmater)	{{filename}	{read combination materials from filename}}
set mged_help_data(rmats)	{{file}	{load view(s) from 'savekey' file}}
set mged_help_data(rot)		$helplib_data(vo_rot)
set mged_help_data(rotobj)	{{[-i] xdeg ydeg zdeg}	{rotate object being edited}}
set mged_help_data(rpp-arch)    {{rppname newname face}     {create an arch at a face of an rpp}}
set mged_help_data(rpp-cap)     {{rppname newname face height [0|1]}	{create a cap (arb6) at a face of an rpp}}
set mged_help_data(rrt)		{{prog [options]}	{invoke prog with view}}
set mged_help_data(rt)		$helplib_data(dgo_rt)
set mged_help_data(rt_gettrees)	$helplib_data(wdb_rt_gettrees)
set mged_help_data(rtabort)	$helplib_data(dgo_rtabort)
set mged_help_data(rtcheck)	$helplib_data(dgo_rtcheck)
set mged_help_data(rtarea)	$helplib_data(dgo_rtarea)
set mged_help_data(rtedge)	$helplib_data(dgo_rtedge)
set mged_help_data(rtweight)	$helplib_data(dgo_rtweight)
set mged_help_data(savekey)	{{file [time]}	{save keyframe in file (experimental)}}
set mged_help_data(saveview)	{{file [args]}	{save view in file for RT}}
set mged_help_data(sca)		$helplib_data(vo_sca)
set mged_help_data(sed)		{{<path>}	{solid-edit named solid}}
set mged_help_data(setview)	$helplib_data(vo_setview)
set mged_help_data(shader)	{{comb {shader_name {keyword value keyword value ...}}}	{assign shader using Tcl list format}}
set mged_help_data(shaded_mode)	{{[-a |-auto] [0|1|2]}	{get/set shaded mode}}
set mged_help_data(shells)	$helplib_data(wdb_shells)
set mged_help_data(showmats)	$helplib_data(wdb_showmats)
set mged_help_data(size)	$helplib_data(vo_size)
set mged_help_data(solids)	{{file object(s)}	{make ascii summary of solid parameters}}
set mged_help_data(sph-part)	{{sph1name sph2name newname}	{create a part from two sph's}}
set mged_help_data(status)	{{[state|Viewscale|base2local|local2base|
	toViewcenter|Viewrot|model2view|view2model|
	model2objview|objview2model|help]}	{get view status}}
set mged_help_data(summary)	$helplib_data(wdb_summary)
set mged_help_data(sv)		$helplib_data(vo_slew)
set mged_help_data(sync)	{{}	{forces UNIX sync}}
set mged_help_data(t)		{{[-a -c -r -s]}	{table of contents}}
set mged_help_data(ted)		{{}	{text edit a solid's parameters}}
set mged_help_data(title)	$helplib_data(wdb_title)
set mged_help_data(tol)		$helplib_data(wdb_tol)
set mged_help_data(tops)	$helplib_data(wdb_tops)
set mged_help_data(tor-rcc)     {{torname newname}     {create an rcc from a tor}}
set mged_help_data(tra)		$helplib_data(vo_tra)
set mged_help_data(track)	{{<parameters>}	{adds tracks to database}}
set mged_help_data(translate)	{{x y z}	{trans object to x,y, z}}
set mged_help_data(tree)	{{[-c] [-i n] [-o outfile] object(s)}	{print out a tree of all members of an object}}
set mged_help_data(t_muves)	{{}	{list all the known MUVES components/systems}}
set mged_help_data(unhide)	$helplib_data(wdb_unhide)
set mged_help_data(units)	$helplib_data(wdb_units)
set mged_help_data(vars)	{{[var=opt]}	{get/set mged variables}}
set mged_help_data(vdraw)	{{write|insert|delete|read|length|send [args]}	{Expermental drawing (cnuzman)}}
set mged_help_data(view)	{{center|size|eye|ypr|quat|aet}	{get/set view parameters (local units).}}
set mged_help_data(vnirt)	{{x y}  	{trace a single ray from x y}}
set mged_help_data(vquery_ray)	{{x y}  	{trace a single ray from x y}}
#set mged_help_data(vrmgr)	{{host {master|slave|overview}}	{link with Virtual Reality manager}}
set mged_help_data(vrot)	$helplib_data(vo_vrot)
set mged_help_data(wcodes)	{{filename object(s)}	{write region ident codes to filename}}
set mged_help_data(wdb_binary)	$helplib_data(wdb_binary)
set mged_help_data(whatid)	$helplib_data(wdb_whatid)
set mged_help_data(whichair)	$helplib_data(wdb_whichair)
set mged_help_data(whichid)	$helplib_data(wdb_whichid)
set mged_help_data(which_shader)	{{Shader(s)}	{lists all combinations using the given shaders}}
set mged_help_data(who)		$helplib_data(dgo_who)
set mged_help_data(wmater)	{{filename comb(s)}	{write combination materials to filename}}
set mged_help_data(x)		$helplib_data(dgo_report)
set mged_help_data(xpush)	$helplib_data(wdb_xpush)
set mged_help_data(Z)		$helplib_data(dgo_zap)
set mged_help_data(zoom)	$helplib_data(vo_zoom)

proc help {args} {
    global mged_help_data

    if {[llength $args] > 0} {
	return [help_comm mged_help_data $args]
    } else {
	return [help_comm mged_help_data]
    }
}

proc ? {} {
    global mged_help_data

    return [?_comm mged_help_data 20 4]
}

proc apropos key {
    global mged_help_data

    return [apropos_comm mged_help_data $key]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
