##
# TYPE: tcltk
##############################################################
#
# CombEditFrame.tcl
#
##############################################################
#
# Author(s):
#    Bob Parker
#
# Description:
#    The class for editing shader parameters within Archer.
#
##############################################################

::itcl::class ShaderEdit {
    inherit ::itk::Widget

    itk_option define -shaderChangedCallback shaderChangedCallback ShaderChangedCallback ""

    constructor {args} {}
    destructor {}

    public {
	proc validateDouble {d}

	method initShader {_shaderSpec}
	method getShaderSpec {}
    }

    protected {
	common SHADER_NAMES_AND_TYPES_FULL { 
	    {Plastic plastic}
	    {Mirror mirror}
	    {Glass glass}
	    {Light light}
	    {"Texture (color)" texture}
	    {"Texture (b/w)" bwtexture}
	    {"Bump Map" bump}
	    {Checker checker}
	    {"Test Map" testmap}
	    {"Fake Star Pattern" fakestar}
	    {Cloud cloud}
	    {Stack stack}
	    {"Env Map" envmap}
	    {Projection prj}
	    {Camouflage camo}
	    {Air air}
	    {Extern extern}
	    {unlisted unlisted}
	    {None ""}
	}

	common SHADER_NAMES { 
	    Plastic
	    Mirror
	    Glass
	    unlisted
	    None
	}

	common SHADER_TYPES { 
	    plastic
	    mirror
	    glass
	    unlisted
	    ""
	}

	common SHADER_NAMES_AND_TYPES { 
	    {Plastic plastic}
	    {Mirror mirror}
	    {Glass glass}
	    {unlisted unlisted}
	    {None ""}
	}

	common DEF_PLASTIC_TRANS 0
	common DEF_PLASTIC_REFL 0
	common DEF_PLASTIC_SPEC 0.7
	common DEF_PLASTIC_DIFF 0.3
	common DEF_PLASTIC_RI 1.0
	common DEF_PLASTIC_EXT 0
	common DEF_PLASTIC_SHINE 10
	common DEF_PLASTIC_EMISS 0

	common DEF_MIRROR_SHINE 4
	common DEF_MIRROR_SPEC 0.6
	common DEF_MIRROR_DIFF 0.4
	common DEF_MIRROR_TRANS 0
	common DEF_MIRROR_REFL 0.75
	common DEF_MIRROR_RI 1.65
	common DEF_MIRROR_EXT 0
	common DEF_MIRROR_EMISS 0

	common DEF_GLASS_SHINE 4
	common DEF_GLASS_SPEC 0.7
	common DEF_GLASS_DIFF 0.3
	common DEF_GLASS_TRANS 0.8
	common DEF_GLASS_REFL 0.1
	common DEF_GLASS_RI 1.65
	common DEF_GLASS_EXT 0
	common DEF_GLASS_EMISS 0

	common DEF_CAMO_LACUN 2.1753974
	common DEF_CAMO_HVAL 1.0
	common DEF_CAMO_OCTAVES 4.0
	common DEF_CAMO_SIZE 1.0
	common DEF_CAMO_SCALE  {1.0 1.0 1.0}
	common DEF_CAMO_C1 {97 74 41}
	common DEF_CAMO_C2 {26 77 10}
	common DEF_CAMO_C3 {38 38 38}
	common DEF_CAMO_T1 -0.25
	common DEF_CAMO_T2 0.25
	common DEF_CAMO_DELTA {1000.0 1000.0 1000.0}

	common DEF_LIGHT_FRACTION 1.0
	common DEF_LIGHT_ANGLE 180
	common DEF_LIGHT_TARGET {0 0 0}
	common DEF_LIGHT_LUMENS 1.0
	common DEF_LIGHT_INFINITE 0
	common DEF_LIGHT_VISIBLE 1
	common DEF_LIGHT_ICON ""

	common DEF_TEXTURE_FILE ""
	common DEF_TEXTURE_MIRROR 0
	common DEF_TEXTURE_WIDTH 512
	common DEF_TEXTURE_HEIGHT 512
	common DEF_TEXTURE_SCALE_U 1
	common DEF_TEXTURE_SCALE_V 1
	common DEF_TEXTURE_TRANS ""
	common DEF_TEXTURE_TRANS_VALID 0

	common COLOR_TEXTURE_NAME "texture"
	common BW_TEXTURE_NAME "bwtexture"

	common DEF_CHECKER_COLOR_A {255 255 255}
	common DEF_CHECKER_COLOR_B {0 0 0}
	common DEF_CHECKER_SCALE 2.0

	common DEF_CLOUD_THRESHOLD 0.35
	common DEF_CLOUD_RANGE 0.3

	common DEF_AIR_DENSITY 0.1
	common DEF_AIR_DELTA 0.0
	common DEF_AIR_SCALE 0.01

	common DEF_UNLISTED_NAME "unlisted"

	variable phongTrans
	variable phongRefl
	variable phongSpec
	variable phongDiff
	variable phongRi
	variable phongExt
	variable phongShine
	variable phongEmiss

	variable camoLacun
	variable camoHval
	variable camoOctaves
	variable camoSize
	variable camoScale
	variable camoC1
	variable camoC2
	variable camoC3
	variable camoT1
	variable camoT2
	variable camoDelta

	variable lightFraction
	variable lightAngle
	variable lightTarget
	variable lightLumens
	variable lightInfinite
	variable lightVisible
	variable lightIcon

	variable textureFile
	variable textureMirror
	variable textureWidth
	variable textureHeight
	variable textureScale_U
	variable textureScale_V
	variable textureTrans
	variable textureTransValid

	variable checkerColorA
	variable checkerColorB
	variable checkerScale

	variable cloudThreshold
	variable cloudRange

	variable airDensity
	variable airDelta
	variable airScale

	variable externFile

	variable projectionFile

	variable unlistedName
	variable unlistedParams

	variable envmapName

	variable stackList {}

	variable shaderSpec ""
	variable shaderType ""
	variable shaderTypeUnlisted 0
	variable ignoreShaderSpec 0

	variable allowCallbacks 1

	method buildShaderGUI {_shaderSpec}

	method build_plastic {parent id}
	method build_mirror {parent id}
	method build_glass {parent id}
	method buildPhong {parent id}

	method build_light {parent id}
	method buildLight {parent id}

	method build_unlisted {parent id}
	method buildUnlisted {parent id}

	method changeShader {stype}
	method updateShader {stype}
	method updateShaderForm {id}
	method destroyCurrentShader {}

	method updateForm_plastic {id}
	method setFormDefaults_plastic {id}

	method updateForm_mirror {id}
	method setFormDefaults_mirror {id}

	method updateForm_glass {id}
	method setFormDefaults_glass {id}

	method updatePhongForm {id}

	method updateForm_unlisted {id}

	method validateDouble_plastic {id d}
	method updatePlasticSpec {id}

	method validateDouble_mirror {id d}
	method updateMirrorSpec {id}

	method validateDouble_glass {id d}
	method updateGlassSpec {id}

	method updateUnlistedSpec {id}
    }

    private {
    }
}

# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body ShaderEdit::constructor {args} {
    buildShaderGUI ""

    eval itk_initialize $args
}

::itcl::body ShaderEdit::destructor {} {
}

# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------

::itcl::configbody ShaderEdit::shaderChangedCallback {
    # Nothing for now
}

# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------

::itcl::body ShaderEdit::initShader {_shaderSpec} {
    set allowCallbacks 0

    set shaderSpec $_shaderSpec
    updateShaderForm 0

    set allowCallbacks 1
}

::itcl::body ShaderEdit::getShaderSpec {} {
    return $shaderSpec
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body ShaderEdit::buildShaderGUI {_shaderSpec} {
    set shaderSpec $_shaderSpec

    itk_component add shaderL {
	::label $itk_interior.shaderL \
	    -text "Spec"
    } {}

    itk_component add shaderF {
	::frame $itk_interior.shaderF \
	    -relief sunken \
	    -bd 1
    } {}

    itk_component add shaderE {
	::entry $itk_component(shaderF).shaderE \
	    -relief flat \
	    -textvariable [::itcl::scope shaderSpec]
    } {}
    bind $itk_component(shaderE) <KeyRelease> [::itcl::code $this updateShaderForm 0]

    itk_component add shaderMB {
	::menubutton $itk_component(shaderF).shaderMB \
	    -relief raised \
	    -bd 1 \
	    -indicatoron 1 \
	    -menu $itk_component(shaderF).shaderMB.shaderM
    } {}

    itk_component add shaderM {
	::menu $itk_component(shaderMB).shaderM
    } {}

    foreach sname $SHADER_NAMES stype $SHADER_TYPES {
	$itk_component(shaderM) add command \
	    -label $sname \
	    -command [::itcl::code $this changeShader $stype]
    }

    itk_component add shaderBody {
	::frame $itk_interior.shaderBody \
	    -relief sunken \
	    -bd 1
    } {}

    set slen [llength $shaderSpec]
    if {$slen > 0} {
	set stype [lindex $shaderSpec 0]
	build_$stype $itk_component(shaderBody) 0
    }

    grid $itk_component(shaderE) $itk_component(shaderMB) \
	-sticky "ew"
    grid columnconfigure $itk_component(shaderF) 0 -weight 1

    set row 0
    grid $itk_component(shaderL) \
	-row $row \
	-column 0 \
	-sticky "e"
    grid $itk_component(shaderF) \
	-row $row \
	-column 1 \
	-sticky "ew"
    incr row
    grid $itk_component(shaderBody) \
	-row $row \
	-column 0 \
	-columnspan 2 \
	-sticky "nsew"

    grid rowconfigure $itk_interior 1 -weight 1
    grid columnconfigure $itk_interior 1 -weight 1
}

::itcl::body ShaderEdit::build_plastic {parent id} {
    set shaderType "plastic"
    set shaderTypeUnlisted 0
    buildPhong $parent $id
    setFormDefaults_plastic $id
}

::itcl::body ShaderEdit::build_mirror {parent id} {
    set shaderType "mirror"
    set shaderTypeUnlisted 0
    buildPhong $parent $id
    setFormDefaults_mirror $id
}

::itcl::body ShaderEdit::build_glass {parent id} {
    set shaderType "glass"
    set shaderTypeUnlisted 0
    buildPhong $parent $id
    setFormDefaults_glass $id
}

::itcl::body ShaderEdit::buildPhong {parent id} {
    itk_component add phong$id\F {
	::frame $parent.phong$id\F
    } {}

    set parent $itk_component(phong$id\F)

    itk_component add phongTrans$id\L {
	::label $parent.phongTrans$id\L \
	    -text "Transparency"
    } {}

    itk_component add phongTrans$id\E {
	::entry $parent.phongTrans$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongTrans($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType $id %P]
    } {}


    itk_component add phongRefl$id\L {
	::label $parent.phongRefl$id\L \
	    -text "Mirror Reflectance"
    } {}

    itk_component add phongRefl$id\E {
	::entry $parent.phongRefl$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongRefl($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType $id %P]
    } {}

    itk_component add phongSpec$id\L {
	::label $parent.phongSpec$id\L \
	    -text "Specular Reflectivity"
    } {}

    itk_component add phongSpec$id\E {
	::entry $parent.phongSpec$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongSpec($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType $id %P]
    } {}

    itk_component add phongDiff$id\L {
	::label $parent.phongDiff$id\L \
	    -text "Diffuse Reflectivity"
    } {}

    itk_component add phongDiff$id\E {
	::entry $parent.phongDiff$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongDiff($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType $id %P]
    } {}

    itk_component add phongRi$id\L {
	::label $parent.phongRi$id\L \
	    -text "Reflective Index"
    } {}

    itk_component add phongRi$id\E {
	::entry $parent.phongRi$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongRi($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType $id %P]
    } {}

    itk_component add phongExt$id\L {
	::label $parent.phongExt$id\L \
	    -text "Extinction"
    } {}

    itk_component add phongExt$id\E {
	::entry $parent.phongExt$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongExt($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType $id %P]
    } {}

    itk_component add phongShine$id\L {
	::label $parent.phongShine$id\L \
	    -text "Shininess"
    } {}

    itk_component add phongShine$id\E {
	::entry $parent.phongShine$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongShine($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType $id %P]
    } {}

    itk_component add phongEmiss$id\L {
	::label $parent.phongEmiss$id\L \
	    -text "Emission"
    } {}

    itk_component add phongEmiss$id\E {
	::entry $parent.phongEmiss$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongEmiss($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType $id %P]
    } {}

    set row 0
    grid $itk_component(phongTrans$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongTrans$id\E) -row $row -column 1 -sticky w
    grid $itk_component(phongRefl$id\L) -row $row -column 2 -sticky e
    grid $itk_component(phongRefl$id\E) -row $row -column 3 -sticky w
    incr row
    grid $itk_component(phongSpec$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongSpec$id\E) -row $row -column 1 -sticky w
    grid $itk_component(phongDiff$id\L) -row $row -column 2 -sticky e
    grid $itk_component(phongDiff$id\E) -row $row -column 3 -sticky w
    incr row
    grid $itk_component(phongRi$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongRi$id\E) -row $row -column 1 -sticky w
    grid $itk_component(phongShine$id\L) -row $row -column 2 -sticky e
    grid $itk_component(phongShine$id\E) -row $row -column 3 -sticky w
    incr row
    grid $itk_component(phongExt$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongExt$id\E) -row $row -column 1 -sticky w
    grid $itk_component(phongEmiss$id\L) -row $row -column 2 -sticky e
    grid $itk_component(phongEmiss$id\E) -row $row -column 3 -sticky w

    pack $itk_component(phong$id\F) -expand yes -fill both
}

::itcl::body ShaderEdit::build_light {parent id} {
    set shaderType "light"
    set shaderTypeUnlisted 0
    buildLight $parent $id

    set lightFraction($id) $DEF_LIGHT_FRACTION
    set lightAngle($id) $DEF_LIGHT_ANGLE
    set lightTarget($id) $DEF_LIGHT_TARGET
    set lightLumens($id) $DEF_LIGHT_LUMENS
    set lightInfinite($id) $DEF_LIGHT_INFINITE
    set lightVisible($id) $DEF_LIGHT_VISIBLE
    set lightIcon($id) $DEF_LIGHT_ICON
}

::itcl::body ShaderEdit::buildLight {parent id} {
    itk_component add light$id\F {
	::frame $parent.light$id\F
    } {}

    set parent $itk_component(light$id\F)

    itk_component add lightFraction$id\L {
	::label $parent.lightFraction$id\L \
	    -text "Fraction"
    } {}

    itk_component add lightFraction$id\E {
	::entry $parent.lightFraction$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope lightFraction($id)] \
	    -validate key \
	    -validatecommand {ShaderEdit::validateDouble %P}
    } {}

    itk_component add lightAngle$id\L {
	::label $parent.lightAngle$id\L \
	    -text "Angle"
    } {}

    itk_component add lightAngle$id\E {
	::entry $parent.lightAngle$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope lightAngle($id)] \
	    -validate key \
	    -validatecommand {ShaderEdit::validateDouble %P}
    } {}

    itk_component add lightTarget$id\L {
	::label $parent.lightTarget$id\L \
	    -text "Target"
    } {}

    itk_component add lightTarget$id\E {
	::entry $parent.lightTarget$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope lightTarget($id)] \
	    -validate key \
	    -validatecommand {ShaderEdit::validateDouble %P}
    } {}

    itk_component add lightLumens$id\L {
	::label $parent.lightLumens$id\L \
	    -text "Lumens"
    } {}

    itk_component add lightLumens$id\E {
	::entry $parent.lightLumens$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope lightLumens($id)] \
	    -validate key \
	    -validatecommand {ShaderEdit::validateDouble %P}
    } {}
}

