#==============================================================================
#
# TCL versions of MGED "help", "?", and "apropos" commands
#
#==============================================================================

set mged_help_data(?)		{{}	{summary of available mged commands}}
set mged_help_data(?lib)	{{}	{summary of available library commands}}
set mged_help_data(?devel)	{{}	{summary of available mged developer commands}}
set mged_help_data(%)		{{}	{Escape to an interactive shell. Note - This only works in a
        command window associated with a tty (i.e. the window used
        to start MGED in classic mode). }}
set mged_help_data(3ptarb)	{{}	{makes arb given 3 pts, 2 coord of 4th pt, and thickness}}
set mged_help_data(adc)		{{[<a1|a2|dst|dh|dv|hv|dx|dy|dz|xyz|reset|help> [value(s)]]}	{control the angle/distance cursor}}
set mged_help_data(ae)		{{[-i] azim elev [twist]}	{set view using azim, elev and twist angles}}
set mged_help_data(analyze)	{{[arbname]}	{analyze faces of ARB}}
set mged_help_data(animmate)	{{[parent]}	{tool for building animation scripts}}
set mged_help_data(apropos)	{{keyword}	{finds commands whose descriptions contain the given keyword}}
set mged_help_data(aproposlib)	{{keyword}	{finds library commands whose descriptions contain the given keyword}}
set mged_help_data(aproposdevel)	{{keyword}	{finds commands used for development whose descriptions
        contain the given keyword}}
