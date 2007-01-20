#                      W I Z A R D . T C L
# BRL-CAD
#
# Copyright (c) 2002-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
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

::itcl::class Wizard {
    inherit itk::Widget

    constructor {args} {}
    destructor {}

    public {
	method getWizardTop {}
	method setWizardTop {_wizardTop}
	method getWizardState {}
	method setWizardState {_wizardState}
	method getWizardOrigin {}
	method setWizardOrigin {_wizardOrigin}
	method getWizardAction {}
	method getWizardXmlAction {}
	method getWizardUnits {}

	method beginSystemXML {name {id ""}}
	method endSystemXML {}
	method beginTagXML {tag}
	method endTagXML {tag}
	method buildTagValXML {tag value}
	method beginZoneXML {name id}
	method endZoneXML {}
	method buildComponentXML {name id}
	method buildVirtualComponentXML {name material density p1 p2 p3}
	method buildWheelXML {type begin end}
	method buildGridZonesXML {name initialId xmax ymax zmax ignoreList}

	common wizardMajorType ""
	common wizardMinorType ""
	common wizardName ""
	common wizardVersion ""
	common wizardClass "Wizard"

	proc vectorAdd {u v}
	proc vectorScale {u s}
    }

    protected {
	variable wizardOrigin ""
	variable wizardState ""
	variable wizardTop ""
	variable wizardAction ""
	variable wizardXmlAction ""
	variable wizardUnits ""
    }
}

::itcl::body Wizard::getWizardTop {} {
    return $wizardTop
}

::itcl::body Wizard::setWizardTop {_wizardTop} {
    set wizardTop $_wizardTop
}

::itcl::body Wizard::getWizardState {} {
    return $wizardState
}

::itcl::body Wizard::setWizardState {_wizardState} {
    set wizardState $_wizardState
}

::itcl::body Wizard::getWizardOrigin {} {
    return $wizardOrigin
}

::itcl::body Wizard::setWizardOrigin {_wizardOrigin} {
    set wizardOrigin $_wizardOrigin
}

::itcl::body Wizard::getWizardAction {} {
    return $wizardAction
}

::itcl::body Wizard::getWizardXmlAction {} {
    return $wizardXmlAction
}

::itcl::body Wizard::getWizardUnits {} {
    return $wizardUnits
}

::itcl::body Wizard::beginSystemXML {name {id ""}} {
    append systemXML [beginTagXML "System"]
    append systemXML [buildTagValXML "Name" $name]

    if {$id != ""} {
	append systemXML [buildTagValXML "Geometry_Reference" [buildTagValXML "ID" $id]]
    }

    return $systemXML
}

::itcl::body Wizard::endSystemXML {} {
    endTagXML "System"
}

::itcl::body Wizard::beginTagXML {tag} {
    return "<$tag>"
}

::itcl::body Wizard::endTagXML {tag} {
    return "</$tag>"
}

::itcl::body Wizard::buildTagValXML {tag value} {
    append systemXML [beginTagXML $tag]
    append systemXML $value
    append systemXML [endTagXML $tag]
}

::itcl::body Wizard::beginZoneXML {name id} {
    append zoneXML [beginSystemXML $name $id]
}

::itcl::body Wizard::endZoneXML {} {
    append zoneXML [beginTagXML "Properties"]
    append zoneXML [beginTagXML "Boolean"]
    append zoneXML [buildTagValXML "Name" "Zone"]
    append zoneXML [buildTagValXML "Value" "true"]
    append zoneXML [beginTagXML "Properties"]
    append zoneXML [beginTagXML "String"]
    append zoneXML [buildTagValXML "Name" "Material"]
    append zoneXML [buildTagValXML "Value" "Steel"]
    append zoneXML [endTagXML "String"]
    append zoneXML [beginTagXML "Percent"]
    append zoneXML [buildTagValXML "Name" "Density"]
    append zoneXML [buildTagValXML "Value" "0.35"]
    append zoneXML [endTagXML "Percent"]
    append zoneXML [endTagXML "Properties"]
    append zoneXML [endTagXML "Boolean"]
    append zoneXML [endTagXML "Properties"]
    append zoneXML [endSystemXML]
}

::itcl::body Wizard::buildComponentXML {name id} {
    return "<Component><Name>$name</Name><Geometry_Reference><ID>$id</ID></Geometry_Reference><Material_Reference>Cold Rolled Steel</Material_Reference><Properties><Percent><Name>Density</Name><Value>100</Value></Percent></Properties></Component>"
}

::itcl::body Wizard::buildVirtualComponentXML {name material density p1 p2 p3} {
    return "<Component><Name>$name</Name><Material_Reference>$material</Material_Reference><Properties><Percent><Name>Density</Name><Value>$density</Value></Percent><String><Name>Zone Weighting</Name><Value>Average</Value><Properties><Percent><Name>Front</Name><Value>$p1</Value></Percent><Percent><Name>Left</Name><Value>$p2</Value></Percent><Percent><Name>Top</Name><Value>$p3</Value></Percent></Properties></String></Properties></Component>"
}

::itcl::body Wizard::buildWheelXML {type begin end} {
    append wheelXML [beginSystemXML "$type Wheels"]
    for {set id $begin; set n 1} {$id < $end} {incr id; incr n} {
	append wheelXML [buildComponentXML "$type Wheel $n" $id]
    }
    append wheelXML [endSystemXML]
}

::itcl::body Wizard::buildGridZonesXML {name initialId xmax ymax zmax ignoreList} {
    set zonesXML ""
    set zoneID 0
    for {set z 1} {$z <= $zmax} {incr z} {
	for {set y 1} {$y <= $ymax} {incr y} {
	    for {set x 1} {$x <= $xmax} {incr x} {
		incr zoneID

		if {[lsearch $ignoreList $zoneID] == -1} {
		    set rid [expr {$initialId + $zoneID}]
		    append zonesXML [beginZoneXML "$name $zoneID" $rid]
		    append zonesXML [endZoneXML]
		}
	    }
	}
    }

    return $zonesXML
}


######################## Class Methods ########################
#
::itcl::body Wizard::vectorAdd {u v} {
    return [list [expr {[lindex $u 0]+[lindex $v 0]}] \
		 [expr {[lindex $u 1]+[lindex $v 1]}] \
		 [expr {[lindex $u 2]+[lindex $v 2]}]]
}

::itcl::body Wizard::vectorScale {u s} {
    return [list [expr {[lindex $u 0] * $s}] \
		 [expr {[lindex $u 1] * $s}] \
		 [expr {[lindex $u 2] * $s}]]
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