::itcl::body ShaderEdit::build_unlisted {parent id} {
    set shaderType "unlisted"
    set shaderTypeUnlisted 1

    buildUnlisted $parent $id

    set slen [llength $shaderSpec]
    if {$slen == 1} {
	set unlistedName($id) [lindex $shaderSpec 0]
	set unlistedParams($id) ""
    } elseif {$slen == 2} {
	set unlistedName($id) [lindex $shaderSpec 0]
	set unlistedParams($id) [lrange $shaderSpec 1 end]
    } else {
	set unlistedName($id) ""
	set unlistedParams($id) ""
    }
}

::itcl::body ShaderEdit::buildUnlisted {parent id} {
    itk_component add unlisted$id\F {
	::frame $parent.unlisted$id\F
    } {}

    set parent $itk_component(unlisted$id\F)

    itk_component add unlistedName$id\L {
	::label $parent.unlistedName$id\L \
	    -text "Name"
    } {}

    itk_component add unlistedName$id\E {
	::entry $parent.unlistedName$id\E \
	    -textvariable [::itcl::scope unlistedName($id)]
    } {}
    bind $itk_component(unlistedName$id\E) <KeyRelease> \
	[::itcl::code $this updateUnlistedSpec 0]

    itk_component add unlistedParams$id\L {
	::label $parent.unlistedParams$id\L \
	    -text "Params"
    } {}

    itk_component add unlistedParams$id\E {
	::entry $parent.unlistedParams$id\E \
	    -textvariable [::itcl::scope unlistedParams($id)]
    } {}
    bind $itk_component(unlistedParams$id\E) <KeyRelease> \
	[::itcl::code $this updateUnlistedSpec 0]

    set row 0
    grid $itk_component(unlistedName$id\L) -row $row -column 0 -sticky e
    grid $itk_component(unlistedName$id\E) -row $row -column 1 -sticky nsew
    incr row
    grid $itk_component(unlistedParams$id\L) -row $row -column 0 -sticky e
    grid $itk_component(unlistedParams$id\E) -row $row -column 1 -sticky nsew

    grid columnconfigure $parent 1 -weight 1

    pack $parent -expand yes -fill both
}

