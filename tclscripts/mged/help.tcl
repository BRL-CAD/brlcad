#==============================================================================
#
# TCL versions of MGED "help", "?", and "apropos" commands
#
#==============================================================================

set help_data(?)		{{}	{summary of available mged commands}}
set help_data(?lib)		{{}	{summary of available library commands}}
set help_data(%)		{{}	{escape to interactive shell}}
set help_data(3ptarb)		{{}	{makes arb given 3 pts, 2 coord of 4th pt, and thickness}}
set help_data(adc)		{{[<a1|a2|dst|dh|dv|hv|dx|dy|dz|xyz|reset|help> value(s)]}	{control the angle/distance cursor}}
set help_data(add_view)         {{} {add another view to the view ring}}
set help_data(ae)		{{[-i] azim elev [twist]}	{set view using azim, elev and twist angles}}
set help_data(aim)		{{[command_window [pathName of display window]]}	{aims command_window at pathName}}
set help_data(aip)		{{[fb]}		{advance illumination pointer or path position forward or backward}}
set help_data(analyze)		{{[arbname]}	{analyze faces of ARB}}
set help_data(apropos)		{{keyword}	{finds commands whose descriptions contain the given keyword}}
set help_data(arb)		{{name rot fb}	{make arb8, rotation + fallback}}
set help_data(arced)		{{a/b ...anim_command...}	{edit matrix or materials on combination's arc}}
set help_data(area)		{{[endpoint_tolerance]}	{calculate presented area of view}}
set help_data(arot)             {{x y z angle} {rotate about axis ((0,0,0) (x,y,z)) by angle (degrees)}}
set help_data(attach)		{{[-d display_string] [-i init_script] [-n name]
	      [-t is_toplevel] [-W width] [-N height]
	      [-S square_size] win_type}	{attach to a display manager}}
set help_data(B)		{{<objects>}	{clear screen, edit objects}}
set help_data(bev)		{{[-t] [-P#] new_obj obj1 op obj2 op obj3 op ...}	{Boolean evaluation of objects via NMG's}}
set help_data(c)		{{[-gr] comb_name [boolean_expr]}	{create or extend a combination using standard notation}}
set help_data(cat)		{{<objects>}	{list attributes (brief)}}
set help_data(center)		{{x y z}	{set view center}}
set help_data(closew)		{{id}	{close display/command window pair}}
set help_data(color)		{{low high r g b str}	{make color entry}}
set help_data(comb)		{{comb_name <operation solid>}	{create or extend combination w/booleans}}
set help_data(comb_color)	{{comb R G B}	{assign a color to a combination (like 'mater')}}
set help_data(copyeval)		{{new_solid path_to_old_solid}	{copy an 'evaluated' path solid}}
set help_data(copymat)		{{a/b c/d}	{copy matrix from one combination's arc to another's}}
set help_data(cp)		{{from to}	{copy [duplicate] object}}
set help_data(cpi)		{{from to}	{copy cylinder and position at end of original cylinder}}
set help_data(d)		{{<objects>}	{remove objects from the screen}}
set help_data(dall)		{{<objects>}	{remove all occurrences of object(s) from the screen}}
set help_data(db)		{{command}	{database manipulation routines}}
set help_data(dbconcat)		{{file [prefix]}	{concatenate 'file' onto end of present database.  Run 'dup file' first.}}
set help_data(debugbu)		{{[hex_code]}	{Show/set debugging bit vector for libbu}}
set help_data(debugdir)		{{}	{Print in-memory directory, for debugging}}
set help_data(debuglib)		{{[hex_code]}	{Show/set debugging bit vector for librt}}
set help_data(debugmem)		{{}	{Print librt memory use map}}
set help_data(debugnmg)		{{[hex code]}	{Show/set debugging bit vector for NMG}}
set help_data(decompose)	{{nmg_solid [prefix]}	{decompose nmg_solid into maximally connected shells}}
set help_data(delay)		{{sec usec}	{Delay for the specified amount of time}}
set help_data(delete_view)      {{vid} {delete view vid from the view ring}}
set help_data(dm)		{{set var [val]}	{Do display-manager specific command}}
set help_data(draw)		{{<objects>}	{draw objects}}
set help_data(dup)		{{file [prefix]}	{check for dup names in 'file'}}
set help_data(E)		{{ [-s] <objects>}	{evaluated edit of objects. Option 's' provides a slower, but better fidelity evaluation}}
set help_data(e)		{{<objects>}	{edit objects}}
set help_data(eac)		{{Air_code(s)}	{display all regions with given air code}}
set help_data(echo)		{{[text]}	{echo arguments back}}
set help_data(edcodes)		{{object(s)}	{edit region ident codes}}
set help_data(edmater)		{{comb(s)}	{edit combination materials}}
set help_data(edcolor)		{{}	{text edit color table}}
set help_data(edcomb)		{{combname Regionflag regionid air los [GIFTmater]}	{edit combination record info}}
set help_data(edgedir)		{{[delta_x delta_y delta_z]|[rot fb]}	{define direction of ARB edge being moved}}
set help_data(erase)		{{<objects>}	{remove objects from the screen}}
set help_data(erase_all)	{{<objects>}	{remove all occurrences of object(s) from the screen}}
set help_data(ev)		{{[-dnqstuvwT] [-P #] <objects>}	{evaluate objects via NMG tessellation}}
set help_data(eqn)		{{A B C}	{planar equation coefficients}}
set help_data(exit)		{{}	{exit}}
set help_data(extrude)		{{#### distance}	{extrude dist from face}}
set help_data(expand)		{{wildcard expression}	{expands wildcard expression}}
set help_data(eye_pt)		{{mx my mz}	{set eye point to given model coordinates (in mm)}}
set help_data(e_muves)		{{MUVES_component_1 MUVES_component2 ...}	{display listed MUVES components/systems}}
set help_data(facedef)		{{####}	{define new face for an arb}}
set help_data(facetize)		{{[-tT] [-P#] new_obj old_obj(s)}	{convert objects to faceted NMG objects at current tol}}
set help_data(find)		{{<objects>}	{find all references to objects}}
set help_data(fracture)		{{NMGsolid [prefix]}	{fracture an NMG solid into many NMG solids, each containing one face\n}}
set help_data(g)		{{groupname <objects>}	{group objects}}
set help_data(get_comb)		{{comb_name} {get information about combination}}
#set help_data(getknob)		{{knobname}	{Gets the current setting of the given knob}}
set help_data(get_view)         {{[-a]} {returns a list of view id numbers that are in the view ring}}
set help_data(goto_view)        {{vid} {make view vid the current view}}
set help_data(help)		{{[commands]}	{give usage message for given commands}}
set help_data(helplib)		{{[library commands]}	{give usage message for given library commands}}
set help_data(history)		{{[-delays]}	{list command history}}
set help_data(hist_prev)	{{}	{Returns previous command in history}}
set help_data(hist_next)	{{}	{Returns next command in history}}
set help_data(hist_add)		{{[command]}	{Adds command to the history (without executing it)}}
set help_data(i)		{{obj combination [operation]}	{add instance of obj to comb}}
set help_data(idents)		{{file object(s)}	{make ascii summary of region idents}}
set help_data(ill)		{{name}	{illuminate object}}
set help_data(in)		{{[-f] [-s] parameters...}	{keyboard entry of solids.  -f for no drawing, -s to enter solid edit}}
set help_data(inside)		{{}	{finds inside solid per specified thicknesses}}
set help_data(item)		{{region item [air [GIFTmater [los]]]}	{set region ident codes}}
set help_data(jcs)		{{id}	{join collaborative session}}
set help_data(joint)		{{command [options]}	{articulation/animation commands}}
set help_data(journal)		{{[-d] fileName}	{record all commands and timings to journal}}
set help_data(keep)		{{keep_file object(s)}	{save named objects in specified file}}
set help_data(keypoint)		{{[x y z | reset]}	{set/see center of editing transformations}}
set help_data(kill)		{{[-f] <objects>}	{delete object[s] from file}}
set help_data(killall)		{{<objects>}	{kill object[s] and all references}}
set help_data(killtree)		{{<object>}	{kill complete tree[s] - BE CAREFUL}}
set help_data(knob)		{{[-e -i -m -v] [id [val]]}	{emulate knob twist}}
set help_data(l)		{{<objects>}	{list attributes (verbose)}}
set help_data(L)		{{1|0 xpos ypos}	{handle a left mouse event}}
set help_data(labelvert)	{{object[s]}	{label vertices of wireframes of objects}}
set help_data(listeval)		{{}	{lists 'evaluated' path solids}}
set help_data(loadtk)		{{[DISPLAY]}	{Initializes the Tk window library}}
set help_data(lookat)		{{x y z}	{Adjust view to look at given coordinates}}
set help_data(ls)		{{}	{table of contents}}
set help_data(l_muves)		{{MUVES_component1 MUVES_component2 ...} {list the components that make up the specified MUVES components/systems}}
set help_data(M)		{{1|0 xpos ypos}	{handle a middle mouse event}}
set help_data(make)		{{name <arb8|sph|ellg|tor|tgc|rpc|rhc|epa|ehy|eto|part|grip|half|nmg|pipe>}	{create a primitive}}
set help_data(make_bb)		{{new_rpp_name obj1_or_path1 [list of objects or paths ...]}	{make a bounding box solid enclosing specified objects/paths}}
set help_data(make_name)	{{templ@te}	{make an object name not occuring in database}}
set help_data(mater)		{{comb [material]}	{assign/delete material to combination}}
set help_data(matpick)		{{# or a/b}	{select arc which has matrix to be edited, in O_PATH state}}
set help_data(memprint)		{{}	{print memory maps}}
set help_data(mged_update)	{{}	{handle outstanding events and refresh}}
set help_data(mirface)		{{#### axis}	{mirror an ARB face}}
set help_data(mirror)		{{old new axis}	{mirror solid or combination around axis}}
set help_data(model2view)	{{mx my mz}	{convert point in model coords (mm) to view coords}}
set help_data(mv)		{{old new}	{rename object}}
set help_data(mvall)		{{oldname newname}	{rename object everywhere}}
set help_data(next_view)        {{} {set the current view to the next view on the view ring}}
set help_data(nirt)		{{}	{trace a single ray from current view}}
set help_data(nmg_collapse)	{{ maximum_error_distance new_solid nmg_solid}	{decimate NMG solid via edge collapse}}
set help_data(nmg_simplify)	{{[arb|tgc|ell|poly] new_solid nmg_solid}	{simplify nmg_solid, if possible}}
set help_data(oed)		{{path_lhs path_rhs}	{Go from view to object_edit of path_lhs/path_rhs}}
set help_data(opendb)		{{[database.g]}	{Close current .g file, and open new .g file}}
set help_data(openw)		{{[-config b|c|g] [-d display string] [-gd graphics display string] [-gt graphics type] [-id name] [-c -h -j -s]}	{open display/command window pair}}
set help_data(orientation)	{{x y z w}	{Set view direction from quaternion}}
set help_data(orot)		{{[-i] xdeg ydeg zdeg}	{rotate object being edited}}
set help_data(oscale)		{{factor}	{scale object by factor}}
set help_data(overlay)		{{file.plot [name]}	{Read UNIX-Plot as named overlay}}
set help_data(p)		{{dx [dy dz]}	{set parameters}}
set help_data(pathlist)		{{name(s)}	{list all paths from name(s) to leaves}}
set help_data(paths)		{{pattern}	{lists all paths matching input path}}
set help_data(pcs)		{{}	{print collaborative participants}}
set help_data(pmp)		{{}	{print mged players}}
set help_data(permute)		{{tuple}	{permute vertices of an ARB}}
set help_data(plot)		{{[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]}	{make UNIX-plot of view}}
set help_data(pl)		{{[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]}	{Experimental - uses dm-plot:make UNIX-plot of view}}
set help_data(polybinout)	{{file}	{store vlist polygons into polygon file (experimental)}}
set help_data(pov)		{{args}	{experimental:  set point-of-view}}
set help_data(prcolor)		{{}	{print color&material table}}
set help_data(prefix)		{{new_prefix object(s)}	{prefix each occurrence of object name(s)}}
set help_data(press)		{{button_label}	{emulate button press}}
set help_data(preview)		{{[-v] [-d sec_delay] [-D start frame] [-K last frame] rt_script_file}	{preview new style RT animation script}}
set help_data(prev_view)        {{} {set the current view to the previous view on the view ring}}
set help_data(ps)		{{[-f font] [-t title] [-c creator] [-s size in inches] [-l linewidth] file}	{creates a postscript file of the current view}}
set help_data(push)		{{object[s]}	{pushes object's path transformations to solids}}
set help_data(put_comb)		{{comb_name is_Region id air gift los color shader inherit boolean_expr} {set combination}}
set help_data(putmat)		{{a/b {I | m0 m1 ... m16}}	{replace matrix on combination's arc}}
set help_data(q)		{{}	{quit}}
set help_data(qcs)		{{id}	{quit collaborative session}}
set help_data(quit)		{{}	{quit}}
set help_data(qorot)		{{x y z dx dy dz theta}	{rotate object being edited about specified vector}}
set help_data(qvrot)		{{dx dy dz theta}	{set view from direction vector and twist angle}}
set help_data(r)		{{region <operation solid>}	{create or extend a Region combination}}
set help_data(R)		{{1|0 xpos ypos}	{handle a right mouse event}}
set help_data(rcodes)		{{filename}	{read region ident codes from filename}}
set help_data(read_muves)	{{MUVES_regionmap_file [sysdef_file]}	{read the MUVES region_map file and optionally the sysdef file}}
set help_data(red)		{{object}	{edit a group or region using a text editor}}
set help_data(refresh)		{{}	{send new control list}}
set help_data(regdebug)		{{[number]}	{toggle display manager debugging or set debug level}}
set help_data(regdef)		{{item [air [los [GIFTmaterial]]]}	{change next region default codes}}
set help_data(regions)		{{file object(s)}	{make ascii summary of regions}}
set help_data(release)		{{[name]}	{release display processor}}
set help_data(rfarb)		{{}	{makes arb given point, 2 coord of 3 pts, rot, fb, thickness}}
set help_data(rm)		{{comb <members>}	{remove members from comb}}
set help_data(rmater)		{{filename}	{read combination materials from filename}}
set help_data(rmats)		{{file}	{load view(s) from 'savekey' file}}
set help_data(rot)              {{x y z} {rotate by x, y, z (degrees)}}
set help_data(rotobj)		{{[-i] xdeg ydeg zdeg}	{rotate object being edited}}
set help_data(rrt)		{{prog [options]}	{invoke prog with view}}
set help_data(rt)		{{[options]}	{do raytrace of view}}
set help_data(rtcheck)		{{[options]}	{check for overlaps in current view}}
set help_data(savekey)		{{file [time]}	{save keyframe in file (experimental)}}
set help_data(saveview)		{{file [args]}	{save view in file for RT}}
set help_data(sca)              {{sfactor} {scale by sfactor}}
set help_data(sed)		{{<path>}	{solid-edit named solid}}
set help_data(setview)		{{x y z}	{set the view given angles x, y, and z in degrees}}
set help_data(shader)		{{comb material [arg(s)]}	{assign materials (like 'mater')}}
set help_data(share_menu)	{{pathName1 pathName2}  {pathName1 shares its menu with pathName2}}
set help_data(share_vars)	{{pathName1 pathName2}	{pathName1 shares its vars with pathName2}}
set help_data(share_view)	{{pathName1 pathName2}	{pathName1 shares its view with pathName2}}
set help_data(shells)		{{nmg_model}	{breaks model into seperate shells}}
set help_data(showmats)		{{path}	{show xform matrices along path}}
set help_data(size)		{{size}	{set view size}}
set help_data(sliders)		{{[on|off]}	{turns the sliders on or off, or reads current state}}
set help_data(solids)		{{file object(s)}	{make ascii summary of solid parameters}}
set help_data(solids_on_ray)	{{h v}	{List all displayed solids along a ray}}
set help_data(status)		{{}	{get view status}}
set help_data(summary)		{{[s r g]}	{count/list solid/reg/groups}}
set help_data(sv)		{{x y [z]}	{Move view center to (x, y, z)}}
set help_data(svb)		{{}	{set view reference base}}
set help_data(sync)		{{}	{forces UNIX sync}}
set help_data(t)		{{}	{table of contents}}
set help_data(ted)		{{}	{text edit a solid's parameters}}
set help_data(title)		{{[string]}	{print or change the title}}
set help_data(toggle_view)      {{} {toggle the current view to the last view}}
set help_data(tol)		{{[abs #] [rel #] [norm #] [dist #] [perp #]}	{show/set tessellation and calculation tolerances}}
set help_data(tops)		{{}	{find all top level objects}}
set help_data(track)		{{<parameters>}	{adds tracks to database}}
set help_data(tra)              {{dx dy dz} {translate by (dx,dy,dz)}}
set help_data(translate)	{{x y z}	{trans object to x,y, z}}
set help_data(tree)		{{object(s)}	{print out a tree of all members of an object}}
set help_data(t_muves)		{{}	{list all the known MUVES components/systems}}
set help_data(unaim)		{{id}	{stop aiming at id}}
set help_data(units)		{{[mm|cm|m|in|ft|...]}	{change units}}
set help_date(unshare_menu)	{{pathName}	{pathName no longer shares its menu}}
set help_data(unshare_vars)	{{pathName}	{pathName no longer shares its vars}}
set help_data(unshare_view)	{{pathName}	{pathName no longer shares its view}}
set help_data(vars)		{{[var=opt]}	{assign/display mged variables}}
set help_data(vdraw)		{{write|insert|delete|read|length|show [args]}	{Expermental drawing (cnuzman)}}
set help_data(viewget)		{{center|size|eye|ypr|quat|aet}	{Experimental - return high-precision view parameters.}}
set help_data(viewset)		{{center|eye|size|ypr|quat|aet}	{Experimental - set several view parameters at once.}}
set help_data(view2model)	{{mx my mz}	{convert point in view coords to model coords (mm)}}
set help_data(vrmgr)		{{host {master|slave|overview}}	{link with Virtual Reality manager}}
set help_data(vrot)		{{xdeg ydeg zdeg}	{rotate viewpoint}}
set help_data(vrot_center)	{{v|m x y z}	{set center point of viewpoint rotation, in model or view coords}}
set help_data(wcodes)		{{filename object(s)}	{write region ident codes to filename}}
set help_data(whatid)		{{region_name}	{display ident number for region}}
set help_data(whichair)		{{air_codes(s)}	{lists all regions with given air code}}
set help_data(whichid)		{{ident(s)}	{lists all regions with given ident code}}
set help_data(which_shader)	{{Shader(s)}	{lists all combinations using the given shaders}}
set help_data(who)		{{[r(eal)|p(hony)|b(oth)]}	{list the top-level objects currently being displayed}}
set help_data(winset)		{{[pathname]}	{sets the current display manager to pathname}}
set help_data(wmater)		{{filename comb(s)}	{write combination materials to filename}}
set help_data(x)		{{lvl}	{print solid table & vector list}}
set help_data(xpush)		{{object}	{Experimental Push Command}}
set help_data(Z)		{{}	{zap all objects off screen}}
set help_data(zoom)		{{scale_factor}	{zoom view in or out}}

proc help {args}	{
	global help_data

	if {[llength $args] > 0} {
                return [help_comm help_data $args]
        } else {
                return [help_comm help_data]
        }
}

proc ? {} {
	global help_data

	return [?_comm help_data 15 5]
}

proc apropos key {
	global help_data

	return [apropos_comm help_data $key]
}