set mged_help_data(arb)		{{name rot fb}	{make arb8, rotation + fallback}}
set mged_help_data(arced)	{{a/b ...anim_command...}	{edit matrix or materials on combination's arc}}
set mged_help_data(area)	{{[endpoint_tolerance]}	{calculate presented area of view (use ev -wT)}}
set mged_help_data(arot)            {{x y z angle} {rotate about axis x,y,z by angle (degrees)}}
set mged_help_data(attach)	{{[-d display_string] [-i init_script] [-n name]
	      [-t is_toplevel] [-W width] [-N height]
	      [-S square_size] win_type}	{attach to a display manager}}
set mged_help_data(attr)        {{object [attr_name [attr_value]] [attr_name attr_value ...]}
	      {get, assign or adjust attribute values for the specified object.
              with only an object specified,
                    displays all the attributes of that object.
              with an object and an attribute name specified,
                    displays the value of that attribute.
              with an object and attribute value pairs specified,
                    it sets the value of the specified attributes for that object}   }
set mged_help_data(attr_rm)     {{object attr_name [attr_name attr_name ...]}
	      {delete attributes for the specified object}}
set mged_help_data(autoview)	{{}	{set view size and center so that all displayed solids are in view}}
set mged_help_data(B)		{{-C#/#/# <objects>}	{clear screen, edit objects}}
set mged_help_data(bev)		{{[-t] [-P#] new_obj obj1 op obj2 op obj3 op ...}	{boolean evaluation of objects via NMG's}}
set mged_help_data(binary)      {{[-i|-o] -u type dest source}
                {manipulate opaque objects.
                 Must specify one of -i (for creating or adjusting objects (input))
                 or -o for extracting objects (output)
                 the "type" must be one of:
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
                 Only uniform array binary objects are currently supported}}
set mged_help_data(import_body)	{{object file type}	{read an object's body from a file}}
set mged_help_data(export_body)	{{file object}	{write an object's body to a file}}
set mged_help_data(bot_condense) {{new_bot_solid old_bot_solid} {remove unreferenced vertices in a BOT solid}}
set mged_help_data(bot_face_fuse) {{new_bot_solid old_bot_solid} {eliminate duplicate faces in a BOT solid}}
set mged_help_data(bot_vertex_fuse) {{new_bot_solid old_bot_solid} {fuse duplicate vertices in a BOT solid}}
set mged_help_data(build_region) {{[-a region_number] tag start end} {build a region from solids matching RE "tag.s*"}}
set mged_help_data(c)		{{[-gr] comb_name [boolean_expr]}	{create or extend a combination using standard notation}}
set mged_help_data(cat)		{{<objects>}	{list attributes (brief)}}
set mged_help_data(center)	{{x y z}	{set view center}}
set mged_help_data(color)	{{low high r g b str}	{make color entry}}
set mged_help_data(comb)	{{comb_name <operation solid>}	{create or extend combination w/booleans}}
set mged_help_data(comb_color)	{{comb R G B}	{assign a color to a combination (like 'mater')}}
set mged_help_data(copyeval)	{{new_solid path_to_old_solid}	{copy an 'evaluated' path solid}}
set mged_help_data(copymat)	{{a/b c/d}	{copy matrix from one combination's arc to another's}}
set mged_help_data(cp)		{{from to}	{copy [duplicate] object}}
set mged_help_data(cpi)		{{from to}	{copy cylinder and position at end of original cylinder}}
set mged_help_data(d)		{{<objects>}	{remove objects from the screen}}
set mged_help_data(dall)	{{<objects>}	{remove all occurrences of object(s) from the screen}}
set mged_help_data(db)		{{command}	{database manipulation routines}}
set mged_help_data(db_glob)	{{cmd_string}	{globs cmd_string against the MGED database
         resulting in an expanded command string}}
set mged_help_data(dbconcat)	{{file [prefix]}	{concatenate 'file' onto end of present database.  Run 'dup file' first.}}
set mged_help_data(dbfind)	{{[-s] <objects>}	{find all references to objects}}
set mged_help_data(debugbu)	{{[hex_code]}	{show/set debugging bit vector for libbu}}
set mged_help_data(debugdir)	{{}	{Print in-memory directory, for debugging}}
set mged_help_data(debuglib)	{{[hex_code]}	{show/set debugging bit vector for librt}}
set mged_help_data(debugmem)	{{}	{Print librt memory use map}}
set mged_help_data(debugnmg)	{{[hex code]}	{show/set debugging bit vector for NMG}}
set mged_help_data(decompose)	{{nmg_solid [prefix]}	{decompose nmg_solid into maximally connected shells}}
set mged_help_data(delay)	{{sec usec}	{delay for the specified amount of time}}
set mged_help_data(dm)		{{set var [val]}	{do display-manager specific command}}
set mged_help_data(draw)	{{-C#/#/# <objects>}	{draw objects}}
set mged_help_data(dup)		{{file [prefix]}	{check for dup names in 'file'}}
set mged_help_data(E)		{{ [-s] <objects>}	{evaluated edit of objects. Option 's' provides a slower,
        but better fidelity evaluation}}
set mged_help_data(e)		{{-C#/#/# <objects>}	{edit objects}}
set mged_help_data(eac)		{{air_code(s)}	{display all regions with given air code}}
set mged_help_data(echo)	{{[text]}	{echo arguments back}}
set mged_help_data(edcodes)	{{object(s)}	{edit region ident codes}}
set mged_help_data(edcolor)	{{}	{text edit color table}}
set mged_help_data(edcomb)	{{combname Regionflag regionid air los [material]}	{edit combination record info}}
set mged_help_data(edgedir)	{{[delta_x delta_y delta_z]|[rot fb]}	{define direction of ARB edge being moved}}
set mged_help_data(edmater)	{{comb(s)}	{edit combination materials}}
set mged_help_data(erase)	{{<objects>}	{remove objects from the screen}}
set mged_help_data(erase_all)	{{<objects>}	{remove all occurrences of object(s) from the screen}}
set mged_help_data(ev)		{{[-dfnqstuvwT] [-P #] <objects>}	{evaluate objects via NMG tessellation}}
set mged_help_data(eqn)		{{A B C}	{planar equation coefficients}}
set mged_help_data(exit)	{{}	{exit}}
set mged_help_data(extrude)	{{#### distance}	{extrude dist from face}}
set mged_help_data(expand)	{{expression}	{globs expression against MGED database objects}}
set mged_help_data(eye_pt)	{{mx my mz}	{set eye point to given model coordinates (in mm)}}
set mged_help_data(e_muves)	{{MUVES_component_1 MUVES_component2 ...}	{display listed MUVES components/systems}}
set mged_help_data(facedef)	{{####}	{define new face for an arb}}
set mged_help_data(facetize)	{{[-ntT] [-P#] new_obj old_obj(s)}	{convert objects to faceted BOT objects (or NMG for -n option) at current tol}}
set mged_help_data(fracture)	{{NMGsolid [prefix]}	{fracture an NMG solid into many NMG solids, each containing one face}}
set mged_help_data(g)		{{groupname <objects>}	{group objects}}
set mged_help_data(garbage_collect)	{{}	{eliminate unused space in database file}}
set mged_help_data(gui)	{{[-config b|c|g] [-d display_string]
        [-gd graphics_display_string] [-dt graphics_type]
        [-id name] [-c -h -j -s]}	{create display/command window pair}}
set mged_help_data(help)	{{[commands]}	{give usage message for given commands}}
set mged_help_data(helplib)	{{[library commands]}	{give usage message for given library commands}}
set mged_help_data(helpdevel)	{{[commands]}	{give usage message for given developer commands}}
set mged_help_data(hide)        {{[objects]} {set the "hidden" flag for the specified objects so they do not appear in a "t" or "ls" command output}}
set mged_help_data(history)	{{[-delays]}	{list command history}}
set mged_help_data(i)		{{obj combination [operation]}	{add instance of obj to comb}}
set mged_help_data(idents)		{{file object(s)}	{make ascii summary of region idents}}
set mged_help_data(ill)		{{name}	{illuminate object}}
set mged_help_data(in)		{{[-f] [-s] parameters...}	{keyboard entry of solids.  -f for no drawing, -s to enter solid edit}}
set mged_help_data(inside)	{{[outside_solid new_inside_solid thicknesses]}	{finds inside solid per specified thicknesses. Note that in an edit mode the edited solid is used for the outside_solid and should not appear on the command line }}
set mged_help_data(item)	{{region ident [air [material [los]]]}	{set region ident codes}}
set mged_help_data(joint)	{{command [options]}	{articulation/animation commands}}
set mged_help_data(journal)	{{[-d] fileName}	{record all commands and timings to journal}}
set mged_help_data(keep)	{{keep_file object(s)}	{save named objects in specified file}}
set mged_help_data(keypoint)	{{[x y z | reset]}	{set/see center of editing transformations}}
set mged_help_data(kill)	{{[-f] <objects>}	{delete object[s] from file}}
set mged_help_data(killall)	{{<objects>}	{kill object[s] and all references}}
set mged_help_data(killtree)	{{<object>}	{kill complete tree[s] - BE CAREFUL}}
set mged_help_data(knob)	{{[-e -i -m -o -v] [id [val]]}	{emulate knob twist}}
set mged_help_data(l)		{{[-r] <object(s)>}	{list attributes (verbose). Objects may be paths}}
set mged_help_data(l_muves)	{{MUVES_component1 MUVES_component2 ...} {list the MGED components that make up the specified MUVES components/systems}}
set mged_help_data(labelvert)	{{object[s]}	{label vertices of wireframes of objects}}
set mged_help_data(listeval)	{{}	{lists 'evaluated' path solids}}
set mged_help_data(loadtk)	{{[DISPLAY]}	{initializes the Tk window library}}
set mged_help_data(lookat)	{{x y z}	{adjust view to look at given coordinates}}
set mged_help_data(ls)		{{[-a -c -r -s -l]}	{table of contents}}
set mged_help_data(M)		{{1|0 xpos ypos}	{invoke a traditional MGED mouse event}}
set mged_help_data(make)	{{-t | name <arb8|sph|ellg|tor|tgc|rpc|rhc|epa|ehy|eto|part|grip|half|nmg|pipe>}	{create a primitive}}
set mged_help_data(make_bb)	{{new_rpp_name obj1_or_path1 [list of objects or paths ...]}	{make a bounding box solid enclosing specified objects/paths}}
set mged_help_data(mater)	{{comb [material]}	{assign/delete material to combination}}
set mged_help_data(matpick)	{{# or a/b}	{select arc which has matrix to be edited, in O_PATH state}}
set mged_help_data(memprint)	{{}	{print memory maps}}
set mged_help_data(mirface)	{{#### axis}	{mirror an ARB face}}
set mged_help_data(mirror)	{{old new axis}	{mirror solid or combination around axis}}
set mged_help_data(mrot)	{{x y z}	{rotate view using model x,y,z}}
set mged_help_data(mv)		{{old new}	{rename object}}
set mged_help_data(mvall)	{{oldname newname}	{rename object everywhere}}
set mged_help_data(nirt)	{{[nirt(1) options] [x y z]}	{trace a single ray from current view}}
set mged_help_data(nmg_collapse)	{{nmg_solid new_solid maximum_error_distance [minimum_allowed_angle]}	{decimate NMG solid via edge collapse}}
set mged_help_data(nmg_simplify)	{{[arb|tgc|ell|poly] new_solid nmg_solid}	{simplify nmg_solid, if possible}}
set mged_help_data(oed)		{{path_lhs path_rhs}	{go from view to object_edit of path_lhs/path_rhs}}
set mged_help_data(opendb)	{{[database.g]}	{close current .g file, and open new .g file}}
set mged_help_data(orientation)	{{x y z w}	{set view direction from quaternion}}
set mged_help_data(orot)	{{[-i] xdeg ydeg zdeg}	{rotate object being edited}}
set mged_help_data(oscale)	{{factor}	{scale object by factor}}
set mged_help_data(overlay)	{{file.plot [name]}	{read UNIX-Plot as named overlay}}
set mged_help_data(p)		{{dx [dy dz]}	{set parameters}}
set mged_help_data(pathlist)	{{name(s)}	{list all paths from name(s) to leaves}}
set mged_help_data(paths)	{{pattern}	{lists all paths matching input path}}
set mged_help_data(permute)	{{tuple}	{permute vertices of an ARB}}
set mged_help_data(plot)	{{[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]}	{make UNIX-plot of view}}
set mged_help_data(pl)		{{[-float] [-zclip] [-2d] [-grid] [out_file] [|filter]}	{Experimental - uses dm-plot:make UNIX-plot of view}}
set mged_help_data(polybinout)	{{file}	{store vlist polygons into polygon file (experimental)}}
set mged_help_data(pov)		{{args}	{experimental:  set point-of-view}}
set mged_help_data(prcolor)	{{}	{print color&material table}}
set mged_help_data(prefix)	{{new_prefix object(s)}	{prefix each occurrence of object name(s)}}
set mged_help_data(press)	{{button_label}	{emulate button press}}
set mged_help_data(prj_add)	{{ [-t] [-b] [-n] shaderfile [image_file] [image_width] [image_height]} {Appends image filename + current view parameters to shaderfile}}
set mged_help_data(preview)	{{[-v] [-d sec_delay] [-D start frame] [-K last frame] rt_script_file}	{preview new style RT animation script}}
set mged_help_data(ps)		{{[-f font] [-t title] [-c creator] [-s size in inches] [-l linewidth] file}	{creates a postscript file of the current view}}
set mged_help_data(push)	{{object[s]}	{pushes object's path transformations to solids}}
set mged_help_data(putmat)	{{a/b {I | m0 m1 ... m16}}	{replace matrix on combination's arc}}
set mged_help_data(q)		{{}	{quit}}
set mged_help_data(qray)	{{subcommand}	{get/set query_ray characteristics}}
set mged_help_data(query_ray)	{{[nirt(1) options] [x y z]}	{trace a single ray from current view}}
set mged_help_data(quit)	{{}	{quit}}
set mged_help_data(qorot)	{{x y z dx dy dz theta}	{rotate object being edited about specified vector}}
set mged_help_data(qvrot)	{{dx dy dz theta}	{set view from direction vector and twist angle}}
set mged_help_data(r)		{{region <operation solid>}	{create or extend a Region combination}}
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
set mged_help_data(release)	{{[name]}	{release display processor}}
set mged_help_data(rfarb)	{{}	{makes arb given point, 2 coord of 3 pts, rot, fb, thickness}}
set mged_help_data(rm)		{{comb <members>}	{remove members from comb}}
set mged_help_data(rmater)	{{filename}	{read combination materials from filename}}
set mged_help_data(rmats)	{{file}	{load view(s) from 'savekey' file}}
set mged_help_data(rot)		{{x y z} {rotate by x, y, z (degrees)}}
set mged_help_data(rotobj)	{{[-i] xdeg ydeg zdeg}	{rotate object being edited}}
set mged_help_data(rrt)		{{prog [options]}	{invoke prog with view}}
set mged_help_data(rt)		{{[options] [-- objects]}	{do raytrace of view or specified objects}}
set mged_help_data(rtcheck)	{{[options]}	{check for overlaps in current view}}
set mged_help_data(savekey)	{{file [time]}	{save keyframe in file (experimental)}}
set mged_help_data(saveview)	{{file [args]}	{save view in file for RT}}
set mged_help_data(sca)		{{sfactor} {scale by sfactor}}
set mged_help_data(sed)		{{<path>}	{solid-edit named solid}}
set mged_help_data(setview)	{{x y z}	{set the view given angles x, y, and z in degrees}}
set mged_help_data(shader)	{{comb {shader_name {keyword value keyword value ...}}}	{assign shader using Tcl list format}}
set mged_help_data(shells)	{{nmg_model}	{breaks model into seperate shells}}
set mged_help_data(showmats)	{{path}	{show xform matrices along path}}
set mged_help_data(size)	{{size}	{set view size}}
set mged_help_data(solids)	{{file object(s)}	{make ascii summary of solid parameters}}
set mged_help_data(status)	{{[state|Viewscale|base2local|local2base|
        toViewcenter|Viewrot|model2view|view2model|
        model2objview|objview2model|help]}	{get view status}}
set mged_help_data(summary)	{{[s r g]}	{count/list solid/reg/groups}}
set mged_help_data(sv)		{{x y [z]}	{move view center to (x, y, z)}}
set mged_help_data(sync)	{{}	{forces UNIX sync}}
set mged_help_data(t)		{{[-a -c -r -s]}	{table of contents}}
set mged_help_data(ted)		{{}	{text edit a solid's parameters}}
set mged_help_data(title)	{{[string]}	{print or change the title}}
set mged_help_data(tol)		{{[abs #] [rel #] [norm #] [dist #] [perp #]}	{show/set tessellation and calculation tolerances}}
set mged_help_data(tops)	{{}	{find all top level objects}}
set mged_help_data(tra)		{{dx dy dz} {translate by (dx,dy,dz)}}
set mged_help_data(track)	{{<parameters>}	{adds tracks to database}}
set mged_help_data(translate)	{{x y z}	{trans object to x,y, z}}
set mged_help_data(tree)	{{[-c] [-i n] [-o outfile] object(s)}	{print out a tree of all members of an object}}
set mged_help_data(t_muves)	{{}	{list all the known MUVES components/systems}}
set mged_help_data(unhide)        {{[objects]} {unset the "hidden" flag for the specified objects so they will appear in a "t" or "ls" command output}}
set mged_help_data(units)	{{[mm|cm|m|in|ft|...]}	{change units}}
set mged_help_data(vars)	{{[var=opt]}	{get/set mged variables}}
set mged_help_data(vdraw)	{{write|insert|delete|read|length|send [args]}	{Expermental drawing (cnuzman)}}
set mged_help_data(view)	{{center|size|eye|ypr|quat|aet}	{get/set view parameters (local units).}}
set mged_help_data(vnirt)	{{x y}  	{trace a single ray from x y}}
set mged_help_data(vquery_ray)	{{x y}  	{trace a single ray from x y}}
set mged_help_data(vrmgr)	{{host {master|slave|overview}}	{link with Virtual Reality manager}}
set mged_help_data(vrot)	{{xdeg ydeg zdeg}	{rotate viewpoint}}
set mged_help_data(wcodes)	{{filename object(s)}	{write region ident codes to filename}}
set mged_help_data(whatid)	{{region_name}	{display ident number for region}}
set mged_help_data(whichair)	{{air_codes(s)}	{lists all regions with given air code}}
set mged_help_data(whichid)	{{[-s] ident(s)}	{lists all regions with given ident code}}
set mged_help_data(which_shader)	{{Shader(s)}	{lists all combinations using the given shaders}}
set mged_help_data(who)		{{[r(eal)|p(hony)|b(oth)]}	{list the top-level objects currently being displayed}}
set mged_help_data(wmater)	{{filename comb(s)}	{write combination materials to filename}}
set mged_help_data(x)		{{[lvl]}	{print solid table & vector list}}
set mged_help_data(xpush)	{{object}	{Experimental Push Command}}
set mged_help_data(Z)		{{}	{zap all objects off screen}}
set mged_help_data(zoom)	{{scale_factor}	{zoom view in or out}}

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
