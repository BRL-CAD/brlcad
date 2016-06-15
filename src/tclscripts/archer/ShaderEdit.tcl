#                  S H A D E R E D I T . T C L
# BRL-CAD
#
# Copyright (c) 2006-2014 United States Government as represented by
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
	    {Unlisted unlisted}
	    {None ""}
	}

	common SHADER_NAMES {
	    Plastic
	    Mirror
	    Glass
	    Light
	    "Texture (color)"
	    "Texture (b/w)"
	    Bump
	    Checker
	    Testmap
	    Fakestar
	    Cloud
	    Stack
	    "Env Map"
	    Projection
	    Camouflage
	    Air
	    Extern
	    Unlisted
	    None
	}

	common SHADER_TYPES {
	    plastic
	    mirror
	    glass
	    light
	    texture
	    bwtexture
	    bump
	    checker
	    testmap
	    fakestar
	    cloud
	    stack
	    envmap
	    prj
	    camo
	    air
	    extern
	    unlisted
	    ""
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
	common DEF_LIGHT_SHADOW_RAYS 0

	common DEF_TEXTURE_FILE ""
	common DEF_TEXTURE_MIRROR 0
	common DEF_TEXTURE_WIDTH 512
	common DEF_TEXTURE_HEIGHT 512
	common DEF_TEXTURE_SCALE_UV "1 1"
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
	variable lightShadowRays
	variable lightInfinite
	variable lightVisible
	variable lightImages

	variable textureFile
	variable textureMirror
	variable textureWidth
	variable textureHeight
	variable textureScale_UV
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

	variable prevUnlistedName
	variable unlistedName
	variable unlistedParams

	variable envmapName

	variable stackList {}
	variable stackLen 0
	variable stackParams

	variable shaderSpec ""
	variable shaderType
	variable shaderTypeUnlisted
	variable ignoreShaderSpec 0

	variable allowCallbacks 1

	method buildShaderGUI {_shaderSpec}

	method build_plastic {parent id}
	method build_mirror {parent id}
	method build_glass {parent id}
	method build_checker {parent id}
	method build_cloud {parent id}
	method build_camo {parent id}
	method buildPhong {parent id}

	method build_light {parent id}
	method buildLight {parent id}

	method add_shader {stype parent}
	method delete_shader {id}
	method build_stack {parent id}

	method build_envmap {parent id}
	method select_shader {stype parent}

	method build_unlisted {parent id}
	method buildUnlisted {parent id}

	method build_air {parent id}
	method build_fakestar {parent id}
	method build_prj {parent id}
	method build_testmap {parent id}
	method build_texture {_parent _id}
	method build_bwtexture {_parent _id}
	method build_bump {_parent _id}
	method build_texture_common {_parent _id _type}
	method build_extern {parent id}

	method changeShader {}
	method updateShader {stype}
	method updateShaderForm {id}
	method destroyCurrentShader {}

	method updateForm_air {spec id}
	method setFormDefaults_air {id}

	method updateForm_plastic {spec id}
	method setFormDefaults_plastic {id}

	method updateForm_mirror {spec id}
	method setFormDefaults_mirror {id}

	method updateForm_glass {spec id}
	method setFormDefaults_glass {id}

	method updatePhongForm {spec id}

	method updateForm_checker {spec id}
	method setFormDefaults_checker {id}

	method updateForm_cloud {spec id}
	method setFormDefaults_cloud {id}

	method updateForm_camo {spec id}
	method setFormDefaults_camo {id}

	method updateForm_light {spec id}
	method setFormDefaults_light {id}

	method updateForm_stack {spec id}
	method setFormDefaults_stack {}

	method updateForm_envmap {spec id}

	method updateForm_unlisted {spec id}

	method updateForm_prj {spec id}

	method updateForm_fakestar {spec id}
	method updateForm_testmap {spec id}

	method updateForm_texture {_spec _id}
	method updateForm_bwtexture {_spec _id}
	method updateForm_bump {_spec _id}
	method updateForm_texture_common {_spec _id}
	method setFormDefaults_texture {id}

	method updateForm_extern {spec id}

	method validateDouble_air {id d}
	method updateAirSpec {id}

	method validateDouble_plastic {id d}
	method updatePlasticSpec {id}

	method validateDouble_mirror {id d}
	method updateMirrorSpec {id}

	method validateDouble_glass {id d}
	method updateGlassSpec {id}

	method validateDouble_light {id d}
	method validateTarget_light {id d}
	method updateLightSpec {id {unused ""}}

	method validate3TupleDoubles_camo {_id _t}
	method validateDouble_camo {id d}
	method validateRgb_camo {id rgb}
	method updateCamoSpec {id}

	method validateDouble_checker {id d}
	method validateRgb_checker {id rgb}
	method updateCheckerSpec {id}

	method validateDouble_cloud {id d}
	method updateCloudSpec {id}

	method updateUnlistedSpec {id}
	method updatePrjSpec {id}
	method updateExternSpec {id}

	method validateDigit_texture {_id _d _type}
	method validateScale_texture {_id _s _type}
	method validateRgb_texture {_id _rgb _type}
	method updateTextureSpec {_id _type}

	method finalSpecSetting {_type _newSpec}
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

    catch {unset shaderType}
    catch {unset shaderTypeUnlisted}
    set shaderType(0) ""
    set shaderTypeUnlisted(0) ""
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
	::ttk::label $itk_interior.shaderL \
	    -text "Spec"
    } {}

    itk_component add shaderCB {
	::ttk::combobox $itk_interior.shaderCB \
	    -state readonly \
	    -textvariable [::itcl::scope shaderSpec] \
	    -values $SHADER_NAMES
    } {}

    itk_component add shaderBody {
	::ttk::frame $itk_interior.shaderBody \
	    -relief sunken \
	    -borderwidth 1
    } {}

    set slen [llength $shaderSpec]
    if {$slen > 0} {
	set stype [lindex $shaderSpec 0]
	build_$stype $itk_component(shaderBody) 0
    }

    set row 0
    grid $itk_component(shaderL) \
	-row $row \
	-column 0 \
	-sticky "e"
    grid $itk_component(shaderCB) \
	-row $row \
	-column 1 \
	-sticky "ew"
    incr row
    grid $itk_component(shaderBody) \
	-row $row \
	-column 0 \
	-columnspan 2 \
	-sticky "nsew"

    grid rowconfigure $itk_interior $row -weight 1
    grid columnconfigure $itk_interior 1 -weight 1

    grid rowconfigure $itk_component(shaderBody) 0 -weight 1
    grid columnconfigure $itk_component(shaderBody) 0 -weight 1

    bind $itk_component(shaderCB) <<ComboboxSelected>> [::itcl::code $this changeShader]
}


