##                 V I E W . T C L
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
#	The View class wraps LIBRT's view object.
#
class View {
    public variable size 1000
    public variable center {0 0 0}
    public variable ae "0 90 0"
    public variable perspective_angle 0
    public variable coord "v"
    public variable rotate_about "v"
    public variable keypoint "0 0 0"
    public variable units "mm"

    constructor {args} {}
    destructor {}

    public method ae {args}
    public method arot {args}
    public method center {args}
    public method coord {args}
    public method eye {args}
    public method eye_pos {args}
    public method get_viewname {}
    public method invSize {args}
    public method keypoint {args}
    public method lookat {args}
    public method model2view {args}
    public method mrot {args}
    public method observer {args}
    public method orientation {args}
    public method perspective {args}
    public method pmat {args}
    public method pmodel2view {args}
    public method pov {args}
    public method rmat {args}
    public method rot {args}
    public method rotate_about {args}
    public method sca {args}
    public method setview {args}
    public method size {args}
    public method slew {args}
    public method tra {args}
    public method units {args}
    public method view2model {args}
    public method vrot {args}
    public method vtra {args}
    public method zoom {args}

    public method ? {}
    public method apropos {key}
    public method help {args}
    public method getUserCmds {}

    private method init {}
    private method help_init {}

    private variable view
    private variable help
}

configbody View::size {
    View::size $size
}

configbody View::center {
    eval View::center $center
}

configbody View::ae {
    eval View::ae $ae
}

configbody View::perspective_angle {
    View::perspective $perspective_angle
}

configbody View::coord {
    View::coord $coord
}

configbody View::rotate_about {
    View::rotate_about $rotate_about
}

configbody View::keypoint {
    eval View::keypoint $keypoint
}

configbody View::units {
    View::units $units
}

body View::constructor {args} {
    # first create view object
    set view [v_open $this\_view]

    # process options
    eval configure $args

    View::init
}

body View::destructor {} {
    rename $view ""
    catch {delete object $help}
}

body View::get_viewname {} {
    return $view
}

body View::ae {args} {
    # get ae
    if {$args == ""} {
	return $ae
    }

    # set ae
    eval $view ae $args
    set ae $args
    return
}

body View::arot {args} {
    eval $view arot $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::center {args} {
    # get center
    if {$args == ""} {
	return $center
    }

    # set center
    eval $view center $args
    set center $args
    return
}

body View::coord {args} {
    # get coord
    if {$args == ""} {
	return $coord
    }

    eval $view coord $args
    set coord $args
    return
}

body View::eye {args} {
    # get eye
    if {$args == ""} {
	return [$view eye]
    }

    eval $view eye $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::eye_pos {args} {
    eval $view eye_pos $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::invSize {args} {
    eval $view invSize $args
}

body View::keypoint {args} {
    # get keypoint
    if {$args == ""} {
	return $keypoint
    }

    eval $view keypoint $args
    set keypoint $args
    return
}

body View::lookat {args} {
    eval $view lookat $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::model2view {args} {
    eval $view model2view $args
}

body View::mrot {args} {
    eval $view mrot $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::observer {args} {
    eval $view observer $args
}

body View::orientation {args} {
    eval $view orientation $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::perspective {args} {
    # get perspective angle
    if {$args == ""} {
	return $perspective_angle
    }

    eval $view perspective $args
    set perspective_angle $args
    return
}

body View::pmat {args} {
    eval $view pmat $args
}

body View::pmodel2view {args} {
    eval $view pmodel2view $args
}

body View::pov {args} {
    eval $view pov $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::rmat {args} {
    # get rotation matrix
    if {$args == ""} {
	return [$view rmat]
    }

    eval $view rmat $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::rot {args} {
    # rotate view
    eval $view rot $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::rotate_about {args} {
    # get rotate_about
    if {$args == ""} {
	return $rotate_about
    }

    eval $view rotate_about $args
    set rotate_about $args
    return
}

body View::sca {args} {
    eval $view sca $args
    set size [$view size]
    return
}

body View::setview {args} {
    eval $view setview $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::size {args} {
    # get size
    if {$args == ""} {
	return $size
    }

    # set size
    eval $view size $args
    set size $args
    return
}

body View::slew {args} {
    # slew the view
    eval $view slew $args

    set center [$view center]
    return
}

body View::tra {args} {
    # translate view
    eval $view tra $args

    set center [$view center]
    return
}

body View::units {args} {
    # get units
    if {$args == ""} {
	return $units
    }

    # set units
    eval $view units $args
    set units $args
}

body View::view2model {args} {
    eval $view view2model
}

body View::vrot {args} {
    # rotate view
    eval $view rot -v $args

    set ae [$view ae]
    set center [$view center]
    return
}

body View::vtra {args} {
    # translate view
    eval $view tra -v $args

    set center [$view center]
    return
}

body View::zoom {args} {
    eval $view zoom $args

    set size [$view size]
    return
}

body View::? {} {
    return [$help ? 20 4]
}

body View::apropos {key} {
    return [$help apropos $key]
}

body View::getUserCmds {} {
    return [$help getCmds]
}

body View::help {args} {
    return [eval $help get $args]
}

body View::init {} {
    View::size $size
    eval View::center $center
    eval View::ae $ae
    View::perspective $perspective_angle
    View::coord $coord
    View::rotate_about $rotate_about
    View::keypoint $keypoint
    View::units $units
    View::help_init
}

body View::help_init {} {
    set help [cadwidgets::Help #auto]

    $help add ae		{{["az el tw"]} {set/get the azimuth, elevation and twist}}
    $help add arot		{{x y z angle} {rotate about axis x,y,z by angle (degrees)}}
    $help add center		{{["x y z"]} {set/get the view center}}
    $help add coord		{{[m|v]} {set/get the coodinate system}}
    $help add eye		{{mx my mz} {set eye point to given model coordinates}}	
    $help add eye_pos		{{mx my mz} {set eye position to given model coordinates}}
    $help add invSize		{{} {returns the inverse of view size}}
    $help add keypoint		{{[point]} {set/get the keypoint}}
    $help add lookat		{{x y z} {adjust view to look at given coordinates}}
    $help add model2view	{{} {returns the model2view matrix}}
    $help add mrot		{{x y z} {rotate view using model x,y,z}}
    $help add orientation	{{x y z w} {set view direction from quaternion}}
    $help add perspective	{{[angle]} {set/get the perspective angle}}
    $help add pmat		{{} {get the perspective matrix}}
    $help add pmodel2view	{{} {get the pmodel2view matrix}}
    $help add pov		{{args}	{experimental:  set point-of-view}}
    $help add rmat		{{} {get/set the rotation matrix}}
    $help add rot		{{"x y z"} {rotate the view}}
    $help add rotate_about	{{[e|k|m|v]} {set/get the rotate about point}}
    $help add sca		{{sfactor} {scale by sfactor}}
    $help add setview		{{x y z} {set the view given angles x, y, and z in degrees}}
    $help add size		{{vsize} {set/get the view size}}
    $help add slew		{{"x y"} {slew the view}}
    $help add tra		{{[-v|-m] "x y z"} {translate the view}}
    $help add units		{{[unit]} {get/set the local units}}
    $help add vrot		{{xdeg ydeg zdeg} {rotate viewpoint}}
    $help add vtra		{{"x y z"} {translate the view}}
    $help add zoom		{{sf} {zoom view by specified scale factor}}
}