## changeShader
#
# Destroy the current shader and build a new one.
#
::itcl::body ShaderEdit::changeShader {stype} {
    if {[llength $shaderSpec] > 0} {
	set o_stype [lindex $shaderSpec 0]

	if {$o_stype == $stype} {
	    # Nothing to do
	    return
	}
    }

    set shaderSpec $stype
    destroyCurrentShader

    if {$stype != ""} {
	build_$stype $itk_component(shaderBody) 0
    }
}

::itcl::body ShaderEdit::updateShader {stype} {
}

## updateShaderForm
#
# This method updates the shader form from
# the shader spec string.
#
::itcl::body ShaderEdit::updateShaderForm {id} {
    if {[catch {llength $shaderSpec} slen]} {
	# Ignore until braces match
	return
    }

    if {$slen > 0} {
	set stype [lindex $shaderSpec 0]

#	set ignoreShaderSpec 1

	if {$shaderType == $stype} {
	    updateForm_$stype $id
	} else {
	    set i [lsearch $SHADER_TYPES $stype]
	    if {$i == -1} {
		if {!$shaderTypeUnlisted} {
		    destroyCurrentShader
		    build_unlisted $itk_component(shaderBody) $id
		}
		updateForm_unlisted $id
	    } else {
		destroyCurrentShader
		build_$stype $itk_component(shaderBody) $id
		updateForm_$stype $id
	    }
	}

#	set ignoreShaderSpec 0
    } else {
	destroyCurrentShader
	set shaderType ""
	set shaderTypeUnlisted 0
    }

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::destroyCurrentShader {} {
    foreach child [pack slaves $itk_component(shaderBody)] {
	destroy $child
    }
}

::itcl::body ShaderEdit::updateForm_plastic {id} {
    setFormDefaults_plastic $id
    updatePhongForm $id
}

::itcl::body ShaderEdit::setFormDefaults_plastic {id} {
    set ignoreShaderSpec 1

    set phongTrans($id) $DEF_PLASTIC_TRANS
    set phongRefl($id) $DEF_PLASTIC_REFL
    set phongSpec($id) $DEF_PLASTIC_SPEC
    set phongDiff($id) $DEF_PLASTIC_DIFF
    set phongRi($id) $DEF_PLASTIC_RI
    set phongExt($id) $DEF_PLASTIC_EXT
    set phongShine($id) $DEF_PLASTIC_SHINE
    set phongEmiss($id) $DEF_PLASTIC_EMISS

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_mirror {id} {
    setFormDefaults_mirror $id
    updatePhongForm $id
}

::itcl::body ShaderEdit::setFormDefaults_mirror {id} {
    set ignoreShaderSpec 1

    set phongTrans($id) $DEF_MIRROR_TRANS
    set phongRefl($id) $DEF_MIRROR_REFL
    set phongSpec($id) $DEF_MIRROR_SPEC
    set phongDiff($id) $DEF_MIRROR_DIFF
    set phongRi($id) $DEF_MIRROR_RI
    set phongExt($id) $DEF_MIRROR_EXT
    set phongShine($id) $DEF_MIRROR_SHINE
    set phongEmiss($id) $DEF_MIRROR_EMISS

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_glass {id} {
    setFormDefaults_glass $id
    updatePhongForm $id
}

::itcl::body ShaderEdit::setFormDefaults_glass {id} {
    set ignoreShaderSpec 1

    set phongTrans($id) $DEF_GLASS_TRANS
    set phongRefl($id) $DEF_GLASS_REFL
    set phongSpec($id) $DEF_GLASS_SPEC
    set phongDiff($id) $DEF_GLASS_DIFF
    set phongRi($id) $DEF_GLASS_RI
    set phongExt($id) $DEF_GLASS_EXT
    set phongShine($id) $DEF_GLASS_SHINE
    set phongEmiss($id) $DEF_GLASS_EMISS

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updatePhongForm {id} {
    if {[llength $shaderSpec] < 2} {
	return
    }

    set ignoreShaderSpec 1

    foreach {key val} [lindex $shaderSpec 1] {
	if {$val != ""} {
	    set notEmptyVal 1
	} else {
	    set notEmptyVal 0
	}

	switch -- $key {
	    "tr" {
		if {$notEmptyVal && [string is double $val]} {
		    set phongTrans($id) $val
		}
	    }
	    "re" {
		if {$notEmptyVal && [string is double $val]} {
		    set phongRefl($id) $val
		}
	    }
	    "sp" {
		if {$notEmptyVal && [string is double $val]} {
		    set phongSpec($id) $val
		}
	    }
	    "di" {
		if {$notEmptyVal && [string is double $val]} {
		    set phongDiff($id) $val
		}
	    }
	    "ri" {
		if {$notEmptyVal && [string is double $val]} {
		    set phongRi($id) $val
		}
	    }
	    "sh" {
		if {$notEmptyVal && [string is double $val]} {
		    set phongShine($id) $val
		}
	    }
	    "ex" {
		if {$notEmptyVal && [string is double $val]} {
		    set phongExt($id) $val
		}
	    }
	    "em" {
		if {$notEmptyVal && [string is double $val]} {
		    set phongEm($id) $val
		}
	    }
	}
    }

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_unlisted {id} {
    set ignoreShaderSpec 1

    set slen [llength $shaderSpec]
    if {$slen == 1} {
	set unlistedName($id) [lindex $shaderSpec 0]
	set unlistedParams($id) ""
    } elseif {$slen == 2} {
	set unlistedName($id) [lindex $shaderSpec 0]
	set unlistedParams($id) [lrange $shaderSpec 1 end]
    } else {
	set unlistedName($id) ""
	set unlistedParams($id) ""
    }

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::validateDouble_plastic {id d} {
    if {![validateDouble $d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updatePlasticSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::updatePlasticSpec {id} {
    set newSpec ""

    if {$phongTrans($id) != $DEF_PLASTIC_TRANS} {
	if {$newSpec == ""} {
	    append newSpec "tr $phongTrans($id)"
	} else {
	    append newSpec " tr $phongTrans($id)"
	}
    }

    if {$phongRefl($id) != $DEF_PLASTIC_REFL} {
	if {$newSpec == ""} {
	    append newSpec "re $phongRefl($id)"
	} else {
	    append newSpec " re $phongRefl($id)"
	}
    }

    if {$phongSpec($id) != $DEF_PLASTIC_SPEC} {
	if {$newSpec == ""} {
	    append newSpec "sp $phongSpec($id)"
	} else {
	    append newSpec " sp $phongSpec($id)"
	}
    }

    if {$phongDiff($id) != $DEF_PLASTIC_DIFF} {
	if {$newSpec == ""} {
	    append newSpec "di $phongDiff($id)"
	} else {
	    append newSpec " di $phongDiff($id)"
	}
    }

    if {$phongRi($id) != $DEF_PLASTIC_RI} {
	if {$newSpec == ""} {
	    append newSpec "ri $phongRi($id)"
	} else {
	    append newSpec " ri $phongRi($id)"
	}
    }

    if {$phongExt($id) != $DEF_PLASTIC_EXT} {
	if {$newSpec == ""} {
	    append newSpec "ex $phongExt($id)"
	} else {
	    append newSpec " ex $phongExt($id)"
	}
    }

    if {$phongShine($id) != $DEF_PLASTIC_SHINE} {
	if {$newSpec == ""} {
	    append newSpec "sh $phongShine($id)"
	} else {
	    append newSpec " sh $phongShine($id)"
	}
    }

    if {$phongEmiss($id) != $DEF_PLASTIC_EMISS} {
	if {$newSpec == ""} {
	    append newSpec "em $phongEmiss($id)"
	} else {
	    append newSpec " em $phongEmiss($id)"
	}
    }

    set shaderSpec "plastic [list $newSpec]"

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::validateDouble_mirror {id d} {
    if {![validateDouble $d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateMirrorSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::updateMirrorSpec {id} {
    set newSpec ""

    if {$phongTrans($id) != $DEF_MIRROR_TRANS} {
	if {$newSpec == ""} {
	    append newSpec "tr $phongTrans($id)"
	} else {
	    append newSpec " tr $phongTrans($id)"
	}
    }

    if {$phongRefl($id) != $DEF_MIRROR_REFL} {
	if {$newSpec == ""} {
	    append newSpec "re $phongRefl($id)"
	} else {
	    append newSpec " re $phongRefl($id)"
	}
    }

    if {$phongSpec($id) != $DEF_MIRROR_SPEC} {
	if {$newSpec == ""} {
	    append newSpec "sp $phongSpec($id)"
	} else {
	    append newSpec " sp $phongSpec($id)"
	}
    }

    if {$phongDiff($id) != $DEF_MIRROR_DIFF} {
	if {$newSpec == ""} {
	    append newSpec "di $phongDiff($id)"
	} else {
	    append newSpec " di $phongDiff($id)"
	}
    }

    if {$phongRi($id) != $DEF_MIRROR_RI} {
	if {$newSpec == ""} {
	    append newSpec "ri $phongRi($id)"
	} else {
	    append newSpec " ri $phongRi($id)"
	}
    }

    if {$phongExt($id) != $DEF_MIRROR_EXT} {
	if {$newSpec == ""} {
	    append newSpec "ex $phongExt($id)"
	} else {
	    append newSpec " ex $phongExt($id)"
	}
    }

    if {$phongShine($id) != $DEF_MIRROR_SHINE} {
	if {$newSpec == ""} {
	    append newSpec "sh $phongShine($id)"
	} else {
	    append newSpec " sh $phongShine($id)"
	}
    }

    if {$phongEmiss($id) != $DEF_MIRROR_EMISS} {
	if {$newSpec == ""} {
	    append newSpec "em $phongEmiss($id)"
	} else {
	    append newSpec " em $phongEmiss($id)"
	}
    }

    set shaderSpec "mirror [list $newSpec]"

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::validateDouble_glass {id d} {
    if {![validateDouble $d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateGlassSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::updateGlassSpec {id} {
    set newSpec ""

    if {$phongTrans($id) != $DEF_GLASS_TRANS} {
	if {$newSpec == ""} {
	    append newSpec "tr $phongTrans($id)"
	} else {
	    append newSpec " tr $phongTrans($id)"
	}
    }

    if {$phongRefl($id) != $DEF_GLASS_REFL} {
	if {$newSpec == ""} {
	    append newSpec "re $phongRefl($id)"
	} else {
	    append newSpec " re $phongRefl($id)"
	}
    }

    if {$phongSpec($id) != $DEF_GLASS_SPEC} {
	if {$newSpec == ""} {
	    append newSpec "sp $phongSpec($id)"
	} else {
	    append newSpec " sp $phongSpec($id)"
	}
    }

    if {$phongDiff($id) != $DEF_GLASS_DIFF} {
	if {$newSpec == ""} {
	    append newSpec "di $phongDiff($id)"
	} else {
	    append newSpec " di $phongDiff($id)"
	}
    }

    if {$phongRi($id) != $DEF_GLASS_RI} {
	if {$newSpec == ""} {
	    append newSpec "ri $phongRi($id)"
	} else {
	    append newSpec " ri $phongRi($id)"
	}
    }

    if {$phongExt($id) != $DEF_GLASS_EXT} {
	if {$newSpec == ""} {
	    append newSpec "ex $phongExt($id)"
	} else {
	    append newSpec " ex $phongExt($id)"
	}
    }

    if {$phongShine($id) != $DEF_GLASS_SHINE} {
	if {$newSpec == ""} {
	    append newSpec "sh $phongShine($id)"
	} else {
	    append newSpec " sh $phongShine($id)"
	}
    }

    if {$phongEmiss($id) != $DEF_GLASS_EMISS} {
	if {$newSpec == ""} {
	    append newSpec "em $phongEmiss($id)"
	} else {
	    append newSpec " em $phongEmiss($id)"
	}
    }

    set shaderSpec "glass [list $newSpec]"

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}


::itcl::body ShaderEdit::updateUnlistedSpec {id} {
    set shaderSpec "$unlistedName($id) $unlistedParams($id)"

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::validateDouble {d} {
    if {$d == "" || $d == "-" || [string is double $d]} {
	return 1
    }

    return 0
}