::itcl::body ShaderEdit::build_checker {parent id} {
    set shaderType($id) "checker"
    set shaderTypeUnlisted($id) 0

    itk_component add checker$id\F {
	::ttk::frame $parent.checker$id\F
    } {}

    set parent $itk_component(checker$id\F)

    itk_component add checkerColorA$id\L {
	::ttk::label $parent.checkerColorA$id\L \
	    -text "Color A"
    } {}

    itk_component add checkerColorA$id\E {
	::ttk::entry $parent.checkerColorA$id\E \
	    -width 15 \
	    -textvariable [::itcl::scope checkerColorA($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateRgb_checker $id %P]
    } {}

    itk_component add checkerColorB$id\L {
	::ttk::label $parent.checkerColorB$id\L \
	    -text "Color B"
    } {}

    itk_component add checkerColorB$id\E {
	::ttk::entry $parent.checkerColorB$id\E \
	    -width 15 \
	    -textvariable [::itcl::scope checkerColorB($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateRgb_checker $id %P]
    } {}

    itk_component add checkerScale$id\L {
	::ttk::label $parent.checkerScale$id\L \
	    -text "Range"
    } {}

    itk_component add checkerScale$id\E {
	::ttk::entry $parent.checkerScale$id\E \
	    -width 15 \
	    -textvariable [::itcl::scope checkerScale($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_checker $id %P]
    } {}

    set row 0
    grid $itk_component(checkerColorA$id\L) -row $row -column 0 -sticky e
    grid $itk_component(checkerColorA$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(checkerColorB$id\L) -row $row -column 0 -sticky e
    grid $itk_component(checkerColorB$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(checkerScale$id\L) -row $row -column 0 -sticky e
    grid $itk_component(checkerScale$id\E) -row $row -column 1 -sticky w

    grid $itk_component(checker$id\F) -sticky nsew

    setFormDefaults_checker $id
}


::itcl::body ShaderEdit::build_cloud {parent id} {
    set shaderType($id) "cloud"
    set shaderTypeUnlisted($id) 0

    itk_component add cloud$id\F {
	::ttk::frame $parent.cloud$id\F
    } {}

    set parent $itk_component(cloud$id\F)

    itk_component add cloudthreshold$id\L {
	::ttk::label $parent.cloudthreshold$id\L \
	    -text "Threshold"
    } {}

    itk_component add cloudthreshold$id\E {
	::ttk::entry $parent.cloudthreshold$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope cloudThreshold($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_cloud $id %P]
    } {}

    itk_component add cloudrange$id\L {
	::ttk::label $parent.cloudrange$id\L \
	    -text "Range"
    } {}

    itk_component add cloudrange$id\E {
	::ttk::entry $parent.cloudrange$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope cloudRange($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_cloud $id %P]
    } {}

    set row 0
    grid $itk_component(cloudthreshold$id\L) -row $row -column 0 -sticky e
    grid $itk_component(cloudthreshold$id\E) -row $row -column 1 -sticky w
    grid $itk_component(cloudrange$id\L) -row $row -column 2 -sticky e
    grid $itk_component(cloudrange$id\E) -row $row -column 3 -sticky w

    #pack $itk_component(cloud$id\F) -expand yes -fill both
    grid $itk_component(cloud$id\F) -sticky nsew

    setFormDefaults_cloud $id
}


::itcl::body ShaderEdit::build_camo {parent id} {
    set shaderType($id) "camo"
    set shaderTypeUnlisted($id) 0

    itk_component add camo$id\F {
	::ttk::frame $parent.camo$id\F
    } {}

    set parent $itk_component(camo$id\F)

    itk_component add camoC1$id\L {
	::ttk::label $parent.camoC1$id\L \
	    -text "Background Color"
    } {}

    itk_component add camoC1$id\E {
	::ttk::entry $parent.camoC1$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoC1($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateRgb_camo $id %P]
    } {}

    itk_component add camoC2$id\L {
	::ttk::label $parent.camoC2$id\L \
	    -text "Color 1"
    } {}

    itk_component add camoC2$id\E {
	::ttk::entry $parent.camoC2$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoC2($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateRgb_camo $id %P]
    } {}

    itk_component add camoC3$id\L {
	::ttk::label $parent.camoC3$id\L \
	    -text "Color 2"
    } {}

    itk_component add camoC3$id\E {
	::ttk::entry $parent.camoC3$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoC3($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateRgb_camo $id %P]
    } {}

    itk_component add camoDelta$id\L {
	::ttk::label $parent.camoDelta$id\L \
	    -text "Noise Delta"
    } {}

    itk_component add camoDelta$id\E {
	::ttk::entry $parent.camoDelta$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoDelta($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validate3TupleDoubles_camo $id %P]
    } {}

    itk_component add camoScale$id\L {
	::ttk::label $parent.camoScale$id\L \
	    -text "Noise Scale"
    } {}

    itk_component add camoScale$id\E {
	::ttk::entry $parent.camoScale$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoScale($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validate3TupleDoubles_camo $id %P]
    } {}

    itk_component add camoSize$id\L {
	::ttk::label $parent.camo$id\L \
	    -text "Noise Size"
    } {}

    itk_component add camoSize$id\E {
	::ttk::entry $parent.camoSize$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoSize($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_camo $id %P]
    } {}

    itk_component add camoT1$id\L {
	::ttk::label $parent.camoT1$id\L \
	    -text "Threshold 1"
    } {}

    itk_component add camoT1$id\E {
	::ttk::entry $parent.camoT1$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoT1($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_camo $id %P]
    } {}

    itk_component add camoT2$id\L {
	::ttk::label $parent.camoT2$id\L \
	    -text "Threshold 2"
    } {}

    itk_component add camoT2$id\E {
	::ttk::entry $parent.camoT2$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoT2($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_camo $id %P]
    } {}

    itk_component add camoLacun$id\L {
	::ttk::label $parent.camoLacun$id\L \
	    -text "Lacunarity"
    } {}

    itk_component add camoLacun$id\E {
	::ttk::entry $parent.camoLacun$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoLacun($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_camo $id %P]
    } {}

    itk_component add camoHval$id\L {
	::ttk::label $parent.camoHval$id\L \
	    -text "H Value"
    } {}

    itk_component add camoHval$id\E {
	::ttk::entry $parent.camoHval$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoHval($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_camo $id %P]
    } {}

    itk_component add camoOctaves$id\L {
	::ttk::label $parent.camoOctaves$id\L \
	    -text "Octaves"
    } {}

    itk_component add camoOctaves$id\E {
	::ttk::entry $parent.camoOctaves$id\E \
	    -width 20 \
	    -textvariable [::itcl::scope camoOctaves($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_camo $id %P]
    } {}

    set row 0
    grid $itk_component(camoC1$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoC1$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoC2$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoC2$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoC3$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoC3$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoDelta$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoDelta$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoScale$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoScale$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoSize$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoSize$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoT1$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoT1$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoT2$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoT2$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoLacun$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoLacun$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoHval$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoHval$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(camoOctaves$id\L) -row $row -column 0 -sticky e
    grid $itk_component(camoOctaves$id\E) -row $row -column 1 -sticky w

    grid $itk_component(camo$id\F) -sticky nsew

    setFormDefaults_camo $id
}

::itcl::body ShaderEdit::build_plastic {parent id} {
    set shaderType($id) "plastic"
    set shaderTypeUnlisted($id) 0
    buildPhong $parent $id
    setFormDefaults_plastic $id
}

::itcl::body ShaderEdit::build_mirror {parent id} {
    set shaderType($id) "mirror"
    set shaderTypeUnlisted($id) 0
    buildPhong $parent $id
    setFormDefaults_mirror $id
}

::itcl::body ShaderEdit::build_glass {parent id} {
    set shaderType($id) "glass"
    set shaderTypeUnlisted($id) 0
    buildPhong $parent $id
    setFormDefaults_glass $id
}

::itcl::body ShaderEdit::buildPhong {parent id} {
    itk_component add phong$id\F {
	::ttk::frame $parent.phong$id\F
    } {}

    set parent $itk_component(phong$id\F)

    itk_component add phongTrans$id\L {
	::ttk::label $parent.phongTrans$id\L \
	    -text "Transparency"
    } {}

    itk_component add phongTrans$id\E {
	::ttk::entry $parent.phongTrans$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongTrans($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType($id) $id %P]
    } {}


    itk_component add phongRefl$id\L {
	::ttk::label $parent.phongRefl$id\L \
	    -text "Mirror Reflectance"
    } {}

    itk_component add phongRefl$id\E {
	::ttk::entry $parent.phongRefl$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongRefl($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType($id) $id %P]
    } {}

    itk_component add phongSpec$id\L {
	::ttk::label $parent.phongSpec$id\L \
	    -text "Specular Reflectivity"
    } {}

    itk_component add phongSpec$id\E {
	::ttk::entry $parent.phongSpec$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongSpec($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType($id) $id %P]
    } {}

    itk_component add phongDiff$id\L {
	::ttk::label $parent.phongDiff$id\L \
	    -text "Diffuse Reflectivity"
    } {}

    itk_component add phongDiff$id\E {
	::ttk::entry $parent.phongDiff$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongDiff($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType($id) $id %P]
    } {}

    itk_component add phongRi$id\L {
	::ttk::label $parent.phongRi$id\L \
	    -text "Reflective Index"
    } {}

    itk_component add phongRi$id\E {
	::ttk::entry $parent.phongRi$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongRi($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType($id) $id %P]
    } {}

    itk_component add phongShine$id\L {
	::ttk::label $parent.phongShine$id\L \
	    -text "Shininess"
    } {}

    itk_component add phongShine$id\E {
	::ttk::entry $parent.phongShine$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongShine($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType($id) $id %P]
    } {}

    itk_component add phongExt$id\L {
	::ttk::label $parent.phongExt$id\L \
	    -text "Extinction"
    } {}

    itk_component add phongExt$id\E {
	::ttk::entry $parent.phongExt$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongExt($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType($id) $id %P]
    } {}

    itk_component add phongEmiss$id\L {
	::ttk::label $parent.phongEmiss$id\L \
	    -text "Emission"
    } {}

    itk_component add phongEmiss$id\E {
	::ttk::entry $parent.phongEmiss$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope phongEmiss($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_$shaderType($id) $id %P]
    } {}

    set row 0
    grid $itk_component(phongTrans$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongTrans$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(phongRefl$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongRefl$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(phongSpec$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongSpec$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(phongDiff$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongDiff$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(phongRi$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongRi$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(phongShine$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongShine$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(phongExt$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongExt$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(phongEmiss$id\L) -row $row -column 0 -sticky e
    grid $itk_component(phongEmiss$id\E) -row $row -column 1 -sticky w

    #pack $itk_component(phong$id\F) -expand yes -fill both
    grid $itk_component(phong$id\F) -sticky nsew
}

::itcl::body ShaderEdit::build_light {parent id} {
    set shaderType($id) "light"
    set shaderTypeUnlisted($id) 0
    buildLight $parent $id
    setFormDefaults_light $id
}

::itcl::body ShaderEdit::buildLight {parent id} {
    itk_component add light$id\F {
	::ttk::frame $parent.light$id\F
    } {}

    set parent $itk_component(light$id\F)

    itk_component add lightFraction$id\L {
	::ttk::label $parent.lightFraction$id\L \
	    -text "Fraction"
    } {}

    itk_component add lightFraction$id\E {
	::ttk::entry $parent.lightFraction$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope lightFraction($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_light $id %P]
    } {}

    itk_component add lightAngle$id\L {
	::ttk::label $parent.lightAngle$id\L \
	    -text "Angle"
    } {}

    itk_component add lightAngle$id\E {
	::ttk::entry $parent.lightAngle$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope lightAngle($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_light $id %P]
    } {}

    itk_component add lightTarget$id\L {
	::ttk::label $parent.lightTarget$id\L \
	    -text "Target"
    } {}

    itk_component add lightTarget$id\E {
	::ttk::entry $parent.lightTarget$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope lightTarget($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateTarget_light $id %P]
    } {}

    itk_component add lightLumens$id\L {
	::ttk::label $parent.lightLumens$id\L \
	    -text "Lumens"
    } {}

    itk_component add lightLumens$id\E {
	::ttk::entry $parent.lightLumens$id\E \
	    -width 5 \
	    -textvariable [::itcl::scope lightLumens($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_light $id %P]
    } {}

    # Load the images
    if {![info exists lightImages(light_i0_v0_s0)]} {
	foreach i { 0 1 } {
	    foreach v { 0 1 } {
		foreach s { 0 1 2 3 4 5 6 7 8 9 } {
		    set lightImages(light_i${i}_v${v}_s${s}) \
			[image create photo -file \
			     [file join [bu_brlcad_data "tclscripts"] archer images l_i${i}_v${v}_s${s}.gif]]
		}
	    }
	}
    }

    itk_component add lightImage$id\L {
	::ttk::label $parent.lightImage$id\L \
	    -relief sunken \
	    -padding 0 \
	    -image $lightImages(light_i0_v0_s0)
    } {}

    itk_component add lightScale$id\S {
	scale $parent.lightScale$id\S \
	    -orient horiz \
	    -label "Shadow Rays" \
	    -from 0 \
	    -to 64 \
	    -bd 1 \
	    -relief sunken \
	    -variable [::itcl::scope lightShadowRays($id)] \
	    -command [::itcl::code $this updateLightSpec $id]
    }

    itk_component add lightInfinite$id\CB {
	::ttk::checkbutton $parent.lightInfinite$id\CB \
	    -text "Infinite" \
	    -variable [::itcl::scope lightInfinite($id)] \
	    -command [::itcl::code $this updateLightSpec $id]
    } {}

    itk_component add lightVisible$id\CB {
	::ttk::checkbutton $parent.lightVisible$id\CB \
	    -text "Visible" \
	    -variable [::itcl::scope lightVisible($id)] \
	    -command [::itcl::code $this updateLightSpec $id]
    } {}

    set row 0
    grid $itk_component(lightFraction$id\L) -row $row -column 0 -sticky e
    grid $itk_component(lightFraction$id\E) -row $row -column 1 -sticky w
    grid $itk_component(lightAngle$id\L) -row $row -column 2 -sticky e
    grid $itk_component(lightAngle$id\E) -row $row -column 3 -sticky w
    incr row
    grid $itk_component(lightTarget$id\L) -row $row -column 0 -sticky e
    grid $itk_component(lightTarget$id\E) -row $row -column 1 -sticky w
    grid $itk_component(lightLumens$id\L) -row $row -column 2 -sticky e
    grid $itk_component(lightLumens$id\E) -row $row -column 3 -sticky w
    incr row
    grid $itk_component(lightScale$id\S) -row $row -column 0 -columnspan 2 -sticky ew -padx 2 -pady 2
    set irow $row
    incr row
    grid $itk_component(lightInfinite$id\CB) -row $row -column 0
    grid $itk_component(lightVisible$id\CB) -row $row -column 1

    grid $itk_component(lightImage$id\L) -row $irow -column 2 -columnspan 2 -rowspan 2 -padx 2 -pady 2

    #pack $itk_component(light$id\F) -expand yes -fill both
    grid $itk_component(light$id\F) -sticky nsew
}

::itcl::body ShaderEdit::add_shader {stype parent} {
    set index $stackLen
    incr stackLen
    frame $parent.stk_$index -bd 2 -relief groove

    set parent $parent.stk_$index
    set stackParams(stk_$index,window) $parent
    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1

    if {[lsearch $SHADER_TYPES $stype] != -1} {
	label $parent.lab -text $stype -bg CadetBlue -fg white
    } else {
	label $parent.lab -text "Unrecognized Shader" -bg CadetBlue -fg white
    }
    grid $parent.lab -columnspan 4 -sticky ew
    set stackParams(stk_$index,name) $stype

    button $parent.del -text delete -width 8 \
	-command [::itcl::code $this delete_shader "stk_$index"]

    switch -- $stype {
	plastic {
	    build_plastic $parent stk_$index
	    set addspec [list plastic {}]
	}
	glass {
	    build_glass $parent stk_$index
	    set addspec [list glass {}]
	}
	mirror {
	    build_mirror $parent stk_$index
	    set addspec [list mirror {}]
	}
	light {
	    build_light $parent stk_$index
	    set addspec [list light {}]
	}
	bump -
	bwtexture -
	texture {
	    build_texture_common $parent stk_$index $stype
	    set addspec [list $stype {}]
	}
	checker {
	    build_checker $parent stk_$index
	    set addspec [list checker {}]
	}
	testmap {
	    build_testmap $parent stk_$index
	    set addspec testmap
	}
	fakestar {
	    build_fakestar $parent stk_$index
	    set addspec fakestar
	}
	cloud {
	    build_cloud $parent stk_$index
	    set addspec [list cloud {}]
	}
	prj {
	    build_prj $parent stk_$index
	    set addspec [list prj {}]
	}
	camo {
	    build_camo $parent stk_$index
	    set addspec [list camo {}]
	}
	air {
	    build_air $parent stk_$index
	    set addspec [list air {}]
	}
	default {
	    build_unlisted $parent stk_$index
	    set addspec [list unlisted {}]
	}
    }

    if {$shaderType(0) == "envmap"} {
	set spec [lindex $shaderSpec 1]
	set subspec [lindex $spec 1]
	lappend subspec $addspec
	set spec [lreplace $spec 1 1 $subspec]
	set shaderSpec [list envmap $spec]
    } else {
	set spec [lindex $shaderSpec 1]
	lappend spec $addspec
	set shaderSpec [list stack $spec]
    }

    grid $parent.del -sticky w
    grid $parent -sticky ew
    grid columnconfigure $parent 0 -minsize 400
}

::itcl::body ShaderEdit::delete_shader {id} {
    # destroy the shader subwindow
    catch {rename $stackParams($id,window) ""}

    # example - {envmap {stack {{camo {}} {plastic {}}}}}
    if {$shaderType(0) == "envmap"} {
	# if we get here, we know that the subtype is stack

	set spec [lindex $shaderSpec 1]
	set subspec [lindex $spec 1]
	set i [lsearch -index 0 $subspec $stackParams($id,name)]

	if {$i != -1} {
	    set subspec [lreplace $subspec $i $i]
	    set shaderSpec [list envmap [list stack $subspec]]
	}
    } else {
	set spec [lindex $shaderSpec 1]
	set i [lsearch -index 0 $spec $stackParams($id,name)]

	if {$i != -1} {
	    set spec [lreplace $spec $i $i]
	    set shaderSpec [list stack $spec]
	}
    }

    set stackParams($id,window) ""
    set stackParams($id,name) ""
}

::itcl::body ShaderEdit::build_stack {parent id} {
    set shaderType($id) "stack"
    set shaderTypeUnlisted($id) 0

    itk_component add stack$id\F {
	::ttk::frame $parent.stack$id\F
    } {}

    set parent $itk_component(stack$id\F)

    itk_component add stack$id\SF {
	::iwidgets::scrolledframe $parent.stack$id\SF -width 432 -height 250 -hscrollmode dynamic
    } {}

    set childsite [$itk_component(stack$id\SF) childsite]
    $childsite configure -background cyan

    itk_component add stackAdd$id\MB {
	::ttk::menubutton $parent.stackAdd$id\MB \
	    -text "Add Shader"
    } {}

    itk_component add stackAdd$id\M {
	::menu $parent.stackAdd$id\M \
	    -tearoff 0
    } {}

    $itk_component(stackAdd$id\M) add command \
	-label "Plastic" -command [::itcl::code $this add_shader plastic $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label "Glass" -command [::itcl::code $this add_shader glass $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label "Mirror" -command [::itcl::code $this add_shader mirror $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label "Light" -command [::itcl::code $this add_shader light $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label "Bump Map" -command [::itcl::code $this add_shader bump $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label "Texture (color)" -command [::itcl::code $this add_shader texture $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label "Texture (bw)" -command [::itcl::code $this add_shader bwtexture $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label Fakestar -command [::itcl::code $this add_shader fakestar $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label Cloud -command [::itcl::code $this add_shader cloud $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label Checker -command [::itcl::code $this add_shader checker $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label Camouflage -command [::itcl::code $this add_shader camo $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label Projection -command [::itcl::code $this add_shader prj $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label Air -command [::itcl::code $this add_shader air $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label Testmap -command [::itcl::code $this add_shader testmap $childsite]
    $itk_component(stackAdd$id\M) add command \
	-label Unlisted -command [::itcl::code $this add_shader unlisted $childsite]

    $itk_component(stackAdd$id\MB) configure -menu $itk_component(stackAdd$id\M)

    set row 0
    grid $itk_component(stackAdd$id\MB) -row $row -column 0 -sticky n
    incr row
    grid $itk_component(stack$id\SF) -row $row -column 0 -sticky nsew

    grid columnconfigure $itk_component(stack$id\F) 0 -weight 1
    grid rowconfigure $itk_component(stack$id\F) $row -weight 1
    grid $itk_component(stack$id\F) -sticky nsew

    setFormDefaults_stack
}

::itcl::body ShaderEdit::build_envmap {parent id} {
    set shaderType($id) "envmap"
    set shaderTypeUnlisted($id) 0

    itk_component add envmap$id\F {
	::ttk::frame $parent.envmap$id\F
    } {}

    set parent $itk_component(envmap$id\F)

    itk_component add envmapSelect$id\MB {
	::ttk::menubutton $parent.envmapSelect$id\MB \
	    -text "Select Shader"
    } {}

    itk_component add envmapSelect$id\M {
	::menu $parent.envmapSelect$id\M \
	    -tearoff 0
    } {}

    itk_component add childsite$id\F {
	::ttk::frame $parent.childsite$id\F
    } {}

    set childsite $itk_component(childsite$id\F)

    $itk_component(envmapSelect$id\M) add command \
	-label "Plastic" -command [::itcl::code $this select_shader plastic $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label "Glass" -command [::itcl::code $this select_shader glass $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label "Mirror" -command [::itcl::code $this select_shader mirror $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label "Bump Map" -command [::itcl::code $this select_shader bump $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label "Texture (color)" -command [::itcl::code $this select_shader texture $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label "Texture (bw)" -command [::itcl::code $this select_shader bwtexture $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label "Fakestar" -command [::itcl::code $this select_shader fakestar $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label Cloud -command [::itcl::code $this select_shader cloud $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label Checker -command [::itcl::code $this select_shader checker $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label Camouflage -command [::itcl::code $this select_shader camo $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label Projection -command [::itcl::code $this select_shader prj $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label Testmap -command [::itcl::code $this select_shader testmap $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label Unlisted -command [::itcl::code $this select_shader unlisted $childsite]
    $itk_component(envmapSelect$id\M) add command \
	-label Stack -command [::itcl::code $this select_shader stack $childsite]

    $itk_component(envmapSelect$id\MB) configure -menu $itk_component(envmapSelect$id\M)

    set row 0
    grid $itk_component(envmapSelect$id\MB) -row $row -column 0 -sticky n
    incr row
    grid $childsite -row $row -column 0 -sticky nsew

    grid rowconfigure $childsite 0 -weight 1
    grid columnconfigure $childsite 0 -weight 1

    grid columnconfigure $itk_component(envmap$id\F) 0 -weight 1
    grid rowconfigure $itk_component(envmap$id\F) $row -weight 1
    grid $itk_component(envmap$id\F) -sticky nsew
}

::itcl::body ShaderEdit::select_shader {stype parent} {
    set index 0
    if {[winfo exists $parent.env_$index]} {
	rename $parent.env_$index ""
    }

    frame $parent.env_$index -bd 2 -relief groove

    set parent $parent.env_$index
    set stackParams(env_$index,window) $parent
    grid rowconfigure $parent 1 -weight 1
    grid columnconfigure $parent 0 -weight 1

    if {[lsearch $SHADER_TYPES $stype] != -1} {
	label $parent.lab -text $stype -bg CadetBlue -fg white
    } else {
	label $parent.lab -text "Unlisted Shader" -bg CadetBlue -fg white
    }
    grid $parent.lab -columnspan 4 -sticky ew
    set stackParams(env_$index,name) $stype

    switch -- $stype {
	plastic {
	    build_plastic $parent env_$index
	    set addspec [list plastic {}]
	}
	glass {
	    build_glass $parent env_$index
	    set addspec [list glass {}]
	}
	mirror {
	    build_mirror $parent env_$index
	    set addspec [list mirror {}]
	}
	light {
	    build_light $parent env_$index
	    set addspec [list light {}]
	}
	bump -
	bwtexture -
	texture {
	    build_texture_common $parent env_$index $stype
	    set addspec [list $stype {}]
	}
	checker {
	    build_checker $parent env_$index
	    set addspec [list checker {}]
	}
	testmap {
	    build_testmap $parent env_$index
	    set addspec testmap
	}
	fakestar {
	    build_fakestar $parent env_$index
	    set addspec fakestar
	}
	cloud {
	    build_cloud $parent env_$index
	    set addspec [list cloud {}]
	}
	prj {
	    build_prj $parent env_$index
	    set addspec [list prj {}]
	}
	camo {
	    build_camo $parent env_$index
	    set addspec [list camo {}]
	}
	stack {
	    build_stack $parent env_$index
	    set addspec [list stack {}]
	}
	air {
	    build_air $parent env_$index
	    set addspec [list air {}]
	}
	default {
	    build_unlisted $parent env_$index
	    set addspec [list unlisted {}]
	}
    }

    set shaderSpec "envmap [list $addspec]"

    grid $parent -columnspan 2 -sticky nsew
}

::itcl::body ShaderEdit::build_unlisted {parent id} {
    set shaderType($id) "unlisted"
    set shaderTypeUnlisted($id) 1

    buildUnlisted $parent $id

    set prevUnlistedName($id) "unlisted"
    set unlistedName($id) "unlisted"
    set unlistedParams($id) ""
}

::itcl::body ShaderEdit::buildUnlisted {parent id} {
    itk_component add unlisted$id\F {
	::ttk::frame $parent.unlisted$id\F
    } {}

    set parent $itk_component(unlisted$id\F)

    itk_component add unlistedName$id\L {
	::ttk::label $parent.unlistedName$id\L \
	    -text "Name"
    } {}

    itk_component add unlistedName$id\E {
	::ttk::entry $parent.unlistedName$id\E \
	    -textvariable [::itcl::scope unlistedName($id)]
    } {}
    bind $itk_component(unlistedName$id\E) <KeyRelease> \
	[::itcl::code $this updateUnlistedSpec $id]

    itk_component add unlistedParams$id\L {
	::ttk::label $parent.unlistedParams$id\L \
	    -text "Params"
    } {}

    itk_component add unlistedParams$id\E {
	::ttk::entry $parent.unlistedParams$id\E \
	    -textvariable [::itcl::scope unlistedParams($id)]
    } {}
    bind $itk_component(unlistedParams$id\E) <KeyRelease> \
	[::itcl::code $this updateUnlistedSpec $id]

    set row 0
    grid $itk_component(unlistedName$id\L) -row $row -column 0 -sticky e
    grid $itk_component(unlistedName$id\E) -row $row -column 1 -sticky ew
    incr row
    grid $itk_component(unlistedParams$id\L) -row $row -column 0 -sticky e
    grid $itk_component(unlistedParams$id\E) -row $row -column 1 -sticky ew

    grid $itk_component(unlisted$id\F) -sticky nsew
    grid columnconfigure $itk_component(unlisted$id\F) 1 -weight 1
}

::itcl::body ShaderEdit::build_air {parent id} {
    set shaderType($id) "air"
    set shaderTypeUnlisted($id) 0

    itk_component add air$id\F {
	::ttk::frame $parent.air$id\F
    } {}

    set parent $itk_component(air$id\F)

    itk_component add airDensity$id\L {
	::ttk::label $parent.airDensity$id\L \
	    -text "Density"
    } {}

    itk_component add airDensity$id\E {
	::ttk::entry $parent.airDensity$id\E \
	    -width 15 \
	    -textvariable [::itcl::scope airDensity($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_air $id %P]
    } {}

    itk_component add airDelta$id\L {
	::ttk::label $parent.airDelta$id\L \
	    -text "Delta"
    } {}

    itk_component add airDelta$id\E {
	::ttk::entry $parent.airDelta$id\E \
	    -width 15 \
	    -textvariable [::itcl::scope airDelta($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_air $id %P]
    } {}

    itk_component add airScale$id\L {
	::ttk::label $parent.airScale$id\L \
	    -text "Scale"
    } {}

    itk_component add airScale$id\E {
	::ttk::entry $parent.airScale$id\E \
	    -width 15 \
	    -textvariable [::itcl::scope airScale($id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble_air $id %P]
    } {}

    set row 0
    grid $itk_component(airDensity$id\L) -row $row -column 0 -sticky e
    grid $itk_component(airDensity$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(airDelta$id\L) -row $row -column 0 -sticky e
    grid $itk_component(airDelta$id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(airScale$id\L) -row $row -column 0 -sticky e
    grid $itk_component(airScale$id\E) -row $row -column 1 -sticky w

    grid $itk_component(air$id\F) -sticky nsew
    grid columnconfigure $itk_component(air$id\F) 1 -weight 1

    setFormDefaults_air $id
}

::itcl::body ShaderEdit::build_fakestar {parent id} {
    set shaderType($id) "fakestar"
    set shaderTypeUnlisted($id) 0

    itk_component add fakestar$id\F {
	::ttk::frame $parent.fakestar$id\F
    } {}

    set parent $itk_component(fakestar$id\F)

    itk_component add fakestarFile$id\L {
	::ttk::label $parent.fakestarFile$id\L \
	    -text "There are no parameters to set \n\
		   for the fakestar texture map"
    } {}

    set row 0
    grid $itk_component(fakestarFile$id\L) -row $row -column 0 -sticky e

    grid $itk_component(fakestar$id\F) -sticky nsew
    grid columnconfigure $itk_component(fakestar$id\F) 1 -weight 1
}

::itcl::body ShaderEdit::build_prj {parent id} {
    set shaderType($id) "prj"
    set shaderTypeUnlisted($id) 0

    itk_component add prj$id\F {
	::ttk::frame $parent.prj$id\F
    } {}

    set parent $itk_component(prj$id\F)

    itk_component add prjFile$id\L {
	::ttk::label $parent.prjFile$id\L \
	    -text "Parameter File"
    } {}

    itk_component add prjFile$id\E {
	::ttk::entry $parent.prjFile$id\E \
	    -textvariable [::itcl::scope projectionFile($id)]
    } {}
    bind $itk_component(prjFile$id\E) <KeyRelease> \
	[::itcl::code $this updatePrjSpec $id]

    set row 0
    grid $itk_component(prjFile$id\L) -row $row -column 0 -sticky e
    grid $itk_component(prjFile$id\E) -row $row -column 1 -sticky w

    grid $itk_component(prj$id\F) -sticky nsew
    grid columnconfigure $itk_component(prj$id\F) 1 -weight 1

    set projectionFile($id) ""
}

::itcl::body ShaderEdit::build_testmap {parent id} {
    set shaderType($id) "testmap"
    set shaderTypeUnlisted($id) 0

    itk_component add testmap$id\F {
	::ttk::frame $parent.testmap$id\F
    } {}

    set parent $itk_component(testmap$id\F)

    itk_component add testmapFile$id\L {
	::ttk::label $parent.testmapFile$id\L \
	    -text "There are no parameters to set \n\
		   for the testmap"
    } {}

    set row 0
    grid $itk_component(testmapFile$id\L) -row $row -column 0 -sticky e

    grid $itk_component(testmap$id\F) -sticky nsew
    grid columnconfigure $itk_component(testmap$id\F) 1 -weight 1
}

::itcl::body ShaderEdit::build_texture {_parent _id} {
    build_texture_common $_parent $_id texture
}

::itcl::body ShaderEdit::build_bwtexture {_parent _id} {
    build_texture_common $_parent $_id bwtexture
}

::itcl::body ShaderEdit::build_bump {_parent _id} {
    build_texture_common $_parent $_id bump
}

::itcl::body ShaderEdit::build_texture_common {_parent _id _type} {
    set shaderType($_id) $_type
    set shaderTypeUnlisted($_id) 0

    itk_component add texture$_id\F {
	::ttk::frame $_parent.texture$_id\F
    } {}

    set parent $itk_component(texture$_id\F)

    itk_component add textureFile$_id\L {
	::ttk::label $parent.textureFile$_id\L \
	    -text "Texture File"
    } {}

    itk_component add textureFile$_id\E {
	::ttk::entry $parent.textureFile$_id\E \
	    -textvariable [::itcl::scope textureFile($_id)]
    } {}
    bind $itk_component(textureFile$_id\E) <KeyRelease> \
	[::itcl::code $this updateTextureSpec $_id $_type]

    itk_component add textureWidth$_id\L {
	::ttk::label $parent.textureWidth$_id\L \
	    -text "Texture Width (pixels)"
    } {}

    itk_component add textureWidth$_id\E {
	::ttk::entry $parent.textureWidth$_id\E \
	    -textvariable [::itcl::scope textureWidth($_id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDigit_texture $_id %P $_type]
    } {}

    itk_component add textureHeight$_id\L {
	::ttk::label $parent.textureHeight$_id\L \
	    -text "Texture Height (pixels)"
    } {}

    itk_component add textureHeight$_id\E {
	::ttk::entry $parent.textureHeight$_id\E \
	    -textvariable [::itcl::scope textureHeight($_id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDigit_texture $_id %P $_type]
    } {}

    itk_component add textureScaleUV$_id\L {
	::ttk::label $parent.textureScaleU$_id\L \
	    -text "Texture Scale UV"
    } {}

    itk_component add textureScaleUV$_id\E {
	::ttk::entry $parent.textureScaleUV$_id\E \
	    -textvariable [::itcl::scope textureScale_UV($_id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateScale_texture $_id %P $_type]
    } {}

    itk_component add textureMirror$_id\CB {
	::ttk::checkbutton $parent.textureMirror$_id\CB \
	    -text "Mirror Adjacent Tiles" \
	    -variable [::itcl::scope textureMirror($_id)] \
	    -command [::itcl::code $this updateTextureSpec $_id $_type]
    } {}

    itk_component add textureTrans$_id\L {
	::ttk::label $parent.textureTrans$_id\L \
	    -text "Transparency (RGB)"
    } {}

    itk_component add textureTrans$_id\E {
	::ttk::entry $parent.textureTrans$_id\E \
	    -textvariable [::itcl::scope textureTrans($_id)] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateRgb_texture $_id %P $_type]
    } {}

    set row 0
    grid $itk_component(textureFile$_id\L) -row $row -column 0 -sticky e
    grid $itk_component(textureFile$_id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(textureWidth$_id\L) -row $row -column 0 -sticky e
    grid $itk_component(textureWidth$_id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(textureHeight$_id\L) -row $row -column 0 -sticky e
    grid $itk_component(textureHeight$_id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(textureScaleUV$_id\L) -row $row -column 0 -sticky e
    grid $itk_component(textureScaleUV$_id\E) -row $row -column 1 -sticky w
    incr row
    grid $itk_component(textureMirror$_id\CB) -row $row -column 0 -sticky e
    incr row
    grid $itk_component(textureTrans$_id\L) -row $row -column 0 -sticky e
    grid $itk_component(textureTrans$_id\E) -row $row -column 1 -sticky w
    grid $itk_component(texture$_id\F) -sticky nsew

    setFormDefaults_texture $_id
}

::itcl::body ShaderEdit::build_extern {parent id} {
    set shaderType($id) "extern"
    set shaderTypeUnlisted($id) 0

    itk_component add extern$id\F {
	::ttk::frame $parent.extern$id\F
    } {}

    set parent $itk_component(extern$id\F)

    itk_component add externFile$id\L {
	::ttk::label $parent.externFile$id\L \
	    -text "File"
    } {}

    itk_component add externFile$id\E {
	::ttk::entry $parent.externFile$id\E \
	    -textvariable [::itcl::scope externFile($id)]
    } {}
    bind $itk_component(externFile$id\E) <KeyRelease> \
	[::itcl::code $this updateExternSpec $id]

    set row 0
    grid $itk_component(externFile$id\L) -row $row -column 0 -sticky e
    grid $itk_component(externFile$id\E) -row $row -column 1 -sticky w

    grid $itk_component(extern$id\F) -sticky nsew
    grid columnconfigure $itk_component(extern$id\F) 1 -weight 1

    set externFile($id) ""
}

## changeShader
#
# Destroy the current shader and build a new one.
#
::itcl::body ShaderEdit::changeShader {} {
    switch -- $shaderSpec {
	"Plastic" {
	    set stype plastic
	}
	"Mirror" {
	    set stype mirror
	}
	"Glass" {
	    set stype glass
	}
	"Light" {
	    set stype light
	}
	"Checker" {
	    set stype checker
	}
	"Cloud" {
	    set stype cloud
	}
	"Stack" {
	    set stype stack
	}
	"Env Map" {
	    set stype envmap
	}
	"Camouflage" {
	    set stype camo
	}
	"Air" {
	    set stype air
	}
	"Unlisted" {
	    set stype unlisted
	}
	"Projection" {
	    set stype prj
	}
	"Fakestar" {
	    set stype fakestar
	}
	"Testmap" {
	    set stype testmap
	}
	"Extern" {
	    set stype extern
	}
	"Texture (color)" {
	    set stype texture
	}
	"Texture (b/w)" {
	    set stype bwtexture
	}
	"Bump" {
	    set stype bump
	}
	default {
	    set stype ""
	}
    }

    set shaderSpec $stype

    if {$shaderType(0) == $stype} {
	# Nothing to do
	return
    }

    destroyCurrentShader
    unset shaderType
    unset shaderTypeUnlisted
    set shaderType(0) ""
    set shaderTypeUnlisted(0) ""

    if {$stype != ""} {
	build_$stype $itk_component(shaderBody) 0
    } else {
    }

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
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

	if {$shaderType($id) == $stype} {
	    updateForm_$stype $shaderSpec $id
	} else {
	    set i [lsearch $SHADER_TYPES $stype]
	    if {$i == -1} {
		destroyCurrentShader
		build_unlisted $itk_component(shaderBody) $id
		updateForm_unlisted $shaderSpec $id
	    } else {
		destroyCurrentShader
		build_$stype $itk_component(shaderBody) $id
		updateForm_$stype $shaderSpec $id
	    }
	}

	#	set ignoreShaderSpec 0
    } else {
	destroyCurrentShader
	unset shaderType
	unset shaderTypeUnlisted
	set shaderType(0) ""
	set shaderTypeUnlisted(0) ""
    }

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::destroyCurrentShader {} {
    foreach child [grid slaves $itk_component(shaderBody)] {
	rename $child ""
    }
}

::itcl::body ShaderEdit::updateForm_air {spec id} {
    setFormDefaults_air $id

    set ignoreShaderSpec 1
    foreach {key val} [lindex $spec 1] {
	if {$val != ""} {
	    set notEmptyVal 1
	} else {
	    set notEmptyVal 0
	}

	switch -- $key {
	    "dpm" {
		if {$notEmptyVal && [string is double $val]} {
		    set airDensity($id) $val
		}
	    }
	    "d" {
		if {$notEmptyVal && [string is double $val]} {
		    set airDelta($id) $val
		}
	    }
	    "s" {
		if {$notEmptyVal && [string is double $val]} {
		    set airScale($id) $val
		}
	    }
	}
    }

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::setFormDefaults_air {id} {
    set ignoreShaderSpec 1

    set airDensity($id) $DEF_AIR_DENSITY
    set airDelta($id) $DEF_AIR_DELTA
    set airScale($id) $DEF_AIR_SCALE

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_plastic {spec id} {
    setFormDefaults_plastic $id
    updatePhongForm $spec $id
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

::itcl::body ShaderEdit::updateForm_mirror {spec id} {
    setFormDefaults_mirror $id
    updatePhongForm $spec $id
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

::itcl::body ShaderEdit::updateForm_glass {spec id} {
    setFormDefaults_glass $id
    updatePhongForm $spec $id
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

::itcl::body ShaderEdit::updatePhongForm {spec id} {
    if {[llength $shaderSpec] < 2} {
	return
    }

    set ignoreShaderSpec 1

    foreach {key val} [lindex $spec 1] {
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
		    set phongEmiss($id) $val
		}
	    }
	}
    }

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_checker {spec id} {
    setFormDefaults_checker $id

    set ignoreShaderSpec 1
    foreach {key val} [lindex $spec 1] {
	if {$val != ""} {
	    set notEmptyVal 1
	} else {
	    set notEmptyVal 0
	}

	switch -- $key {
	    "a" {
		if {$notEmptyVal && [llength $val] == 3} {
		    set checkerColorA($id) $val
		}
	    }
	    "b" {
		if {$notEmptyVal && [llength $val] == 3} {
		    set checkerColorB($id) $val
		}
	    }
	    "s" {
		if {$notEmptyVal && [string is double $val]} {
		    set checkerScale($id) $val
		}
	    }
	}
    }
    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::setFormDefaults_checker {id} {
    set ignoreShaderSpec 1

    set checkerColorA($id) $DEF_CHECKER_COLOR_A
    set checkerColorB($id) $DEF_CHECKER_COLOR_B
    set checkerScale($id) $DEF_CHECKER_SCALE

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_cloud {spec id} {
    setFormDefaults_cloud $id

    set ignoreShaderSpec 1
    foreach {key val} [lindex $spec 1] {
	if {$val != ""} {
	    set notEmptyVal 1
	} else {
	    set notEmptyVal 0
	}

	switch -- $key {
	    "range" {
		if {$notEmptyVal && [string is double $val]} {
		    set cloudRange($id) $val
		}
	    }
	    "thresh" {
		if {$notEmptyVal && [string is double $val]} {
		    set cloudThreshold($id) $val
		}
	    }
	}
    }
    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::setFormDefaults_cloud {id} {
    set ignoreShaderSpec 1

    set cloudThreshold($id) $DEF_CLOUD_THRESHOLD
    set cloudRange($id) $DEF_CLOUD_RANGE

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_camo {spec id} {
    setFormDefaults_checker $id

    set ignoreShaderSpec 1
    foreach {key val} [lindex $spec 1] {
	if {$val != ""} {
	    set notEmptyVal 1
	} else {
	    set notEmptyVal 0
	}

	switch -- $key {
	    "l" {
		if {$notEmptyVal && [string is double $val]} {
		    set camoLacun($id) $val
		}
	    }
	    "H" {
		if {$notEmptyVal && [string is double $val]} {
		    set camoHval($id) $val
		}
	    }
	    "o" {
		if {$notEmptyVal && [string is double $val]} {
		    set camoOctaves($id) $val
		}
	    }
	    "s" {
		if {$notEmptyVal && [string is double $val]} {
		    set camoSize($id) $val
		}
	    }
	    "v" {
		if {$notEmptyVal && [string is double $val]} {
		    set camoScale($id) $val
		}
	    }
	    "c1" {
		if {$notEmptyVal && [llength $val] == 3} {
		    set camoC1($id) $val
		}
	    }
	    "c2" {
		if {$notEmptyVal && [llength $val] == 3} {
		    set camoC2($id) $val
		}
	    }
	    "c3" {
		if {$notEmptyVal && [llength $val] == 3} {
		    set camoC3($id) $val
		}
	    }
	    "t1" {
		if {$notEmptyVal && [string is double $val]} {
		    set camoT1($id) $val
		}
	    }
	    "t2" {
		if {$notEmptyVal && [string is double $val]} {
		    set camoT2($id) $val
		}
	    }
	    "d" {
		if {$notEmptyVal && [string is double $val]} {
		    set camoDelta($id) $val
		}
	    }
	}
    }
    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::setFormDefaults_camo {id} {
    set ignoreShaderSpec 1

    set camoLacun($id) $DEF_CAMO_LACUN
    set camoHval($id) $DEF_CAMO_HVAL
    set camoOctaves($id) $DEF_CAMO_OCTAVES
    set camoSize($id) $DEF_CAMO_SIZE
    set camoScale($id) $DEF_CAMO_SCALE
    set camoC1($id) $DEF_CAMO_C1
    set camoC2($id) $DEF_CAMO_C2
    set camoC3($id) $DEF_CAMO_C3
    set camoT1($id) $DEF_CAMO_T1
    set camoT2($id) $DEF_CAMO_T2
    set camoDelta($id) $DEF_CAMO_DELTA

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_light {spec id} {
    setFormDefaults_light $id

    set ignoreShaderSpec 1
    foreach {key val} [lindex $spec 1] {
	if {$val != ""} {
	    set notEmptyVal 1
	} else {
	    set notEmptyVal 0
	}

	switch -- $key {
	    "a" {
		if {$notEmptyVal && [string is double $val]} {
		    set lightAngle($id) $val
		}
	    }
	    "b" {
		if {$notEmptyVal && [string is double $val]} {
		    set lightLumens($id) $val
		}
	    }
	    "d" {
		if {[llength $val] == 3} {
		    set x [lindex $val 0]
		    set y [lindex $val 1]
		    set z [lindex $val 2]
		    if {$notEmptyVal &&
			[string is double $x] &&
			[string is double $y] &&
			[string is double $z]} {
			set lightTarget($id) $val
		    }
		}
	    }
	    "f" {
		if {$notEmptyVal && [string is double $val]} {
		    set lightFraction($id) $val
		}
	    }
	    "i" {
		if {$notEmptyVal && [string is double $val]} {
		    set lightInfinite($id) $val
		}
	    }
	    "s" {
		if {$notEmptyVal && [string is digit $val]} {
		    set lightShadowRays($id) $val
		}
	    }
	}
    }
    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::setFormDefaults_light {id} {
    set ignoreShaderSpec 1

    set lightFraction($id) $DEF_LIGHT_FRACTION
    set lightAngle($id) $DEF_LIGHT_ANGLE
    set lightTarget($id) $DEF_LIGHT_TARGET
    set lightLumens($id) $DEF_LIGHT_LUMENS
    set lightInfinite($id) $DEF_LIGHT_INFINITE
    set lightVisible($id) $DEF_LIGHT_VISIBLE
    set lightShadowRays($id) $DEF_LIGHT_SHADOW_RAYS

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_stack {spec id} {
    set stackLen 0
    set childsite [$itk_component(stack$id\SF) childsite]

    foreach subspec [lindex $spec 1] {
	set stype [lindex $subspec 0]
	set index $stackLen
	incr stackLen
	frame $childsite.stk_$index -bd 2 -relief groove
	set parent $childsite.stk_$index
	set stackParams(stk_$index,window) $parent

	grid rowconfigure $parent 0 -weight 1
	grid columnconfigure $parent 0 -weight 1

	if {[lsearch $SHADER_TYPES $stype] != -1} {
	    label $parent.lab -text $stype -bg CadetBlue -fg white
	} else {
	    label $parent.lab -text "Unrecognized Shader" -bg CadetBlue -fg white
	}
	grid $parent.lab -columnspan 4 -sticky ew
	set stackParams(stk_$index,name) $stype

	button $parent.del -text delete -width 8 \
	    -command [::itcl::code $this delete_shader "stk_$index"]

	switch -- $stype {
	    plastic {
		build_plastic $parent stk_$index
		updateForm_plastic $subspec stk_$index
	    }
	    glass {
		build_glass $parent stk_$index
		updateForm_glass $subspec stk_$index
	    }
	    mirror {
		build_mirror $parent stk_$index
		updateForm_mirror $subspec stk_$index
	    }
	    light {
		build_light $parent stk_$index
		updateForm_light $subspec stk_$index
	    }
	    bump -
	    bwtexture -
	    texture {
		build_texture_common $parent stk_$index $stype
		updateForm_texture_common $subspec stk_$index
	    }
	    checker {
		build_checker $parent stk_$index
		updateForm_checker $subspec stk_$index
	    }
	    testmap {
		build_testmap $parent stk_$index
		updateForm_testmap $subspec stk_$index
	    }
	    fakestar {
		build_fakestar $parent stk_$index
		updateForm_fakestar $subspec stk_$index
	    }
	    cloud {
		build_cloud $parent stk_$index
		updateForm_cloud $subspec stk_$index
	    }
	    prj {
		build_prj $parent stk_$index
		updateForm_prj $subspec stk_$index
	    }
	    camo {
		build_camo $parent stk_$index
		updateForm_camo $subspec stk_$index
	    }
	    air {
		build_air $parent stk_$index
		updateForm_air $subspec stk_$index
	    }
	    default {
		build_unlisted $parent stk_$index
		updateForm_unlisted $subspec stk_$index
	    }
	}

	grid $parent.del -sticky w
	grid $parent -sticky ew
	grid columnconfigure $parent 0 -minsize 400
    }
}

::itcl::body ShaderEdit::setFormDefaults_stack {} {
    set stackLen 0
}

::itcl::body ShaderEdit::updateForm_envmap {spec id} {
    set index 0
    set subspec [lindex $spec 1]
    set stype [lindex $subspec 0]

    set parent $itk_component(childsite$id\F)
    frame $parent.env_$index -bd 2 -relief groove

    set parent $parent.env_$index
    set stackParams(env_$index,window) $parent
    grid rowconfigure $parent 1 -weight 1
    grid columnconfigure $parent 0 -weight 1

    if {[lsearch $SHADER_TYPES $stype] != -1} {
	label $parent.lab -text $stype -bg CadetBlue -fg white
    } else {
	label $parent.lab -text "Unrecognized Shader" -bg CadetBlue -fg white
    }
    grid $parent.lab -columnspan 4 -sticky ew
    set stackParams(env_$index,name) $stype

    switch -- $stype {
	plastic {
	    build_plastic $parent env_$index
	    updateForm_plastic $subspec env_$index
	}
	glass {
	    build_glass $parent env_$index
	    updateForm_glass $subspec env_$index
	}
	mirror {
	    build_mirror $parent env_$index
	    updateForm_mirror $subspec env_$index
	}
	light {
	    build_light $parent env_$index
	    updateForm_light $subspec env_$index
	}
	bump -
	bwtexture -
	texture {
	    build_texture_common $parent env_$index $stype
	    updateForm_texture_common $subspec env_$index
	}
	checker {
	    build_checker $parent env_$index
	    updateForm_checker $subspec env_$index
	}
	testmap {
	    build_testmap $parent env_$index
	    updateForm_testmap $subspec env_$index
	}
	fakestar {
	    build_fakestar $parent env_$index
	    updateForm_fakestar $subspec env_$index
	}
	cloud {
	    build_cloud $parent env_$index
	    updateForm_cloud $subspec env_$index
	}
	prj {
	    build_prj $parent env_$index
	    updateForm_prj $subspec env_$index
	}
	camo {
	    build_camo $parent env_$index
	    updateForm_camo $subspec env_$index
	}
	stack {
	    build_stack $parent env_$index
	    updateForm_stack $subspec env_$index
	}
	air {
	    build_air $parent env_$index
	    updateForm_air $subspec env_$index
	}
	default {
	    build_unlisted $parent env_$index
	    updateForm_unlisted $subspec env_$index
	}
    }

    grid $parent -columnspan 2 -sticky nsew
}

::itcl::body ShaderEdit::updateForm_unlisted {spec id} {
    set ignoreShaderSpec 1

    set prevUnlistedName($id) [lindex $spec 0]
    set unlistedName($id) [lindex $spec 0]
    set unlistedParams($id) [lindex $spec 1]

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_prj {spec id} {
    set ignoreShaderSpec 1
    set projectionFile($id) [lindex $spec 1]
    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_fakestar {spec id} {
}

::itcl::body ShaderEdit::updateForm_testmap {spec id} {
}


::itcl::body ShaderEdit::updateForm_texture {_spec _id} {
    updateForm_texture_common $_spec $_id
}


::itcl::body ShaderEdit::updateForm_bwtexture {_spec _id} {
    updateForm_texture_common $_spec $_id
}


::itcl::body ShaderEdit::updateForm_bump {_spec _id} {
    updateForm_texture_common $_spec $_id
}

::itcl::body ShaderEdit::updateForm_texture_common {_spec _id} {
    setFormDefaults_texture $_id

    set ignoreShaderSpec 1
    foreach {key val} [lindex $_spec 1] {
	if {$val != ""} {
	    set notEmptyVal 1
	} else {
	    set notEmptyVal 0
	}

	switch -- $key {
	    "file" {
		set textureFile($_id) $val
	    }
	    "m" {
		if {$notEmptyVal && [::string is boolean $val]} {
		    if {$val} {
			set textureMirror($_id) 1
		    } else {
			set textureMirror($_id) 0
		    }
		}
	    }
	    "w" {
		if {$notEmptyVal && [::cadwidgets::Ged::validateDigit $val]} {
		    set textureWidth($_id) $val
		}
	    }
	    "n" {
		if {$notEmptyVal && [::cadwidgets::Ged::validateDigit $val]} {
		    set textureHeight($_id) $val
		}
	    }
	    "uv" {
		if {$notEmptyVal} {
		    set uvlist [::split $val ","]
		    set uvlen [llength $uvlist]
		    set u [lindex $uvlist 0]
		    set v [lindex $uvlist 1]
		    if {$uvlen == 2 && [::string is digit $u] && [::string is digit $v]} {
			set textureScale_UV($_id) $uvlist
		    }
		}
	    }
	    "transp" {
		if {$notEmptyVal && [::cadwidgets::Ged::validateRgb $val]} {
		    set textureTrans($_id) $val
		}
	    }
	    "trans_valid" {
		if {$notEmptyVal && [::string is boolean $val]} {
		    if {$val} {
			set textureTransValid($_id) 1
		    } else {
			set textureTransValid($_id) 0
		    }
		}
	    }
	}
    }
    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::setFormDefaults_texture {id} {
    set ignoreShaderSpec 1

    set textureFile($id) $DEF_TEXTURE_FILE
    set textureMirror($id) $DEF_TEXTURE_MIRROR
    set textureWidth($id) $DEF_TEXTURE_WIDTH
    set textureHeight($id) $DEF_TEXTURE_HEIGHT
    set textureScale_UV($id) $DEF_TEXTURE_SCALE_UV
    set textureTrans($id) $DEF_TEXTURE_TRANS
    set textureTransValid($id) $DEF_TEXTURE_TRANS_VALID

    set ignoreShaderSpec 0
}

::itcl::body ShaderEdit::updateForm_extern {spec id} {
    set ignoreShaderSpec 1
    set externFile($id) [lindex $spec 1]
    set ignoreShaderSpec 0
}


::itcl::body ShaderEdit::validateDouble_air {id d} {
    if {![::cadwidgets::Ged::validateDouble $d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateAirSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::updateAirSpec {id} {
    set newSpec ""

    if {$airDensity($id) != $DEF_AIR_DENSITY} {
	if {$newSpec == ""} {
	    append newSpec "dpm $airDensity($id)"
	} else {
	    append newSpec " dpm $airDensity($id)"
	}
    }

    if {$airDelta($id) != $DEF_AIR_DELTA} {
	if {$newSpec == ""} {
	    append newSpec "d $airDelta($id)"
	} else {
	    append newSpec " d $airDelta($id)"
	}
    }

    if {$airScale($id) != $DEF_AIR_SCALE} {
	if {$newSpec == ""} {
	    append newSpec "s $airScale($id)"
	} else {
	    append newSpec " s $airScale($id)"
	}
    }

    finalSpecSetting air $newSpec
}

::itcl::body ShaderEdit::validateDouble_light {id d} {
    if {![::cadwidgets::Ged::validateDouble $d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateLightSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::validateTarget_light {_id _d} {
    if {![::cadwidgets::Ged::validate3TupleDoubles $_d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateLightSpec $_id]
    }

    return 1
}

::itcl::body ShaderEdit::updateLightSpec {id {unused ""}} {
    set newSpec ""

    if {$lightFraction($id) != $DEF_LIGHT_FRACTION} {
	if {$newSpec == ""} {
	    append newSpec "f $lightFraction($id)"
	} else {
	    append newSpec " f $lightFraction($id)"
	}
    }

    if {$lightAngle($id) != $DEF_LIGHT_ANGLE} {
	if {$newSpec == ""} {
	    append newSpec "a $lightAngle($id)"
	} else {
	    append newSpec " a $lightAngle($id)"
	}
    }

    if {$lightTarget($id) != $DEF_LIGHT_TARGET} {
	if {$newSpec == ""} {
	    append newSpec "d {$lightTarget($id)}"
	} else {
	    append newSpec " d {$lightTarget($id)}"
	}
    }

    if {$lightLumens($id) != $DEF_LIGHT_LUMENS} {
	if {$newSpec == ""} {
	    append newSpec "b $lightLumens($id)"
	} else {
	    append newSpec " b $lightLumens($id)"
	}
    }

    if {$lightShadowRays($id) != $DEF_LIGHT_SHADOW_RAYS} {
	if {$newSpec == ""} {
	    append newSpec "s $lightShadowRays($id)"
	} else {
	    append newSpec " s $lightShadowRays($id)"
	}
    }

    if {$lightInfinite($id) != $DEF_LIGHT_INFINITE} {
	if {$newSpec == ""} {
	    append newSpec "i $lightInfinite($id)"
	} else {
	    append newSpec " i $lightInfinite($id)"
	}
    }

    if {$lightVisible($id) != $DEF_LIGHT_VISIBLE} {
	if {$newSpec == ""} {
	    append newSpec "v $lightVisible($id)"
	} else {
	    append newSpec " v $lightVisible($id)"
	}
    }

    finalSpecSetting light $newSpec

    if {$lightShadowRays($id) > 9} {
	set s 9
    } else {
	set s $lightShadowRays($id)
    }

    $itk_component(lightImage$id\L) configure -image $lightImages(light_i$lightInfinite($id)_v$lightVisible($id)_s$s)

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::validate3TupleDoubles_camo {_id _t} {
    if {![::cadwidgets::Ged::validate3TupleDoubles $_t]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateCamoSpec $_id]
    }

    return 1
}

::itcl::body ShaderEdit::validateDouble_camo {id d} {
    if {![::cadwidgets::Ged::validateDouble $d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateCamoSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::validateRgb_camo {id rgb} {
    if {![::cadwidgets::Ged::validateRgb $rgb]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateCamoSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::updateCamoSpec {id} {
    set newSpec ""

    if {$camoLacun($id) != $DEF_CAMO_LACUN} {
	if {$newSpec == ""} {
	    append newSpec "l {$camoLacun($id)}"
	} else {
	    append newSpec " l {$camoLacun($id)}"
	}
    }

    if {$camoHval($id) != $DEF_CAMO_HVAL} {
	if {$newSpec == ""} {
	    append newSpec "H {$camoHval($id)}"
	} else {
	    append newSpec " H {$camoHval($id)}"
	}
    }

    if {$camoOctaves($id) != $DEF_CAMO_OCTAVES} {
	if {$newSpec == ""} {
	    append newSpec "o {$camoOctaves($id)}"
	} else {
	    append newSpec " o {$camoOctaves($id)}"
	}
    }

    if {$camoSize($id) != $DEF_CAMO_SIZE} {
	if {$newSpec == ""} {
	    append newSpec "s {$camoSize($id)}"
	} else {
	    append newSpec " s {$camoSize($id)}"
	}
    }

    if {$camoScale($id) != $DEF_CAMO_SCALE} {
	if {$newSpec == ""} {
	    append newSpec "v {$camoScale($id)}"
	} else {
	    append newSpec " v {$camoScale($id)}"
	}
    }

    if {$camoC1($id) != $DEF_CAMO_C1} {
	if {$newSpec == ""} {
	    append newSpec "c1 {$camoC1($id)}"
	} else {
	    append newSpec " c1 {$camoC1($id)}"
	}
    }

    if {$camoC2($id) != $DEF_CAMO_C2} {
	if {$newSpec == ""} {
	    append newSpec "c2 {$camoC2($id)}"
	} else {
	    append newSpec " c2 {$camoC2($id)}"
	}
    }

    if {$camoC3($id) != $DEF_CAMO_C3} {
	if {$newSpec == ""} {
	    append newSpec "c3 {$camoC3($id)}"
	} else {
	    append newSpec " c3 {$camoC3($id)}"
	}
    }

    if {$camoT1($id) != $DEF_CAMO_T1} {
	if {$newSpec == ""} {
	    append newSpec "t1 {$camoT1($id)}"
	} else {
	    append newSpec " t1 {$camoT1($id)}"
	}
    }

    if {$camoT2($id) != $DEF_CAMO_T2} {
	if {$newSpec == ""} {
	    append newSpec "t2 {$camoT2($id)}"
	} else {
	    append newSpec " t2 {$camoT2($id)}"
	}
    }

    if {$camoDelta($id) != $DEF_CAMO_DELTA} {
	if {$newSpec == ""} {
	    append newSpec "d {$camoDelta($id)}"
	} else {
	    append newSpec " d {$camoDelta($id)}"
	}
    }

    finalSpecSetting camo $newSpec

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::validateDouble_checker {id d} {
    if {![::cadwidgets::Ged::validateDouble $d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateCheckerSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::validateRgb_checker {id rgb} {
    if {![::cadwidgets::Ged::validateRgb $rgb]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateCheckerSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::updateCheckerSpec {id} {
    set newSpec ""

    if {$checkerColorA($id) != $DEF_CHECKER_COLOR_A} {
	if {$newSpec == ""} {
	    append newSpec "a {$checkerColorA($id)}"
	} else {
	    append newSpec " a {$checkerColorA($id)}"
	}
    }

    if {$checkerColorB($id) != $DEF_CHECKER_COLOR_B} {
	if {$newSpec == ""} {
	    append newSpec "b {$checkerColorB($id)}"
	} else {
	    append newSpec " b {$checkerColorB($id)}"
	}
    }

    if {$checkerScale($id) != $DEF_CHECKER_SCALE} {
	if {$newSpec == ""} {
	    append newSpec "s $checkerScale($id)"
	} else {
	    append newSpec " s $checkerScale($id)"
	}
    }

    finalSpecSetting checker $newSpec

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::validateDouble_cloud {id d} {
    if {![::cadwidgets::Ged::validateDouble $d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateCloudSpec $id]
    }

    return 1
}

::itcl::body ShaderEdit::updateCloudSpec {id} {
    set newSpec ""

    if {$cloudThreshold($id) != $DEF_CLOUD_THRESHOLD} {
	if {$newSpec == ""} {
	    append newSpec "thresh $cloudThreshold($id)"
	} else {
	    append newSpec " thresh $cloudThreshold($id)"
	}
    }

    if {$cloudRange($id) != $DEF_CLOUD_RANGE} {
	if {$newSpec == ""} {
	    append newSpec "thresh $cloudRange($id)"
	} else {
	    append newSpec " thresh $cloudRange($id)"
	}
    }

    finalSpecSetting cloud $newSpec

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::validateDouble_plastic {id d} {
    if {![::cadwidgets::Ged::validateDouble $d]} {
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

    finalSpecSetting plastic $newSpec

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::validateDouble_mirror {id d} {
    if {![::cadwidgets::Ged::validateDouble $d]} {
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

    finalSpecSetting mirror $newSpec

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::validateDouble_glass {id d} {
    if {![::cadwidgets::Ged::validateDouble $d]} {
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

    finalSpecSetting glass $newSpec

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::updateUnlistedSpec {id} {
    if {$shaderType(0) == "stack"} {
	set spec [lindex $shaderSpec 1]
	set i [lsearch -index 0 $spec $prevUnlistedName($id)]
	if {$i != -1} {
	    set spec [lreplace $spec $i $i [list $unlistedName($id) $unlistedParams($id)]]
	}

	set shaderSpec [list stack $spec]
    } elseif {$shaderType(0) == "envmap"} {
	set spec [lindex $shaderSpec 1]
	set subType [lindex $spec 0]
	set subSpec [lindex $spec 1]

	if {$subType == "stack"} {
	    set i [lsearch -index 0 $subSpec $prevUnlistedName($id)]
	    if {$i != -1} {
		set subSpec [lreplace $subSpec $i $i [list $unlistedName($id) $unlistedParams($id)]]
	    }

	    set spec [list stack $subSpec]
	} else {
	    set spec [list $unlistedName($id) $unlistedParams($id)]
	}

	set shaderSpec [list envmap $spec]
    } else {
	set shaderSpec [list $unlistedName($id) $unlistedParams($id)]
    }

    set prevUnlistedName($id) $unlistedName($id)

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}

::itcl::body ShaderEdit::updatePrjSpec {id} {
    finalSpecSetting prj $projectionFile($id)

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}


::itcl::body ShaderEdit::updateExternSpec {id} {
    set shaderSpec [list extern $externFile($id)]

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}


::itcl::body ShaderEdit::validateDigit_texture {_id _d _type} {
    if {![::cadwidgets::Ged::validateDigit $_d]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateTextureSpec $_id $_type]
    }

    return 1
}


::itcl::body ShaderEdit::validateScale_texture {_id _s _type} {
    if {![::cadwidgets::Ged::validate2TupleNonZeroDigits $_s]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateTextureSpec $_id $_type]
    }

    return 1
}


::itcl::body ShaderEdit::validateRgb_texture {_id _rgb _type} {
    if {![::cadwidgets::Ged::validateRgb $_rgb]} {
	return 0
    }

    if {!$ignoreShaderSpec} {
	after idle [::itcl::code $this updateTextureSpec $_id $_type]
    }

    return 1
}


::itcl::body ShaderEdit::updateTextureSpec {_id _type} {
    set newSpec ""

    if {$textureFile($_id) != $DEF_TEXTURE_FILE} {
	append newSpec " file {$textureFile($_id)}"
    }

    if {$textureMirror($_id) != $DEF_TEXTURE_MIRROR} {
	append newSpec " m $textureMirror($_id)"
    }

    if {$textureWidth($_id) != $DEF_TEXTURE_WIDTH} {
	append newSpec " w $textureWidth($_id)"
    }

    if {$textureHeight($_id) != $DEF_TEXTURE_HEIGHT} {
	append newSpec " n $textureHeight($_id)"
    }

    if {$textureScale_UV($_id) != $DEF_TEXTURE_SCALE_UV} {
	set u [lindex $textureScale_UV($_id) 0]
	set v [lindex $textureScale_UV($_id) 1]

	if {$u != "" && $v != "" && [::string is digit $u] && [::string is digit $v]} {
	    append newSpec " uv $u,$v"
	}
    }

    if {$textureTrans($_id) != $DEF_TEXTURE_TRANS} {
	append newSpec " transp {$textureTrans($_id)}"

	if {[llength $textureTrans($_id)] == 3 && [::cadwidgets::Ged::validateRgb $textureTrans($_id)]} {
	    append newSpec " trans_valid 1"
	}
    }

    finalSpecSetting $_type $newSpec
}


::itcl::body ShaderEdit::finalSpecSetting {_type _newSpec} {
    if {$shaderType(0) == "stack"} {
	set spec [lindex $shaderSpec 1]
	set i [lsearch -index 0 $spec $_type]
	if {$i != -1} {
	    set spec [lreplace $spec $i $i [list $_type $_newSpec]]
	}

	set shaderSpec "stack [list $spec]"
    } elseif {$shaderType(0) == "envmap"} {
	set spec [lindex $shaderSpec 1]
	set subType [lindex $spec 0]
	set subSpec [lindex $spec 1]

	if {$subType == "stack"} {
	    set i [lsearch -index 0 $subSpec $_type]
	    if {$i != -1} {
		set spec [lreplace $subSpec $i $i [list $_type $_newSpec]]
	    }

	    set spec "stack [list $spec]"
	} else {
	    set spec "$_type [list $_newSpec]"
	}

	set shaderSpec "envmap [list $spec]"
    } else {
	set shaderSpec "$_type [list $_newSpec]"
    }

    if {$allowCallbacks && $itk_option(-shaderChangedCallback) != ""} {
	$itk_option(-shaderChangedCallback)
    }
}


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
