#                      A R C H E R . T C L
# BRL-CAD
#
# Copyright (c) 2002-2007 United States Government as represented by
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
#    Doug Howard
#
# Description:
#    Archer megawidget.
#

if {![info exists env(ARCHER_HOME)]} {
  catch {
    set env(ARCHER_HOME) [file normalize [file join [file dir $argv0] ..]]
  }
}

if {[info exists env(ARCHER_HOME)]} {
  LoadArcherLibs $env(ARCHER_HOME)
} else {
  LoadArcherLibs .
}

package provide Archer 1.0

namespace eval Archer {
    if {![info exists parentClass]} {
	if {$tcl_platform(os) == "Windows NT"} {
	    set parentClass itk::Toplevel
	    set inheritFromToplevel 1
	} else {
	    set parentClass TabWindow
	    set inheritFromToplevel 0
	}
    }

    if {![info exists debug]} {
	set debug 0
    }

    if {![info exists haveSdb]} {
	set haveSdb 0
    }

    set methodDecls ""
    set methodImpls ""
    set extraMgedCommands ""
    set corePluginInit ""
    if {[file exists [file join $env(ARCHER_HOME) plugins archer Core]]} {
	set savePwd [pwd]
	cd [file join $env(ARCHER_HOME) plugins archer Core]
	catch {
	    foreach filename [lsort [glob -nocomplain *]] {
		if [file isfile $filename] {
		    set ext [file extension $filename]
		    switch -exact -- $ext {
			".tcl" -
			".itk" -
			".itcl" {
			    source $filename
			}
			".sh" {
			    # silently ignore
			}
			default {
			    # silently ignore
			}
		    }
		}
	    }
	}
	cd $savePwd
    }
    if {[file exists [file join $env(ARCHER_HOME) plugins archer Commands]]} {
	set savePwd [pwd]
	cd [file join $env(ARCHER_HOME) plugins archer Commands]
	catch {
	    foreach filename [lsort [glob -nocomplain *]] {
		if [file isfile $filename] {
		    set ext [file extension $filename]
		    switch -exact -- $ext {
			".tcl" -
			".itk" -
			".itcl" {
			    source $filename
			}
			".sh" {
			    # silently ignore
			}
			default {
			    # silently ignore
			}
		    }
		}
	    }
	}
	cd $savePwd
    }
}

::itcl::class Archer {
    inherit $Archer::parentClass

    itk_option define -quitcmd quitCmd Command {}
    itk_option define -master master Master "."
    itk_option define -primaryToolbar primaryToolbar PrimaryToolbar 1
    itk_option define -viewToolbar viewToolbar ViewToolbar 1
    itk_option define -statusbar statusbar Statusbar 1
#    itk_option define -menuFont menuFont MenuFont application

    constructor {{viewOnly 0} {noCopy 0} args} {}
    destructor {}


    # Dynamically load methods
    if {$Archer::methodDecls != ""} {
	foreach meth $::Archer::methodDecls {
	    eval $meth
	}
    }

    public {
	proc packTree              {data}
	proc unpackTree            {tree}

	# public window commands
	method dockarea            {{position "south"}}
	method primary_toolbar_add_btn     {name {args ""}}
	method primary_toolbar_add_sep     {name {args ""}}
	method primary_toolbar_remove_itm  {index}
	method closeHierarchy {}
	method openHierarchy {{fraction 30}}
	method refreshTree {{restore 1}}
	method mouseRay {_dm _x _y}
	method shootRay {_start _op _target _prep _no_bool _onehit}
	method addMouseRayCallback {_callback}
	method deleteMouseRayCallback {_callback}

	# public database commands
	method dbCmd               {args}
	method cmd                 {args}
        method png                 {filename}

	# general
	method UpdateTheme         {theme} {set mTheme $theme; _update_theme}
	method Load                {type filename}
	method GetUserCmds         {}
	method WhatsOpen           {}
	method Close               {}
	method askToSave           {}
	method getTkColor          {r g b}
	method getRgbColor         {tkColor}
	method setSave {}
	method getLastSelectedDir  {}
	method refreshDisplay      {}

	# wizard plugin related commands
	method registerWizardXmlCallback {callback}
	method unRegisterWizardXmlCallback {callback}

	# Commands exposed to the user via the command line.
	# More to be added later...
	method abort               {args}
	method adjust              {args}
	method adjustNormals       {args}
	method blast               {args}
	method c                   {args}
	method cd                  {args}
	method clear               {args}
	method comb                {args}
	method compact             {args}
	method concat              {args}
	method copy                {args}
	method copyeval            {args}
	method cp                  {args}
	method dbExpand	           {args}
	method decimate            {args}
	method decimate2           {args}
	method decimateAttr        {args}
	method decimateAttr2       {args}
	method delete              {args}
	method draw                {args}
	method E                   {args}
	method erase               {args}
	method erase_all           {args}
	method ev                  {args}
	method exit                {args}
	method exportFg4	   {}
	method exportStl	   {}
	method exportVrml	   {}
	method find                {args}
	method hide                {args}
	method g                   {args}
	method get                 {args}
	method group               {args}
	method importFg4	   {}
	method importStl	   {importDir}
	method i                   {args}
	method importFg4Sections   {slist wlist delta}
	method kill                {args}
	method killall             {args}
	method killtree            {args}
	method ls                  {args}
	method make_bb             {args}
	method move                {args}
	method mv                  {args}
	method mvall               {args}
	method ocenter		   {args}
	method orotate		   {obj rx ry rz kx ky kz}
	method oscale		   {obj sf kx ky kz}
	method otranslate	   {obj dx dy dz}
	method push                {args}
	method purgeHistory        {}
	method put                 {args}
	method pwd                 {}
	method r                   {args}
	method report              {args}
	method reverseNormals      {args}
	method rm                  {args}
	method track               {args}
	method unhide              {args}
	method units               {args}
	method vdraw               {args}
	method whichid             {args}
	method who                 {args}
	method Z                   {args}
	method zap                 {args}

	# Plugin related commands
	proc initArcher {}
	method pluginUpdateSaveMode {mode}
	method pluginUpdateProgressBar {percent}
	method pluginUpdateStatusBar {msg}
	method pluginGetMinAllowableRid {}
	proc pluginQuery {name}
	proc pluginRegister {majorType minorType name class file \
				 {description ""} \
				 {version ""} \
				 {developer ""} \
				 {icon ""} \
				 {tooltip ""} \
				 {action ""} \
				 {xmlAction ""}}
	proc pluginUnregister {name}
	proc pluginLoader {}
	proc pluginLoadCWDFiles {}
	proc pluginDialog {w}
	proc pluginMged {_archer}
	proc pluginSdb {_archer}

	# Public Class Variables
	common archer ""
	common splash ""
	common showWindow 0
	common plugins ""

	common ROTATE_MODE 0
	common TRANSLATE_MODE 1
	common SCALE_MODE 2
	common CENTER_MODE 3
	common COMP_PICK_MODE 4
	common MEASURE_MODE 5
	common OBJECT_ROTATE_MODE 6
	common OBJECT_SCALE_MODE 7
	common OBJECT_TRANSLATE_MODE 8
	common OBJECT_CENTER_MODE 9

	common OBJ_EDIT_VIEW_MODE 0
	common OBJ_ATTR_VIEW_MODE 1

	common pluginMajorTypeCore "Core"
	common pluginMajorTypeCommand "Command"
	common pluginMajorTypeWizard "Wizard"
	common pluginMajorTypeUtility "Utility"
	common pluginMinorTypeMged "Mged"
	common pluginMinorTypeSdb "Sdb"
	common pluginMinorTypeBoth "Both"

	common brlcadDataPath
	common SystemWindowFont
	common SystemWindowText
	common SystemWindow
	common SystemHighlight
	common SystemHighlightText
	common SystemButtonFace

	common ZCLIP_SMALL_CUBE 0
	common ZCLIP_MEDIUM_CUBE 1
	common ZCLIP_LARGE_CUBE 2
	common ZCLIP_NONE 3

	common MEASURING_STICK "archer_measuring_stick"

	if {$tcl_platform(os) != "Windows NT"} {
	    set brlcadDataPath [bu_brlcad_data ""]
	    set SystemWindowFont Helvetica
	    set SystemWindowText black
	    set SystemWindow \#d9d9d9
	    set SystemHighlight black
	    set SystemHighlightText \#ececec
	    set SystemButtonFace \#d9d9d9
	} else {
	  if {[info exists env(ARCHER_HOME)]} {
	    set brlcadDataPath $env(ARCHER_HOME)
	  } else {
	    set brlcadDataPath [bu_brlcad_data ""]
	  }
	    set SystemWindowFont SystemWindowText
	    set SystemWindowText SystemWindowText
	    set SystemWindow SystemWindow
	    set SystemHighlight SystemHighlight
	    set SystemHighlightText SystemHighlightText
	    set SystemButtonFace SystemButtonFace
	}
    }

    protected {
	proc unpackTreeGuts      {tree}

	variable wizardXmlCallbacks ""

	variable mArcherVersion "0.9.1"
	variable mLastSelectedDir ""

	variable mFontArrowsName "arrowFont"
	variable mFontArrows {Wingdings 3}
	variable mLeftArrow "t"
	variable mRightArrow "u"
	variable mFontText
	variable mFontTextBold

	variable mRestoringTree 0
	variable mViewOnly 0
	variable mTarget ""
	variable mTargetCopy ""
	variable mTargetOldCopy ""
	variable mDisplayType
	variable mLighting 2
	variable mRenderMode -1
	variable mShowNormals 0
	variable mShowNormalsTag "ShowNormals"
	variable mActivePane
	variable mStatusStr ""
	variable mDbType ""
	variable mDbReadOnly 0
	variable mDbNoCopy 0
	variable mDbShared 0
	variable mProgress 0
	variable mProgressBarWidth 200
	variable mProgressBarHeight ""
	#variable mProgressString ""
	variable mMode 0
	variable mNeedSave 0
	variable mBackupObj ""
	variable mPrevSelectedObjPath ""
	variable mPrevSelectedObj ""
	variable mSelectedObjPath ""
	variable mSelectedObj ""
	variable mSelectedObjType ""
	variable mPasteActive 0
	variable mPendingEdits 0
	variable mMultiPane 0

	variable mHPaneFraction1 80
	variable mHPaneFraction2 20
	variable mVPaneFraction1 20
	variable mVPaneFraction2 80
	variable mVPaneFraction3 20
	variable mVPaneFraction4 60
	variable mVPaneFraction5 20

	variable mVPaneToggle1 20
	variable mVPaneToggle3 20
	variable mVPaneToggle5 20

	variable mShowViewAxes 1
	variable mShowModelAxes 0
	variable mShowModelAxesTicks 1
	variable mShowGroundPlane 0
	variable mShowPrimitiveLabels 0
	variable mShowViewingParams 1
	variable mShowScale 0

	# variables for preference state
	variable mZClipMode 0
	variable mZClipModePref ""

	variable mBindingMode 0
	variable mBindingModePref ""
	variable mBackground "0 0 0"
	variable mBackgroundRedPref 
	variable mBackgroundGreenPref 
	variable mBackgroundBluePref 
	variable mPrimitiveLabelColor Yellow
	variable mPrimitiveLabelColorPref
	variable mViewingParamsColor Yellow
	variable mViewingParamsColorPref
	variable mTheme "Crystal_Large"
	variable mThemePref ""
	variable mSdbTopGroup all
	variable mScaleColor Yellow
	variable mScaleColorPref ""
	variable mMeasuringStickMode 0
	variable mMeasuringStickModePref ""
	variable mMeasuringStickColor Yellow
	variable mMeasuringStickColorPref ""
	variable mMeasuringStickColorVDraw ffff00
	variable mEnableBigE 0
	variable mEnableBigEPref ""

	variable mGroundPlaneSize 20000
	variable mGroundPlaneSizePref ""
	variable mGroundPlaneInterval 1000
	variable mGroundPlaneIntervalPref ""
	variable mGroundPlaneMajorColor Yellow
	variable mGroundPlaneMajorColorPref ""
	variable mGroundPlaneMinorColor Grey
	variable mGroundPlaneMinorColorPref ""

	variable mViewAxesSize Small
	variable mViewAxesSizePref ""
	variable mViewAxesPosition "Lower Right"
	variable mViewAxesPositionPref ""
	variable mViewAxesLineWidth 1
	variable mViewAxesLineWidthPref ""
	variable mViewAxesColor Triple
	variable mViewAxesColorPref ""
	variable mViewAxesLabelColor Yellow
	variable mViewAxesLabelColorPref ""

	variable mModelAxesSize "View (1x)"
	variable mModelAxesSizePref ""
	variable mModelAxesPosition "0 0 0"
	variable mModelAxesPositionXPref ""
	variable mModelAxesPositionYPref ""
	variable mModelAxesPositionZPref ""
	variable mModelAxesLineWidth 1
	variable mModelAxesLineWidthPref ""
	variable mModelAxesColor White
	variable mModelAxesColorPref ""
	variable mModelAxesLabelColor Yellow
	variable mModelAxesLabelColorPref ""

	variable mModelAxesTickInterval 100
	variable mModelAxesTickIntervalPref ""
	variable mModelAxesTicksPerMajor 10
	variable mModelAxesTicksPerMajorPref ""
	variable mModelAxesTickThreshold 8
	variable mModelAxesTickThresholdPref ""
	variable mModelAxesTickLength 4
	variable mModelAxesTickLengthPref ""
	variable mModelAxesTickMajorLength 8
	variable mModelAxesTickMajorLengthPref ""
	variable mModelAxesTickColor Yellow
	variable mModelAxesTickColorPref ""
	variable mModelAxesTickMajorColor Red
	variable mModelAxesTickMajorColorPref ""

	variable mDefaultBindingMode 0
	variable mPrevObjViewMode 0
	variable mObjViewMode 0

	variable archerCommands { \
	    cd clear copy cp dbExpand delete draw exit \
	    g get group kill ls move mv ocenter orotate \
	    otranslate packTree pwd rm units whichid who \
	    unpackTree Z zap
	}
	variable mgedCommands { \
	    bot2pipe \
	    adjust blast c comb concat copyeval E erase_all \
	    ev find hide importFg4Sections killall killtree \
	    make_bb mvall push purgeHistory put r report track \
	    unhide vdraw
	}
	variable sdbCommands { \
	    abort adjustNormals compact decimate decimate2 \
	    decimateAttr decimateAttr2 erase exportFg4 \
	    exportStl exportVrml  importFg4 importStl \
	    reverseNormals
	}
	variable dbSpecificCommands {}
	variable unwrappedDbCommands {}
	variable bannedDbCommands {
	    dbip nmg_collapse nmb_simplify
	    open move_arb_edge move_arb_face
	    shells xpush illum label qray
	    rotate_arb_face rtabort
	    shaded_mode png
	}

	variable mAboutInfo "
     Archer Version 0.9.1 (Prototype)

Archer is a geometry editor/viewer
developed by SURVICE Engineering.
It can be used to edit/view Brlcad
geometry database files as well as
SURVICE database files.
"

	variable mArcherLicenseInfo ""
	variable mIvaviewLicenseInfo ""
	variable mBrlcadLicenseInfo ""
	variable mAckInfo ""

	variable mMouseOverrideInfo "
Translate     Shift-Right
Rotate        Shift-Left
Scale         Shift-Middle
Center        Shift-Ctrl-Right
Popup Menu    Right or Ctrl-Left
"

	variable colorList {Grey Black Blue Cyan Green Magenta Red White Yellow Triple}
	variable colorListNoTriple {Grey Black Blue Cyan Green Magenta Red White Yellow}
	variable defaultNodeColor {150 150 150}

	variable doStatus 1
	variable mDbName ""
	variable mDbUnits ""
	variable mDbTitle ""

	variable currentDisplay ""

	# plugin list
	variable mWizardClass ""
	variable mWizardTop ""
	variable mWizardState ""
	variable mNoWizardActive 0

	variable mMouseRayCallbacks ""

	method archerWrapper {cmd eflag hflag sflag tflag args}
	method mgedWrapper {cmd eflag hflag sflag tflag args}
	method sdbWrapper {cmd eflag hflag sflag tflag args}
	method purgeObjHistory {obj}

	method _build_canvas_menubar {}

	method _build_utility_menu {}
	method _update_utility_menu {}
	method _invoke_utility_dialog {class wname w}

	method _build_wizard_menu {}
	method _update_wizard_menu {}
	method _invoke_wizard_dialog {class action wname}
	method _invoke_wizard_update {wizard action oname name}
	method _build_wizard_obj {dialog wizard action oname}

	method _build_db_attr_view {}
	method _init_db_attr_view {name}
	method _build_obj_view_toolbar {}
	method _build_obj_attr_view {}
	method _init_obj_attr_view {}
	method _build_obj_edit_view {}
	method _init_obj_edit_view {}
	method _init_obj_history {obj}
	method _init_obj_edit {obj}
	method _init_obj_wizard {obj wizardLoaded}
	method _init_no_wizard {parent msg}
	method _update_obj_edit_view {}
	method _update_obj_edit {updateObj needInit needSave}
	method _finalize_obj_edit {obj path}
	method _init_edit {}
	method _reset_edit {}
	method _apply_edit {}
	method _update_prev_obj_button {obj}
	method _update_next_obj_button {obj}
	method _goto_prev_obj {}
	method _goto_next_obj {}
	method _update_obj_history {obj}
	method _redraw_obj {obj {wflag 1}}

	method _create_obj {type}
	method _create_arb4 {name}
	method _create_arb5 {name}
	method _create_arb6 {name}
	method _create_arb7 {name}
	method _create_arb8 {name}
	method _create_bot {name}
	method _create_comb {name}
	method _create_ehy {name}
	method _create_ell {name}
	method _create_epa {name}
	method _create_eto {name}
	method _create_extrude {name}
	method _create_grip {name}
	method _create_half {name}
	method _create_part {name}
	method _create_pipe {name}
	method _create_rhc {name}
	method _create_rpc {name}
	method _create_sketch {name}
	method _create_sphere {name}
	method _create_tgc {name}
	method _create_torus {name}

	method _build_arb4_edit_view {}
	method _build_arb5_edit_view {}
	method _build_arb6_edit_view {}
	method _build_arb7_edit_view {}
	method _build_arb8_edit_view {}
	method _build_bot_edit_view {}
	method _build_comb_edit_view {}
	method _build_ehy_edit_view {}
	method _build_ell_edit_view {}
	method _build_epa_edit_view {}
	method _build_eto_edit_view {}
	method _build_extrude_edit_view {}
	method _build_grip_edit_view {}
	method _build_half_edit_view {}
	method _build_part_edit_view {}
	method _build_pipe_edit_view {}
	method _build_rhc_edit_view {}
	method _build_rpc_edit_view {}
	method _build_sketch_edit_view {}
	method _build_sphere_edit_view {}
	method _build_tgc_edit_view {}
	method _build_torus_edit_view {}

	method _init_arb4_edit_view {odata}
	method _init_arb5_edit_view {odata}
	method _init_arb6_edit_view {odata}
	method _init_arb7_edit_view {odata}
	method _init_arb8_edit_view {odata}
	method _init_bot_edit_view {odata}
	method _init_comb_edit_view {odata}
	method _init_ehy_edit_view {odata}
	method _init_ell_edit_view {odata}
	method _init_epa_edit_view {odata}
	method _init_eto_edit_view {odata}
	method _init_extrude_edit_view {odata}
	method _init_grip_edit_view {odata}
	method _init_half_edit_view {odata}
	method _init_part_edit_view {odata}
	method _init_pipe_edit_view {odata}
	method _init_rhc_edit_view {odata}
	method _init_rpc_edit_view {odata}
	method _init_sketch_edit_view {odata}
	method _init_sphere_edit_view {odata}
	method _init_tgc_edit_view {odata}
	method _init_torus_edit_view {odata}

	method _build_toplevel_menubar {}
	method _update_modes_menu {}
	method _build_embedded_menubar {}
	method _menu_statusCB {w}
	method _set_mode {{updateFractions 0}}
	method _set_basic_mode {}
	method _set_intermediate_mode {}
	method _set_advanced_mode {}
	method _update_theme {}
	method _update_creation_buttons {on}
	method _update_save_mode {}
	method _update_cut_mode {}
	method _update_copy_mode {}
	method _update_paste_mode {}
	method _create_target_copy {}
	method _delete_target_old_copy {}

	method _cut_obj {}
	method _copy_obj {}
	method _paste_obj {}

	method _get_vdraw_color {color}
	method _build_ground_plane {}
	method _show_ground_plane {}
	method _show_primitive_labels {}
	method _show_view_params {}
	method _show_scale {}

	# pane commands
	method _toggle_tree_view {state}
	method _toggle_attr_view {state}
	method _update_toggle_mode {}

	variable mActiveEditDialogs {}
	method editComponent {comp}
	method editApplyCallback {comp}
	method editDismissCallback {dialog}

	method launchNirt {}
	method launchRtApp {app size}
	#method refreshDisplay {}
	#method mouseRay {_dm _x _y}

	method adjustCompNormals {comp}
	method reverseCompNormals {comp}
	method toggleCompNormals {comp}

	method launchDisplayMenuBegin {w m x y}
	method launchDisplayMenuEnd {}

	method handleObjCenter {obj x y}
	method handleObjRotate {obj rx ry rz kx ky kz}
	method handleObjScale {obj sf kx ky kz}
	method handleObjTranslate {obj dx dy dz}

	method updateDisplaySettings {}
    }

    private {
	variable _imgdir ""
	variable _centerX ""
	variable _centerY ""
	variable _centerZ ""

	variable _images

	variable mMeasureStart
	variable mMeasureEnd

	# init functions
	method _init_tree          {}
	method _init_mged          {}
	method _init_sdb           {}
	method _close_mged         {}
	method _close_sdb          {}

	# interface operations
	method _new_db             {}
	method _open_db            {}
	method _save_db            {}
	method _close_db           {}
	method _primary_toolbar_add        {type name {args ""}}
	method _primary_toolbar_remove     {index}

	# private window commands
	method _do_primary_toolbar {}
	method _do_view_toolbar    {}
	method _do_statusbar       {}

	# tree commands
	method _alter_treenode_children {node option value}
	method _refresh_tree       {{restore 1}}
	method _toggle_tree_path   {_path}
	method _update_tree        {}
	method _fill_tree          {{node ""}}
	method _select_node        {tags {rflag 1}}
	method _dbl_click          {tags}
	method _load_menu          {menu snode}

	# db/display commands
	method _get_node_children  {node}
	method _render_comp        {node}
	method _render             {node state trans updateTree {wflag 1}}
	method selectDisplayColor  {node}
	method setDisplayColor	   {node rgb}
	method setTransparency	   {node alpha}
	method _ray_trace_panel    {}
	method _do_png             {}
	method _set_active_pane    {pane}
	method _update_active_pane {args}
	method _do_multi_pane      {}
	method _do_lighting        {}

	# view commands
	method _do_view_reset {}
	method _do_autoview {}
	method _do_view_center {}
	method _do_ae {az el}

	method _show_view_axes     {}
	method _show_model_axes    {}
	method _show_model_axes_ticks {}
	method _toggle_model_axes  {pane}
	method _toggle_model_axes_ticks {pane}
	method _toggle_view_axes   {pane}
	method _toggle_ground_plane   {}

	# private mged commands
	method _alter_obj          {operation obj}
	method _delete_obj         {obj}
	method _group_obj          {obj}
	method _region_obj         {obj}
	method _cp_mv              {top obj cmd}

	method _build_view_toolbar {}
	method _update_view_toolbar_for_edit {}

	method beginViewRotate {}
	method endViewRotate {dm}

	method beginViewScale {}
	method endViewScale {dm}

	method beginViewTranslate {}
	method endViewTranslate {dm}

	method _init_center_mode {}

	method initCompPick {}

	method initMeasure {}
	method beginMeasure {_dm _x _y}
	method handleMeasure {_dm _x _y}
	method endMeasure {_dm}

	method beginObjRotate {}
	method endObjRotate {dm obj}

	method beginObjScale {}
	method endObjScale {dm obj}

	method beginObjTranslate {}
	method endObjTranslate {dm obj}

	method beginObjCenter {}
	method endObjCenter {dm obj}

	method _init_default_bindings {}
	method _init_brlcad_bindings {}
	method _build_about_dialog {}
	method _build_generic_dialog {}
	method _build_mouse_overrides_dialog {}
	method _build_info_dialog {name title info size wrapOption modality}
	method _build_info_dialogs {}
	method _build_save_dialog {}
	method _build_view_center_dialog {}
	method _build_preferences_dialog {}
	method _build_general_preferences {}
	method _build_background_color {parent}
	method _build_model_axes_preferences {}
	method _build_model_axes_position {parent}
	method _build_view_axes_preferences {}
	method _build_ground_plane_preferences {}
	method _build_display_preferences {}
	method _build_combo_box {parent name1 name2 varName text listOfChoices}
	method _validateDigit {d}
	method _validateDouble {d}
	method _validateTickInterval {ti}
	method _validateColorComp {c}

	# private dialogs
	method _do_about_archer    {}
	method _do_mouse_overrides {}
	method doArcherHelp         {}

	method _background_color {r g b}
	method _do_preferences {}
	method _apply_preferences_if_diff {}
	method _apply_general_preferences_if_diff {}
	method _apply_view_axes_preferences_if_diff {}
	method _apply_model_axes_preferences_if_diff {}
	method _apply_ground_plane_preferences_if_diff {}
	method _apply_display_preferences_if_diff {}
	method _read_preferences {}
	method _write_preferences {}
	method _apply_preferences {}
	method _apply_general_preferences {}
	method _apply_display_preferences {}
	method _apply_view_axes_preferences {}
	method _apply_model_axes_preferences {}

	method _update_hpane_fractions {}
	method _update_vpane_fractions {}

	method _set_color_option {cmd colorOption color {tripleColorOption ""}}

	method _add_history {cmd}
    }
}

# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------
::itcl::body Archer::constructor {{viewOnly 0} {noCopy 0} args} {
    global env
    global tcl_platform

    if {$Archer::extraMgedCommands != ""} {
	eval lappend mgedCommands $Archer::extraMgedCommands
    }

    set mLastSelectedDir [pwd]

    set mFontText [list $SystemWindowFont 8]
    set mFontTextBold [list $SystemWindowFont 8 bold]

    set mProgressBarHeight [expr {[font metrics $mFontText -linespace] + 1}]
    set mViewOnly $viewOnly
    set mDbNoCopy $noCopy

    if {$Archer::inheritFromToplevel} {
	wm withdraw [namespace tail $this]
    }

    if {![info exists env(DISPLAY)]} {
	set env(DISPLAY) ":0"
    }

    set _imgdir [file join $brlcadDataPath tclscripts archer images]

    if {[llength $args] == 1} {
	set args [lindex $args 0]

    }

    if {!$mViewOnly} {
	if {$Archer::inheritFromToplevel} {
	    _build_toplevel_menubar
	    $this component hull configure -menu $itk_component(menubar)
	} else {
	    _build_embedded_menubar
	    pack $itk_component(menubar) -side top -fill x -padx 1
	}
    }

    set mDisplayType [dm_bestXType $env(DISPLAY)]

    # horizontal panes
    itk_component add hpane {
	::iwidgets::panedwindow $itk_interior.hpane \
		-orient horizontal \
		-thickness 5 \
		-sashborderwidth 1 \
		-sashcursor sb_v_double_arrow \
		-showhandle 0
    } {}

    $itk_component(hpane) add topView
    $itk_component(hpane) paneconfigure topView \
	-margin 0

    if {!$mViewOnly} {
	$itk_component(hpane) add bottomView
	$itk_component(hpane) paneconfigure bottomView \
	    -margin 0 \
	    -minimum 90

    set parent [$itk_component(hpane) childsite bottomView]
    itk_component add advancedTabs {
	blt::tabnotebook $parent.tabs \
	    -relief flat \
	    -tiers 99 \
	    -tearoff 1 \
	    -gap 3 \
	    -width 0 \
	    -height 0 \
	    -outerpad 0 \
	    -highlightthickness 1 \
	    -selectforeground black
    } {}
    $itk_component(advancedTabs) configure \
	-highlightcolor [$itk_component(advancedTabs) cget -background] \
	-borderwidth 0 \
	-font $mFontText
    $itk_component(advancedTabs) insert end -text "Command" -stipple gray25
    $itk_component(advancedTabs) insert end -text "History" -stipple gray25

    itk_component add cmd {
	Command $itk_component(advancedTabs).cmd \
		-relief sunken -borderwidth 2 \
		-hscrollmode none -vscrollmode dynamic \
		-scrollmargin 2 -visibleitems 80x15 \
		-textbackground $SystemWindow -prompt "Archer> " \
		-prompt2 "% " -result_color black -cmd_color red
    } {
    }
    set i 0
    $itk_component(advancedTabs) tab configure $i \
	-window $itk_component(cmd) \
	-fill both
    incr i

    itk_component add history {
	::iwidgets::scrolledtext $itk_component(advancedTabs).history \
		-relief sunken -borderwidth 2 \
		-hscrollmode none -vscrollmode dynamic \
		-scrollmargin 2 -visibleitems 80x15 \
		-textbackground $SystemWindow
    } {
    }
    $itk_component(advancedTabs) tab configure $i \
	-window $itk_component(history) \
	-fill both
    [$itk_component(history) component text] configure -state disabled
    }

    # vertical panes
    set parent [$itk_component(hpane) childsite topView]
    itk_component add vpane {
	::iwidgets::panedwindow $parent.vpane \
		-orient vertical \
		-thickness 5 \
		-sashborderwidth 1 \
		-sashcursor sb_h_double_arrow \
		-showhandle 0
    } {}

    $itk_component(vpane) add hierarchyView
    if {!$mViewOnly} {
	$itk_component(vpane) add geomView
	$itk_component(vpane) add attrView
	$itk_component(vpane) paneconfigure hierarchyView \
	    -margin 0 \
	    -minimum 200
	$itk_component(vpane) paneconfigure geomView \
	    -margin 0
	$itk_component(vpane) paneconfigure attrView \
	    -margin 0 \
	    -minimum 200
    } else {
	$itk_component(vpane) add geomView
	$itk_component(vpane) paneconfigure geomView \
	    -margin 0
	$itk_component(vpane) hide hierarchyView
    }

    # frame for all geometry canvas's
    set parent [$itk_component(vpane) childsite geomView]
    itk_component add canvasF {
	::frame $parent.canvasF \
	    -bd 1 \
	    -relief sunken
    }

    if {!$mViewOnly} {
    itk_component add tree_expand {
	::button $itk_component(canvasF).tree_expand
    } {
    }
    $itk_component(tree_expand) configure -relief flat \
	    -image [image create photo -file \
	    [file join $_imgdir Themes $mTheme "pane_blank.png"]] \
	    -state disabled \
	    -command [::itcl::code $this _toggle_tree_view "open"]

    set font [$itk_component(menubar) cget -font]
    itk_component add canvas_menu {
	::iwidgets::menubar $itk_component(canvasF).canvas_menu \
	    -helpvariable [::itcl::scope mStatusStr] \
	    -font $font \
	    -activeborderwidth 2 \
	    -borderwidth 0 \
	    -activebackground $SystemHighlight \
	    -activeforeground $SystemHighlightText
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }
    } else {
	itk_component add canvas_menu {
	    ::iwidgets::menubar $itk_component(canvasF).canvas_menu \
		-helpvariable [::itcl::scope mStatusStr] \
		-font $mFontText \
		-activeborderwidth 2 \
		-borderwidth 0 \
		-activebackground $SystemHighlight \
		-activeforeground $SystemHighlightText
	} {
	    keep -background
	}
    }
    $itk_component(canvas_menu) component menubar configure \
	-relief flat
    _build_canvas_menubar

    if {!$mViewOnly} {
    itk_component add attr_expand {
	::button $itk_component(canvasF).attr_expand
    } {}
    $itk_component(attr_expand) configure -relief flat \
	    -image [image create photo -file \
	    [file join $_imgdir Themes $mTheme "pane_blank.png"]] \
	    -state disabled \
	    -command [::itcl::code $this _toggle_attr_view "open"]

    # dummy canvas
    itk_component add canvas {
	::frame $itk_component(canvasF).canvas \
	    -bd 0 \
	    -relief flat
    }
    grid $itk_component(tree_expand) -row 0 -column 0 -sticky w
    grid $itk_component(canvas_menu) -row 0 -column 1 -sticky w
    grid $itk_component(attr_expand) -row 0 -column 2 -sticky e
    grid $itk_component(canvas) -row 1 -column 0 -columnspan 3 -sticky news
    grid columnconfigure $itk_component(canvasF) 1 -weight 1
    grid rowconfigure $itk_component(canvasF) 1 -weight 1

    # stuff that goes at top of attributes frame
    itk_component add attr_title_frm {
	::frame [$itk_component(vpane) childsite attrView].attrTitleF
    } {}

    itk_component add attr_collapse {
	::button $itk_component(attr_title_frm).attr_collapse
    } {}
    $itk_component(attr_collapse) configure -relief flat \
	    -image [image create photo -file \
	    [file join $_imgdir Themes $mTheme "pane_expand.png"]] \
	    -command [::itcl::code $this _toggle_attr_view "close"]

    itk_component add attr_label {
	::label $itk_component(attr_title_frm).attr_label \
		-text "Attributes" -font $font
    } {}

    set sep [::frame $itk_component(attr_title_frm).sep -height 2 \
	    -bd 1 -relief sunken]
    pack $sep -side bottom -fill x
    pack $itk_component(attr_collapse) -side left -pady 1
    pack $itk_component(attr_label) -side left -fill x
    pack $itk_component(attr_title_frm) -side top -fill x -padx 2

    # tool bar
    itk_component add primaryToolbar {
	::iwidgets::toolbar $itk_interior.primarytoolbar \
		-helpvariable [::itcl::scope mStatusStr] \
		-balloonfont "{CG Times} 8" \
		-balloonbackground \#ffffdd \
		-borderwidth 1 \
		-orient horizontal
    } {
	# XXX If uncommented, the following line hoses things
	# WRT -balloonbackground option and the addition of frames
	# acting as filler (i.e. it causes borders to appear
	# around the fill objects)
	#usual
    }

    $itk_component(primaryToolbar) add button new \
	-balloonstr "Create a new geometry file" \
	-helpstr "Create a new geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _new_db]

    $itk_component(primaryToolbar) add button open \
	-balloonstr "Open an existing geometry file" \
	-helpstr "Open an existing geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _open_db]

    $itk_component(primaryToolbar) add button save \
	-balloonstr "Save the current geometry file" \
	-helpstr "Save the current geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _save_db]

#    $itk_component(primaryToolbar) add button close \
	-image [image create photo \
		    -file [file join $_imgdir Themes $mTheme closeall.png]] \
	-balloonstr "Close the current geometry file" \
	-helpstr "Close the current geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _close_db]

    # half-size spacer
    $itk_component(primaryToolbar) add frame sep1 \
	-relief sunken \
	-width 2

    $itk_component(primaryToolbar) add button cut \
	-balloonstr "Cut current object" \
	-helpstr "Cut current object" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _cut_obj]

    $itk_component(primaryToolbar) add button copy \
	-balloonstr "Copy current object" \
	-helpstr "Copy current object" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _copy_obj]

    $itk_component(primaryToolbar) add button paste \
	-balloonstr "Paste object" \
	-helpstr "Paste object" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _paste_obj]

    # half-size spacer
    $itk_component(primaryToolbar) add frame sep2 \
	-relief sunken \
	-width 2

    $itk_component(primaryToolbar) add button preferences \
	-balloonstr "Set application preferences" \
	-helpstr "Set application preferences" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _do_preferences]

    # half-size spacer
    $itk_component(primaryToolbar) add frame sep3 \
	-relief sunken \
	-width 2

    $itk_component(primaryToolbar) add button arb6 \
	-balloonstr "Create an arb6" \
	-helpstr "Create an arb6" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj arb6]

    $itk_component(primaryToolbar) add button arb8 \
	-balloonstr "Create an arb8" \
	-helpstr "Create an arb8" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj arb8]

    $itk_component(primaryToolbar) add button cone \
	-balloonstr "Create a tgc" \
	-helpstr "Create a tgc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj tgc]

    $itk_component(primaryToolbar) add button sphere \
	-balloonstr "Create a sphere" \
	-helpstr "Create a sphere" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj sph]

    $itk_component(primaryToolbar) add button torus \
	-balloonstr "Create a torus" \
	-helpstr "Create a torus" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj tor]

#    $itk_component(primaryToolbar) add button pipe \
	-balloonstr "Create a pipe" \
	-helpstr "Create a pipe" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj pipe]

    $itk_component(primaryToolbar) add menubutton other \
	-balloonstr "Create other primitives" \
	-helpstr "Create other primitives" \
	-relief flat

    # half-size spacer
    $itk_component(primaryToolbar) add frame sep4 \
	-relief sunken \
	-width 2

    $itk_component(primaryToolbar) add button comb \
	-state disabled \
	-balloonstr "Create a combination" \
	-helpstr "Create a combination" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj comb]

    if {$::Archer::plugins != ""} {
	# half-size spacer
	$itk_component(primaryToolbar) add frame sep5 \
	    -relief sunken \
	    -width 2
    }

    _build_wizard_menu

    set parent [$itk_component(primaryToolbar) component other]
    itk_component add primitiveMenu {
	::menu $parent.menu
    } {
	keep -background
    }
    itk_component add arbMenu {
	::menu $itk_component(primitiveMenu).arbmenu
    } {
	keep -background
    }
    $itk_component(arbMenu) add command \
	-label arb4 \
	-command [::itcl::code $this _create_obj arb4]
    $itk_component(arbMenu) add command \
	-label arb5 \
	-command [::itcl::code $this _create_obj arb5]
    $itk_component(arbMenu) add command \
	-label arb6 \
	-command [::itcl::code $this _create_obj arb6]
    $itk_component(arbMenu) add command \
	-label arb7 \
	-command [::itcl::code $this _create_obj arb7]
    $itk_component(arbMenu) add command \
	-label arb8 \
	-command [::itcl::code $this _create_obj arb8]

    $itk_component(primitiveMenu) add cascade \
	-label Arbs \
	-menu $itk_component(arbMenu)
#    $itk_component(primitiveMenu) add command \
	-label bot \
	-command [::itcl::code $this _create_obj bot]
#    $itk_component(primitiveMenu) add command \
	-label comb \
	-command [::itcl::code $this _create_obj comb]
#    $itk_component(primitiveMenu) add command \
	-label ehy \
	-command [::itcl::code $this _create_obj ehy]
#    $itk_component(primitiveMenu) add command \
	-label ell \
	-command [::itcl::code $this _create_obj ell]
#    $itk_component(primitiveMenu) add command \
	-label epa \
	-command [::itcl::code $this _create_obj epa]
#    $itk_component(primitiveMenu) add command \
	-label eto \
	-command [::itcl::code $this _create_obj eto]
#    $itk_component(primitiveMenu) add command \
	-label extrude \
	-command [::itcl::code $this _create_obj extrude]
#    $itk_component(primitiveMenu) add command \
	-label grip \
	-command [::itcl::code $this _create_obj grip]
#    $itk_component(primitiveMenu) add command \
	-label half \
	-command [::itcl::code $this _create_obj half]
#    $itk_component(primitiveMenu) add command \
	-label part \
	-command [::itcl::code $this _create_obj part]
#    $itk_component(primitiveMenu) add command \
	-label pipe \
	-command [::itcl::code $this _create_obj pipe]
#    $itk_component(primitiveMenu) add command \
	-label rhc \
	-command [::itcl::code $this _create_obj rhc]
#    $itk_component(primitiveMenu) add command \
	-label rpc \
	-command [::itcl::code $this _create_obj rpc]
#    $itk_component(primitiveMenu) add command \
	-label sketch \
	-command [::itcl::code $this _create_obj sketch]
    $itk_component(primitiveMenu) add command \
	-label sph \
	-command [::itcl::code $this _create_obj sph]
    $itk_component(primitiveMenu) add command \
	-label tgc \
	-command [::itcl::code $this _create_obj tgc]
    $itk_component(primitiveMenu) add command \
	-label tor \
	-command [::itcl::code $this _create_obj tor]

    set parent [$itk_component(primaryToolbar) component other]
    $parent configure \
	-menu $itk_component(primitiveMenu) \
	-indicator on \
	-activebackground [$parent cget -background]

#    $itk_component(primaryToolbar) add button ehy \
	-balloonstr "Create an ehy" \
	-helpstr "Create an ehy" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj ehy]

#    $itk_component(primaryToolbar) add button epa \
	-balloonstr "Create an epa" \
	-helpstr "Create an epa" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj epa]

#    $itk_component(primaryToolbar) add button rpc \
	-balloonstr "Create an rpc" \
	-helpstr "Create an rpc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj rpc]

#    $itk_component(primaryToolbar) add button rhc \
	-balloonstr "Create an rhc" \
	-helpstr "Create an rhc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj rhc]

#    $itk_component(primaryToolbar) add button ell \
	-balloonstr "Create an ellipsoid" \
	-helpstr "Create an ellipsoid" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj ell]

#    $itk_component(primaryToolbar) add button eto \
	-balloonstr "Create an eto" \
	-helpstr "Create an eto" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj eto]

#    $itk_component(primaryToolbar) add button half \
	-balloonstr "Create a half space" \
	-helpstr "Create a half space" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj half]

#    $itk_component(primaryToolbar) add button part \
	-balloonstr "Create a particle" \
	-helpstr "Create a particle" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj part]

#    $itk_component(primaryToolbar) add button grip \
	-balloonstr "Create a grip" \
	-helpstr "Create a grip" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj grip]

#    $itk_component(primaryToolbar) add button extrude \
	-balloonstr "Create an extrusion" \
	-helpstr "Create an extrusion" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj extrude]

#    $itk_component(primaryToolbar) add button sketch \
	-balloonstr "Create a sketch" \
	-helpstr "Create a sketch" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj sketch]

#    $itk_component(primaryToolbar) add button bot \
	-balloonstr "Create a bot" \
	-helpstr "Create a bot" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this _create_obj bot]

    eval pack configure [pack slaves $itk_component(primaryToolbar)] -padx 2

    # status bar
    itk_component add statusF {
	::frame $itk_interior.statfrm
    } {
	usual
    }

    itk_component add status {
	::label  $itk_component(statusF).status -anchor w -relief sunken -bd 1 \
	    -font $mFontText \
	    -textvariable [::itcl::scope mStatusStr]
    } {
	usual
    }

    itk_component add progress {
	::canvas $itk_component(statusF).progress \
	    -relief sunken \
	    -bd 1 \
	    -width $mProgressBarWidth \
	    -height $mProgressBarHeight
    } {}
#    itk_component add progress {
#	::label $itk_component(statusF).progress -relief sunken -bd 1 \
#	    -font $mFontText \
#	    -textvariable [::itcl::scope mProgressString] \
#	    -anchor w \
#	    -width 40
#    } {
#	usual
#    }

    itk_component add editLabel {
	::label  $itk_component(statusF).edit -relief sunken -bd 1 \
	    -font $mFontText \
	    -width 5
    } {
	usual
    }

    itk_component add dbtype {
	::label  $itk_component(statusF).dbtype -anchor w -relief sunken -bd 1 \
	    -font $mFontText \
	    -width 8 -textvariable [::itcl::scope mDbType]
    } {
	usual
    }

    pack $itk_component(dbtype) -side right -padx 1 -pady 1
    pack $itk_component(editLabel) -side right -padx 1 -pady 1
    pack $itk_component(progress) -fill y -side right -padx 1 -pady 1
    pack $itk_component(status) -expand yes -fill x -padx 1 -pady 1

    # tree control
    _init_tree
    } else {
	# dummy canvas
	itk_component add canvas {
	    ::frame $itk_component(canvasF).canvas \
		-bd 0 \
		-relief flat
	}

	grid $itk_component(canvas_menu) -row 0 -column 0 -sticky w
	grid $itk_component(canvas) -row 1 -column 0 -sticky news
	grid columnconfigure $itk_component(canvasF) 0 -weight 1
	grid rowconfigure $itk_component(canvasF) 1 -weight 1

	# tree control
	_init_tree
    }

    # docking areas
    itk_component add north {
	::frame $itk_interior.north -height 0
    } {
	usual
    }
    itk_component add south {
	::frame $itk_interior.south -height 0
    } {
	usual
    }
    set parent [$itk_component(hpane) childsite topView]
    itk_component add east {
	::frame $parent.east -width 0
    } {
	usual
    }
    itk_component add west {
	::frame $itk_interior.west -width 0
    } {
	usual
    }

    pack $itk_component(south) -side bottom -fill x
    pack $itk_component(north) -side top -fill x
    pack $itk_component(west)  -side left -fill y
    pack $itk_component(east)  -side right -fill y
    if {!$mViewOnly} {
	pack $itk_component(advancedTabs) -fill both -expand yes
    }
    pack $itk_component(tree_frm) -fill both -expand yes
    pack $itk_component(hpane) -fill both -expand yes
    pack $itk_component(vpane) -fill both -expand yes
    pack $itk_component(canvasF) -fill both -expand yes

    if {!$mViewOnly} {
	set _fontNames [font names]
	set _fontIndex [lsearch $_fontNames $mFontArrowsName]
	if {$_fontIndex == -1} {
	    font create $mFontArrowsName -family $mFontArrows
	}
    }

    _build_view_toolbar
    if {!$mViewOnly} {
	_build_info_dialogs
	_build_save_dialog
	_build_view_center_dialog
	_build_preferences_dialog
	_build_db_attr_view
	_build_obj_view_toolbar
	_build_obj_attr_view
	_build_obj_edit_view
    }

    eval itk_initialize $args

    if {!$mViewOnly} {
	# set initial toggle variables
	switch -- $mMode {
	    0 {
		set mVPaneToggle1 $mVPaneFraction1
	    }
	    default {
		set mVPaneToggle3 $mVPaneFraction3
		set mVPaneToggle5 $mVPaneFraction5
	    }
	}

	_read_preferences
	_update_creation_buttons 0
	_update_save_mode
	_update_cut_mode
	_update_copy_mode
	_update_paste_mode
    } else {
	_background_color [lindex $mBackground 0] \
	    [lindex $mBackground 1] \
	    [lindex $mBackground 2]
	_update_theme
    }
    update idletasks
}

# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------
::itcl::configbody Archer::primaryToolbar {
    if {!$mViewOnly} {
	if {$itk_option(-primaryToolbar)} {
	    pack $itk_component(primaryToolbar) \
		-before $itk_component(north) \
		-side top \
		-fill x \
		-pady 2
	} else {
	    pack forget $itk_component(primaryToolbar)
	}
    }
}

::itcl::configbody Archer::viewToolbar {
    if {!$mViewOnly} {
	if {$itk_option(-viewToolbar)} {
	    pack $itk_component(viewToolbar) -expand yes -fill both
	} else {
	    pack forget $itk_component(viewToolbar)
	}
    }
}

::itcl::configbody Archer::statusbar {
    if {!$mViewOnly} {
	if {$itk_option(-statusbar)} {
	    pack $itk_component(statusF) -before $itk_component(south) -side bottom -fill x
	} else {
	    pack forget $itk_component(statusF)
	}
    }
}

::itcl::body Archer::archerWrapper {cmd eflag hflag sflag tflag args} {
    SetWaitCursor

    if {$eflag} {
	set optionsAndArgs [eval dbExpand $args]
	set options [lindex $optionsAndArgs 0]
	set expandedArgs [lindex $optionsAndArgs 1]
    } else {
	set options {}
	set expandedArgs $args
    }

    if {$hflag} {
	set obj [lindex $expandedArgs 0]
	if {$obj != ""} {
	    # First, apply the command to hobj if necessary.
	    # Note - we're making the (ass)umption that the object
	    #        name is the first item in the "expandedArgs" list.
	    if {![catch {dbCmd attr get $obj history} hobj] &&
		$obj != $hobj} {
		set tmpArgs [lreplace $expandedArgs 0 0 $hobj]
		catch {eval dbCmd $cmd $options $tmpArgs}
	    }
	}
    }

    if {[catch {eval dbCmd $cmd $options $expandedArgs} ret]} {
	SetNormalCursor
	return $ret
    }

    if {$sflag} {
	set mNeedSave 1
	_update_save_mode
    }

    dbCmd configure -primitiveLabels {}
    if {$tflag} {
	catch {_refresh_tree}
    }
    SetNormalCursor

    return $ret
}

::itcl::body Archer::mgedWrapper {cmd eflag hflag sflag tflag args} {
    if {![info exists itk_component(mged)]} {
	return
    }

    eval archerWrapper $cmd $eflag $hflag $sflag $tflag $args
}

::itcl::body Archer::sdbWrapper {cmd eflag hflag sflag tflag args} {
    if {![info exists itk_component(sdb)]} {
	return
    }

    eval archerWrapper $cmd $eflag $hflag $sflag $tflag $args
}

::itcl::body Archer::_build_canvas_menubar {}  {
    # View Menu
    $itk_component(canvas_menu) add menubutton view \
	    -text "View" -menu {
	options -tearoff 1

	command front -label "Front" \
		-helpstr "Set view to front"
	command rear -label "Rear" \
		-helpstr "Set view to rear"
	command port -label "Port" \
		-helpstr "Set view to port/left"
	command starboard -label "Starboard" \
		-helpstr "Set view to starboard/right"
	command top -label "Top" \
		-helpstr "Set view to top"
	command bottom -label "Bottom" \
		-helpstr "Set view to bottom"
	separator sep0
	command 35,25 -label "35,25" \
		-helpstr "Set view to az=35, el=25"
	command 45,45 -label "45,45" \
		-helpstr "Set view to az=45, el=45"
    }

    $itk_component(canvas_menu) menuconfigure .view.front \
	-command [::itcl::code $this _do_ae 0 0] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.rear \
	-command [::itcl::code $this _do_ae 180 0] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.port \
	-command [::itcl::code $this _do_ae 90 0] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.starboard \
	-command [::itcl::code $this _do_ae -90 0] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.top \
	-command [::itcl::code $this _do_ae 0 90] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.bottom \
	-command [::itcl::code $this _do_ae 0 -90] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.35,25 \
	-command [::itcl::code $this _do_ae 35 25] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.45,45 \
	-command [::itcl::code $this _do_ae 45 45] \
	-state disabled

    # Background Menu
    $itk_component(canvas_menu) add menubutton background \
	    -text "Background" -menu {
	options -tearoff 1

	command black -label "Black" \
		-helpstr "Set background color black"
	command grey -label "Grey" \
		-helpstr "Set background color grey"
	command white -label "White" \
		-helpstr "Set background color white"
	command lblue -label "Light Blue" \
		-helpstr "Set background color light blue"
	command dblue -label "Dark Blue" \
		-helpstr "Set background color dark blue"
    }

    $itk_component(canvas_menu) menuconfigure .background.black \
	    -command [::itcl::code $this _background_color 0 0 0]
    $itk_component(canvas_menu) menuconfigure .background.grey \
	    -command [::itcl::code $this _background_color 100 100 100]
    $itk_component(canvas_menu) menuconfigure .background.white \
	    -command [::itcl::code $this _background_color 255 255 255]
    $itk_component(canvas_menu) menuconfigure .background.lblue \
	    -command [::itcl::code $this _background_color 0 198 255]
    $itk_component(canvas_menu) menuconfigure .background.dblue \
	    -command [::itcl::code $this _background_color 0 0 160]

    # Raytrace Menu
    if {!$mViewOnly} {
	$itk_component(canvas_menu) add menubutton raytrace \
	    -text "Raytrace" -menu {
		options -tearoff 1

		cascade rt \
		    -label "rt" \
		    -menu {
			command fivetwelve \
			    -label "512x512" \
			    -helpstr "Raytrace at a resolution of 512x512"
			command tentwenty \
			    -label "1024x1024" \
			    -helpstr "Raytrace at a resolution of 1024x1024"
			command window \
			    -label "Window Size" \
			    -helpstr "Raytrace at a resolution matching the window width"
		    }
		cascade rtcheck \
		    -label "rtcheck" \
		    -menu {
			command fifty \
			    -label "50x50" \
			    -helpstr "Check for overlaps using a 50x50 grid"
			command hundred \
			    -label "100x100" \
			    -helpstr "Check for overlaps using a 100x100 grid"
			command twofiftysix \
			    -label "256x256" \
			    -helpstr "Check for overlaps using a 256x256 grid"
			command fivetwelve \
			    -label "512x512" \
			    -helpstr "Check for overlaps using a 512x512 grid"
		    }
		cascade rtedge \
		    -label "rtedge" \
		    -menu {
			command fivetwelve \
			    -label "512x512" \
			    -helpstr "Raytrace at a resolution of 512x512"
			command tentwenty \
			    -label "1024x1024" \
			    -helpstr "Raytrace at a resolution of 1024x1024"
			command window \
			    -label "Window Size" \
			    -helpstr "Raytrace at a resolution matching the window width"
		    }
		separator sep0
		command nirt \
		    -label "nirt" \
		    -helpstr "Launch nirt from view center"
	    }

	$itk_component(canvas_menu) menuconfigure .raytrace.rt \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rt.fivetwelve \
	    -command [::itcl::code $this launchRtApp rt 512] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rt.tentwenty \
	    -command [::itcl::code $this launchRtApp rt 1024] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rt.window \
	    -command [::itcl::code $this launchRtApp rt window] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.fifty \
	    -command [::itcl::code $this launchRtApp rtcheck 50] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.hundred \
	    -command [::itcl::code $this launchRtApp rtcheck 100] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.twofiftysix \
	    -command [::itcl::code $this launchRtApp rtcheck 256] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.fivetwelve \
	    -command [::itcl::code $this launchRtApp rtcheck 512] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge.fivetwelve \
	    -command [::itcl::code $this launchRtApp rtedge 512] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge.tentwenty \
	    -command [::itcl::code $this launchRtApp rtedge 1024] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge.window \
	    -command [::itcl::code $this launchRtApp rtedge window] \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.nirt \
	    -command [::itcl::code $this launchNirt] \
	    -state disabled
    }
}

::itcl::body Archer::_build_utility_menu {} {
    if {$Archer::inheritFromToplevel} {
	# Utility
	itk_component add utilityMenu {
	    ::menu $itk_component(menubar).utilityMenu \
		-tearoff 1
	} {
	    #	rename -font -menuFont menuFont MenuFont
	    #	keep -font
	    keep -background
	}

	$itk_component(menubar) insert "Help" cascade \
	    -label "Utilities" \
	    -menu $itk_component(utilityMenu)
    } else {
	set mb [$itk_component(menubar) insert end menubutton \
		    utilities -text "Utilities"]
	set itk_component(utilityMenu) $mb.menu
    }
}

::itcl::body Archer::_update_utility_menu {} {
    foreach dialog [::itcl::find object -class ::iwidgets::Dialog] {
	if {[regexp {utility_dialog} $dialog]} {
	    catch {rename $dialog ""}
	}
    }

    if {$mMode == 0} {
	if {$Archer::inheritFromToplevel} {
	    if {[info exists itk_component(utilityMenu)]} {
		$itk_component(menubar) delete "Utilities"
		set umenu $itk_component(utilityMenu)
		itk_component delete utilityMenu
		rename $umenu ""
	    }
	} else {
	    if {[info exists itk_component(utilityMenu)]} {
		$itk_component(menubar) delete utilities
		unset itk_component(utilityMenu)
	    }
	}

	return
    }

    # Look for appropriate utility plugins
    set uplugins {}
    foreach plugin $::Archer::plugins {
	set majorType [$plugin get -majorType]
	if {$majorType != $pluginMajorTypeUtility} {
	    continue
	}

	set minorType [$plugin get -minorType]
	if {($minorType == $pluginMinorTypeBoth &&
	     ([info exists itk_component(mged)] ||
	      [info exists itk_component(sdb)])) ||
	    ($minorType == $pluginMinorTypeMged &&
	     [info exists itk_component(mged)]) ||
	    ($minorType == $pluginMinorTypeSdb &&
	     [info exists itk_component(sdb)])} {
	    lappend uplugins $plugin
	}
    }

    if {$uplugins == {}} {
	if {$Archer::inheritFromToplevel} {
	    if {[info exists itk_component(utilityMenu)]} {
		$itk_component(menubar) delete "Utilities"
		set umenu $itk_component(utilityMenu)
		itk_component delete utilityMenu
		rename $umenu ""
	    }
	} else {
	    if {[info exists itk_component(utilityMenu)]} {
		$itk_component(menubar) delete utilities
		unset itk_component(utilityMenu)
	    }
	}

	return
    }

    if {![info exists itk_component(utilityMenu)]} {
	_build_utility_menu
    }

    $itk_component(utilityMenu) delete 0 end

    foreach plugin $uplugins {
	set class [$plugin get -class]
	set wname [$plugin get -name]

	$itk_component(utilityMenu) add command \
	    -label $wname \
	    -command [::itcl::code $this _invoke_utility_dialog $class $wname [namespace tail $this]]
    }
}

::itcl::body Archer::_invoke_utility_dialog {class wname w} {
    set instance [::itcl::find object -class $class]
    if {$instance != ""} {
	raise [winfo toplevel [namespace tail $instance]]
	return
    }

#    set dialog $itk_interior.utilityDialog
#    ::iwidgets::dialog $dialog
    set dialog [::iwidgets::dialog $itk_interior.utility_\#auto \
		    -modality none \
		    -title "$wname Dialog" \
		    -background $SystemButtonFace]

    $dialog hide 0
    $dialog hide 1
    $dialog hide 3
    $dialog configure \
	-thickness 2 \
	-buttonboxpady 0

    # Turn of the default button
    bind $dialog <Return> {}
#    $dialog buttonconfigure 0 \
	-defaultring no
#    $dialog buttonconfigure 2 \
	-defaultring yes \
	-defaultringpad 1 \
	-borderwidth 1 \
	-pady 0

    # ITCL can be nasty
    set win [$dialog component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$dialog childsite]
    set utility [$class $parent.\#auto $this]

    set row 0
    grid $utility -row $row -stick nsew
    grid rowconfigure $parent $row -weight 1
    grid columnconfigure $parent 0 -weight 1

    $dialog buttonconfigure Cancel \
	-borderwidth 1 \
	-pady 0 \
	-text "Dismiss" \
	-command "$dialog deactivate; ::itcl::delete object $dialog"

    wm protocol $dialog WM_DELETE_WINDOW "$dialog deactivate; ::itcl::delete object $dialog"
#    wm geometry $dialog "500x500"

    # Event bindings
    bind $dialog <Enter> "raise $dialog"
    bind $dialog <Configure> "raise $dialog"
    bind $dialog <FocusOut> "raise $dialog"

    $dialog center $w
    $dialog activate
}

::itcl::body Archer::_build_wizard_menu {} {
    $itk_component(primaryToolbar) add menubutton wizards \
	-balloonstr "Wizard Plugins" \
	-helpstr "Wizard Plugins" \
	-relief flat

    set parent [$itk_component(primaryToolbar) component wizards]
    itk_component add wizardMenu {
	::menu $parent.menu
    } {
	keep -background
    }

    $parent configure \
	-menu $itk_component(wizardMenu) \
	-indicatoron on \
	-activebackground [$parent cget -background]
}

::itcl::body Archer::_update_wizard_menu {} {
    # Look for appropriate wizard plugins
    set wplugins {}
    foreach plugin $::Archer::plugins {
	set majorType [$plugin get -majorType]
	if {$majorType != $pluginMajorTypeWizard} {
	    continue
	}

	set minorType [$plugin get -minorType]
	if {($minorType == $pluginMinorTypeBoth &&
	     ([info exists itk_component(mged)] ||
	      [info exists itk_component(sdb)])) ||
	    ($minorType == $pluginMinorTypeMged &&
	     [info exists itk_component(mged)]) ||
	    ($minorType == $pluginMinorTypeSdb &&
	     [info exists itk_component(sdb)])} {
	    lappend wplugins $plugin
	}
    }

    $itk_component(wizardMenu) delete 0 end

    if {$wplugins == {}} {
	$itk_component(primaryToolbar) itemconfigure wizards -state disabled
	
	return
    }

    $itk_component(primaryToolbar) itemconfigure wizards -state normal

    foreach plugin $wplugins {
	set class [$plugin get -class]
	set wname [$plugin get -name]
	set action [$plugin get -action]

	$itk_component(wizardMenu) add command \
	    -label $wname \
	    -command [::itcl::code $this _invoke_wizard_dialog $class $action $wname]
    }
}

::itcl::body Archer::_invoke_wizard_dialog {class action wname} {
    dbCmd make_name -s 1
    set name [string tolower $class]
    set name [regsub wizard $name ""]
    #XXX Temporary special case for TankWizardI
    #if {$class == "TankWizardI"} {
    #set name "simpleTank"
    #} else {
    #set name [dbCmd make_name $name]
    #}
    set name [dbCmd make_name $name]
    set oname $name
    set origin [dbCmd center]
    set units [dbCmd units]

    set dialog $itk_interior.wizardDialog
    ::iwidgets::dialog $dialog \
	    -modality application \
	    -title "$wname Dialog" \
	    -background $SystemButtonFace

    $dialog hide 1
    $dialog hide 3
    $dialog configure \
	-thickness 2 \
	-buttonboxpady 0
    $dialog buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 1 \
	-borderwidth 1 \
	-pady 0
    $dialog buttonconfigure 2 \
	-borderwidth 1 \
	-pady 0

    # ITCL can be nasty
#    set win [$dialog component bbox component OK component hull]
#    after idle "$win configure -relief flat"
    set win [$dialog component bbox component Cancel component hull]
    after idle "$win configure -relief flat"

    set parent [$dialog childsite]
    set wizard [$class $parent.\#auto $this $name "" $origin $units]

    set row 0
    grid $wizard -row $row -stick nsew
    grid rowconfigure $parent $row -weight 1
    grid columnconfigure $parent 0 -weight 1

    $dialog buttonconfigure OK \
	-command [::itcl::code $this _build_wizard_obj $dialog $wizard $action $oname]
    $dialog buttonconfigure Cancel \
	-command "$dialog deactivate; ::itcl::delete object $dialog"

    wm protocol $dialog WM_DELETE_WINDOW "$dialog deactivate; ::itcl::delete object $dialog"
    wm geometry $dialog "400x400"
    $dialog center [namespace tail $this]
    $dialog activate
}

::itcl::body Archer::_build_wizard_obj {dialog wizard action oname} {
    $dialog deactivate

    set name [$wizard getWizardTop]
    set origin [$wizard getWizardOrigin]

    if {[llength $origin] != 3 ||
	![string is double [lindex $origin 0]] ||
	![string is double [lindex $origin 1]] ||
	![string is double [lindex $origin 2]]} {
	set origin {0.0 0.0 0.0}
    }

    if {$name != ""} {
	$wizard setWizardTop $name
	$wizard setWizardOrigin $origin
	_invoke_wizard_update $wizard $action $oname $name
    }

    ::itcl::delete object $wizard
    ::itcl::delete object $dialog
}

::itcl::body Archer::_invoke_wizard_update {wizard action oname name} {
    if {$name == ""} {
	set name [$wizard getWizardTop]
    }

    #XXX Temporary special case for TankWizardI
    if {[namespace tail [$wizard info class]] != "TankWizardI"} {
    # Here we have the case where the name is being
    # changed to an object that already exists.
    if {$oname != $name && ![catch {dbCmd get $name} stuff]} {
	::sdialogs::Stddlgs::errordlg "User Error" \
		"$name already exists!"
	return
    }
    }

    dbCmd erase $oname
    dbCmd killtree $oname
    dbCmd configure -autoViewEnable 0
    set obj [$wizard $action]
    dbCmd configure -autoViewEnable 1

    set mNeedSave 1
    _update_save_mode

    set xmlAction [$wizard getWizardXmlAction]
    if {$xmlAction != ""} {
	set xml [$wizard $xmlAction]
	foreach callback $wizardXmlCallbacks {
	    $callback $xml
	}
    }

    _refresh_tree
    _select_node [$itk_component(tree) find $obj]
}

::itcl::body Archer::_build_db_attr_view {} {
    set parent [$itk_component(vpane) childsite attrView]
    $parent configure -borderwidth 1 -relief sunken

    itk_component add dbAttrView {
	::iwidgets::scrolledframe $parent.dbattrview \
	    -vscrollmode dynamic \
	    -hscrollmode dynamic
    } {}

    set parent [$itk_component(dbAttrView) childsite]

    itk_component add dbnamekey {
	::label $parent.namekey \
	    -font $mFontTextBold \
	    -text "Database:" -anchor e
    } {}

    itk_component add dbnameval {
	::label $parent.nameval \
	    -font $mFontText \
	    -textvariable [::itcl::scope mDbName] \
	    -anchor w \
	    -borderwidth 1 \
	    -relief flat
#	    -fg #7e7e7e
    } {}

    itk_component add dbtitlekey {
	::label $parent.titlekey \
	    -font $mFontTextBold \
	    -text "Title:" -anchor e
    } {}

    itk_component add dbtitleval {
	::label $parent.titleval \
	    -font $mFontText \
	    -textvariable [::itcl::scope mDbTitle] \
	    -anchor w \
	    -borderwidth 1 \
	    -relief flat
#	    -fg #7e7e7e
    } {}

    itk_component add dbunitskey {
	::label $parent.unitskey \
	    -font $mFontTextBold \
	    -text "Units:" -anchor e
    } {}

    itk_component add dbunitsval {
	::label $parent.unitsval \
	    -font $mFontText \
	    -textvariable [::itcl::scope mDbUnits] \
	    -anchor w \
	    -borderwidth 1 \
	    -relief flat
#	    -fg #7e7e7e
    } {}

    set i 0
    grid $itk_component(dbnamekey) $itk_component(dbnameval) \
	-row $i -sticky new
    incr i
    grid $itk_component(dbtitlekey) $itk_component(dbtitleval) \
	-row $i -sticky new
    incr i
    grid $itk_component(dbunitskey) $itk_component(dbunitsval) \
	-row $i -sticky new
    grid columnconfigure $itk_component(dbAttrView) 1 -weight 1
}

::itcl::body Archer::_init_db_attr_view {name} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    catch {pack forget $itk_component(dbAttrView)}
    catch {pack forget $itk_component(objViewToolbar)}
    catch {pack forget $itk_component(objAttrView)}
    catch {pack forget $itk_component(objEditView)}
    catch {pack forget $itk_component(objEditToolbarF)}
    catch {pack forget $itk_component(noWizard)}
    set mNoWizardActive 0

    # delete the previous wizard instance
    if {$mWizardClass != ""} {
	::destroy $itk_component($mWizardClass)
	::destroy $itk_component(wizardUpdate)
	set mWizardClass ""
	set mWizardTop ""
	set mWizardState ""
    }

    #XXX These should get moved to the Load method
    set mDbName $name
    if {[info exists itk_component(mged)]} {
	set mDbTitle [$itk_component(mged) title]
	set mDbUnits [$itk_component(mged) units]
    }
    set mPrevObjViewMode $OBJ_ATTR_VIEW_MODE
    set mPrevSelectedObjPath ""
    set mPrevSelectedObj ""
    set mSelectedObjPath ""
    set mSelectedObj ""
    set mSelectedObjType ""
    set mPasteActive 0
    set mPendingEdits 0

    # The scrollmode options are needed so that the
    # scrollbars dynamically appear/disappear. Sheesh!
    update
    after idle $itk_component(dbAttrView) configure \
	-hscrollmode dynamic \
	-vscrollmode dynamic

    pack $itk_component(dbAttrView) -expand yes -fill both
}

::itcl::body Archer::_build_obj_view_toolbar {} {
    set parent [$itk_component(vpane) childsite attrView]
    itk_component add objViewToolbar {
	::iwidgets::toolbar $parent.objViewToolbar \
		-helpvariable [::itcl::scope mStatusStr] \
		-balloonfont "{CG Times} 8" \
		-balloonbackground \#ffffdd \
		-borderwidth 1 \
		-orient horizontal
    } {
	# XXX If uncommented, the following line hoses things
	#usual
    }

    $itk_component(objViewToolbar) add radiobutton objEditView \
	-helpstr "Object edit mode" \
	-balloonstr "Object edit mode" \
	-variable [::itcl::scope mObjViewMode] \
	-value $OBJ_EDIT_VIEW_MODE \
	-command [::itcl::code $this _init_obj_edit_view]

    $itk_component(objViewToolbar) add radiobutton objAttrView \
	-helpstr "Object text mode" \
	-balloonstr "Object text mode" \
	-variable [::itcl::scope mObjViewMode] \
	-value $OBJ_ATTR_VIEW_MODE \
	-command [::itcl::code $this _init_obj_attr_view]
}

::itcl::body Archer::_build_obj_attr_view {} {
    set parent [$itk_component(vpane) childsite attrView]
    itk_component add objAttrView {
	::frame $parent.objattrview
    } {}

    itk_component add objAttrText {
	::iwidgets::scrolledtext $itk_component(objAttrView).objattrtext \
	    -vscrollmode dynamic \
	    -hscrollmode dynamic \
	    -wrap none \
	    -state disabled \
	    -borderwidth 1 \
	    -relief sunken
    } {}

    pack $itk_component(objAttrText) -expand yes -fill both
}

::itcl::body Archer::_init_obj_attr_view {} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    if {$mSelectedObj == ""} {
	return
    }

    if {$mWizardClass == "" &&
	!$mNoWizardActive &&
	$mPrevObjViewMode == $OBJ_EDIT_VIEW_MODE} {
	_finalize_obj_edit $mPrevSelectedObj $mPrevSelectedObjPath
    }
    set mPrevObjViewMode $mObjViewMode

    catch {pack forget $itk_component(dbAttrView)}
    catch {pack forget $itk_component(objViewToolbar)}
    catch {pack forget $itk_component(objAttrView)}
    catch {pack forget $itk_component(objEditView)}
    catch {pack forget $itk_component(objEditToolbarF)}
    catch {pack forget $itk_component(noWizard)}
    set mNoWizardActive 0

    # delete the previous wizard instance
    if {$mWizardClass != ""} {
	::destroy $itk_component($mWizardClass)
	::destroy $itk_component(wizardUpdate)
	set mWizardClass ""
	set mWizardTop ""
	set mWizardState ""
    }

    $itk_component(objAttrText) configure \
	-state normal
    $itk_component(objAttrText) delete 1.0 end
    set odata [$itk_component(mged) l $mSelectedObj]
    $itk_component(objAttrText) insert end "$odata"

    # The scrollmode options are needed so that the
    # scrollbars dynamically appear/disappear. Sheesh!
    update
    after idle $itk_component(objAttrText) configure \
	-state disabled \
	-hscrollmode dynamic \
	-vscrollmode dynamic

    pack $itk_component(objViewToolbar) -expand no -fill both -anchor n
    pack $itk_component(objAttrView) -expand yes -fill both -anchor n
}

::itcl::body Archer::_build_obj_edit_view {} {
    set parent [$itk_component(vpane) childsite attrView]
    itk_component add objEditView {
	::frame $parent.objeditview \
	    -borderwidth 1 \
	    -relief sunken
    } {}

    itk_component add objEditToolbarF {
	::frame $parent.objEditToolbarF \
	    -borderwidth 1 \
	    -relief sunken
    } {}

    itk_component add objEditToolbar {
	::iwidgets::toolbar $itk_component(objEditToolbarF).objEditToolbar \
		-helpvariable [::itcl::scope mStatusStr] \
		-balloonfont "{CG Times} 8" \
		-balloonbackground \#ffffdd \
		-borderwidth 1 \
		-orient horizontal \
		-font $mFontText
    } {
	# XXX If uncommented, the following line hoses things
	#usual
    }

    $itk_component(objEditToolbar) add button prev \
	-text $mLeftArrow \
	-helpstr "Go to the previous object state" \
	-balloonstr "Go to the previous object state" \
	-pady 0 \
	-command [::itcl::code $this _goto_prev_obj]
    $itk_component(objEditToolbar) add button apply \
	-text Apply \
	-helpstr "Apply the current edits" \
	-balloonstr "Apply the current edits" \
	-pady 0 \
	-command [::itcl::code $this _apply_edit]
    $itk_component(objEditToolbar) add button reset \
	-text Reset \
	-helpstr "Delete the current edits" \
	-balloonstr "Delete the current edits" \
	-pady 0 \
	-command [::itcl::code $this _reset_edit]
    $itk_component(objEditToolbar) add button next \
	-text $mRightArrow \
	-helpstr "Go to the next object state" \
	-balloonstr "Go to the next object state" \
	-pady 0 \
	-command [::itcl::code $this _goto_next_obj]

    # Configure the font here so that the font actually gets
    # applied to the button. Sheesh!!!
    $itk_component(objEditToolbar) itemconfigure prev \
	-font $mFontArrowsName
    $itk_component(objEditToolbar) itemconfigure next \
	-font $mFontArrowsName

    pack $itk_component(objEditToolbar) -expand yes
}

::itcl::body Archer::_init_obj_edit_view {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    if {$mSelectedObj == ""} {
	return
    }

    # If the required wizard is already loaded,
    # initialize and return
    if {$mWizardClass != "" &&
	$mPrevSelectedObj == $mSelectedObj} {
	#XXX place holder for code to initialize the wizard
	return
    }

    if {$mPendingEdits} {
	_finalize_obj_edit $mPrevSelectedObj $mPrevSelectedObjPath
    }
    set mPrevObjViewMode $mObjViewMode

    catch {pack forget $itk_component(dbAttrView)}
    catch {pack forget $itk_component(objViewToolbar)}
    catch {pack forget $itk_component(objAttrView)}
    catch {pack forget $itk_component(objEditView)}
    catch {pack forget $itk_component(objEditToolbarF)}
    catch {pack forget $itk_component(noWizard)}
    set mNoWizardActive 0

	
    # delete the previous wizard instance
    if {$mWizardClass != ""} {
	::destroy $itk_component($mWizardClass)
	::destroy $itk_component(wizardUpdate)
	set mWizardClass ""
	set mWizardTop ""
	set mWizardState ""
    }

    if {[catch {$itk_component(mged) attr get $mSelectedObj WizardTop} mWizardTop]} {
	set mWizardTop ""
    } else {
	if {[catch {$itk_component(mged) attr get $mWizardTop WizardClass} mWizardClass]} {
	    set mWizardClass ""
	}
    }

    if {$mWizardClass == ""} {
	_init_obj_edit $mSelectedObj

	# free the current primitive view if any
	set _slaves [pack slaves $itk_component(objEditView)]
	catch {eval pack forget $_slaves}

	_init_edit

	if {[info exists itk_component(mged)]} {
	    pack $itk_component(objViewToolbar) -expand no -fill x -anchor n
	    pack $itk_component(objEditView) -expand yes -fill both -anchor n
	    pack $itk_component(objEditToolbarF) -expand no -fill x -anchor s
	}
    } else {
	if {[pluginQuery $mWizardClass] == -1} {
	    # the wizard plugin has not been loaded
	    _init_obj_wizard $mSelectedObj 0
	} else {
	    _init_obj_wizard $mSelectedObj 1
	}
    }
}

::itcl::body Archer::_init_obj_history {obj} {
    if {$obj == "" ||
	[catch {dbCmd get $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    # The history list is non-existent or
    # the link to it is broken, so create
    # a new one.
    if {$hname == "" ||
	[catch {dbCmd get $hname} stuff]} {
	dbCmd make_name -s 1
	set hname [dbCmd make_name $obj.version]
	#dbCmd attr set $obj history $hname

	dbCmd cp $obj $hname
	dbCmd hide $hname
	dbCmd attr set $hname previous ""
	dbCmd attr set $hname next ""
	dbCmd attr set $obj history $hname
    }
}

::itcl::body Archer::_init_obj_edit {obj} {
    if {![info exists itk_component(mged)]} {
	return
    }

    set mPendingEdits 0

    _init_obj_history $obj

    _update_prev_obj_button $obj
    _update_next_obj_button $obj

    # Disable the apply and reset buttons
    $itk_component(objEditToolbar) itemconfigure apply \
	-state disabled
    $itk_component(objEditToolbar) itemconfigure reset \
	-state disabled
}

## - _init_obj_wizard
#
# Note - Before we get here, any previous wizard instances are destroyed
#        and mWizardClass has been initialized to the name of the wizard class.
#        Also, mWizardState is initialized to "".
#
::itcl::body Archer::_init_obj_wizard {obj wizardLoaded} {
    set parent [$itk_component(vpane) childsite attrView]

    if {[catch {$itk_component(mged) attr get $mWizardTop WizardState} mWizardState]} {
	set wizardStateFound 0
    } else {
	set wizardStateFound 1
    }

    if {!$wizardLoaded ||
	!$wizardStateFound} {

	if {!$wizardLoaded} {
	    set msg "The $mWizardClass wizard has not been loaded."
	} elseif {!$wizardTopFound} {
	    set msg "The \"WizardTop\" attribute has not been set."
	} else {
	    set msg "The \"WizardState\" attribute has not been set."
	}

	_init_no_wizard $parent $msg
    } else {
	if {[catch {$itk_component(mged) attr get $mWizardTop WizardOrigin} wizOrigin]} {
	    set wizOrigin [dbCmd center]
	    set wizUnits [dbCmd units]
	} elseif {[catch {$itk_component(mged) attr get $mWizardTop WizardUnits} wizUnits]} {
	    set wizUnits mm
	}

	set fail [catch {$mWizardClass $parent.\#auto $this $mWizardTop $mWizardState $wizOrigin $wizUnits} wiz]

	if {$fail} {
	    _init_no_wizard $parent "Failed to initialize the $mWizardClass wizard."
	} else {
	    # try to create a new wizard instance
	    itk_component add $mWizardClass {
		set wiz $wiz
	    } {}

	    set action [$itk_component($mWizardClass) getWizardAction]

	    # create the update button
	    itk_component add wizardUpdate {
		::button $parent.wizardUpdate \
		    -text "Update" \
		    -command [::itcl::code $this _invoke_wizard_update \
				  $itk_component($mWizardClass) $action \
				  $mWizardTop ""]
	    } {}

	    pack $itk_component(objViewToolbar) -expand no -fill x -anchor n
	    pack $itk_component($mWizardClass) -expand yes -fill both -anchor n
	    pack $itk_component(wizardUpdate) -expand no -fill none -anchor s
	}
    }
}

::itcl::body Archer::_init_no_wizard {parent msg} {
    if {![info exists itk_component(noWizard)]} {
	itk_component add noWizard {
	    ::label $parent.noWizard
	} {}
    }

    $itk_component(noWizard) configure -text $msg

    pack $itk_component(objViewToolbar) -expand no -fill x -anchor n
    pack $itk_component(noWizard) -expand yes -fill both -anchor n

    set mWizardClass ""
    set mWizardTop ""
    set mWizardState ""
    set mNoWizardActive 1
}

::itcl::body Archer::_update_obj_edit_view {} {
    set mPendingEdits 1
    _redraw_obj $mSelectedObjPath

    # Enable the apply and reset buttons
    $itk_component(objEditToolbar) itemconfigure apply \
	-state normal
    $itk_component(objEditToolbar) itemconfigure reset \
	-state normal
}

::itcl::body Archer::_update_obj_edit {updateObj needInit needSave} {
    set renderData [dbCmd how -b $mSelectedObjPath]
    set renderMode [lindex $renderData 0]
    set renderTrans [lindex $renderData 1]
    dbCmd configure -autoViewEnable 0
    dbCmd kill $mSelectedObj
    dbCmd cp $updateObj $mSelectedObj
    dbCmd unhide $mSelectedObj
    dbCmd attr rm $mSelectedObj previous
    dbCmd attr rm $mSelectedObj next

    if {$needInit} {
	_init_edit
    }

    if {$needSave} {
	set mNeedSave 1
	_update_save_mode
    }

    _render $mSelectedObjPath $renderMode $renderTrans 0
    dbCmd configure -autoViewEnable 1
}

::itcl::body Archer::_finalize_obj_edit {obj path} {
    if {$obj == "" ||
	[catch {dbCmd get $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    # No history
    if {$hname == "" ||
	[catch {dbCmd get $hname} stuff]} {
	return
    }

    # Presumably, by the time we get here the
    # user has either saved or refused to save
    # any pending edits and so the history list
    # contains the only known valid version of
    # the object. We now copy the valid version
    # in place of the object.
    set renderData [dbCmd how -b $path]
    set renderMode [lindex $renderData 0]
    set renderTrans [lindex $renderData 1]
    dbCmd configure -autoViewEnable 0
    dbCmd kill $obj
    dbCmd cp $hname $obj
    dbCmd unhide $obj
    dbCmd attr rm $obj previous
    dbCmd attr rm $obj next
    _render $path $renderMode $renderTrans 0
    dbCmd configure -autoViewEnable 1
}

::itcl::body Archer::_reset_edit {} {
    set obj $mSelectedObj

    if {$obj == "" ||
	[catch {dbCmd get $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get $hname} stuff]} {
	return
    }

    _update_obj_edit $hname 1 0
    #_refresh_tree

    # Disable the apply and reset buttons
    $itk_component(objEditToolbar) itemconfigure apply \
	-state disabled
    $itk_component(objEditToolbar) itemconfigure reset \
	-state disabled
}

::itcl::body Archer::_init_edit {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    set odata [dbCmd get $mSelectedObj]
#    set mSelectedObjType [lindex $odata 0]
    set mSelectedObjType [dbCmd get_type $mSelectedObj]
    set odata [lrange $odata 1 end]

    switch -- $mSelectedObjType {
	"arb4" {
	    if {![info exists itk_component(arb4View)]} {
		_build_arb4_edit_view
	    }
	    _init_arb4_edit_view $odata
	}
	"arb5" {
	    if {![info exists itk_component(arb5View)]} {
		_build_arb5_edit_view
	    }
	    _init_arb5_edit_view $odata
	}
	"arb6" {
	    if {![info exists itk_component(arb6View)]} {
		_build_arb6_edit_view
	    }
	    _init_arb6_edit_view $odata
	}
	"arb7" {
	    if {![info exists itk_component(arb7View)]} {
		_build_arb7_edit_view
	    }
	    _init_arb7_edit_view $odata
	}
	"arb8" {
	    if {![info exists itk_component(arb8View)]} {
		_build_arb8_edit_view
	    }
	    _init_arb8_edit_view $odata
	}
	"bot" {
	    if {![info exists itk_component(botView)]} {
		_build_bot_edit_view
	    }
	    _init_bot_edit_view $odata
	}
	"comb" {
	    if {![info exists itk_component(combView)]} {
		_build_comb_edit_view
	    }
	    _init_comb_edit_view $odata
	}
	"ell" {
	    if {![info exists itk_component(ellView)]} {
		_build_ell_edit_view
	    }
	    _init_ell_edit_view $odata
	}
	"ehy" {
	    if {![info exists itk_component(ehyView)]} {
		_build_ehy_edit_view
	    }
	    _init_ehy_edit_view $odata
	}
	"epa" {
	    if {![info exists itk_component(epaView)]} {
		_build_epa_edit_view
	    }
	    _init_epa_edit_view $odata
	}
	"eto" {
	    if {![info exists itk_component(etoView)]} {
		_build_eto_edit_view
	    }
	    _init_eto_edit_view $odata
	}
	"extrude" {
	    if {![info exists itk_component(extrudeView)]} {
		_build_extrude_edit_view
	    }
	    _init_extrude_edit_view $odata
	}
	"grip" {
	    if {![info exists itk_component(gripView)]} {
		_build_grip_edit_view
	    }
	    _init_grip_edit_view $odata
	}
	"half" {
	    if {![info exists itk_component(halfView)]} {
		_build_half_edit_view
	    }
	    _init_half_edit_view $odata
	}
	"part" {
	    if {![info exists itk_component(partView)]} {
		_build_part_edit_view
	    }
	    _init_part_edit_view $odata
	}
	"pipe" {
	    if {![info exists itk_component(pipeView)]} {
		_build_pipe_edit_view
	    }
	    _init_pipe_edit_view $odata
	}
	"rpc" {
	    if {![info exists itk_component(rpcView)]} {
		_build_rpc_edit_view
	    }
	    _init_rpc_edit_view $odata
	}
	"rhc" {
	    if {![info exists itk_component(rhcView)]} {
		_build_rhc_edit_view
	    }
	    _init_rhc_edit_view $odata
	}
	"sketch" {
	    if {![info exists itk_component(sketchView)]} {
		_build_sketch_edit_view
	    }
	    _init_sketch_edit_view $odata
	}
	"sph" {
	    if {![info exists itk_component(sphView)]} {
		_build_sphere_edit_view
	    }
	    _init_sphere_edit_view $odata
	}
	"tgc" {
	    if {![info exists itk_component(tgcView)]} {
		_build_tgc_edit_view
	    }
	    _init_tgc_edit_view $odata
	}
	"tor" {
	    if {![info exists itk_component(torView)]} {
		_build_torus_edit_view
	    }
	    _init_torus_edit_view $odata
	}
    }
}

::itcl::body Archer::_apply_edit {} {
    set doRefreshTree 0
    switch -- $mSelectedObjType {
	"arb4" {
	    $itk_component(arb4View) updateGeometry
	}
	"arb5" {
	    $itk_component(arb5View) updateGeometry
	}
	"arb6" {
	    $itk_component(arb6View) updateGeometry
	}
	"arb7" {
	    $itk_component(arb7View) updateGeometry
	}
	"arb8" {
	    $itk_component(arb8View) updateGeometry
	}
	"bot" {
	    $itk_component(botView) updateGeometry
	}
	"comb" {
	    $itk_component(combView) updateGeometry
	    set doRefreshTree 1
	}
	"ell" {
	    $itk_component(ellView) updateGeometry
	}
	"ehy" {
	    $itk_component(ehyView) updateGeometry
	}
	"epa" {
	    $itk_component(epaView) updateGeometry
	}
	"eto" {
	    $itk_component(etoView) updateGeometry
	}
	"extrude" {
	    $itk_component(extrudeView) updateGeometry
	}
	"grip" {
	    $itk_component(gripView) updateGeometry
	}
	"half" {
	    $itk_component(halfView) updateGeometry
	}
	"part" {
	    $itk_component(partView) updateGeometry
	}
	"pipe" {
	    $itk_component(pipeView) updateGeometry
	}
	"rpc" {
	    $itk_component(rpcView) updateGeometry
	}
	"rhc" {
	    $itk_component(rhcView) updateGeometry
	}
	"sketch" {
	    $itk_component(sketchView) updateGeometry
	}
	"sph" {
	    $itk_component(sphView) updateGeometry
	}
	"tgc" {
	    $itk_component(tgcView) updateGeometry
	}
	"tor" {
	    $itk_component(torView) updateGeometry
	}
    }

    _update_obj_history $mSelectedObj
    if {$doRefreshTree} {
	_refresh_tree
    }

    set mNeedSave 1
    _update_save_mode	

    # Disable the "apply" and "reset" buttons
    $itk_component(objEditToolbar) itemconfigure apply \
	-state disabled
    $itk_component(objEditToolbar) itemconfigure reset \
	-state disabled
}

::itcl::body Archer::_redraw_obj {obj {wflag 1}} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    if {$obj == ""} {
	return
    }

    set renderData [dbCmd how -b $obj]
    set renderMode [lindex $renderData 0]
    set renderTrans [lindex $renderData 1]

    if {$renderMode == -1} {
	return
    }

    _render $obj $renderMode $renderTrans 0 $wflag
}

::itcl::body Archer::_update_prev_obj_button {obj} {
    if {$obj == "" ||
	[catch {dbCmd get $obj} stuff]} {
	$itk_component(objEditToolbar) itemconfigure prev \
	    -state disabled

	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get $hname} stuff]} {
	$itk_component(objEditToolbar) itemconfigure prev \
	    -state disabled

	return
    }

    if {[catch {dbCmd attr get $hname previous} previous]} {
	set previous ""
    }

    if {$previous == "" ||
	[catch {dbCmd get $previous} stuff]} {
	$itk_component(objEditToolbar) itemconfigure prev \
	    -state disabled
    } else {
	$itk_component(objEditToolbar) itemconfigure prev \
	    -state normal
    }
}

::itcl::body Archer::_update_next_obj_button {obj} {
    if {$obj == "" ||
	[catch {dbCmd get $obj} stuff]} {
	$itk_component(objEditToolbar) itemconfigure next \
	    -state disabled

	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get $hname} stuff]} {
	$itk_component(objEditToolbar) itemconfigure next \
	    -state disabled

	return
    }

    if {[catch {dbCmd attr get $hname next} next]} {
	set next ""
    }

    if {$next == "" ||
	[catch {dbCmd get $next} stuff]} {
	$itk_component(objEditToolbar) itemconfigure next \
	    -state disabled
    } else {
	$itk_component(objEditToolbar) itemconfigure next \
	    -state normal
    }
}

::itcl::body Archer::_goto_prev_obj {} {
    set obj $mSelectedObj

    if {$obj == "" ||
	[catch {dbCmd get $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get $hname} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $hname previous} previous]} {
	set previous ""
    }

    if {$previous == "" ||
	[catch {dbCmd get $previous} stuff]} {
	return
    }

    _update_obj_edit $previous 1 1
    _update_prev_obj_button $obj
    _update_next_obj_button $obj

    if {$mSelectedObjType == "comb"} {
	_refresh_tree
    }
}

::itcl::body Archer::_goto_next_obj {} {
    set obj $mSelectedObj

    if {$obj == "" ||
	[catch {dbCmd get $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get $hname} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $hname next} next]} {
	set next ""
    }

    if {$next == "" ||
	[catch {dbCmd get $next} stuff]} {
	return
    }

    _update_obj_edit $next 1 1
    _update_prev_obj_button $obj
    _update_next_obj_button $obj

    if {$mSelectedObjType == "comb"} {
	_refresh_tree
    }
}

::itcl::body Archer::_update_obj_history {obj} {
    if {$obj == "" ||
	[catch {dbCmd get $obj} stuff]} {
	return
    }

    dbCmd make_name -s 1
    set new_hname [dbCmd make_name $obj.version]
    dbCmd cp $obj $new_hname
    dbCmd hide $new_hname

    if {[catch {dbCmd attr get $obj history} old_hname]} {
	set old_hname ""
    }

    if {$old_hname != "" &&
	![catch {dbCmd get $old_hname} stuff]} {
	# Insert into the history list

	if {[catch {dbCmd attr get $old_hname next} next]} {
	    set next ""
	}

	# Delete the future
	if {$next != "" &&
	    ![catch {dbCmd get $next} stuff]} {

	    while {$next != ""} {
		set deadObj $next

		if {[catch {dbCmd attr get $next next} next]} {
		    set next ""
		} elseif {[catch {dbCmd get $next} stuff]} {
		    set next ""
		}

		dbCmd kill $deadObj
	    }
	}
	
	dbCmd attr set $old_hname next $new_hname
	dbCmd attr set $new_hname previous $old_hname
    } else {
	# Initialize the history list
	# Note - we shouldn't get here

	dbCmd attr set $new_hname previous ""
	dbCmd attr set $new_hname history $new_hname
    }

    dbCmd attr set $new_hname next ""
    dbCmd attr set $new_hname history $new_hname
    dbCmd attr set $obj history $new_hname
    _update_prev_obj_button $obj
    _update_next_obj_button $obj
}

::itcl::body Archer::_create_obj {type} {
    dbCmd make_name -s 1

    switch -- $type {
	"arb4" {
	    set name [dbCmd make_name "arb4."]
	    _create_arb4 $name
	}
	"arb5" {
	    set name [dbCmd make_name "arb5."]
	    _create_arb5 $name
	}
	"arb6" {
	    set name [dbCmd make_name "arb6."]
	    _create_arb6 $name
	}
	"arb7" {
	    set name [dbCmd make_name "arb7."]
	    _create_arb7 $name
	}
	"arb8" {
	    set name [dbCmd make_name "arb8."]
	    _create_arb8 $name
	}
	"bot" {
	    #XXX Not ready yet
	    return

	    set name [dbCmd make_name "bot."]
	    _create_bot $name
	}
	"comb" {
	    set name [dbCmd make_name "comb."]
	    _create_comb $name
	}
	"ehy" {
	    set name [dbCmd make_name "ehy."]
	    _create_ehy $name
	}
	"ell" {
	    set name [dbCmd make_name "ell."]
	    _create_ell $name
	}
	"epa" {
	    set name [dbCmd make_name "epa."]
	    _create_epa $name
	}
	"eto" {
	    set name [dbCmd make_name "eto."]
	    _create_eto $name
	}
	"extrude" {
	    #XXX Not ready yet
	    return

	    set name [dbCmd make_name "extrude."]
	    _create_extrude $name
	}
	"grip" {
	    set name [dbCmd make_name "grip."]
	    _create_grip $name
	}
	"half" {
	    set name [dbCmd make_name "half."]
	    _create_half $name
	}
	"part" {
	    set name [dbCmd make_name "part."]
	    _create_part $name
	}
	"pipe" {
	    #XXX Not ready yet
	    return

	    set name [dbCmd make_name "pipe."]
	    _create_pipe $name
	}
	"rhc" {
	    set name [dbCmd make_name "rhc."]
	    _create_rhc $name
	}
	"rpc" {
	    set name [dbCmd make_name "rpc."]
	    _create_rpc $name
	}
	"sketch" {
	    set name [dbCmd make_name "sketch."]
	    _create_sketch $name
	}
	"sph" {
	    set name [dbCmd make_name "sph."]
	    _create_sphere $name
	}
	"tgc" {
	    set name [dbCmd make_name "tgc."]
	    _create_tgc $name
	}
	"tor" {
	    set name [dbCmd make_name "tor."]
	    _create_torus $name
	}
	default {
	    return
	}
    }

    $itk_component(tree) selection clear
    set node [$itk_component(tree) insert end "root" $name "leaf"]
    $itk_component(tree) alternode $node -color blue
    $itk_component(tree) redraw; # force redraw to 
                                  # make sure stuff is there to select
    $itk_component(tree) selection set $node

    dbCmd configure -autoViewEnable 0
    _dbl_click [$itk_component(tree) selection get]
    dbCmd configure -autoViewEnable 1

    set mNeedSave 1
    _update_save_mode
}

::itcl::body Archer::_create_arb4 {name} {
    if {![info exists itk_component(arb4View)]} {
	_build_arb4_edit_view
	$itk_component(arb4View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb4View) createGeometry $name
}

::itcl::body Archer::_create_arb5 {name} {
    if {![info exists itk_component(arb5View)]} {
	_build_arb5_edit_view
	$itk_component(arb5View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb5View) createGeometry $name
}

::itcl::body Archer::_create_arb6 {name} {
    if {![info exists itk_component(arb6View)]} {
	_build_arb6_edit_view
	$itk_component(arb6View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb6View) createGeometry $name
}

::itcl::body Archer::_create_arb7 {name} {
    if {![info exists itk_component(arb7View)]} {
	_build_arb7_edit_view
	$itk_component(arb7View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb7View) createGeometry $name
}

::itcl::body Archer::_create_arb8 {name} {
    if {![info exists itk_component(arb8View)]} {
	_build_arb8_edit_view
	$itk_component(arb8View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb8View) createGeometry $name
}

::itcl::body Archer::_create_bot {name} {
    #XXX Not ready yet
    return

    if {![info exists itk_component(botView)]} {
	_build_bot_edit_view
	$itk_component(botView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(botView) createGeometry $name
}

::itcl::body Archer::_create_comb {name} {
    if {![info exists itk_component(combView)]} {
	_build_comb_edit_view
	$itk_component(combView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(combView) createGeometry $name
}

::itcl::body Archer::_create_ehy {name} {
    if {![info exists itk_component(ehyView)]} {
	_build_ehy_edit_view
	$itk_component(ehyView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(ehyView) createGeometry $name
}

::itcl::body Archer::_create_ell {name} {
    if {![info exists itk_component(ellView)]} {
	_build_ell_edit_view
	$itk_component(ellView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(ellView) createGeometry $name
}

::itcl::body Archer::_create_epa {name} {
    if {![info exists itk_component(epaView)]} {
	_build_epa_edit_view
	$itk_component(epaView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(epaView) createGeometry $name
}

::itcl::body Archer::_create_eto {name} {
    if {![info exists itk_component(etoView)]} {
	_build_eto_edit_view
	$itk_component(etoView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(etoView) createGeometry $name
}

::itcl::body Archer::_create_extrude {name} {
    #XXX Not ready yet
    return

    if {![info exists itk_component(extrudeView)]} {
	_build_extrude_edit_view
	$itk_component(extrudeView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(extrudeView) createGeometry $name
}

::itcl::body Archer::_create_grip {name} {
    if {![info exists itk_component(gripView)]} {
	_build_grip_edit_view
	$itk_component(gripView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(gripView) createGeometry $name
}

::itcl::body Archer::_create_half {name} {
    if {![info exists itk_component(halfView)]} {
	_build_half_edit_view
	$itk_component(halfView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(halfView) createGeometry $name
}

::itcl::body Archer::_create_part {name} {
    if {![info exists itk_component(partView)]} {
	_build_part_edit_view
	$itk_component(partView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(partView) createGeometry $name
}

::itcl::body Archer::_create_pipe {name} {
    #XXX Not ready yet
    return

    if {![info exists itk_component(pipeView)]} {
	_build_pipe_edit_view
	$itk_component(pipeView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(pipeView) createGeometry $name
}

::itcl::body Archer::_create_rhc {name} {
    if {![info exists itk_component(rhcView)]} {
	_build_rhc_edit_view
	$itk_component(rhcView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(rhcView) createGeometry $name
}

::itcl::body Archer::_create_rpc {name} {
    if {![info exists itk_component(rpcView)]} {
	_build_rpc_edit_view
	$itk_component(rpcView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(rpcView) createGeometry $name
}

::itcl::body Archer::_create_sketch {name} {
    if {![info exists itk_component(sketchView)]} {
	_build_sketch_edit_view
	$itk_component(sketchView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(sketchView) createGeometry $name
}

::itcl::body Archer::_create_sphere {name} {
    if {![info exists itk_component(sphView)]} {
	_build_sphere_edit_view
	$itk_component(sphView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(sphView) createGeometry $name
}

::itcl::body Archer::_create_tgc {name} {
    if {![info exists itk_component(tgcView)]} {
	_build_tgc_edit_view
	$itk_component(tgcView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(tgcView) createGeometry $name
}

::itcl::body Archer::_create_torus {name} {
    if {![info exists itk_component(torView)]} {
	_build_torus_edit_view
	$itk_component(torView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(torView) createGeometry $name
}

::itcl::body Archer::_build_arb4_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add arb4View {
	Arb4EditFrame $parent.arb4view \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_arb5_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add arb5View {
	Arb5EditFrame $parent.arb5view \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_arb6_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add arb6View {
	Arb6EditFrame $parent.arb6view \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_arb7_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add arb7View {
	Arb7EditFrame $parent.arb7view \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_arb8_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add arb8View {
	Arb8EditFrame $parent.arb8view \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_bot_edit_view {} {
    #XXX Not ready yet
    return

    set parent $itk_component(objEditView)
    itk_component add botView {
	BotEditFrame $parent.botview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_comb_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add combView {
	CombEditFrame $parent.combview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_ehy_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add ehyView {
	EhyEditFrame $parent.ehyview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_ell_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add ellView {
	EllEditFrame $parent.ellview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_epa_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add epaView {
	EpaEditFrame $parent.epaview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_eto_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add etoView {
	EtoEditFrame $parent.etoview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_extrude_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add extrudeView {
	ExtrudeEditFrame $parent.extrudeview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_grip_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add gripView {
	GripEditFrame $parent.gripview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_half_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add halfView {
	HalfEditFrame $parent.halfview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_part_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add partView {
	PartEditFrame $parent.partview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_pipe_edit_view {} {
    #XXX Not ready yet
    return

    set parent $itk_component(objEditView)
    itk_component add pipeView {
	PipeEditFrame $parent.pipeview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_rhc_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add rhcView {
	RhcEditFrame $parent.rhcview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_rpc_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add rpcView {
	RpcEditFrame $parent.rpcview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_sketch_edit_view {} {
    #XXX Not ready yet
    return

    set parent $itk_component(objEditView)
    itk_component add sketchView {
	SketchEditFrame $parent.sketchview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_sphere_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add sphView {
	SphereEditFrame $parent.sphview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_tgc_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add tgcView {
	TgcEditFrame $parent.tgcview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_build_torus_edit_view {} {
    set parent $itk_component(objEditView)
    itk_component add torView {
	TorusEditFrame $parent.torview \
	    -units "mm"
    } {}

#	    -vscrollmode dynamic
#	    -hscrollmode none
}

::itcl::body Archer::_init_arb4_edit_view {odata} {
    $itk_component(arb4View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb4View) initGeometry $odata

    pack $itk_component(arb4View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_arb5_edit_view {odata} {
    $itk_component(arb5View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb5View) initGeometry $odata

    pack $itk_component(arb5View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_arb6_edit_view {odata} {
    $itk_component(arb6View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb6View) initGeometry $odata

    pack $itk_component(arb6View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_arb7_edit_view {odata} {
    $itk_component(arb7View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb7View) initGeometry $odata

    pack $itk_component(arb7View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_arb8_edit_view {odata} {
    $itk_component(arb8View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb8View) initGeometry $odata

    pack $itk_component(arb8View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_bot_edit_view {odata} {
    #XXX Not ready yet
    return

    $itk_component(botView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(botView) initGeometry $odata

    pack $itk_component(botView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_comb_edit_view {odata} {
    $itk_component(combView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(combView) initGeometry $odata

    pack $itk_component(combView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_ehy_edit_view {odata} {
    $itk_component(ehyView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(ehyView) initGeometry $odata

    pack $itk_component(ehyView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_ell_edit_view {odata} {
    $itk_component(ellView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(ellView) initGeometry $odata

    pack $itk_component(ellView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_epa_edit_view {odata} {
    $itk_component(epaView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(epaView) initGeometry $odata

    pack $itk_component(epaView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_eto_edit_view {odata} {
    $itk_component(etoView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(etoView) initGeometry $odata

    pack $itk_component(etoView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_extrude_edit_view {odata} {
    $itk_component(extrudeView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(extrudeView) initGeometry $odata

    pack $itk_component(extrudeView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_grip_edit_view {odata} {
    $itk_component(gripView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(gripView) initGeometry $odata

    pack $itk_component(gripView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_half_edit_view {odata} {
    $itk_component(halfView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(halfView) initGeometry $odata

    pack $itk_component(halfView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_part_edit_view {odata} {
    $itk_component(partView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(partView) initGeometry $odata

    pack $itk_component(partView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_pipe_edit_view {odata} {
    #XXX Not ready yet
    return

    $itk_component(pipeView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(pipeView) initGeometry $odata

    pack $itk_component(pipeView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_rhc_edit_view {odata} {
    $itk_component(rhcView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(rhcView) initGeometry $odata

    pack $itk_component(rhcView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_rpc_edit_view {odata} {
    $itk_component(rpcView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(rpcView) initGeometry $odata

    pack $itk_component(rpcView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_sketch_edit_view {odata} {
    #XXX Not ready yet
    return

    $itk_component(sketchView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(sketchView) initGeometry $odata

    pack $itk_component(sketchView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_sphere_edit_view {odata} {
    $itk_component(sphView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(sphView) initGeometry $odata

    pack $itk_component(sphView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_tgc_edit_view {odata} {
    $itk_component(tgcView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(tgcView) initGeometry $odata

    pack $itk_component(tgcView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_init_torus_edit_view {odata} {
    $itk_component(torView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this _update_obj_edit_view] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(torView) initGeometry $odata

    pack $itk_component(torView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::_build_toplevel_menubar {} {
    itk_component add menubar {
	menu $itk_interior.menubar \
	    -tearoff 0
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }

    # File
    itk_component add filemenu {
	::menu $itk_component(menubar).filemenu \
	    -tearoff 1
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }
    # Import
    itk_component add importMenu {
	::menu $itk_component(filemenu).importMenu \
	    -tearoff 1
    } {
	keep -background
    }
    $itk_component(importMenu) add command \
	-label "Fastgen 4 Import..." \
	-command [::itcl::code $this importFg4]
    $itk_component(importMenu) add command \
	-label "STL Import Directory..." \
	-command [::itcl::code $this importStl 1]
    $itk_component(importMenu) add command \
	-label "STL Import File..." \
	-command [::itcl::code $this importStl 0]

    # Export
    itk_component add exportMenu {
	::menu $itk_component(filemenu).exportMenu \
	    -tearoff 1
    } {
	keep -background
    }
    $itk_component(exportMenu) add command \
	-label "Fastgen 4 Export..." \
	-command [::itcl::code $this exportFg4]
    $itk_component(exportMenu) add command \
	-label "STL Export..." \
	-command [::itcl::code $this exportStl]
    $itk_component(exportMenu) add command \
	-label "VRML Export..." \
	-command [::itcl::code $this exportVrml]

    # Populate File menu
    $itk_component(filemenu) add command \
	-label "New..." \
	-command [::itcl::code $this _new_db]
    $itk_component(filemenu) add command \
	-label "Open..." \
	-command [::itcl::code $this _open_db]
    $itk_component(filemenu) add command \
	-label "Save" \
	-command [::itcl::code $this _save_db]
#    $itk_component(filemenu) add command \
	-label "Compact" \
	-command [::itcl::code $this compact] \
	-state disabled
    $itk_component(filemenu) add cascade \
	-label "Import" \
	-menu $itk_component(importMenu) \
	-state disabled
    $itk_component(filemenu) add cascade \
	-label "Export" \
	-menu $itk_component(exportMenu) \
	-state disabled
    $itk_component(filemenu) add separator
#    $itk_component(filemenu) add command \
	-label "Raytrace Control Panel..." \
	-command [::itcl::code $this _ray_trace_panel] \
	-state disabled
#    $itk_component(filemenu) add command \
	-label "Export Geometry to PNG..." \
	-command [::itcl::code $this _do_png] \
	-state disabled
#    $itk_component(filemenu) add separator
    $itk_component(filemenu) add command \
	-label "Preferences..." \
	-command [::itcl::code $this _do_preferences]
    $itk_component(filemenu) add separator
    $itk_component(filemenu) add command \
	-label "Quit" \
	-command [::itcl::code $this Close]

    itk_component add displaymenu {
	menu $itk_component(menubar).displaymenu \
	    -tearoff 1
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }
    $itk_component(displaymenu) add command \
	-label "Reset" \
	-command [::itcl::code $this _do_view_reset] \
	-state disabled
    $itk_component(displaymenu) add command \
	-label "Autoview" \
	-command [::itcl::code $this _do_autoview] \
	-state disabled
    $itk_component(displaymenu) add command \
	-label "Center..." \
	-command [::itcl::code $this _do_view_center] \
	-state disabled
    itk_component add stdviewsmenu {
	menu $itk_component(displaymenu).stdviewsmenu \
	    -tearoff 1
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }
    $itk_component(stdviewsmenu) add command \
	-label "Front" \
	-command [::itcl::code $this _do_ae 0 0]
    $itk_component(stdviewsmenu) add command \
	-label "Rear" \
	-command [::itcl::code $this _do_ae 180 0]
    $itk_component(stdviewsmenu) add command \
	-label "Port" \
	-command [::itcl::code $this _do_ae 90 0]
    $itk_component(stdviewsmenu) add command \
	-label "Starboard" \
	-command [::itcl::code $this _do_ae -90 0]
    $itk_component(stdviewsmenu) add command \
	-label "Top" \
	-command [::itcl::code $this _do_ae 0 90]
    $itk_component(stdviewsmenu) add command \
	-label "Bottom" \
	-command [::itcl::code $this _do_ae 0 -90]
    $itk_component(stdviewsmenu) add separator
    $itk_component(stdviewsmenu) add command \
	-label "35,25" \
	-command [::itcl::code $this _do_ae 35 25]
    $itk_component(stdviewsmenu) add command \
	-label "45,45" \
	-command [::itcl::code $this _do_ae 45 45]
    $itk_component(displaymenu) add cascade \
	-label "Standard Views" \
	-menu $itk_component(stdviewsmenu) \
	-state disabled
    $itk_component(displaymenu) add command \
	-label "Clear" \
	-command [::itcl::code $this clear] \
	-state disabled
    $itk_component(displaymenu) add command \
	-label "Refresh" \
	-command [::itcl::code $this refreshDisplay] \
	-state disabled
    $itk_component(displaymenu) add separator
    itk_component add toolbarsmenu {
	menu $itk_component(displaymenu).toolbarsmenu \
	    -tearoff 1
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }
#    $itk_component(toolbarsmenu) add checkbutton \
	-label "Primary" \
	-offvalue 0 \
	-onvalue 1 \
	-variable  [::itcl::scope itk_option(-primaryToolbar)] \
	-command [::itcl::code $this _do_primary_toolbar]
    $itk_component(toolbarsmenu) add checkbutton \
	-label "View Controls" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope itk_option(-viewToolbar)] \
	-command [::itcl::code $this _do_view_toolbar]
    $itk_component(displaymenu) add cascade \
	-label "Toolbars" \
	-menu $itk_component(toolbarsmenu)
    $itk_component(displaymenu) add checkbutton \
	-label "Status Bar" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope itk_option(-statusbar)] \
	-command [::itcl::code $this _do_statusbar]

    itk_component add modesmenu {
	menu $itk_component(menubar).modesmenu \
	    -tearoff 1
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }

    itk_component add activepanemenu {
	menu $itk_component(modesmenu).activepanemenu \
	    -tearoff 1
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }
    set i 0
    $itk_component(activepanemenu) add radiobutton \
	-label "Upper Left" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this _set_active_pane ul]
    incr i
    $itk_component(activepanemenu) add radiobutton \
	-label "Upper Right" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this _set_active_pane ur]
    set mActivePane $i
    incr i
    $itk_component(activepanemenu) add radiobutton \
	-label "Lower Left" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this _set_active_pane ll]
    incr i
    $itk_component(activepanemenu) add radiobutton \
	-label "Lower Right" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this _set_active_pane lr]

    _update_modes_menu
    _update_utility_menu

    itk_component add helpmenu {
	menu $itk_component(menubar).helpmenu \
	    -tearoff 1
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }
    $itk_component(helpmenu) add command \
	-label "Archer Help..." \
	-command [::itcl::code $this doArcherHelp]
    $itk_component(helpmenu) add separator
    $itk_component(helpmenu) add command \
	-label "About Plug-ins..." \
	-command "::Archer::pluginDialog [namespace tail $this]"
    $itk_component(helpmenu) add command \
	-label "About Archer..." \
	-command [::itcl::code $this _do_about_archer]
#    $itk_component(helpmenu) add command \
	-label "Mouse Mode Overrides..." \
	-command [::itcl::code $this _do_mouse_overrides]

    # Hook in the first tier of menus
    $itk_component(menubar) add cascade \
	-label "File" \
	-menu $itk_component(filemenu)

    $itk_component(menubar) add cascade \
	-label "Display" \
	-menu $itk_component(displaymenu)

    $itk_component(menubar) add cascade \
	-label "Modes" \
	-menu $itk_component(modesmenu) \

    $itk_component(menubar) add cascade \
	-label "Help" \
	-menu $itk_component(helpmenu)

    # set up bindings for status
    bind $itk_component(filemenu) <<MenuSelect>> [::itcl::code $this _menu_statusCB %W]
    bind $itk_component(importMenu) <<MenuSelect>> [::itcl::code $this _menu_statusCB %W]
    bind $itk_component(exportMenu) <<MenuSelect>> [::itcl::code $this _menu_statusCB %W]
    bind $itk_component(displaymenu) <<MenuSelect>> [::itcl::code $this _menu_statusCB %W]
    bind $itk_component(stdviewsmenu) <<MenuSelect>> [::itcl::code $this _menu_statusCB %W]
    bind $itk_component(toolbarsmenu) <<MenuSelect>> [::itcl::code $this _menu_statusCB %W]
    bind $itk_component(modesmenu) <<MenuSelect>> [::itcl::code $this _menu_statusCB %W]
    bind $itk_component(activepanemenu) <<MenuSelect>> [::itcl::code $this _menu_statusCB %W]
    bind $itk_component(helpmenu) <<MenuSelect>> [::itcl::code $this _menu_statusCB %W]
}

::itcl::body Archer::_update_modes_menu {} {
    if {$Archer::inheritFromToplevel} {
	catch {
	    $itk_component(modesmenu) delete 0 end
	}

	set i 0
	$itk_component(modesmenu) add radiobutton \
	    -label "Basic" \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this _set_mode 1]
	incr i
	$itk_component(modesmenu) add radiobutton \
	    -label "Intermediate" \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this _set_mode 1]
	incr i
	$itk_component(modesmenu) add radiobutton \
	    -label "Advanced" \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this _set_mode 1]

	if {([info exists itk_component(mged)] ||
	     [info exists itk_component(sdb)]) && $mMode != 0} {
	    $itk_component(modesmenu) add separator
	    $itk_component(modesmenu) add cascade \
		-label "Active Pane" \
		-menu $itk_component(activepanemenu)
	    $itk_component(modesmenu) add checkbutton \
		-label "Quad View" \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mMultiPane] \
		-command [::itcl::code $this _do_multi_pane]
	}

	$itk_component(modesmenu) add separator

	$itk_component(modesmenu) add checkbutton \
	    -label "View Axes" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowViewAxes] \
	    -command [::itcl::code $this _show_view_axes]
	$itk_component(modesmenu) add checkbutton \
	    -label "Model Axes" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowModelAxes] \
	    -command [::itcl::code $this _show_model_axes]
	$itk_component(modesmenu) add checkbutton \
	    -label "Ground Plane" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowGroundPlane] \
	    -command [::itcl::code $this _show_ground_plane]
	$itk_component(modesmenu) add checkbutton \
	    -label "Primitive Labels" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowPrimitiveLabels] \
	    -command [::itcl::code $this _show_primitive_labels]
	$itk_component(modesmenu) add checkbutton \
	    -label "Viewing Parameters" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowViewingParams] \
	    -command [::itcl::code $this _show_view_params]
	$itk_component(modesmenu) add checkbutton \
	    -label "Scale" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowScale] \
	    -command [::itcl::code $this _show_scale]
	$itk_component(modesmenu) add checkbutton \
	    -label "Two Sided Lighting" \
	    -offvalue 1 \
	    -onvalue 2 \
	    -variable [::itcl::scope mLighting] \
	    -command [::itcl::code $this _do_lighting]

	if {![info exists itk_component(mged)] &&
	    ![info exists itk_component(sdb)]} {
	    # Disable a few entries until we read a database
	    #$itk_component(modesmenu) entryconfigure "Active Pane" -state disabled
	    $itk_component(modesmenu) entryconfigure "View Axes" -state disabled
	    $itk_component(modesmenu) entryconfigure "Model Axes" -state disabled
	}
    } else {
	if {([info exists itk_component(mged)] ||
	     [info exists itk_component(sdb)]) && $mMode != 0} {
	    $itk_component(menubar) menuconfigure .modes \
		-text "Modes" \
		-menu {
		    options -tearoff 1
		    radiobutton basic \
			-label "Basic" \
			-helpstr "Basic user mode"
		    radiobutton intermediate \
			-label "Intermediate" \
			-helpstr "Intermediate user mode"
		    radiobutton advanced \
			-label "Advanced" \
			-helpstr "Advanced user mode"

		    separator sep1

		    cascade activePane -label "Active Pane" -menu {
			radiobutton ul -label "Upper Left" \
			    -helpstr "Set the active pane to the upper left pane"
			radiobutton ur -label "Upper Right" \
			    -helpstr "Set the active pane to the upper right pane"
			radiobutton ll -label "Lower Left" \
			    -helpstr "Set the active pane to the lower left pane"
			radiobutton lr -label "Lower Right" \
			    -helpstr "Set the active pane to the lower right pane"
		    
		    }
		    checkbutton quadview -label "Quad View" \
			-helpstr "Hide/Show multiple (4) views"

		    separator sep2

		    checkbutton viewaxes -label "View Axes" \
			-helpstr "Hide/Show View Axes"
		    checkbutton modelaxes -label "Model Axes" \
			-helpstr "Hide/Show model Axes"
		    checkbutton groundplane -label "Ground Plane" \
			-helpstr "Hide/Show ground plane"
		    checkbutton primitiveLabels -label "Primitive Labels" \
			-helpstr "Hide/Show primitive labels"
		    checkbutton viewingParams -label "Viewing Parameters" \
			-helpstr "Hide/Show viewing parameters"
		    checkbutton scale -label "Scale" \
			-helpstr "Hide/Show scale"
		    checkbutton lighting -label "Two Sided Lighting" \
			-helpstr "Light front and back of polygons"
		}
	} else {
	    $itk_component(menubar) menuconfigure .modes \
		-text "Modes" \
		-menu {
		    options -tearoff 1
		    radiobutton basic \
			-label "Basic" \
			-helpstr "Basic user mode"
		    radiobutton intermediate \
			-label "Intermediate" \
			-helpstr "Intermediate user mode"
		    radiobutton advanced \
			-label "Advanced" \
			-helpstr "Advanced user mode"

		    separator sep2

		    checkbutton viewaxes -label "View Axes" \
			-helpstr "Hide/Show View Axes"
		    checkbutton modelaxes -label "Model Axes" \
			-helpstr "Hide/Show model Axes"
		    checkbutton groundplane -label "Ground Plane" \
			-helpstr "Hide/Show ground plane"
		    checkbutton primitiveLabels -label "Primitive Labels" \
			-helpstr "Hide/Show primitive labels"
		    checkbutton viewingParams -label "Viewing Parameters" \
			-helpstr "Hide/Show viewing parameters"
		    checkbutton scale -label "Scale" \
			-helpstr "Hide/Show scale"
		    checkbutton lighting -label "Two Sided Lighting" \
			-helpstr "Light front and back of polygons"
		}
	}

	set i 0
	$itk_component(menubar) menuconfigure .modes.basic \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this _set_mode 1]
	incr i
	$itk_component(menubar) menuconfigure .modes.intermediate \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this _set_mode 1]
	incr i
	$itk_component(menubar) menuconfigure .modes.advanced \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this _set_mode 1]

	if {([info exists itk_component(mged)] ||
	     [info exists itk_component(sdb)]) && $mMode != 0} {
	    set i 0
	    $itk_component(menubar) menuconfigure .modes.activePane.ul \
		-value $i \
		-variable [::itcl::scope mActivePane] \
		-command [::itcl::code $this _set_active_pane ul]
	    incr i
	    $itk_component(menubar) menuconfigure .modes.activePane.ur \
		-value $i \
		-variable [::itcl::scope mActivePane] \
		-command [::itcl::code $this _set_active_pane ur]
	    set mActivePane $i
	    incr i
	    $itk_component(menubar) menuconfigure .modes.activePane.ll \
		-value $i \
		-variable [::itcl::scope mActivePane] \
		-command [::itcl::code $this _set_active_pane ll]
	    incr i
	    $itk_component(menubar) menuconfigure .modes.activePane.lr \
		-value $i \
		-variable [::itcl::scope mActivePane] \
		-command [::itcl::code $this _set_active_pane lr]
	    $itk_component(menubar) menuconfigure .modes.quadview \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mMultiPane] \
		-command [::itcl::code $this _do_multi_pane]
	}

	if {![info exists itk_component(mged)] &&
	    ![info exists itk_component(sdb)]} {
	    $itk_component(menubar) menuconfigure .modes.viewaxes \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mShowViewAxes] \
		-command [::itcl::code $this _show_view_axes] \
		-state disabled
	    $itk_component(menubar) menuconfigure .modes.modelaxes \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mShowModelAxes] \
		-command [::itcl::code $this _show_model_axes] \
		-state disabled
	} else {
	    $itk_component(menubar) menuconfigure .modes.viewaxes \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mShowViewAxes] \
		-command [::itcl::code $this _show_view_axes]
	    $itk_component(menubar) menuconfigure .modes.modelaxes \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mShowModelAxes] \
		-command [::itcl::code $this _show_model_axes]
	}
	$itk_component(menubar) menuconfigure .modes.groundplane \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowGroundPlane] \
	    -command [::itcl::code $this _show_ground_plane]
	$itk_component(menubar) menuconfigure .modes.primitiveLabels \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowPrimitiveLabels] \
	    -command [::itcl::code $this _show_primitive_labels]
	$itk_component(menubar) menuconfigure .modes.viewingParams \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowViewingParams] \
	    -command [::itcl::code $this _show_view_params]
	$itk_component(menubar) menuconfigure .modes.scale \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowScale] \
	    -command [::itcl::code $this _show_scale]
	$itk_component(menubar) menuconfigure .modes.lighting \
	    -offvalue 1 \
	    -onvalue 2 \
	    -variable [::itcl::scope mLighting] \
	    -command [::itcl::code $this _do_lighting]
    }
}

::itcl::body Archer::_build_embedded_menubar {} {
    # menubar
    itk_component add menubar {
	::iwidgets::menubar $itk_interior.menubar \
	    -helpvariable [::itcl::scope mStatusStr] \
	    -font $mFontText \
	    -activebackground $SystemHighlight \
	    -activeforeground $SystemHighlightText
    } {
#	rename -font -menuFont menuFont MenuFont
#	keep -font
	keep -background
    }

    $itk_component(menubar) component menubar configure \
	-relief flat

    # File
    $itk_component(menubar) add menubutton file \
	    -text "File" -menu {
	options -tearoff 1

	command new \
	    -label "New..." \
	    -helpstr "Open target description"
	command open \
	    -label "Open..." \
	    -helpstr "Open target description"
	command save \
	    -label "Save" \
	    -helpstr "Save target description"
#	command compact \
	    -label "Compact" \
	    -helpstr "Compact the target description"
	cascade import \
	    -label "Import" \
	    -menu {
		command importFg4 \
		    -label "Fastgen 4 Import..." \
		    -helpstr "Import from Fastgen 4"
		command importStlDir \
		    -label "STL Import Directory..." \
		    -helpstr "Import from STL"
		command importStlFile \
		    -label "STL Import File..." \
		    -helpstr "Import from STL"
	    }
	cascade export \
	    -label "Export" \
	    -menu {
		command exportFg4 \
		    -label "Fastgen 4 Export..." \
		    -helpstr "Export to Fastgen 4"
		command exportStl \
		    -label "STL Export..." \
		    -helpstr "Export to STL"
		command exportVrml \
		    -label "VRML Export..." \
		    -helpstr "Export to VRML"
	    }
	separator sep0
#	command rt -label "Raytrace Control Panel..." \
		 -helpstr "Launch Ray Trace Panel"
#	command png -label "Export Geometry to PNG..." \
		 -helpstr "Launch PNG Export Panel"
#	separator sep1
	command pref \
	    -label "Preferences..." \
	    -command [::itcl::code $this _do_preferences] \
	    -helpstr "Set application preferences"
	separator sep2
	command exit \
	    -label "Quit" \
	    -helpstr "Exit Archer"
    }
    $itk_component(menubar) menuconfigure .file.new \
	-command [::itcl::code $this _new_db]
    $itk_component(menubar) menuconfigure .file.open \
	-command [::itcl::code $this _open_db]
    $itk_component(menubar) menuconfigure .file.save \
	-command [::itcl::code $this _save_db]
#    $itk_component(menubar) menuconfigure .file.compact \
	-command [::itcl::code $this compact] \
	-state disabled
    $itk_component(menubar) menuconfigure .file.import \
	-state disabled
    $itk_component(menubar) menuconfigure .file.import.importFg4 \
	-command [::itcl::code $this importFg4]
    $itk_component(menubar) menuconfigure .file.import.importStlDir \
	-command [::itcl::code $this importStl 1]
    $itk_component(menubar) menuconfigure .file.import.importStlFile \
	-command [::itcl::code $this importStl 0]
    $itk_component(menubar) menuconfigure .file.export \
	-state disabled
    $itk_component(menubar) menuconfigure .file.export.exportFg4 \
	-command [::itcl::code $this exportFg4]
    $itk_component(menubar) menuconfigure .file.export.exportStl \
	-command [::itcl::code $this exportStl]
    $itk_component(menubar) menuconfigure .file.export.exportVrml \
	-command [::itcl::code $this exportVrml]
#    $itk_component(menubar) menuconfigure .file.rt \
	-command [::itcl::code $this _ray_trace_panel] \
	-state disabled
#    $itk_component(menubar) menuconfigure .file.png \
	-command [::itcl::code $this _do_png] \
	-state disabled
    $itk_component(menubar) menuconfigure .file.pref \
	-command [::itcl::code $this _do_preferences]
    $itk_component(menubar) menuconfigure .file.exit \
	-command [::itcl::code $this Close]

    # View Menu
    $itk_component(menubar) add menubutton display \
	    -text "Display" -menu {
	options -tearoff 1

	command reset -label "Reset" \
	    -helpstr "Set view to default"
	command autoview -label "Autoview" \
	    -helpstr "Set view size and center according to what's being displayed"
	command center -label "Center..." \
	    -helpstr "Set the view center"

	cascade standard -label "Standard Views" -menu {
	    command front -label "Front" \
		    -helpstr "Set view to front"
	    command rear -label "Rear" \
		    -helpstr "Set view to rear"
	    command port -label "Port" \
		    -helpstr "Set view to port/left"
	    command starboard -label "Starboard" \
		    -helpstr "Set view to starboard/right"
	    command top -label "Top" \
		    -helpstr "Set view to top"
	    command bottom -label "Bottom" \
		    -helpstr "Set view to bottom"
	    separator sep0
	    command 35,25 -label "35,25" \
		    -helpstr "Set view to az=35, el=25"
	    command 45,45 -label "45,45" \
		    -helpstr "Set view to az=45, el=45"
	}

	command clear -label "Clear" \
	    -helpstr "Clear the display"
	command refresh -label "Refresh" \
	    -helpstr "Refresh the display"
	separator sep1
	cascade toolbars -label "Toolbars" -menu {
#	    checkbutton primary -label "Primary" \
		    -helpstr "Toggle on/off primary toolbar"
	    checkbutton views -label "View Controls" \
		    -helpstr "Toggle on/off view toolbar"
	}

	checkbutton statusbar -label "Status Bar" \
		-helpstr "Toggle on/off status bar"
    }
    $itk_component(menubar) menuconfigure .display.standard \
	-state disabled
    $itk_component(menubar) menuconfigure .display.reset \
	-command [::itcl::code $this _do_view_reset] \
	-state disabled
    $itk_component(menubar) menuconfigure .display.autoview \
	-command [::itcl::code $this _do_autoview] \
	-state disabled
    $itk_component(menubar) menuconfigure .display.center \
	-command [::itcl::code $this _do_view_center] \
	-state disabled
    $itk_component(menubar) menuconfigure .display.standard.front \
	-command [::itcl::code $this _do_ae 0 0]
    $itk_component(menubar) menuconfigure .display.standard.rear \
	-command [::itcl::code $this _do_ae 180 0]
    $itk_component(menubar) menuconfigure .display.standard.port \
	-command [::itcl::code $this _do_ae 90 0]
    $itk_component(menubar) menuconfigure .display.standard.starboard \
	-command [::itcl::code $this _do_ae -90 0]
    $itk_component(menubar) menuconfigure .display.standard.top \
	-command [::itcl::code $this _do_ae 0 90]
    $itk_component(menubar) menuconfigure .display.standard.bottom \
	-command [::itcl::code $this _do_ae 0 -90]
    $itk_component(menubar) menuconfigure .display.standard.35,25 \
	-command [::itcl::code $this _do_ae 35 25]
    $itk_component(menubar) menuconfigure .display.standard.45,45 \
	-command [::itcl::code $this _do_ae 45 45]
    $itk_component(menubar) menuconfigure .display.clear \
	-command [::itcl::code $this clear] \
	-state disabled
    $itk_component(menubar) menuconfigure .display.refresh \
	-command [::itcl::code $this refreshDisplay] \
	-state disabled
#    $itk_component(menubar) menuconfigure .display.toolbars.primary -offvalue 0 -onvalue 1 \
	-variable [::itcl::scope itk_option(-primaryToolbar)] \
	-command [::itcl::code $this _do_primary_toolbar]
    $itk_component(menubar) menuconfigure .display.toolbars.views -offvalue 0 -onvalue 1 \
	-variable [::itcl::scope itk_option(-viewToolbar)] \
	-command [::itcl::code $this _do_view_toolbar]
    $itk_component(menubar) menuconfigure .display.statusbar -offvalue 0 -onvalue 1 \
	-variable [::itcl::scope itk_option(-statusbar)] \
	-command [::itcl::code $this _do_statusbar]

    # Modes Menu
    $itk_component(menubar) add menubutton modes \
	-text "Modes" \

    _update_modes_menu
    _update_utility_menu

    # Help Menu
    $itk_component(menubar) add menubutton help \
	-text "Help" \
	-menu {
	    options -tearoff 1
	    command archerHelp -label "Archer Help..." \
		-helpstr "Archer's User Manual"
	    separator sep0
	    command aboutPlugins -label "About Plug-ins..." \
		-helpstr "Info about Plug-ins"
	    command aboutArcher -label "About Archer..." \
		-helpstr "Info about Archer"

#	    command overrides -label "Mouse Mode Overrides..." \
		-helpstr "Mouse mode override definitions"
	}
    $itk_component(menubar) menuconfigure .help.archerHelp \
	-command [::itcl::code $this doArcherHelp]
    $itk_component(menubar) menuconfigure .help.aboutPlugins \
	-command "::Archer::pluginDialog [namespace tail $this]"
    $itk_component(menubar) menuconfigure .help.aboutArcher \
	-command [::itcl::code $this _do_about_archer]
#    $itk_component(menubar) menuconfigure .help.overrides \
	-command [::itcl::code $this _do_mouse_overrides]
}

# ------------------------------------------------------------
#                       DESTRUCTOR
# ------------------------------------------------------------
::itcl::body Archer::destructor {} {
    _write_preferences

    # We do this to remove librt's reference
    # to $mTargetCopy now. Once this reference is
    # gone, we can successfully delete the temporary
    # file here in the destructor.
    catch {::itcl::delete object $itk_component(mged)}
    catch {::itcl::delete object $itk_component(sdb)}

    set mTargetOldCopy $mTargetCopy
    _delete_target_old_copy
}

::itcl::body Archer::_init_tree {} {
    set parent [$itk_component(vpane) childsite hierarchyView]
    itk_component add tree_frm {
	::frame $parent.tree_frm \
		-bd 1 \
		-relief sunken
    } {
    }

    itk_component add tree_title {
	::label $itk_component(tree_frm).tree_title \
		-text "Hierarchy" -font $mFontText
    } {
    }

    if {!$mViewOnly} {
	itk_component add tree_collapse {
	    ::button $itk_component(tree_frm).tree_collapse
	} {
	}
	$itk_component(tree_collapse) configure -relief flat \
	    -image [image create photo -file \
			[file join $_imgdir Themes $mTheme "pane_collapse.png"]] \
	    -command [::itcl::code $this _toggle_tree_view "close"]
    }

    itk_component add tree {
	::swidgets::tree $itk_component(tree_frm).tree \
		-background white \
		-selectfill 1 \
		-selectbackground black \
		-selectforeground $SystemWindow \
		-querycmd [::itcl::code $this _fill_tree %n] \
		-selectcmd [::itcl::code $this _select_node %n] \
		-dblselectcmd [::itcl::code $this _dbl_click %n] \
		-textmenuloadcommand [::itcl::code $this _load_menu]
    } {
    }

    [$itk_component(tree) component popupmenu] configure \
	-background $SystemButtonFace \
	-activebackground $SystemHighlight \
	-activeforeground $SystemHighlightText

    grid $itk_component(tree_title) -row 0 -column 0 -sticky w
    if {!$mViewOnly} {
	grid $itk_component(tree_collapse) -row 0 -column 1 -sticky e -pady 1
    }
    grid $itk_component(tree) -row 1 -column 0 -columnspan 2 -sticky news
    grid rowconfigure $itk_component(tree_frm) 1 -weight 1
    grid columnconfigure $itk_component(tree_frm) 0 -weight 1
}

::itcl::body Archer::_init_mged {} {
    itk_component add mged {
	if {$mDbNoCopy || $mDbReadOnly} {
	    set _target $mTarget
	} else {
	    set _target $mTargetCopy
	}

	Mged $itk_component(canvasF).mged $_target \
	    -type $mDisplayType \
	    -showhandle 0 \
	    -sashcursor sb_v_double_arrow \
	    -hsashcursor sb_h_double_arrow \
	    -showViewingParams $mShowViewingParams
    } {
	keep -sashwidth -sashheight -sashborderwidth
	keep -sashindent -thickness
    }
    set tmp_dbCommands [$itk_component(mged) getUserCmds]
    set unwrappedDbCommands {}
    foreach cmd $tmp_dbCommands {
	if {[lsearch $bannedDbCommands $cmd] == -1} {
	    lappend unwrappedDbCommands $cmd
	}
    }

    set dbSpecificCommands $mgedCommands


    if {!$mViewOnly} {
	$itk_component(mged) set_outputHandler "$itk_component(cmd) putstring"
    }
    $itk_component(mged) transparencyAll 1
    $itk_component(mged) bounds "-4096 4095 -4096 4095 -4096 4095"
    
    # RT Control Panel
    itk_component add rtcntrl {
	RtControl $itk_interior.rtcp -mged $itk_component(mged)
    } {
	usual
    }
#    $itk_component(mged) fb_active 0
#    $itk_component(rtcntrl) update_fb_mode

    # create view axes control panel
#    itk_component add vac {
#	ViewAxesControl $itk_interior.vac -mged $itk_component(mged)
#    } {
#	usual
#    }
#    $itk_component(menubar) menuconfigure .modes.viewaxesCP \
	-command "$itk_component(vac) show"
    
    # create model axes control panel
#    itk_component add mac {
#	ModelAxesControl $itk_interior.mac -mged $itk_component(mged)
#    } {
#	usual
#    }
#   $itk_component(menubar) menuconfigure .modes.modelaxesCP \
	    -command "$itk_component(mac) show"
    
#    wm protocol $itk_component(vac) WM_DELETE_WINDOW "$itk_component(vac) hide"
#    wm protocol $itk_component(mac) WM_DELETE_WINDOW "$itk_component(mac) hide"
    
#    $itk_component(mged) configure -unitsCallback "$itk_component(mac) updateControlPanel"
    $itk_component(mged) configure -paneCallback [::itcl::code $this _update_active_pane]
    
    # Override axes hot keys in the Mged widget
#    bind [$itk_component(mged) component ul component dm] m [::itcl::code $this _toggle_model_axes ul]
#    bind [$itk_component(mged) component ur component dm] m [::itcl::code $this _toggle_model_axes ur]
#    bind [$itk_component(mged) component ll component dm] m [::itcl::code $this _toggle_model_axes ll]
#    bind [$itk_component(mged) component lr component dm] m [::itcl::code $this _toggle_model_axes lr]
#    bind [$itk_component(mged) component ul component dm] T [::itcl::code $this _toggle_model_axes_ticks ul]
#    bind [$itk_component(mged) component ur component dm] T [::itcl::code $this _toggle_model_axes_ticks ur]
#    bind [$itk_component(mged) component ll component dm] T [::itcl::code $this _toggle_model_axes_ticks ll]
#    bind [$itk_component(mged) component lr component dm] T [::itcl::code $this _toggle_model_axes_ticks lr]
#    bind [$itk_component(mged) component ul component dm] v [::itcl::code $this _toggle_view_axes ul]
#    bind [$itk_component(mged) component ur component dm] v [::itcl::code $this _toggle_view_axes ur]
#    bind [$itk_component(mged) component ll component dm] v [::itcl::code $this _toggle_view_axes ll]
#    bind [$itk_component(mged) component lr component dm] v [::itcl::code $this _toggle_view_axes lr]


    # Other bindings for mged
    #bind $itk_component(mged) <Enter> {focus %W}

    if {!$mViewOnly} {
	if {$Archer::inheritFromToplevel} {
	    #$itk_component(filemenu) entryconfigure "Raytrace Control Panel..." -state normal
	    #$itk_component(filemenu) entryconfigure "Export Geometry to PNG..." -state normal
	    #$itk_component(filemenu) entryconfigure "Compact" -state disabled
	    catch {$itk_component(filemenu) delete "Compact"}

	    $itk_component(filemenu) insert "Import" command \
		-label "Purge History" \
		-command [::itcl::code $this purgeHistory]

	    $itk_component(filemenu) entryconfigure "Import" -state disabled
	    $itk_component(filemenu) entryconfigure "Export" -state disabled
	    $itk_component(displaymenu) entryconfigure "Standard Views" -state normal
	    $itk_component(displaymenu) entryconfigure "Reset" -state normal
	    $itk_component(displaymenu) entryconfigure "Autoview" -state normal
	    $itk_component(displaymenu) entryconfigure "Center..." -state normal
	    $itk_component(modesmenu) entryconfigure "View Axes" -state normal
	    $itk_component(modesmenu) entryconfigure "Model Axes" -state normal
	    $itk_component(displaymenu) entryconfigure "Clear" -state normal
	    $itk_component(displaymenu) entryconfigure "Refresh" -state normal
	} else {
	    #$itk_component(menubar) menuconfigure .file.rt  -state normal
	    #$itk_component(menubar) menuconfigure .file.png -state normal
	    #$itk_component(menubar) menuconfigure .file.compact -state disabled
	    catch {$itk_component(menubar) delete .file.compact}

	    $itk_component(menubar) insert .file.import command purgeHist \
		-label "Purge History" \
		-helpstr "Remove all object history"

	    $itk_component(menubar) menuconfigure .file.import -state disabled
	    $itk_component(menubar) menuconfigure .file.export -state disabled
	    $itk_component(menubar) menuconfigure .display.standard -state normal
	    $itk_component(menubar) menuconfigure .display.reset -state normal
	    $itk_component(menubar) menuconfigure .display.autoview -state normal
	    $itk_component(menubar) menuconfigure .display.center -state normal
	    $itk_component(menubar) menuconfigure .display.clear -state normal
	    $itk_component(menubar) menuconfigure .display.refresh -state normal

	    $itk_component(menubar) menuconfigure .file.purgeHist \
		-command [::itcl::code $this purgeHistory]
	}

	$itk_component(canvas_menu) menuconfigure .raytrace.rt \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rt.fivetwelve \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rt.tentwenty \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rt.window \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.fifty \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.hundred \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.twofiftysix \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.fivetwelve \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge.fivetwelve \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge.tentwenty \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge.window \
	    -state normal
	$itk_component(canvas_menu) menuconfigure .raytrace.nirt \
	    -state normal
    }

    $itk_component(canvas_menu) menuconfigure .view.front \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.rear \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.port \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.starboard \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.top \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.bottom \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.35,25 \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.45,45 \
	-state normal    
}

::itcl::body Archer::_init_sdb {} {
    itk_component add sdb {
	if {$mDbNoCopy || $mDbReadOnly} {
	    set _target $mTarget
	} else {
	    set _target $mTargetCopy
	}
	
	SdbBrlView $itk_component(canvasF).sdb $_target \
	    -type $mDisplayType \
	    -showhandle 0 \
	    -sashcursor sb_v_double_arrow \
	    -hsashcursor sb_h_double_arrow \
	    -showViewingParams $mShowViewingParams
    } {
	keep -sashwidth -sashheight -sashborderwidth
	keep -sashindent -thickness
    }
    set tmp_dbCommands [$itk_component(sdb) getUserCmds]
    set unwrappedDbCommands {}
    foreach cmd $tmp_dbCommands {
	if {[lsearch $bannedDbCommands $cmd] == -1} {
	    lappend unwrappedDbCommands $cmd
	}
    }

    set dbSpecificCommands $sdbCommands

    $itk_component(sdb) transparencyAll 1
    $itk_component(sdb) bounds "-4096 4095 -4096 4095 -4096 4095"
    $itk_component(sdb) configure -paneCallback [::itcl::code $this _update_active_pane]
    
    # Override axes hot keys in the Mged widget
#    bind [$itk_component(sdb) component ul component dm] m [::itcl::code $this _toggle_model_axes ul]
#    bind [$itk_component(sdb) component ur component dm] m [::itcl::code $this _toggle_model_axes ur]
#    bind [$itk_component(sdb) component ll component dm] m [::itcl::code $this _toggle_model_axes ll]
#    bind [$itk_component(sdb) component lr component dm] m [::itcl::code $this _toggle_model_axes lr]
#    bind [$itk_component(sdb) component ul component dm] T [::itcl::code $this _toggle_model_axes_ticks ul]
#    bind [$itk_component(sdb) component ur component dm] T [::itcl::code $this _toggle_model_axes_ticks ur]
#    bind [$itk_component(sdb) component ll component dm] T [::itcl::code $this _toggle_model_axes_ticks ll]
#    bind [$itk_component(sdb) component lr component dm] T [::itcl::code $this _toggle_model_axes_ticks lr]
#    bind [$itk_component(sdb) component ul component dm] v [::itcl::code $this _toggle_view_axes ul]
#    bind [$itk_component(sdb) component ur component dm] v [::itcl::code $this _toggle_view_axes ur]
#    bind [$itk_component(sdb) component ll component dm] v [::itcl::code $this _toggle_view_axes ll]
#    bind [$itk_component(sdb) component lr component dm] v [::itcl::code $this _toggle_view_axes lr]


    # Other bindings for mged
    #bind $itk_component(sdb) <Enter> {focus %W}

    if {!$mViewOnly} {
	if {$Archer::inheritFromToplevel} {
	    #$itk_component(filemenu) entryconfigure "Raytrace Control Panel..." -state normal
	    #$itk_component(filemenu) entryconfigure "Export Geometry to PNG..." -state normal
	    #$itk_component(filemenu) entryconfigure "Compact" -state normal
	    catch {$itk_component(filemenu) delete "Purge History"}

	    $itk_component(filemenu) insert "Import" command \
		-label "Compact" \
		-command [::itcl::code $this compact]

	    $itk_component(filemenu) entryconfigure "Import" -state normal
	    $itk_component(filemenu) entryconfigure "Export" -state normal
	    $itk_component(displaymenu) entryconfigure "Standard Views" -state normal
	    $itk_component(displaymenu) entryconfigure "Reset" -state normal
	    $itk_component(displaymenu) entryconfigure "Autoview" -state normal
	    $itk_component(displaymenu) entryconfigure "Center..." -state normal
	    $itk_component(modesmenu) entryconfigure "View Axes" -state normal
	    $itk_component(modesmenu) entryconfigure "Model Axes" -state normal
	} else {
	    #$itk_component(menubar) menuconfigure .file.rt  -state normal
	    #$itk_component(menubar) menuconfigure .file.png -state normal
	    #$itk_component(menubar) menuconfigure .file.compact -state normal
	    catch {$itk_component(menubar) delete .file.purgeHist}

	    $itk_component(menubar) insert .file.import command compact \
		-label "Compact" \
		-helpstr "Compact the target description"
		
	    $itk_component(menubar) menuconfigure .file.import -state normal
	    $itk_component(menubar) menuconfigure .file.export -state normal
	    $itk_component(menubar) menuconfigure .display.standard -state normal
	    $itk_component(menubar) menuconfigure .display.reset -state normal
	    $itk_component(menubar) menuconfigure .display.autoview -state normal
	    $itk_component(menubar) menuconfigure .display.center -state normal

	    $itk_component(menubar) menuconfigure .file.compact \
		-command [::itcl::code $this compact]
	}

	$itk_component(canvas_menu) menuconfigure .raytrace.rt \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rt.fivetwelve \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rt.tentwenty \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rt.window \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.fifty \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.hundred \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.twofiftysix \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtcheck.fivetwelve \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge.fivetwelve \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge.tentwenty \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.rtedge.window \
	    -state disabled
	$itk_component(canvas_menu) menuconfigure .raytrace.nirt \
	    -state disabled
    }

    $itk_component(canvas_menu) menuconfigure .view.front \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.rear \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.port \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.starboard \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.top \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.bottom \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.35,25 \
	-state normal
    $itk_component(canvas_menu) menuconfigure .view.45,45 \
	-state normal
}

::itcl::body Archer::_close_mged {} {
    catch {delete object $itk_component(rtcntrl)}
#    catch {delete object $itk_component(vac)}
#    catch {delete object $itk_component(mac)}
    catch {delete object $itk_component(mged)}

    if {$Archer::inheritFromToplevel} {
    } else {
#	$itk_component(menubar) menuconfigure .file.rt  -state disabled
#	$itk_component(menubar) menuconfigure .file.png -state disabled
#	$itk_component(menubar) delete .modes
    }
}

::itcl::body Archer::_close_sdb {} {
    catch {delete object $itk_component(sdb)}

    if {$Archer::inheritFromToplevel} {
    } else {
#	$itk_component(menubar) menuconfigure .file.rt  -state disabled
#	$itk_component(menubar) menuconfigure .file.png -state disabled
#	$itk_component(menubar) delete .modes
    }
}

# ------------------------------------------------------------
#                 INTERFACE OPERATIONS
# ------------------------------------------------------------
::itcl::body Archer::_new_db {} {
    if {$::Archer::haveSdb} {
	set typelist {
	    {"BRL-CAD Database" {".g"}}
	    {"IVAVIEW Database" {".xg"}}
	    {"All Files" {*}}
	}
    } else {
	set typelist {
	    {"BRL-CAD Database" {".g"}}
	    {"All Files" {*}}
	}
    }

    #XXX This is not quite right, but it gets us
    #    enough of the behavior we want (for the moment).
    set target [tk_getSaveFile -parent $itk_interior \
	    -initialdir $mLastSelectedDir \
	    -title "Create a New Database" \
	    -filetypes $typelist]
    
    if {$target == ""} {
	return
    } else {
	set mLastSelectedDir [file dirname $target]
    }

    if {[file exists $target]} {
	file delete -force $target
    }

    set type ""
    if {$::Archer::haveSdb} {
	switch -- [file extension $target] {
	    ".g"   {
		set type "BRL-CAD"
		set db [Db \#auto $target]
		::itcl::delete object $db
	    }
	    ".xg" {
		set type "IVAVIEW"
		set sdb [Sdb \#auto $target]
		::itcl::delete object $sdb
	    }
	    default {
		return
	    }
	}
    } else {
	switch -- [file extension $target] {
	    ".g"   {
		set type "BRL-CAD"
		set db [Db \#auto $target]
		::itcl::delete object $db
	    }
	    default {
		return
	    }
	}
    }
    Load $type $target
}

::itcl::body Archer::_open_db {} {
    if {$::Archer::haveSdb} {
	set typelist {
	    {"BRL-CAD Database" {".g"}}
	    {"IVAVIEW Database" {".xg"}}
	    {"All Files" {*}}
	}
    } else {
	set typelist {
	    {"BRL-CAD Database" {".g"}}
	    {"All Files" {*}}
	}
    }

    set target [tk_getOpenFile -parent $itk_interior \
	    -initialdir $mLastSelectedDir \
	    -title "Open Database" \
	    -filetypes $typelist]
    
    if {$target == ""} {
	return
    } else {
	set mLastSelectedDir [file dirname $target]
    }

    set type ""
    if {$::Archer::haveSdb} {
	switch -- [file extension $target] {
	    ".g"   {set type "BRL-CAD"}
	    ".xg" {set type "IVAVIEW"}
	    default {
		return
	    }
	}
    } else {
	switch -- [file extension $target] {
	    ".g"   {set type "BRL-CAD"}
	    default {
		return
	    }
	}
    }
    Load $type $target
}

::itcl::body Archer::_save_db {} {
    set mNeedSave 0
    _update_save_mode	

    # Sanity
    if {$mTarget == "" ||
	$mTargetCopy == "" ||
        $mDbReadOnly ||
	$mDbNoCopy} {
	return
    }

    file copy -force $mTargetCopy $mTarget
}

::itcl::body Archer::_close_db {} {
    switch -- $mDbType {
	"BRL-CAD" {
	    pack forget $itk_component(mged)
	    _close_mged
	}
	"IVAVIEW" {
	    pack forget $itk_component(sdb)
	    _close_sdb
	}
	default {
	}
    }
    grid $itk_component(canvas) -row 1 -column 0 -columnspan 3 -sticky news
    set mDbType ""

    _refresh_tree 0
}

::itcl::body Archer::_primary_toolbar_add {type name {args ""}} {
    if {[llength $args] > 1} {
	eval $itk_component(primaryToolbar) add $type $name $args
    } else {
	eval $itk_component(primaryToolbar) add $type $name [lindex $args 0]
    }
    return [$itk_component(primaryToolbar) index $name]
}

::itcl::body Archer::_primary_toolbar_remove {index} {
    eval $itk_component(primaryToolbar) delete $index
}

# ------------------------------------------------------------
#                    WINDOW COMMANDS
# ------------------------------------------------------------
::itcl::body Archer::_do_primary_toolbar {} {
    configure -primaryToolbar $itk_option(-primaryToolbar)
}

::itcl::body Archer::_do_view_toolbar {} {
    configure -viewToolbar $itk_option(-viewToolbar)
}

::itcl::body Archer::_do_statusbar {} {
    configure -statusbar $itk_option(-statusbar)
}

::itcl::body Archer::dockarea {{position "south"}} {
    switch -- $position {
	"north" -
	"south" -
	"east"  -
	"west"  {return $itk_component($position)}
	default {
	    error "Archer::dockarea: unrecognized area `$position'"
	}
    }
}

::itcl::body Archer::primary_toolbar_add_btn {name {args ""}} {
    if [catch {_primary_toolbar_add button $name $args} err] {error $err}
    return $err
}

::itcl::body Archer::primary_toolbar_add_sep {name {args ""}} {
    if [catch {_primary_toolbar_add frame $name $args} err] {error $err}
    return $err
}

::itcl::body Archer::primary_toolbar_remove_itm  {index} {
    if [catch {_primary_toolbar_remove $index} err] {error $err}
}

::itcl::body Archer::closeHierarchy {} {
    $itk_component(vpane) hide hierarchyView
}

::itcl::body Archer::openHierarchy {{fraction 30}} {
    #XXX We should check to see if fraction is between
    #    0 and 100 inclusive.
    $itk_component(vpane) show hierarchyView
    $itk_component(vpane) fraction $fraction [expr {100 - $fraction}]
}

# ------------------------------------------------------------
#                     MGED COMMANDS
# ------------------------------------------------------------
::itcl::body Archer::_alter_obj {operation comp} {
    if {[winfo exists .alter]} {
	destroy .alter
    }

    set top [toplevel .alter]
    wm withdraw $top
    wm transient $top $itk_interior
    set x [expr [winfo rootx $itk_interior] + 100]
    set y [expr [winfo rooty $itk_interior] + 100]
    wm geometry $top +$x+$y

    set entry [::iwidgets::entryfield $top.entry -textbackground $SystemWindow -width 20]
    pack $entry -fill x -padx 3 -pady 2

    set cmd ""
    switch -- $operation {
	"Copy" {
	    wm title $top "Copy $comp"
	    set cmd "cp"
	    $entry configure -labeltext "New Component:"
	}
	"Rename" {
	    wm title $top "Rename $comp"
	    set cmd "mvall"
	    $entry configure -labeltext "New Name:"
	}
    }

    set cancel [button $top.cancel -text "Cancel" -width 7 -command "destroy $top"]
    set ok [button $top.ok -text "OK" -width 7 -command [::itcl::code $this _cp_mv $top $comp $cmd]]
    pack $cancel -side right -anchor e -padx 3 -pady 2
    pack $ok -side right -anchor e -padx 3 -pady 2

    set_focus $top $entry
    tkwait window $top
}

::itcl::body Archer::_delete_obj {comp} {
    if {[do_question "Are you sure you wish to delete `$comp'."] == "no"} {
	return
    }

    set mNeedSave 1
    _update_save_mode	
    SetWaitCursor
    dbCmd kill $comp

    set select [$itk_component(tree) selection get]
    #set element [lindex [split $select ":"] 1]
    set element [split $select ":"]
    if {[llength $element] > 1} {
	set element [lindex $element 1]
    }

    set node [$itk_component(tree) query -path $element]
    
    set node ""
    foreach t $tags {
	if {[string compare [string trim $t] "leaf"] != 0 &&
	[string compare [string trim $t] "branch"] != 0} {
	    set node $t
	}
    }
    set flist [file split $node]
    if {[llength $flist] > 1} {
	set grp [lindex $flist [expr [llength $flist] -2]]
	dbCmd rm $grp $comp
    }

    # remove from tree
    set parent [$itk_component(tree) query -parent $element]
    $itk_component(tree) remove $element $parent
    _refresh_tree
    SetNormalCursor
}

::itcl::body Archer::_group_obj {comp} {
    set mNeedSave 1
    _update_save_mode	
}

::itcl::body Archer::_region_obj {comp} {
    set mNeedSave 1
    _update_save_mode	
}

::itcl::body Archer::_cp_mv {top comp cmd} {
    set mNeedSave 1
    _update_save_mode	
    SetWaitCursor
    set comp2 [string trim [$top.entry get]]
    wm withdraw $top
    dbCmd $cmd $comp 
    _refresh_tree
    SetNormalCursor
    destroy $top
}

::itcl::body Archer::_build_view_toolbar {} {
    # build the shift grip mode toolbar
    itk_component add viewToolbar {
	::iwidgets::toolbar $itk_component(east).viewToolbar \
		-helpvariable [::itcl::scope mStatusStr] \
		-balloonfont "{CG Times} 8" \
		-balloonbackground \#ffffdd \
		-borderwidth 1 \
		-orient vertical \
		-state disabled
    } {
	# XXX If uncommented, the following line hoses things
	#usual
    }

    $itk_component(viewToolbar) add radiobutton rotate \
	-balloonstr "Rotate view" \
	-helpstr "Rotate view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $ROTATE_MODE \
	-command [::itcl::code $this beginViewRotate]
    $itk_component(viewToolbar) add radiobutton translate \
	-balloonstr "Translate view" \
	-helpstr "Translate view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $TRANSLATE_MODE \
	-command [::itcl::code $this beginViewTranslate]
    $itk_component(viewToolbar) add radiobutton scale \
	-balloonstr "Scale view" \
	-helpstr "Scale view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $SCALE_MODE \
	-command [::itcl::code $this beginViewScale]
    $itk_component(viewToolbar) add radiobutton center \
	-balloonstr "Center view" \
	-helpstr "Center view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $CENTER_MODE \
	-command [::itcl::code $this _init_center_mode]
    $itk_component(viewToolbar) add radiobutton cpick \
	-balloonstr "Component Pick" \
	-helpstr "Component Pick" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $COMP_PICK_MODE \
	-command [::itcl::code $this initCompPick]
    $itk_component(viewToolbar) add radiobutton measure \
	-balloonstr "Measuring Tool" \
	-helpstr "Measuring Tool" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $MEASURE_MODE \
	-command [::itcl::code $this initMeasure]

    pack $itk_component(viewToolbar) -expand yes -fill both
}

::itcl::body Archer::_update_view_toolbar_for_edit {} {
    catch {
	# add spacer
	$itk_component(viewToolbar) add frame fill1 \
	    -borderwidth 1 \
	    -relief flat \
	    -height 5
	$itk_component(viewToolbar) add frame sep1 \
	    -borderwidth 1 \
	    -relief sunken \
	    -height 2
	$itk_component(viewToolbar) add frame fill2 \
	    -borderwidth 1 \
	    -relief flat \
	    -height 5
	$itk_component(viewToolbar) add radiobutton edit_rotate \
	    -balloonstr "Rotate selected object" \
	    -helpstr "Rotate selected object" \
	    -variable [::itcl::scope mDefaultBindingMode] \
	    -value $OBJECT_ROTATE_MODE \
	    -command [::itcl::code $this beginObjRotate] \
	    -image [image create photo \
			-file [file join $_imgdir Themes $mTheme edit_rotate.png]]
	$itk_component(viewToolbar) add radiobutton edit_translate \
	    -balloonstr "Translate selected object" \
	    -helpstr "Translate selected object" \
	    -variable [::itcl::scope mDefaultBindingMode] \
	    -value $OBJECT_TRANSLATE_MODE \
	    -command [::itcl::code $this beginObjTranslate] \
	    -image [image create photo \
			-file [file join $_imgdir Themes $mTheme edit_translate.png]]
	$itk_component(viewToolbar) add radiobutton edit_scale \
	    -balloonstr "Scale selected object" \
	    -helpstr "Scale selected object" \
	    -variable [::itcl::scope mDefaultBindingMode] \
	    -value $OBJECT_SCALE_MODE \
	    -command [::itcl::code $this beginObjScale] \
	    -image [image create photo \
			-file [file join $_imgdir Themes $mTheme edit_scale.png]]
	$itk_component(viewToolbar) add radiobutton edit_center \
	    -balloonstr "Center selected object" \
	    -helpstr "Center selected object" \
	    -variable [::itcl::scope mDefaultBindingMode] \
	    -value $OBJECT_CENTER_MODE \
	    -command [::itcl::code $this beginObjCenter] \
	    -image [image create photo \
			-file [file join $_imgdir Themes $mTheme edit_select.png]]

	$itk_component(viewToolbar) itemconfigure edit_rotate -state disabled
	$itk_component(viewToolbar) itemconfigure edit_translate -state disabled
	$itk_component(viewToolbar) itemconfigure edit_scale -state disabled
	$itk_component(viewToolbar) itemconfigure edit_center -state disabled
    }
}

::itcl::body Archer::beginViewRotate {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm rotate_mode %x %y; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endViewRotate $dm]; break"
    }
}

::itcl::body Archer::endViewRotate {dsp} {
    $dsp idle_mode

    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    set ae [$_comp ae]
    _add_history "ae $ae"
}

::itcl::body Archer::beginViewScale {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm scale_mode %x %y; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endViewScale $dm]; break"
    }
}

::itcl::body Archer::endViewScale {dsp} {
    $dsp idle_mode

    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    set size [$_comp size]
    _add_history "size $size"
}

::itcl::body Archer::beginViewTranslate {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm translate_mode %x %y; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endViewTranslate $dm]; break"
    }
}

::itcl::body Archer::endViewTranslate {dsp} {
    $dsp idle_mode

    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    set center [$_comp center]
    _add_history "center $center"
}

::itcl::body Archer::_init_center_mode {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm slew %x %y; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endViewTranslate $dm]; break"
    }
}

::itcl::body Archer::initCompPick {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "[::itcl::code $this mouseRay $dm %x %y]; break"
	bind $win <ButtonRelease-1> ""
    }
}

::itcl::body Archer::initMeasure {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]
	bind $win <1> "[::itcl::code $this beginMeasure $dm %x %y]; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endMeasure $dm]; break"
    }
}

::itcl::body Archer::beginMeasure {_dm _x _y} {
    if {$mMeasuringStickMode == 0} {
	# Draw on the front face of the viewing cube
	set view [$_dm screen2view $_x $_y]
	set bounds [$_dm bounds]
	set vZ [expr {[lindex $bounds 4] / -2048.0}]
	set mMeasureStart [$_dm v2mPoint [lindex $view 0] [lindex $view 1] $vZ]
    } else {
	# Draw on the center of the viewing cube (i.e. view Z is 0)
	set mMeasureStart [$_dm screen2model $_x $_y]
    }

    # start receiving motion events
    set win [$_dm component dm]
    bind $win <Motion> "[::itcl::code $this handleMeasure $_dm %x %y]; break"

    set mMeasuringStickColorVDraw [_get_vdraw_color $mMeasuringStickColor]
}

::itcl::body Archer::handleMeasure {_dm _x _y} {
    catch {dbCmd vdraw vlist delete $MEASURING_STICK}

    if {$mMeasuringStickMode == 0} {
	# Draw on the front face of the viewing cube
	set view [$_dm screen2view $_x $_y]
	set bounds [$_dm bounds]
	set vZ [expr {[lindex $bounds 4] / -2048.0}]
	set mMeasureEnd [$_dm v2mPoint [lindex $view 0] [lindex $view 1] $vZ]
    } else {
	# Draw on the center of the viewing cube (i.e. view Z is 0)
	set mMeasureEnd [$_dm screen2model $_x $_y]
    }

    set move 0
    set draw 1
    dbCmd vdraw open $MEASURING_STICK
    dbCmd vdraw params color $mMeasuringStickColorVDraw
    eval dbCmd vdraw write next $move $mMeasureStart
    eval dbCmd vdraw write next $draw $mMeasureEnd
    dbCmd vdraw send
}

::itcl::body Archer::endMeasure {_dm} {
    $_dm idle_mode

    catch {dbCmd vdraw vlist delete $MEASURING_STICK}
    dbCmd erase _VDRW$MEASURING_STICK

    set diff [vsub2 $mMeasureEnd $mMeasureStart]
    set delta [expr {[magnitude $diff] * [dbCmd base2local]}]
    tk_messageBox -message "Measured distance:  $delta [dbCmd units]"
}

::itcl::body Archer::beginObjRotate {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	::sdialogs::Stddlgs::errordlg "User Error" \
		"You must first select an object to rotate!"
	return
    }

    if {[info exists itk_component(sdb)]} {
	set savePwd [pwd]
	cd /
	set center [ocenter $obj]
	cd $savePwd

	set x [lindex $center 0]
	set y [lindex $center 1]
	set z [lindex $center 2]
    } else {
	# These values are insignificant (i.e. they will be ignored by the callback)
	set x 0
	set y 0
	set z 0
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm orotate_mode %x %y [list [::itcl::code $this handleObjRotate]] $obj $x $y $z; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjRotate $dm $obj]; break"
    }
}

::itcl::body Archer::endObjRotate {dsp obj} {
    $dsp idle_mode

    #XXX Need code to track overall transformation
    if {[info exists itk_component(mged)]} {
	#_add_history "orotate obj rx ry rz"
    } else {
	#_add_history "orotate obj rx ry rz kx ky kz"
    }
}

::itcl::body Archer::beginObjScale {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	::sdialogs::Stddlgs::errordlg "User Error" \
		"You must first select an object to scale!"
	return
    }

    if {[info exists itk_component(sdb)]} {
	set savePwd [pwd]
	cd /
	set center [ocenter $obj]
	cd $savePwd

	set x [lindex $center 0]
	set y [lindex $center 1]
	set z [lindex $center 2]
    } else {
	# These values are insignificant (i.e. they will be ignored by the callback)
	set x 0
	set y 0
	set z 0
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm oscale_mode %x %y [list [::itcl::code $this handleObjScale]] $obj $x $y $z; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjScale $dm $obj]; break"
    }
}

::itcl::body Archer::endObjScale {dsp obj} {
    $dsp idle_mode

    #XXX Need code to track overall transformation
    if {[info exists itk_component(mged)]} {
	#_add_history "oscale obj sf"
    } else {
	#_add_history "oscale obj sf kx ky kz"
    }
}

::itcl::body Archer::beginObjTranslate {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	::sdialogs::Stddlgs::errordlg "User Error" \
		"You must first select an object to translate!"
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm otranslate_mode %x %y [list [::itcl::code $this handleObjTranslate]] $obj; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjTranslate $dm $obj]; break"
    }
}

::itcl::body Archer::endObjTranslate {dsp obj} {
    $dsp idle_mode

    #XXX Need code to track overall transformation
    if {[info exists itk_component(mged)]} {
	#_add_history "otranslate obj dx dy dz"
    } else {
	#_add_history "otranslate obj dx dy dz"
    }
}

::itcl::body Archer::beginObjCenter {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	::sdialogs::Stddlgs::errordlg "User Error" \
		"You must first select an object to move!"
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]
	bind $win <1> "[::itcl::code $this handleObjCenter $obj %x %y]; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjCenter $dm $obj]; break"
    }
}

::itcl::body Archer::endObjCenter {dsp obj} {
    $dsp idle_mode

    if {[info exists itk_component(mged)]} {
	set center [$itk_component(mged) ocenter $obj]
    } elseif {[info exists itk_component(sdb)]} {
	set savePwd [pwd]
	cd /
	set center [$itk_component(sdb) ocenter $obj]
	cd $savePwd
    } else {	
	return
    }

    _add_history "ocenter $center"
}

::itcl::body Archer::_init_default_bindings {} {
    if {[info exists itk_component(mged)]} {
	set _comp $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set _comp $itk_component(sdb)
    } else {	
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win [$dm component dm]

	# Turn off mouse bindings
	bind $win <1> {}
	bind $win <2> {}
	bind $win <3> {}
	bind $win <ButtonRelease-1> {}

	# Turn off rotate mode
	bind $win <Control-ButtonPress-1> {}
	bind $win <Control-ButtonPress-2> {}
	bind $win <Control-ButtonPress-3> {}

	# Turn off translate mode
	bind $win <Shift-ButtonPress-1> {}
	bind $win <Shift-ButtonPress-2> {}
	bind $win <Shift-ButtonPress-3> {}

	# Turn off scale mode
	bind $win <Control-Shift-ButtonPress-1> {}
	bind $win <Control-Shift-ButtonPress-2> {}
	bind $win <Control-Shift-ButtonPress-3> {}

	# Turn off constrained rotate mode
	bind $win <Alt-Control-ButtonPress-1> {}
	bind $win <Alt-Control-ButtonPress-2> {}
	bind $win <Alt-Control-ButtonPress-3> {}

	# Turn off constrained translate mode
	bind $win <Alt-Shift-ButtonPress-1> {}
	bind $win <Alt-Shift-ButtonPress-2> {}
	bind $win <Alt-Shift-ButtonPress-3> {}

	# Turn off constrained scale mode
	bind $win <Alt-Control-Shift-ButtonPress-1> {}
	bind $win <Alt-Control-Shift-ButtonPress-2> {}
	bind $win <Alt-Control-Shift-ButtonPress-3> {}

	# Turn off key bindings
	bind $win 3 {}
	bind $win 4 {}
	bind $win f {}
	bind $win R {}
	bind $win r {}
	bind $win l {}
	bind $win t {}
	bind $win b {}
	bind $win m {}
	bind $win T {}
	bind $win v {}
	bind $win <F2> {}
	bind $win <F3> {}
	bind $win <F4> {}
	bind $win <F5> {}
	bind $win <F10> {}

	# overrides
	bind $win <Shift-ButtonPress-1> "$_comp rotate_mode %x %y; break"
	bind $win <Shift-ButtonPress-2> "$_comp scale_mode %x %y; break"
	bind $win <Shift-ButtonPress-3> "$_comp translate_mode %x %y; break"
	bind $win <Control-Shift-ButtonPress-3> "$_comp slew %x %y; break"

	bind $win <Shift-ButtonRelease-1> "[::itcl::code $this endViewRotate $dm]; break"
	bind $win <Shift-ButtonRelease-2> "[::itcl::code $this endViewScale $dm]; break"
	bind $win <Shift-ButtonRelease-3> "[::itcl::code $this endViewTranslate $dm]; break"

	if {!$mViewOnly} {
	    if {$Archer::inheritFromToplevel} {
		bind $win <Control-ButtonPress-1> \
		    "[::itcl::code $this launchDisplayMenuBegin $dm $itk_component(displaymenu) %X %Y]; break"
		bind $win <3> \
		    "[::itcl::code $this launchDisplayMenuBegin $dm $itk_component(displaymenu) %X %Y]; break"
	    } else {
		bind $win <Control-ButtonPress-1> \
		    "[::itcl::code $this launchDisplayMenuBegin $dm [$itk_component(menubar) component display-menu] %X %Y]; break"
		bind $win <3> \
		    "[::itcl::code $this launchDisplayMenuBegin $dm [$itk_component(menubar) component display-menu] %X %Y]; break"
	    }
	}
    }

    $itk_component(viewToolbar) configure -state normal
    if {$mDbType == "BRL-CAD"} {
	$itk_component(viewToolbar) itemconfigure cpick \
	    -state normal
    } else {
	$itk_component(viewToolbar) itemconfigure cpick \
	    -state disabled
    }

    if {$mMode != 0} {
	$itk_component(viewToolbar) itemconfigure edit_rotate -state normal
	$itk_component(viewToolbar) itemconfigure edit_translate -state normal
	$itk_component(viewToolbar) itemconfigure edit_scale -state normal
	$itk_component(viewToolbar) itemconfigure edit_center -state normal
    }
    catch {eval [$itk_component(viewToolbar) itemcget $mDefaultBindingMode -command]}
}

::itcl::body Archer::_init_brlcad_bindings {} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    $itk_component(viewToolbar) configure -state disabled

    if {[info exists itk_component(mged)]} {
	$itk_component(mged) resetBindingsAll
    } else {
	$itk_component(sdb) resetBindingsAll
    }
}

::itcl::body Archer::_build_about_dialog {} {
    global env

    itk_component add aboutDialog {
	::iwidgets::dialog $itk_interior.aboutDialog \
	    -modality application \
	    -title "About Archer" \
	    -background $SystemButtonFace
    } {}
    $itk_component(aboutDialog) hide 1
    $itk_component(aboutDialog) hide 2
    $itk_component(aboutDialog) hide 3
    $itk_component(aboutDialog) configure \
	-thickness 2 \
	-buttonboxpady 0
    $itk_component(aboutDialog) buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0

    # ITCL can be nasty
    set win [$itk_component(aboutDialog) component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$itk_component(aboutDialog) childsite]

    itk_component add aboutDialogTabs {
	blt::tabnotebook $parent.tabs \
	    -side bottom \
	    -relief flat \
	    -tiers 99 \
	    -tearoff 0 \
	    -gap 3 \
	    -width 0 \
	    -height 0 \
	    -outerpad 0 \
	    -highlightthickness 1 \
	    -selectforeground black
    } {}
    $itk_component(aboutDialogTabs) configure \
	-highlightcolor [$itk_component(aboutDialogTabs) cget -background] \
	-borderwidth 0 \
	-font $mFontText
    $itk_component(aboutDialogTabs) insert end -text "About" -stipple gray25
    $itk_component(aboutDialogTabs) insert end -text "License" -stipple gray25
    $itk_component(aboutDialogTabs) insert end -text "Acknowledgements" -stipple gray25


    # About Info
#    set aboutImg [image create photo -file [file join $env(ARCHER_HOME) $brlcadDataPath tclscripts archer images aboutArcher.png]]
    set aboutImg [image create photo -file [file join $brlcadDataPath tclscripts archer images aboutArcher.png]]
    itk_component add aboutInfo {
	::label $itk_component(aboutDialogTabs).aboutInfo \
	    -image $aboutImg
    } {}

    set aboutTabIndex 0
    $itk_component(aboutDialogTabs) tab configure $aboutTabIndex \
	-window $itk_component(aboutInfo) \
	-fill both


    # License Info
    itk_component add licenseDialogTabs {
	blt::tabnotebook $itk_component(aboutDialogTabs).tabs \
	    -side top \
	    -relief flat \
	    -tiers 99 \
	    -tearoff 0 \
	    -gap 3 \
	    -width 0 \
	    -height 0 \
	    -outerpad 0 \
	    -highlightthickness 1 \
	    -selectforeground black
    } {}
    $itk_component(licenseDialogTabs) configure \
	-highlightcolor [$itk_component(licenseDialogTabs) cget -background] \
	-borderwidth 0 \
	-font $mFontText
    if {$::Archer::haveSdb} {
	$itk_component(licenseDialogTabs) insert end -text "IVAVIEW" -stipple gray25
    }
    $itk_component(licenseDialogTabs) insert end -text "BRL-CAD" -stipple gray25

    incr aboutTabIndex
    $itk_component(aboutDialogTabs) tab configure $aboutTabIndex \
	-window $itk_component(licenseDialogTabs) \
	-fill both

    set licenseTabIndex -1

    # IVAVIEW License Info
    if {$::Archer::haveSdb} {
	set fd [open [file join $brlcadDataPath doc ivaviewLicense.txt] r]
	set mIvaviewLicenseInfo [read $fd]
	close $fd
	itk_component add ivaviewLicenseInfo {
	    ::iwidgets::scrolledtext $itk_component(licenseDialogTabs).ivaviewLicenseInfo \
		-wrap word \
		-hscrollmode dynamic \
		-vscrollmode dynamic \
		-textfont $mFontText \
		-background $SystemButtonFace \
		-textbackground $SystemButtonFace
	} {}
	$itk_component(ivaviewLicenseInfo) insert 0.0 $mIvaviewLicenseInfo
	$itk_component(ivaviewLicenseInfo) configure -state disabled

	incr licenseTabIndex
	$itk_component(licenseDialogTabs) tab configure $licenseTabIndex \
	    -window $itk_component(ivaviewLicenseInfo) \
	    -fill both
    }

    # BRL-CAD License Info
#    set fd [open [file join $env(ARCHER_HOME) $brlcadDataPath COPYING] r]
    set fd [open [file join $brlcadDataPath COPYING] r]
    set mBrlcadLicenseInfo [read $fd]
    close $fd
    itk_component add brlcadLicenseInfo {
	::iwidgets::scrolledtext $itk_component(licenseDialogTabs).brlcadLicenseInfo \
	    -wrap word \
	    -hscrollmode dynamic \
	    -vscrollmode dynamic \
	    -textfont $mFontText \
	    -background $SystemButtonFace \
	    -textbackground $SystemButtonFace
    } {}
    $itk_component(brlcadLicenseInfo) insert 0.0 $mBrlcadLicenseInfo
    $itk_component(brlcadLicenseInfo) configure -state disabled

    incr licenseTabIndex
    $itk_component(licenseDialogTabs) tab configure $licenseTabIndex \
	-window $itk_component(brlcadLicenseInfo) \
	-fill both


    # Acknowledgement Info
#    set fd [open [file join $env(ARCHER_HOME) $brlcadDataPath doc archer_ack.txt] r]
    set fd [open [file join $brlcadDataPath doc archer_ack.txt] r]
    set mAckInfo [read $fd]
    close $fd
    itk_component add ackInfo {
	::iwidgets::scrolledtext $itk_component(aboutDialogTabs).info \
	    -wrap word \
	    -hscrollmode dynamic \
	    -vscrollmode dynamic \
	    -textfont $mFontText \
	    -background $SystemButtonFace \
	    -textbackground $SystemButtonFace
    } {}
    $itk_component(ackInfo) insert 0.0 $mAckInfo
    $itk_component(ackInfo) configure -state disabled

    incr aboutTabIndex
    $itk_component(aboutDialogTabs) tab configure $aboutTabIndex \
	-window $itk_component(ackInfo) \
	-fill both

    # Version Info
    itk_component add versionInfo {
	::label $parent.versionInfo \
	    -text "Version: $mArcherVersion" \
	    -pady 0 \
	    -borderwidth 0 \
	    -font $mFontText \
	    -anchor se
    } {}

    pack $itk_component(versionInfo) \
	-expand yes \
	-fill x \
	-side bottom \
	-pady 0 \
	-ipady 0 \
	-anchor se

    pack $itk_component(aboutDialogTabs) -expand yes -fill both

    wm geometry $itk_component(aboutDialog) "600x540"
}

::itcl::body Archer::_build_mouse_overrides_dialog {} {
    itk_component add mouseOverridesDialog {
	::iwidgets::dialog $itk_interior.mouseOverridesDialog \
	    -modality application \
	    -title "Mouse Overrides" \
	    -background $SystemButtonFace
    } {}
    $itk_component(mouseOverridesDialog) hide 1
    $itk_component(mouseOverridesDialog) hide 2
    $itk_component(mouseOverridesDialog) hide 3
    $itk_component(mouseOverridesDialog) configure \
	-thickness 2 \
	-buttonboxpady 0
    $itk_component(mouseOverridesDialog) buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0

    # ITCL can be nasty
    set win [$itk_component(mouseOverridesDialog) component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$itk_component(mouseOverridesDialog) childsite]

    itk_component add mouseOverridesTranL1 {
	::label $parent.tranl1 \
	    -text "Translate: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesTranL2 {
	::label $parent.tranl2 \
	    -text "Shift-Right" \
	    -font $mFontText \
	    -anchor w
    } {}
    itk_component add mouseOverridesRotateL1 {
	::label $parent.rotateL1 \
	    -text "Rotate: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesRotateL2 {
	::label $parent.rotateL2 \
	    -text "Shift-Left" \
	    -font $mFontText \
	    -anchor w
    } {}
    itk_component add mouseOverridesScaleL1 {
	::label $parent.scaleL1 \
	    -text "Scale: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesScaleL2 {
	::label $parent.scaleL2 \
	    -text "Shift-Middle" \
	    -font $mFontText \
	    -anchor w
    } {}
    itk_component add mouseOverridesCenterL1 {
	::label $parent.centerL1 \
	    -text "Center: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesCenterL2 {
	::label $parent.centerL2 \
	    -text "Shift-Ctrl-Right" \
	    -font $mFontText \
	    -anchor w
    } {}
    itk_component add mouseOverridesPopupL1 {
	::label $parent.popupL1 \
	    -text "Popup Menu: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesPopupL2 {
	::label $parent.popupL2 \
	    -text "Right or Ctrl-Left" \
	    -font $mFontText \
	    -anchor w
    } {}

    set row 0
    grid $itk_component(mouseOverridesTranL1) \
	$itk_component(mouseOverridesTranL2) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(mouseOverridesRotateL1) \
	$itk_component(mouseOverridesRotateL2) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(mouseOverridesScaleL1) \
	$itk_component(mouseOverridesScaleL2) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(mouseOverridesCenterL1) \
	$itk_component(mouseOverridesCenterL2) \
	-row $row \
	-sticky nsew
    incr row
    grid $itk_component(mouseOverridesPopupL1) \
	$itk_component(mouseOverridesPopupL2) \
	-row $row \
	-sticky nsew

    wm geometry $itk_component(mouseOverridesDialog) "370x190"
}

::itcl::body Archer::_build_info_dialog {name title info size wrapOption modality} {
    itk_component add $name {
	::iwidgets::dialog $itk_interior.$name \
	    -modality $modality \
	    -title $title \
	    -background $SystemButtonFace
    } {}
    $itk_component($name) hide 1
    $itk_component($name) hide 2
    $itk_component($name) hide 3
    $itk_component($name) configure \
	-thickness 2 \
	-buttonboxpady 0
    $itk_component($name) buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0

    # ITCL can be nasty
    set win [$itk_component($name) component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$itk_component($name) childsite]

    itk_component add $name\Info {
	::iwidgets::scrolledtext $parent.info \
	    -wrap $wrapOption \
	    -hscrollmode dynamic \
	    -vscrollmode dynamic \
	    -textfont $mFontText \
	    -background $SystemButtonFace \
	    -textbackground $SystemButtonFace
    } {}
    $itk_component($name\Info) insert 0.0 $info
    wm geometry $itk_component($name) $size
#    wm overrideredirect $itk_component($name) 1

    after idle "$itk_component($name\Info) configure -state disabled"
    after idle "$itk_component($name) center"

    pack $itk_component($name\Info) -expand yes -fill both
}

::itcl::body Archer::_build_info_dialogs {} {
    global tcl_platform

    _build_about_dialog
    _build_mouse_overrides_dialog
#    _build_info_dialog mouseOverridesDialog \
	"Mouse Overrides" $mMouseOverrideInfo \
	370x190 word application

    # Lame tcl/tk won't center the window properly
    # the first time unless we call update.
    update

    after idle "$itk_component(aboutDialog) center; $itk_component(mouseOverridesDialog) center"

#    if {$tcl_platform(os) == "Windows NT"} {
#	wm attributes $itk_component(aboutDialog) -topmost 1
#	wm attributes $itk_component(mouseOverridesDialog) -topmost 1
#    }

#    wm group $itk_component(aboutDialog) [namespace tail $this]
#    wm group $itk_component(mouseOverridesDialog) [namespace tail $this]
}

::itcl::body Archer::_build_save_dialog {} {
    _build_info_dialog saveDialog \
	"Save Target?" \
	"Do you wish to save the current target?" \
	450x85 none application

    $itk_component(saveDialog) show 2
    $itk_component(saveDialog) buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0 \
	-text Yes
    $itk_component(saveDialog) buttonconfigure 2 \
	-borderwidth 1 \
	-pady 0 \
	-text No
    $itk_component(saveDialogInfo) configure \
	-vscrollmode none \
	-hscrollmode none
}

::itcl::body Archer::_build_view_center_dialog {} {
    itk_component add centerDialog {
	::iwidgets::dialog $itk_interior.centerDialog \
	    -modality application \
	    -title "View Center"
    } {}
    $itk_component(centerDialog) hide 1
    $itk_component(centerDialog) hide 3
    $itk_component(centerDialog) configure \
	-thickness 2 \
	-buttonboxpady 0
    $itk_component(centerDialog) buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0
    $itk_component(centerDialog) buttonconfigure 2 \
	-borderwidth 1 \
	-pady 0

    # ITCL can be nasty
    set win [$itk_component(centerDialog) component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$itk_component(centerDialog) childsite]

    itk_component add centerDialogXL {
	::label $parent.xl \
	    -text "X:"
    } {}
    set hbc [$itk_component(centerDialogXL) cget -background]
    itk_component add centerDialogXE {
	::entry $parent.xe \
	    -width 12 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope _centerX] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateDouble %P]
    } {}
    itk_component add centerDialogXUL {
	::label $parent.xul \
	    -textvariable [::itcl::scope mDbUnits]
    } {}
    itk_component add centerDialogYL {
	::label $parent.yl \
	    -text "Y:"
    } {}
    itk_component add centerDialogYE {
	::entry $parent.ye \
	    -width 12 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope _centerY] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateDouble %P]
    } {}
    itk_component add centerDialogYUL {
	::label $parent.yul \
	    -textvariable [::itcl::scope mDbUnits]
    } {}
    itk_component add centerDialogZL {
	::label $parent.zl \
	    -text "Z:"
    } {}
    itk_component add centerDialogZE {
	::entry $parent.ze \
	    -width 12 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope _centerZ] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateDouble %P]
    } {}
    itk_component add centerDialogZUL {
	::label $parent.zul \
	    -textvariable [::itcl::scope mDbUnits]
    } {}

    after idle "$itk_component(centerDialog) center"

    set col 0
    set row 0
    grid $itk_component(centerDialogXL) -row $row -column $col
    incr col
    grid $itk_component(centerDialogXE) -row $row -column $col -sticky ew
    grid columnconfigure $parent $col -weight 1
    incr col
    grid $itk_component(centerDialogXUL) -row $row -column $col

    set col 0
    incr row
    grid $itk_component(centerDialogYL) -row $row -column $col
    incr col
    grid $itk_component(centerDialogYE) -row $row -column $col -sticky ew
    grid columnconfigure $parent $col -weight 1
    incr col
    grid $itk_component(centerDialogYUL) -row $row -column $col

    set col 0
    incr row
    grid $itk_component(centerDialogZL) -row $row -column $col
    incr col
    grid $itk_component(centerDialogZE) -row $row -column $col -sticky ew
    grid columnconfigure $parent $col -weight 1
    incr col
    grid $itk_component(centerDialogZUL) -row $row -column $col

    wm geometry $itk_component(centerDialog) "275x125"
}

::itcl::body Archer::_build_preferences_dialog {} {
    itk_component add preferencesDialog {
	::iwidgets::dialog $itk_interior.preferencesDialog \
	    -modality application \
	    -title "Preferences"
    } {}
    $itk_component(preferencesDialog) hide 3
    $itk_component(preferencesDialog) configure \
	-thickness 2 \
	-buttonboxpady 0
    $itk_component(preferencesDialog) buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0
    $itk_component(preferencesDialog) buttonconfigure 1 \
	-borderwidth 1 \
	-pady 0 \
	-command [::itcl::code $this _apply_preferences_if_diff]
    $itk_component(preferencesDialog) buttonconfigure 2 \
	-borderwidth 1 \
	-pady 0

    grid [$itk_component(preferencesDialog) component bbox] -sticky e

    # ITCL can be nasty
    set win [$itk_component(preferencesDialog) component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$itk_component(preferencesDialog) childsite]
    itk_component add preferenceTabs {
	blt::tabnotebook $parent.tabs \
	    -side left \
	    -relief flat \
	    -tiers 99 \
	    -tearoff 0 \
	    -gap 3 \
	    -width 0 \
	    -height 0 \
	    -selectforeground black
    } {}

    $itk_component(preferenceTabs) insert end -text "General" -stipple gray25
    $itk_component(preferenceTabs) insert end -text "Model Axes" -stipple gray25
    $itk_component(preferenceTabs) insert end -text "View Axes" -stipple gray25
    $itk_component(preferenceTabs) insert end -text "Ground Plane" -stipple gray25
    $itk_component(preferenceTabs) insert end -text "Display" -stipple gray25

    set i 0
    _build_general_preferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(generalLF) -fill both

    incr i
    _build_model_axes_preferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(modelAxesF) -fill both

    incr i
    _build_view_axes_preferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(viewAxesF) -fill both

    incr i
    _build_ground_plane_preferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(groundPlaneF) -fill both

    incr i
    _build_display_preferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(displayLF) -fill both

    pack $itk_component(preferenceTabs) -expand yes -fill both

    wm geometry $itk_component(preferencesDialog) 450x500
}

::itcl::body Archer::_build_general_preferences {} {
    itk_component add generalLF {
	::iwidgets::Labeledframe $itk_component(preferenceTabs).generalLF \
		-labeltext "User Interface" \
		-labelpos nw -borderwidth 2 -relief groove
    }

    set parent [$itk_component(generalLF) childsite]
    itk_component add generalF {
	::frame $parent.generalF
    } {}

    itk_component add bindingL {
	::label $itk_component(generalF).bindingL \
	    -text "Display Bindings:"
    } {}

    set i 0
    set mBindingMode $i
    itk_component add defaultBindingRB {
	::radiobutton $itk_component(generalF).defaultBindingRB \
	    -text "Default" \
	    -value $i \
	    -variable [::itcl::scope mBindingModePref]
    } {}

    incr i
    itk_component add brlcadBindingRB {
	::radiobutton $itk_component(generalF).brlcadBindingRB \
	    -text "BRL-CAD" \
	    -value $i \
	    -variable [::itcl::scope mBindingModePref]
    } {}

    itk_component add measuringL {
	::label $itk_component(generalF).measuringL \
	    -text "Measuring Stick Mode:"
    } {}

    set i 0
    set mMeasuringStickMode $i
    itk_component add topMeasuringStickRB {
	::radiobutton $itk_component(generalF).topMeasuringStickRB \
	    -text "Default (Top)" \
	    -value $i \
	    -variable [::itcl::scope mMeasuringStickModePref]
    } {}

    incr i
    itk_component add embeddedMeasuringStickRB {
	::radiobutton $itk_component(generalF).embeddedMeasuringStickRB \
	    -text "Embedded" \
	    -value $i \
	    -variable [::itcl::scope mMeasuringStickModePref]
    } {}

    itk_component add backgroundColorL {
	::label $itk_component(generalF).backgroundColorL \
	    -text "Background Color:"
    }
    itk_component add backgroundColorF {
	::frame $itk_component(generalF).backgroundColorF
    } {}
    _build_background_color $itk_component(backgroundColorF)

    _build_combo_box $itk_component(generalF) \
	primitiveLabelColor \
	color \
	mPrimitiveLabelColorPref \
	"Primitive Label Color:" \
	$colorListNoTriple

    _build_combo_box $itk_component(generalF) \
	scaleColor \
	scolor \
	mScaleColorPref \
	"Scale Color:" \
	$colorListNoTriple

    _build_combo_box $itk_component(generalF) \
	measuringStickColor \
	mcolor \
	mMeasuringStickColorPref \
	"Measuring Stick Color:" \
	$colorListNoTriple

    _build_combo_box $itk_component(generalF) \
	viewingParamsColor \
	vcolor \
	mViewingParamsColorPref \
	"Viewing Parameters Color:" \
	$colorListNoTriple

    set tmp_themes [glob [file join $_imgdir Themes *]]
    set themes {}
    foreach theme $tmp_themes {
	set theme [file tail $theme]

	# This is not needed for the released code.
	# However, it won't hurt anything to leave it.
	if {$theme != "CVS"} {
	    lappend themes $theme
	}
    }
    _build_combo_box $itk_component(generalF) \
	themes \
	themes \
	mThemePref \
	"Themes:" \
	$themes

    itk_component add bigEMenuItemCB {
	::checkbutton $itk_component(generalF).bigECB \
	    -text "Enable Evaluate Menu Item (Experimental)" \
	    -variable [::itcl::scope mEnableBigEPref]
    } {}

    set i 0
    grid $itk_component(bindingL) -column 0 -row $i -sticky e
    grid $itk_component(defaultBindingRB) -column 1 -row $i -sticky w
    incr i
    grid $itk_component(brlcadBindingRB) -column 1 -row $i -sticky w
    incr i
    grid $itk_component(measuringL) -column 0 -row $i -sticky e
    grid $itk_component(topMeasuringStickRB) -column 1 -row $i -sticky w
    incr i
    grid $itk_component(embeddedMeasuringStickRB) -column 1 -row $i -sticky w
    incr i
    grid $itk_component(backgroundColorL) -column 0 -row $i -sticky ne
    grid $itk_component(backgroundColorF) -column 1 -row $i -sticky w
    incr i
    grid $itk_component(primitiveLabelColorL) -column 0 -row $i -sticky e
    grid $itk_component(primitiveLabelColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(scaleColorL) -column 0 -row $i -sticky e
    grid $itk_component(scaleColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(measuringStickColorL) -column 0 -row $i -sticky e
    grid $itk_component(measuringStickColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(viewingParamsColorL) -column 0 -row $i -sticky e
    grid $itk_component(viewingParamsColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(themesL) -column 0 -row $i -sticky ne
    grid $itk_component(themesF) -column 1 -row $i -sticky w
    incr i
    grid $itk_component(bigEMenuItemCB) \
	-columnspan 2 \
	-column 0 \
	-row $i \
	-sticky sw
    grid rowconfigure $itk_component(generalF) $i -weight 1

    set i 0
    grid $itk_component(generalF) -column 0 -row $i -sticky nsew

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body Archer::_build_background_color {parent} {
    itk_component add backgroundRedL {
	::label $parent.redl \
	    -anchor e \
	    -text "Red:"
    } {}

    set hbc [$itk_component(backgroundRedL) cget -background]
    itk_component add backgroundRedE {
	::entry $parent.rede \
	    -width 3 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope mBackgroundRedPref] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateColorComp %P]
    } {}
    itk_component add backgroundGreenL {
	::label $parent.greenl \
	    -anchor e \
	    -text "Green:"
    } {}
    itk_component add backgroundGreenE {
	::entry $parent.greene \
	    -width 3 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope mBackgroundGreenPref] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateColorComp %P]
    } {}
    itk_component add backgroundBlueL {
	::label $parent.bluel \
	    -anchor e \
	    -text "Blue:"
    } {}
    itk_component add backgroundBlueE {
	::entry $parent.bluee \
	    -width 3 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope mBackgroundBluePref] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateColorComp %P]
    } {}

    set row 0
    grid $itk_component(backgroundRedL) -row $row -column 0 -sticky ew
    grid $itk_component(backgroundRedE) -row $row -column 1 -sticky ew
    incr row
    grid $itk_component(backgroundGreenL) -row $row -column 0 -sticky ew
    grid $itk_component(backgroundGreenE) -row $row -column 1 -sticky ew
    incr row
    grid $itk_component(backgroundBlueL) -row $row -column 0 -sticky ew
    grid $itk_component(backgroundBlueE) -row $row -column 1 -sticky ew

    grid columnconfigure $parent 1 -weight 1
}

::itcl::body Archer::_build_model_axes_preferences {} {
    itk_component add modelAxesF {
	::iwidgets::Labeledframe $itk_component(preferenceTabs).modelAxesF \
	    -labeltext "Model Axes Settings" \
	    -labelpos nw \
	    -borderwidth 2 \
	    -relief groove
    }

    set parent [$itk_component(modelAxesF) childsite]
    itk_component add modelAxesF2 {
	::frame $parent.modelAxesF
    } {}

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesSize \
	size \
	mModelAxesSizePref \
	"Size:" \
	{Small Medium Large X-Large \
	 "View (1x)" "View (2x)" "View (4x)" "View (8x)"}

#    itk_component add modelAxesPositionL {
#	::label $itk_component(modelAxesF2).positionL \
#	    -text "Position:"
#    }
#    itk_component add modelAxesPositionF {
#	::frame $itk_component(modelAxesF2).positionF
#    } {}
#    _build_model_axes_position $itk_component(modelAxesPositionF)
    _build_model_axes_position $itk_component(modelAxesF2)

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesLineWidth \
	lineWidth \
	mModelAxesLineWidthPref \
	"Line Width:" \
	{1 2 3}

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesColor \
	color \
	mModelAxesColorPref \
	"Axes Color:" \
	$colorList

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesLabelColor \
	labelColor \
	mModelAxesLabelColorPref \
	"Label Color:" \
	$colorListNoTriple

    itk_component add modelAxesTickIntervalL {
	::label $itk_component(modelAxesF2).tickIntervalL \
	    -text "Tick Interval:"
    } {}
    set hbc [$itk_component(modelAxesTickIntervalL) cget -background]
    itk_component add modelAxesTickIntervalE {
	::entry $itk_component(modelAxesF2).tickIntervalE \
	    -textvariable [::itcl::scope mModelAxesTickIntervalPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this _validateTickInterval %P] \
	    -background $SystemWindow \
	    -highlightbackground $hbc
    } {}

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesTicksPerMajor \
	ticksPerMajor \
	mModelAxesTicksPerMajorPref \
	"Ticks Per Major:" \
	{2 3 4 5 6 8 10 12}

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesTickThreshold \
	tickThreshold \
	mModelAxesTickThresholdPref \
	"Tick Threshold:" \
	{4 8 16 32 64}

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesTickLength \
	tickLength \
	mModelAxesTickLengthPref \
	"Tick Length:" \
	{2 4 8 16}

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesTickMajorLength \
	tickMajorLength \
	mModelAxesTickMajorLengthPref \
	"Major Tick Length:" \
	{2 4 8 16}

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesTickColor \
	tickColor \
	mModelAxesTickColorPref \
	"Tick Color:" \
	$colorListNoTriple

    _build_combo_box $itk_component(modelAxesF2) \
	modelAxesTickMajorColor \
	tickMajorColor \
	mModelAxesTickMajorColorPref \
	"Major Tick Color:" \
	$colorListNoTriple

    set i 0
    grid $itk_component(modelAxesSizeL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesSizeF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesPositionXL) -row $i -column 0 -sticky e
    grid $itk_component(modelAxesPositionXE) -row $i -column 1 -sticky ew
    incr i
    grid $itk_component(modelAxesPositionYL) -row $i -column 0 -sticky e
    grid $itk_component(modelAxesPositionYE) -row $i -column 1 -sticky ew
    incr i
    grid $itk_component(modelAxesPositionZL) -row $i -column 0 -sticky e
    grid $itk_component(modelAxesPositionZE) -row $i -column 1 -sticky ew
#    incr i
#    grid $itk_component(modelAxesPositionL) -column 0 -row $i -sticky ne
#    grid $itk_component(modelAxesPositionF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesLineWidthL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesLineWidthF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesColorL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesLabelColorL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesLabelColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesTickIntervalL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesTickIntervalE) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesTicksPerMajorL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesTicksPerMajorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesTickThresholdL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesTickThresholdF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesTickLengthL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesTickLengthF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesTickMajorLengthL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesTickMajorLengthF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesTickColorL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesTickColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(modelAxesTickMajorColorL) -column 0 -row $i -sticky e
    grid $itk_component(modelAxesTickMajorColorF) -column 1 -row $i -sticky ew

    set i 0
    grid $itk_component(modelAxesF2) -column 0 -row $i -sticky nw

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body Archer::_build_model_axes_position {parent} {
    itk_component add modelAxesPositionXL {
	::label $parent.xl \
	    -anchor e \
	    -text "Position X:"
    } {}

    set hbc [$itk_component(modelAxesPositionXL) cget -background]
    itk_component add modelAxesPositionXE {
	::entry $parent.xe \
	    -width 12 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope mModelAxesPositionXPref] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateDouble %P]
    } {}
    itk_component add modelAxesPositionYL {
	::label $parent.yl \
	    -anchor e \
	    -text "Position Y:"
    } {}
    itk_component add modelAxesPositionYE {
	::entry $parent.ye \
	    -width 12 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope mModelAxesPositionYPref] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateDouble %P]
    } {}
    itk_component add modelAxesPositionZL {
	::label $parent.zl \
	    -anchor e \
	    -text "Position Z:"
    } {}
    itk_component add modelAxesPositionZE {
	::entry $parent.ze \
	    -width 12 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope mModelAxesPositionZPref] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateDouble %P]
    } {}

#    set row 0
#    grid $itk_component(modelAxesPositionXL) -row $row -column 0
#    grid $itk_component(modelAxesPositionXE) -row $row -column 1 -sticky ew
#    incr row
#    grid $itk_component(modelAxesPositionYL) -row $row -column 0
#    grid $itk_component(modelAxesPositionYE) -row $row -column 1 -sticky ew
#    incr row
#    grid $itk_component(modelAxesPositionZL) -row $row -column 0
#    grid $itk_component(modelAxesPositionZE) -row $row -column 1 -sticky ew
#
#    grid columnconfigure $parent 1 -weight 1
}

::itcl::body Archer::_build_view_axes_preferences {} {
    itk_component add viewAxesF {
	::iwidgets::Labeledframe $itk_component(preferenceTabs).viewAxesF \
	    -labeltext "View Axes Settings" \
	    -labelpos nw \
	    -borderwidth 2 \
	    -relief groove
    }

    set parent [$itk_component(viewAxesF) childsite]
    itk_component add viewAxesF2 {
	::frame $parent.viewAxesF
    } {}

    _build_combo_box $itk_component(viewAxesF2) \
	viewAxesSize \
	size \
	mViewAxesSizePref \
	"Size:" \
	{Small Medium Large X-Large}

    _build_combo_box $itk_component(viewAxesF2) \
	viewAxesPosition \
	position \
	mViewAxesPositionPref \
	"Position:" \
	{Center "Upper Left" "Upper Right" "Lower Left" "Lower Right"}

    _build_combo_box $itk_component(viewAxesF2) \
	viewAxesLineWidth \
	lineWidth \
	mViewAxesLineWidthPref \
	"Line Width:" \
	{1 2 3}

    _build_combo_box $itk_component(viewAxesF2) \
	viewAxesColor \
	color \
	mViewAxesColorPref \
	"Axes Color:" \
	$colorList

    _build_combo_box $itk_component(viewAxesF2) \
	viewAxesLabelColor \
	labelColor \
	mViewAxesLabelColorPref \
	"Label Color:" \
	$colorListNoTriple

    set i 0
    grid $itk_component(viewAxesSizeL) -column 0 -row $i -sticky e
    grid $itk_component(viewAxesSizeF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(viewAxesPositionL) -column 0 -row $i -sticky e
    grid $itk_component(viewAxesPositionF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(viewAxesLineWidthL) -column 0 -row $i -sticky e
    grid $itk_component(viewAxesLineWidthF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(viewAxesColorL) -column 0 -row $i -sticky e
    grid $itk_component(viewAxesColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(viewAxesLabelColorL) -column 0 -row $i -sticky e
    grid $itk_component(viewAxesLabelColorF) -column 1 -row $i -sticky ew

    set i 0
    grid $itk_component(viewAxesF2) -column 0 -row $i -sticky nw

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body Archer::_build_ground_plane_preferences {} {
    itk_component add groundPlaneF {
	::iwidgets::Labeledframe $itk_component(preferenceTabs).groundPlaneF \
	    -labeltext "Ground Plane Settings" \
	    -labelpos nw \
	    -borderwidth 2 \
	    -relief groove
    }

    set parent [$itk_component(groundPlaneF) childsite]
    itk_component add groundPlaneF2 {
	::frame $parent.groundPlaneF
    } {}

    itk_component add groundPlaneSizeL {
	::label $itk_component(groundPlaneF2).sizeL \
	    -anchor e \
	    -text "Square Size:"
    } {}
    set hbc [$itk_component(groundPlaneSizeL) cget -background]
    itk_component add groundPlaneSizeE {
	::entry $itk_component(groundPlaneF2).sizeE \
	    -width 12 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope mGroundPlaneSizePref] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateDouble %P]
    } {}
    itk_component add groundPlaneSizeUnitsL {
	::label $itk_component(groundPlaneF2).sizeUnitsL \
	    -anchor e \
	    -text "mm"
    } {}

    itk_component add groundPlaneIntervalL {
	::label $itk_component(groundPlaneF2).intervalL \
	    -anchor e \
	    -text "Line Interval:"
    } {}
    set hbc [$itk_component(groundPlaneIntervalL) cget -background]
    itk_component add groundPlaneIntervalE {
	::entry $itk_component(groundPlaneF2).intervalE \
	    -width 12 \
	    -background $SystemWindow \
	    -highlightbackground $hbc \
	    -textvariable [::itcl::scope mGroundPlaneIntervalPref] \
	    -validate key \
	    -vcmd [::itcl::code $this _validateDouble %P]
    } {}
    itk_component add groundPlaneIntervalUnitsL {
	::label $itk_component(groundPlaneF2).intervalUnitsL \
	    -anchor e \
	    -text "mm"
    } {}

    _build_combo_box $itk_component(groundPlaneF2) \
	groundPlaneMajorColor \
	majorColor \
	mGroundPlaneMajorColorPref \
	"Major Color:" \
	$colorListNoTriple

    _build_combo_box $itk_component(groundPlaneF2) \
	groundPlaneMinorColor \
	minorColor \
	mGroundPlaneMinorColorPref \
	"Minor Color:" \
	$colorListNoTriple

    set i 0
    grid $itk_component(groundPlaneSizeL) -column 0 -row $i -sticky e
    grid $itk_component(groundPlaneSizeE) -column 1 -row $i -sticky ew
    grid $itk_component(groundPlaneSizeUnitsL) -column 2 -row $i -sticky ew
    incr i
    grid $itk_component(groundPlaneIntervalL) -column 0 -row $i -sticky e
    grid $itk_component(groundPlaneIntervalE) -column 1 -row $i -sticky ew
    grid $itk_component(groundPlaneIntervalUnitsL) -column 2 -row $i -sticky ew
    incr i
    grid $itk_component(groundPlaneMajorColorL) -column 0 -row $i -sticky e
    grid $itk_component(groundPlaneMajorColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(groundPlaneMinorColorL) -column 0 -row $i -sticky e
    grid $itk_component(groundPlaneMinorColorF) -column 1 -row $i -sticky ew

    set i 0
    grid $itk_component(groundPlaneF2) -column 0 -row $i -sticky nw

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1
}

::itcl::body Archer::_build_display_preferences {} {
    itk_component add displayLF {
	::iwidgets::Labeledframe $itk_component(preferenceTabs).displayLF \
	    -labeltext "Display Settings" \
	    -labelpos nw \
	    -borderwidth 2 \
	    -relief groove
    }

    set oglParent [$itk_component(displayLF) childsite]

    itk_component add displayF {
	::frame $oglParent.displayF
    } {}

    set parent $itk_component(displayF)

    itk_component add zclipL {
	::label $parent.zclipL \
	    -text "Viewing Cube:"
    } {}

    set i 0
    set mZClipMode $i
    itk_component add smallZClipRB {
	::radiobutton $parent.smallZClipRB \
	    -text "Small (Default)" \
	    -value $i \
	    -variable [::itcl::scope mZClipModePref]
    } {}

    incr i
    itk_component add mediumZClipRB {
	::radiobutton $parent.mediumZClipRB \
	    -text "Medium" \
	    -value $i \
	    -variable [::itcl::scope mZClipModePref]
    } {}

    incr i
    itk_component add largeZClipRB {
	::radiobutton $parent.largeZClipRB \
	    -text "Large" \
	    -value $i \
	    -variable [::itcl::scope mZClipModePref]
    } {}

    incr i
    itk_component add infiniteZClipRB {
	::radiobutton $parent.infiniteZClipRB \
	    -text "Infinite" \
	    -value $i \
	    -variable [::itcl::scope mZClipModePref]
    } {}

    set i 0
    grid $itk_component(zclipL) -column 0 -row $i -sticky w
    grid $itk_component(smallZClipRB) -column 1 -row $i -sticky w
    incr i
    grid $itk_component(mediumZClipRB) -column 1 -row $i -sticky w
    incr i
    grid $itk_component(largeZClipRB) -column 1 -row $i -sticky w
    incr i
    grid $itk_component(infiniteZClipRB) -column 1 -row $i -sticky w

    incr i
    grid rowconfigure $parent $i -weight 1
    grid columnconfigure $parent 1 -weight 1

    set i 0
    grid $parent -column 0 -row $i -sticky nsew

    grid rowconfigure $oglParent 0 -weight 1
    grid columnconfigure $oglParent 0 -weight 1
}

::itcl::body Archer::_build_combo_box {parent name1 name2 varName text listOfChoices} {
    itk_component add $name1\L {
	::label $parent.$name2\L \
	    -text $text
    } {}

    set hbc [$itk_component($name1\L) cget -background]

    itk_component add $name1\F {
	::frame $parent.$name2\F \
	    -relief sunken \
	    -bd 2
    } {}

    set listHeight [expr [llength $listOfChoices] * 19]
    itk_component add $name1\CB {
	::iwidgets::combobox $itk_component($name1\F).$name2\CB \
	    -editable false \
	    -textvariable [::itcl::scope $varName] \
	    -listheight $listHeight \
	    -background $SystemWindow \
	    -textbackground $SystemWindow \
	    -relief flat
    } {}
    #XXX I wouldn't have to break encapsulation if they'd make better widgets!
    #    Yeah, I could tweak the combobox. But, then I'd have to manage the mods
    #    of their code. It's easier to do it this way.
    $itk_component($name1\CB) component entry configure \
	-disabledbackground $SystemWindow \
	-disabledforeground $SystemWindowText
    eval $itk_component($name1\CB) insert list end $listOfChoices

    $itk_component($name1\CB) component arrowBtn configure \
	-background $hbc \
	-highlightbackground $hbc
    pack $itk_component($name1\CB) -expand yes -fill both
}

::itcl::body Archer::_validateDigit {d} {
    if {[string is digit $d]} {
	return 1
    }

    return 0
}

::itcl::body Archer::_validateDouble {d} {
    if {$d == "-" || [string is double $d]} {
	return 1
    }

    return 0
}

::itcl::body Archer::_validateTickInterval {ti} {
    if {$ti == ""} {
	return 1
    }

    if {[string is double $ti]} {
	if {0 <= $ti} {
	    return 1
	}

	return 0
    } else {
	return 0
    }
}

::itcl::body Archer::_validateColorComp {c} {
    if {$c == ""} {
	return 1
    }

    if {[string is digit $c]} {
	if {$c <= 255} {
	    return 1
	}

	return 0
    } else {
	return 0
    }
}

::itcl::body Archer::_do_about_archer {} {
    bind $itk_component(aboutDialog) <Enter> "raise $itk_component(aboutDialog)"
    bind $itk_component(aboutDialog) <Configure> "raise $itk_component(aboutDialog)"
    bind $itk_component(aboutDialog) <FocusOut> "raise $itk_component(aboutDialog)"

    $itk_component(aboutDialog) center [namespace tail $this]
    $itk_component(aboutDialog) activate
}

::itcl::body Archer::_do_mouse_overrides {} {
#    $itk_component(mouseOverridesDialog) center [namespace tail $this]
#    update
    $itk_component(mouseOverridesDialog) center [namespace tail $this]
    $itk_component(mouseOverridesDialog) activate
}

::itcl::body Archer::doArcherHelp {} {
    global env
    global tcl_platform

    if {$tcl_platform(os) == "Windows NT"} {
#	exec hh [file join $env(ARCHER_HOME) $brlcadDataPath doc html manuals archer Archer_Documentation.chm] &
	exec hh [file join $brlcadDataPath doc html manuals archer Archer_Documentation.chm] &
    }
}

::itcl::body Archer::_background_color {r g b} {
    set mBackground [list $r $g $b]

    if {![info exists itk_component(mged)] &&
    ![info exists itk_component(sdb)]} {
	set color [getTkColor \
		[lindex $mBackground 0] \
		[lindex $mBackground 1] \
		[lindex $mBackground 2]]
	$itk_component(canvas) configure -bg $color
    } else {
	eval dbCmd bgAll $mBackground
    }
}

::itcl::body Archer::_do_preferences {} {
    # update preference variables
    set mZClipModePref $mZClipMode

    set mBindingModePref $mBindingMode
    set mMeasuringStickModePref $mMeasuringStickMode
    set mBackgroundRedPref [lindex $mBackground 0]
    set mBackgroundGreenPref [lindex $mBackground 1]
    set mBackgroundBluePref [lindex $mBackground 2]
    set mPrimitiveLabelColorPref $mPrimitiveLabelColor
    set mScaleColorPref $mScaleColor
    set mMeasuringStickColorPref $mMeasuringStickColor
    set mViewingParamsColorPref $mViewingParamsColor
    set mThemePref $mTheme
    set mEnableBigEPref $mEnableBigE

    set mGroundPlaneSizePref $mGroundPlaneSize
    set mGroundPlaneIntervalPref $mGroundPlaneInterval
    set mGroundPlaneMajorColorPref $mGroundPlaneMajorColor
    set mGroundPlaneMinorColorPref $mGroundPlaneMinorColor

    set mViewAxesSizePref $mViewAxesSize
    set mViewAxesPositionPref $mViewAxesPosition
    set mViewAxesLineWidthPref $mViewAxesLineWidth
    set mViewAxesColorPref $mViewAxesColor
    set mViewAxesLabelColorPref $mViewAxesLabelColor

    set mModelAxesSizePref $mModelAxesSize
    set mModelAxesPositionXPref [lindex $mModelAxesPosition 0]
    set mModelAxesPositionYPref [lindex $mModelAxesPosition 1]
    set mModelAxesPositionZPref [lindex $mModelAxesPosition 2]
    set mModelAxesLineWidthPref $mModelAxesLineWidth
    set mModelAxesColorPref $mModelAxesColor
    set mModelAxesLabelColorPref $mModelAxesLabelColor

    set mModelAxesTickIntervalPref $mModelAxesTickInterval
    set mModelAxesTicksPerMajorPref $mModelAxesTicksPerMajor
    set mModelAxesTickThresholdPref $mModelAxesTickThreshold
    set mModelAxesTickLengthPref $mModelAxesTickLength
    set mModelAxesTickMajorLengthPref $mModelAxesTickMajorLength
    set mModelAxesTickColorPref $mModelAxesTickColor
    set mModelAxesTickMajorColorPref $mModelAxesTickMajorColor

    $itk_component(preferencesDialog) center [namespace tail $this]
    if {[$itk_component(preferencesDialog) activate]} {
	_apply_preferences_if_diff
    }
}

::itcl::body Archer::_apply_preferences_if_diff {} {
    _apply_general_preferences_if_diff
    _apply_view_axes_preferences_if_diff
    _apply_model_axes_preferences_if_diff
    _apply_ground_plane_preferences_if_diff
    _apply_display_preferences_if_diff
}

::itcl::body Archer::_apply_general_preferences_if_diff {} {
    if {$mBindingModePref != $mBindingMode} {
	set mBindingMode $mBindingModePref
	switch -- $mBindingMode {
	    0 {
		_init_default_bindings
	    }
	    1 {
		_init_brlcad_bindings
	    }
	}
    }

    if {$mMeasuringStickModePref != $mMeasuringStickMode} {
	set mMeasuringStickMode $mMeasuringStickModePref
    }

    set r [lindex $mBackground 0]
    set g [lindex $mBackground 1]
    set b [lindex $mBackground 2]
    if {$mBackgroundRedPref == ""} {
	set mBackgroundRedPref 0
    }
    if {$mBackgroundGreenPref == ""} {
	set mBackgroundGreenPref 0
    }
    if {$mBackgroundBluePref == ""} {
	set mBackgroundBluePref 0
    }

    if {$mBackgroundRedPref != $r ||
	$mBackgroundGreenPref != $g ||
	$mBackgroundBluePref != $b} {
	
        _background_color $mBackgroundRedPref $mBackgroundGreenPref $mBackgroundBluePref 
    }

    if {$mPrimitiveLabelColor != $mPrimitiveLabelColorPref} {
	set mPrimitiveLabelColor $mPrimitiveLabelColorPref
	_set_color_option dbCmd -primitiveLabelColor $mPrimitiveLabelColor
    }

    if {$mViewingParamsColor != $mViewingParamsColorPref} {
	set mViewingParamsColor $mViewingParamsColorPref
	_set_color_option dbCmd -viewingParamsColor $mViewingParamsColor
    }

    if {$mScaleColor != $mScaleColorPref} {
	set mScaleColor $mScaleColorPref
	_set_color_option dbCmd -scaleColor $mScaleColor
    }

    if {$mMeasuringStickColor != $mMeasuringStickColorPref} {
	set mMeasuringStickColor $mMeasuringStickColorPref
    }

    if {$mTheme != $mThemePref} {
	set mTheme $mThemePref
	_update_theme
    }

    if {$mEnableBigEPref != $mEnableBigE} {
	set mEnableBigE $mEnableBigEPref
    }
}

::itcl::body Archer::_apply_view_axes_preferences_if_diff {} {

    set positionNotSet 1
    if {$mViewAxesSizePref != $mViewAxesSize} {
	set mViewAxesSize $mViewAxesSizePref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    # sanity
	    set offset 0.0
	    switch -- $mViewAxesSize {
		"Small" {
		    set offset 0.85
		    dbCmd configure -viewAxesSize 0.2
		}
		"Medium" {
		    set offset 0.75
		    dbCmd configure -viewAxesSize 0.4
		}
		"Large" {
		    set offset 0.55
		    dbCmd configure -viewAxesSize 0.8
		}
		"X-Large" {
		    set offset 0.0
		    dbCmd configure -viewAxesSize 1.6
		}
	    }
	}

	set positionNotSet 0
	set mViewAxesPosition $mViewAxesPositionPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    switch -- $mViewAxesPosition {
		default -
		"Center" {
		    dbCmd configure -viewAxesPosition {0 0 0}
		}
		"Upper Left" {
		    dbCmd configure -viewAxesPosition "-$offset $offset 0"
		}
		"Upper Right" {
		    dbCmd configure -viewAxesPosition "$offset $offset 0"
		}
		"Lower Left" {
		    dbCmd configure -viewAxesPosition "-$offset -$offset 0"
		}
		"Lower Right" {
		    dbCmd configure -viewAxesPosition "$offset -$offset 0"
		}
	    }
	}
    }

    if {$positionNotSet &&
	$mViewAxesPositionPref != $mViewAxesPosition} {
	set mViewAxesPosition $mViewAxesPositionPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    # sanity
	    set offset 0.0
	    switch -- $mViewAxesSize {
		"Small" {
		    set offset 0.85
		}
		"Medium" {
		    set offset 0.75
		}
		"Large" {
		    set offset 0.55
		}
		"X-Large" {
		    set offset 0.0
		}
	    }

	    switch -- $mViewAxesPosition {
		default -
		"Center" {
		    dbCmd configure -viewAxesPosition {0 0 0}
		}
		"Upper Left" {
		    dbCmd configure -viewAxesPosition "-$offset $offset 0"
		}
		"Upper Right" {
		    dbCmd configure -viewAxesPosition "$offset $offset 0"
		}
		"Lower Left" {
		    dbCmd configure -viewAxesPosition "-$offset -$offset 0"
		}
		"Lower Right" {
		    dbCmd configure -viewAxesPosition "$offset -$offset 0"
		}
	    }
	}
    }

    if {$mViewAxesLineWidthPref != $mViewAxesLineWidth} {
	set mViewAxesLineWidth $mViewAxesLineWidthPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    dbCmd configure -viewAxesLineWidth $mViewAxesLineWidth
	}
    }

    if {$mViewAxesColorPref != $mViewAxesColor} {
	set mViewAxesColor $mViewAxesColorPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    _set_color_option dbCmd -viewAxesColor $mViewAxesColor -viewAxesTripleColor
	}
    }

    if {$mViewAxesLabelColorPref != $mViewAxesLabelColor} {
	set mViewAxesLabelColor $mViewAxesLabelColorPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    _set_color_option dbCmd -viewAxesLabelColor $mViewAxesLabelColor
	}
    }
}

::itcl::body Archer::_apply_model_axes_preferences_if_diff {} {
    if {$mModelAxesSizePref != $mModelAxesSize} {
	set mModelAxesSize $mModelAxesSizePref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    switch -- $mModelAxesSize {
		"Small" {
		    dbCmd configure -modelAxesSize 0.2
		}
		"Medium" {
		    dbCmd configure -modelAxesSize 0.4
		}
		"Large" {
		    dbCmd configure -modelAxesSize 0.8
		}
		"X-Large" {
		    dbCmd configure -modelAxesSize 1.6
		}
		"View (1x)" {
		    dbCmd configure -modelAxesSize 2.0
		}
		"View (2x)" {
		    dbCmd configure -modelAxesSize 4.0
		}
		"View (4x)" {
		    dbCmd configure -modelAxesSize 8.0
		}
		"View (8x)" {
		    dbCmd configure -modelAxesSize 16.0
		}
	    }
	}
    }

    set X [lindex $mModelAxesPosition 0]
    set Y [lindex $mModelAxesPosition 0]
    set Z [lindex $mModelAxesPosition 0]
    if {$mModelAxesPositionXPref != $X ||
	$mModelAxesPositionYPref != $Y ||
	$mModelAxesPositionZPref != $Z} {
	set mModelAxesPosition \
	    "$mModelAxesPositionXPref $mModelAxesPositionYPref $mModelAxesPositionZPref"

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    dbCmd configure -modelAxesPosition $mModelAxesPosition
	}
    }

    if {$mModelAxesLineWidthPref != $mModelAxesLineWidth} {
	set mModelAxesLineWidth $mModelAxesLineWidthPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    dbCmd configure -modelAxesLineWidth $mModelAxesLineWidth
	}
    }

    if {$mModelAxesColorPref != $mModelAxesColor} {
	set mModelAxesColor $mModelAxesColorPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    _set_color_option dbCmd -modelAxesColor $mModelAxesColor -modelAxesTripleColor
	}
    }

    if {$mModelAxesLabelColorPref != $mModelAxesLabelColor} {
	set mModelAxesLabelColor $mModelAxesLabelColorPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    _set_color_option dbCmd -modelAxesLabelColor $mModelAxesLabelColor
	}
    }

    if {$mModelAxesTickIntervalPref != $mModelAxesTickInterval} {
	set mModelAxesTickInterval $mModelAxesTickIntervalPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    dbCmd configure -modelAxesTickInterval $mModelAxesTickInterval
	}
    }

    if {$mModelAxesTicksPerMajorPref != $mModelAxesTicksPerMajor} {
	set mModelAxesTicksPerMajor $mModelAxesTicksPerMajorPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    dbCmd configure -modelAxesTicksPerMajor $mModelAxesTicksPerMajor
	}
    }

    if {$mModelAxesTickThresholdPref != $mModelAxesTickThreshold} {
	set mModelAxesTickThreshold $mModelAxesTickThresholdPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    dbCmd configure -modelAxesTickThreshold $mModelAxesTickThreshold
	}
    }

    if {$mModelAxesTickLengthPref != $mModelAxesTickLength} {
	set mModelAxesTickLength $mModelAxesTickLengthPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    dbCmd configure -modelAxesTickLength $mModelAxesTickLength
	}
    }

    if {$mModelAxesTickMajorLengthPref != $mModelAxesTickMajorLength} {
	set mModelAxesTickMajorLength $mModelAxesTickMajorLengthPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    dbCmd configure -modelAxesTickMajorLength $mModelAxesTickMajorLength
	}
    }

    if {$mModelAxesTickColorPref != $mModelAxesTickColor} {
	set mModelAxesTickColor $mModelAxesTickColorPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    _set_color_option dbCmd -modelAxesTickColor $mModelAxesTickColor
	}
    }

    if {$mModelAxesTickMajorColorPref != $mModelAxesTickMajorColor} {
	set mModelAxesTickMajorColor $mModelAxesTickMajorColorPref

	if {[info exists itk_component(mged)] ||
	    [info exists itk_component(sdb)]} {
	    _set_color_option dbCmd -modelAxesTickMajorColor $mModelAxesTickMajorColor
	}
    }
}

::itcl::body Archer::_apply_ground_plane_preferences_if_diff {} {
    if {$mGroundPlaneSize != $mGroundPlaneSizePref ||
	$mGroundPlaneInterval != $mGroundPlaneIntervalPref ||
	$mGroundPlaneMajorColor != $mGroundPlaneMajorColorPref ||
	$mGroundPlaneMinorColor != $mGroundPlaneMinorColorPref} {

	set mGroundPlaneSize $mGroundPlaneSizePref
	set mGroundPlaneInterval $mGroundPlaneIntervalPref
	set mGroundPlaneMajorColor $mGroundPlaneMajorColorPref
	set mGroundPlaneMinorColor $mGroundPlaneMinorColorPref

	_build_ground_plane
	_show_ground_plane
    }
}

::itcl::body Archer::_apply_display_preferences_if_diff {} {
    if {$mZClipModePref != $mZClipMode} {
	set mZClipMode $mZClipModePref
	updateDisplaySettings
    }
}

::itcl::body Archer::_read_preferences {} {
    global env 

    if {$mViewOnly} {
	return
    }

    if {[info exists env(HOME)]} {
	set home $env(HOME)
    } else {
	set home .
    }

    # Read in the preferences file.
    if {![catch {open [file join $home .archerrc] r} pfile]} {
	set lines [split [read $pfile] "\n"]
	close $pfile

	foreach line $lines {
	    catch {eval $line}
	}

	# Make sure we're backwards compatible
	if {$mTheme == "Crystal (Large)"} {
	    set mTheme "Crystal_Large"
	}
    }

    _background_color [lindex $mBackground 0] \
	    [lindex $mBackground 1] \
	    [lindex $mBackground 2]
    _set_color_option dbCmd -primitiveLabelColor $mPrimitiveLabelColor
    _set_color_option dbCmd -viewingParamsColor $mViewingParamsColor
    _set_color_option dbCmd -scaleColor $mScaleColor

    update
    _set_mode
    _update_theme
    _update_toggle_mode
}

::itcl::body Archer::_write_preferences {} {
    global env 

    if {$mViewOnly} {
	return
    }

    if {[info exists env(HOME)]} {
	set home $env(HOME)
    } else {
	set home .
    }

    _update_hpane_fractions
    _update_vpane_fractions

    # Write the preferences file.
    if {![catch {open [file join $home .archerrc] w} pfile]} {
	puts $pfile "# Archer's Preferences File"
	puts $pfile "# Version 0.7.5"
	puts $pfile "#"
	puts $pfile "# DO NOT EDIT THIS FILE"
	puts $pfile "#"
	puts $pfile "# This file is created and updated by Archer."
	puts $pfile "#"
	puts $pfile "set mLastSelectedDir \"$mLastSelectedDir\""
	puts $pfile "set mBindingMode $mBindingMode"
	puts $pfile "set mMeasuringStickMode $mMeasuringStickMode"
	puts $pfile "set mBackground \"$mBackground\""
	puts $pfile "set mPrimitiveLabelColor \"$mPrimitiveLabelColor\""
	puts $pfile "set mScaleColor \"$mScaleColor\""
	puts $pfile "set mMeasuringStickColor \"$mMeasuringStickColor\""
	puts $pfile "set mViewingParamsColor \"$mViewingParamsColor\""
	puts $pfile "set mTheme \"$mTheme\""
	puts $pfile "set mEnableBigE $mEnableBigE"
	puts $pfile "set mViewAxesSize \"$mViewAxesSize\""
	puts $pfile "set mViewAxesPosition \"$mViewAxesPosition\""
	puts $pfile "set mViewAxesLineWidth $mViewAxesLineWidth"
	puts $pfile "set mViewAxesColor \"$mViewAxesColor\""
	puts $pfile "set mViewAxesLabelColor \"$mViewAxesLabelColor\""
	puts $pfile "set mModelAxesSize \"$mModelAxesSize\""
	puts $pfile "set mModelAxesPosition \"$mModelAxesPosition\""
	puts $pfile "set mModelAxesLineWidth $mModelAxesLineWidth"
	puts $pfile "set mModelAxesColor \"$mModelAxesColor\""
	puts $pfile "set mModelAxesLabelColor \"$mModelAxesLabelColor\""
	puts $pfile "set mModelAxesTickInterval $mModelAxesTickInterval"
	puts $pfile "set mModelAxesTicksPerMajor $mModelAxesTicksPerMajor"
	puts $pfile "set mModelAxesTickThreshold $mModelAxesTickThreshold"
	puts $pfile "set mModelAxesTickLength $mModelAxesTickLength"
	puts $pfile "set mModelAxesTickMajorLength $mModelAxesTickMajorLength"
	puts $pfile "set mModelAxesTickColor \"$mModelAxesTickColor\""
	puts $pfile "set mModelAxesTickMajorColor \"$mModelAxesTickMajorColor\""
	puts $pfile "set mMode $mMode"
	puts $pfile "set mZClipMode $mZClipMode"
	puts $pfile "set mHPaneFraction1 $mHPaneFraction1"
	puts $pfile "set mHPaneFraction2 $mHPaneFraction2"
	puts $pfile "set mVPaneFraction1 $mVPaneFraction1"
	puts $pfile "set mVPaneFraction2 $mVPaneFraction2"
	puts $pfile "set mVPaneFraction3 $mVPaneFraction3"
	puts $pfile "set mVPaneFraction4 $mVPaneFraction4"
	puts $pfile "set mVPaneFraction5 $mVPaneFraction5"
	puts $pfile "set mVPaneToggle1 $mVPaneToggle1"
	puts $pfile "set mVPaneToggle3 $mVPaneToggle3"
	puts $pfile "set mVPaneToggle5 $mVPaneToggle5"
	close $pfile
    } else {
	puts "Failed to write the preferences file:\n$pfile"
    }
}

::itcl::body Archer::_apply_preferences {} {
    # Apply preferences to the cad widget.
    _apply_general_preferences
    _apply_view_axes_preferences
    _apply_model_axes_preferences
    _apply_display_preferences
}

::itcl::body Archer::_apply_general_preferences {} {
    switch -- $mBindingMode {
	0 {
	    _init_default_bindings
	}
	1 {
	    _init_brlcad_bindings
	}
    }

    _background_color [lindex $mBackground 0] \
	    [lindex $mBackground 1] \
	    [lindex $mBackground 2] 
    _set_color_option dbCmd -primitiveLabelColor $mPrimitiveLabelColor
    _set_color_option dbCmd -scaleColor $mScaleColor
    _set_color_option dbCmd -viewingParamsColor $mViewingParamsColor
}

::itcl::body Archer::_apply_display_preferences {} {
    updateDisplaySettings
}

::itcl::body Archer::_apply_view_axes_preferences {} {
    # sanity
    set offset 0.0
    switch -- $mViewAxesSize {
	"Small" {
	    set offset 0.85
	    dbCmd configure -viewAxesSize 0.2
	}
	"Medium" {
	    set offset 0.75
	    dbCmd configure -viewAxesSize 0.4
	}
	"Large" {
	    set offset 0.55
	    dbCmd configure -viewAxesSize 0.8
	}
	"X-Large" {
	    set offset 0.0
	    dbCmd configure -viewAxesSize 1.6
	}
    }

    switch -- $mViewAxesPosition {
	default -
	"Center" {
	    dbCmd configure -viewAxesPosition {0 0 0}
	}
	"Upper Left" {
	    dbCmd configure -viewAxesPosition "-$offset $offset 0"
	}
	"Upper Right" {
	    dbCmd configure -viewAxesPosition "$offset $offset 0"
	}
	"Lower Left" {
	    dbCmd configure -viewAxesPosition "-$offset -$offset 0"
	}
	"Lower Right" {
	    dbCmd configure -viewAxesPosition "$offset -$offset 0"
	}
    }

    dbCmd configure -viewAxesLineWidth $mViewAxesLineWidth

    _set_color_option dbCmd -viewAxesColor $mViewAxesColor -viewAxesTripleColor
    _set_color_option dbCmd -viewAxesLabelColor $mViewAxesLabelColor
}

::itcl::body Archer::_apply_model_axes_preferences {} {
    switch -- $mModelAxesSize {
	"Small" {
	    dbCmd configure -modelAxesSize 0.2
	}
	"Medium" {
	    dbCmd configure -modelAxesSize 0.4
	}
	"Large" {
	    dbCmd configure -modelAxesSize 0.8
	}
	"X-Large" {
	    dbCmd configure -modelAxesSize 1.6
	}
	"View (1x)" {
	    dbCmd configure -modelAxesSize 2.0
	}
	"View (2x)" {
	    dbCmd configure -modelAxesSize 4.0
	}
	"View (4x)" {
	    dbCmd configure -modelAxesSize 8.0
	}
	"View (8x)" {
	    dbCmd configure -modelAxesSize 16.0
	}
    }

    dbCmd configure -modelAxesPosition $mModelAxesPosition
    dbCmd configure -modelAxesLineWidth $mModelAxesLineWidth

    _set_color_option dbCmd -modelAxesColor $mModelAxesColor -modelAxesTripleColor
    _set_color_option dbCmd -modelAxesLabelColor $mModelAxesLabelColor

    dbCmd configure -modelAxesTickInterval $mModelAxesTickInterval
    dbCmd configure -modelAxesTicksPerMajor $mModelAxesTicksPerMajor
    dbCmd configure -modelAxesTickThreshold $mModelAxesTickThreshold
    dbCmd configure -modelAxesTickLength $mModelAxesTickLength
    dbCmd configure -modelAxesTickMajorLength $mModelAxesTickMajorLength

    _set_color_option dbCmd -modelAxesTickColor $mModelAxesTickColor
    _set_color_option dbCmd -modelAxesTickMajorColor $mModelAxesTickMajorColor
}

::itcl::body Archer::_update_hpane_fractions {} {
    if {$mViewOnly} {
	return
    }

    set fractions [$itk_component(hpane) fraction]

    if {[llength $fractions] == 2} {
	set mHPaneFraction1 [lindex $fractions 0]
	set mHPaneFraction2 [lindex $fractions 1]
    }
}

::itcl::body Archer::_update_vpane_fractions {} {
    if {$mViewOnly} {
	return
    }

    set fractions [$itk_component(vpane) fraction]

    switch -- [llength $fractions] {
	2 {
	    set mVPaneFraction1 [lindex $fractions 0]
	    set mVPaneFraction2 [lindex $fractions 1]
	}
	3 {
	    set mVPaneFraction3 [lindex $fractions 0]
	    set mVPaneFraction4 [lindex $fractions 1]
	    set mVPaneFraction5 [lindex $fractions 2]
	}
    }
}

::itcl::body Archer::_set_color_option {cmd colorOption color {tripleColorOption ""}} {
    if {$tripleColorOption != ""} {
	$cmd configure $tripleColorOption 0
    }

    switch -- $color {
	"Grey" {
	    $cmd configure $colorOption {100 100 100}
	}
	"Black" {
	    $cmd configure $colorOption {0 0 0}
	}
	"Blue" {
	    $cmd configure $colorOption {100 100 255}
	}
	"Cyan" {
	    $cmd configure $colorOption {0 255 255}
	}
	"Green" {
	    $cmd configure $colorOption {100 255 100}
	}
	"Magenta" {
	    $cmd configure $colorOption {255 0 255}
	}
	"Red" {
	    $cmd configure $colorOption {255 100 100}
	}
	default -
	"White" {
	    $cmd configure $colorOption {255 255 255}
	}
	"Yellow" {
	    $cmd configure $colorOption {255 255 0}
	}
	"Triple" {
	    $cmd configure $tripleColorOption 1
	}
    }
}

::itcl::body Archer::_add_history {cmd} {
    if {$mViewOnly} {
	return
    }

    set maxlines 1000
    set tw [$itk_component(history) component text]

    # construct line
    set str "> "
    append str $cmd "\n"

    # insert line
    $tw configure -state normal
    $itk_component(history) insert end $str

    # check to see it does not exceed maxline count
    set nlines [expr int([$tw index end])]
    if {$nlines > $maxlines} {
	$tw delete 1.0 [expr $nlines - $maxlines].end
    }

    # disable text widget
    $tw configure -state disabled
    $itk_component(history) see end

    update idletasks
}

::itcl::body Archer::dbCmd {args} {
    switch -- $mDbType {
	"BRL-CAD" {
	    if {![info exists itk_component(mged)]} {
		return
	    }

	    return [eval $itk_component(mged) $args]
	}
	"IVAVIEW" {
	    if {![info exists itk_component(sdb)]} {
		return
	    }

	    return [eval $itk_component(sdb) $args]
	}
	default  {
	}
    }
}

::itcl::body Archer::cmd {args} {
    set cmd [lindex $args 0]
    if {$cmd == ""} {
	return
    }

    set arg1 [lindex $args 1]
    if {$cmd == "info"} {
	switch $arg1 {
	    function {
		if {[llength $args] == 3} {
		    set subcmd [lindex $args 2]
		    if {[lsearch $subcmd $unwrappedDbCommands] == -1 &&
			[lsearch $subcmd $dbSpecificCommands] == -1 &&
			[lsearch $subcmd $archerCommands] == -1} {
			 error "Archer::cmd: unrecognized command - $subcmd"
		     } else {
			 return
		     }
		} else {
		    return [eval list $archerCommands $dbSpecificCommands $unwrappedDbCommands]
		}
	    }
	    class {
		return [info class]
	    }
	    default {
		return
	    }
	}
    }

    set i [lsearch $archerCommands $cmd]
    if {$i != -1} {
	_add_history $args
	return [eval $args]
    }

    set i [lsearch $dbSpecificCommands $cmd]
    if {$i != -1} {
	_add_history $args
	return [eval $args]
    }

    set i [lsearch $unwrappedDbCommands $cmd]
    if {$i != -1} {
	_add_history $args
	return [eval dbCmd $args]
    }

    error "Archer::cmd: unrecognized command - $args, check source code"
}

::itcl::body Archer::png {filename} {
    SetWaitCursor
    dbCmd sync
    dbCmd png $filename
    SetNormalCursor
}

# ------------------------------------------------------------
#                  DB/DISPLAY COMMANDS
# ------------------------------------------------------------
::itcl::body Archer::_get_node_children {node} {
    if {$node == ""} {
	return {}
    }

    if {[catch {dbCmd get $node tree} tlist]} {
	return {}
    }

    # first remove any matrices
    regsub -all {\{[0-9]+[^\}]+[0-9]+\}} $tlist "" tlist

    # remove all other unwanted stuff
    regsub -all {^[lun!GXN^-] |\{[lun!GXN^-]|\}} $tlist "" tlist

    # finally, remove any duplicates
    set ntlist {}
    foreach item $tlist {
	if {[lsearch $ntlist $item] == -1} {
	    lappend ntlist $item
	}
    }

    return $ntlist
}

::itcl::body Archer::_render_comp {node} {
    set renderMode [dbCmd how $node]
    if {$renderMode < 0} {
	_render $node 0 1 1
    } else {
	_render $node -1 1 1
    }
}

::itcl::body Archer::_render {node state trans updateTree {wflag 1}} {
    if {$wflag} {
	SetWaitCursor
    }

    switch -- $mDbType {
	"BRL-CAD" {
	    set savePwd ""
	    set tnode [file tail $node]
	    set saveGroundPlane 0
	}
	"IVAVIEW" {
	    set savePwd [pwd]
	    cd /
	    set tnode $node

	    if {$mShowGroundPlane && [dbCmd isBlankScreen]} {
		# Temporarily turn off ground plane
		set saveGroundPlane 1
		set mShowGroundPlane 0
		_show_ground_plane
	    } else {
		set saveGroundPlane 0
	    }
	}
	default {
	    return
	}
    }

    if {[catch {$itk_component(sdb) attr get $node $mShowNormalsTag} mShowNormals]} {
	set mShowNormals 0
    }

    if {$mShowPrimitiveLabels} {
	set plnode $node
    } else {
	set plnode {}
    }

    if {$mShowNormals} {
	if {[catch {dbCmd attr get \
			$tnode displayColor} displayColor]} {
	    switch -exact -- $state {
		"0" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m0 -n -x$trans $node
		}
		"1" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m1 -n -x$trans $node
		}
		"2" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m2 -n -x$trans $node
		}
		"3" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd E $node
		}
		"-1" {
		    dbCmd configure -primitiveLabels {}
		    dbCmd erase $node
		}
	    }
	} else {
	    switch -exact -- $state {
		"0" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m0 -n -x$trans \
			-C$displayColor $node
		}
		"1" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m1 -n -x$trans \
			-C$displayColor $node
		}
		"2" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m2 -n -x$trans \
			-C$displayColor $node
		}
		"3" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd E -C$displayColor $node
		}
		"-1" {
		    dbCmd configure -primitiveLabels {}
		    dbCmd erase $node
		}
	    }
	}
    } else {
	if {[catch {dbCmd attr get \
			$tnode displayColor} displayColor]} {
	    switch -exact -- $state {
		"0" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m0 -x$trans $node
		}
		"1" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m1 -x$trans $node
		}
		"2" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m2 -x$trans $node
		}
		"3" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd E $node
		}
		"-1" {
		    dbCmd configure -primitiveLabels {}
		    dbCmd erase $node
		}
	    }
	} else {
	    switch -exact -- $state {
		"0" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m0 -x$trans \
			-C$displayColor $node
		}
		"1" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m1 -x$trans \
			-C$displayColor $node
		}
		"2" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd draw -m2 -x$trans \
			-C$displayColor $node
		}
		"3" {
		    dbCmd configure -primitiveLabels $plnode
		    dbCmd E -C$displayColor $node
		}
		"-1" {
		    dbCmd configure -primitiveLabels {}
		    dbCmd erase $node
		}
	    }
	}
    }

    # Change back to previous directory
    if {$savePwd != ""} {
	cd $savePwd
    }

    # Turn ground plane back on if it was on before the draw
    if {$saveGroundPlane} {
	set mShowGroundPlane 1
	_show_ground_plane
    }

    if {$updateTree} {
	_update_tree
    }

    if {$wflag} {
	SetNormalCursor
    }
}

::itcl::body Archer::selectDisplayColor {node} {
    switch -- $mDbType {
	"BRL-CAD" {
	    set tnode [file tail $node]
	}
	"IVAVIEW" {
	    set tnode $node
	}
	default {
	    return
	}
    }

    if {[catch {dbCmd attr get \
		    $tnode displayColor} displayColor] &&
	[catch {dbCmd attr get \
		    $tnode rgb} displayColor]} {
	set displayColor [eval format "%d/%d/%d" $defaultNodeColor]
    }

    set rgb [split $displayColor /]
    set color [tk_chooseColor \
		   -parent $itk_interior \
		   -initialcolor [getTkColor [lindex $rgb 0] [lindex $rgb 1] [lindex $rgb 2]]]
    if {$color == ""} {
	return
    }

    setDisplayColor $node [getRgbColor $color]
}

::itcl::body Archer::setDisplayColor {node rgb} {
    switch -- $mDbType {
	"BRL-CAD" {
	    set tnode [file tail $node]
	    set savePwd ""
	}
	"IVAVIEW" {
	    set tnode $node
	    set savePwd [pwd]
	    cd /
	}
	default {
	    return
	}
    }

    if {$rgb == {}} {
	dbCmd attr rm $tnode displayColor
    } else {
	dbCmd attr set $tnode \
	    displayColor "[lindex $rgb 0]/[lindex $rgb 1]/[lindex $rgb 2]"
    }

    set drawState [dbCmd how -b $node]

    if {$savePwd != ""} {
	cd $savePwd
    }

    # redraw with a potentially different color
    if {[llength $drawState] != 0} {
	_render $node [lindex $drawState 0] \
	    [lindex $drawState 1] 0 1
    }

    set mNeedSave 1
    _update_save_mode
}

::itcl::body Archer::setTransparency {node alpha} {
    switch -- $mDbType {
	"BRL-CAD" {
	    set savePwd ""
	}
	"IVAVIEW" {
	    set savePwd [pwd]
	    cd /
	}
	default {
	    return
	}
    }

    dbCmd set_transparency $node $alpha

    if {$savePwd != ""} {
	cd $savePwd
    }
}

::itcl::body Archer::_ray_trace_panel {} {
    $itk_component(rtcntrl) configure -size "Size of Pane"
    $itk_component(rtcntrl) center [namespace tail $this]
    $itk_component(rtcntrl) activate
}

::itcl::body Archer::_do_png {} {
    set typelist {
	{"PNG Files" {".png"}}
	{"All Files" {*}}
    }
    set filename [tk_getSaveFile -parent $itk_interior \
	    -title "Export Geometry to PNG" \
	    -initialdir $mLastSelectedDir -filetypes $typelist]

    if {$filename != ""} {
	set mLastSelectedDir [file dirname $finename]
	
	#XXX Hack! Hack! Hack!
	raise .

	update
	after idle [::itcl::code $this png $filename]
    }
}

::itcl::body Archer::_set_active_pane {pane} {
    switch -- $mDbType {
	"BRL-CAD" {$itk_component(mged) pane $pane}
	"IVAVIEW"    {$itk_component(sdb) pane $pane}
	default  {}
    }
    _update_active_pane $pane
}

::itcl::body Archer::_update_active_pane {args} {
    # update active pane radiobuttons
    switch -- $args {
	ul {set mActivePane 0}
	ur {set mActivePane 1}
	ll {set mActivePane 2}
	lr {set mActivePane 3}
    }

    set mShowModelAxes [dbCmd setModelAxesEnable]
    set mShowModelAxesTicks [dbCmd setModelAxesTickEnable]
    set mShowViewAxes [dbCmd setViewAxesEnable]
}

::itcl::body Archer::_do_multi_pane {} {
    dbCmd configure -multi_pane $mMultiPane
}

::itcl::body Archer::_do_lighting {} {
    SetWaitCursor

    if {$mZClipMode != $ZCLIP_NONE} {
	dbCmd zclipAll $mLighting
    }

    dbCmd zbufferAll $mLighting
    dbCmd lightAll $mLighting

    SetNormalCursor
}

::itcl::body Archer::_do_view_reset {} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    if {[info exists itk_component(sdb)] && $mShowGroundPlane} {
	# Turn off ground plane
	set saveGroundPlane 1
	set mShowGroundPlane 0
	_show_ground_plane
	dbCmd autoviewAll
	dbCmd default_views

	# Turn ground plane back on
	set mShowGroundPlane 1
	_show_ground_plane
    } else {
	dbCmd autoviewAll
	dbCmd default_views
    }
}

::itcl::body Archer::_do_autoview {} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    if {[info exists itk_component(sdb)] && $mShowGroundPlane} {
	# Turn off ground plane
	set saveGroundPlane 1
	set mShowGroundPlane 0
	_show_ground_plane
	dbCmd autoviewAll

	# Turn ground plane back on
	set mShowGroundPlane 1
	_show_ground_plane
    } else {
	dbCmd autoviewAll
    }
}

::itcl::body Archer::_do_view_center {} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    if {$currentDisplay == ""} {
	set dm dbCmd
    } else {
	set dm $currentDisplay
    }

    set center [$dm center]
    set _centerX [lindex $center 0]
    set _centerY [lindex $center 1]
    set _centerZ [lindex $center 2]

    set mDbUnits [dbCmd units]
    $itk_component(centerDialog) center [namespace tail $this]
    if {[$itk_component(centerDialog) activate]} {
	$dm center $_centerX $_centerY $_centerZ
    }
}

::itcl::body Archer::_do_ae {az el} {
    if {$currentDisplay == ""} {
	dbCmd ae $az $el
    } else {
	$currentDisplay ae $az $el
    }

    _add_history "ae $az $el"
}

::itcl::body Archer::_show_view_axes {} {
    catch {dbCmd setViewAxesEnable $mShowViewAxes}
}

::itcl::body Archer::_show_model_axes {} {
    catch {dbCmd setModelAxesEnable $mShowModelAxes}
}

::itcl::body Archer::_show_model_axes_ticks {} {
    catch {dbCmd setModelAxesTickEnable $mShowModelAxesTicks}
}

::itcl::body Archer::_toggle_model_axes {pane} {
    set currPane [dbCmd pane]
    if {$currPane == $pane} {
	# update menu checkbutton
	set mShowModelAxes [expr !$mShowModelAxes]
    }

    dbCmd toggle_modelAxesEnable $pane
}

::itcl::body Archer::_toggle_model_axes_ticks {pane} {
    set currPane [dbCmd pane]
    if {$currPane == $pane} {
	# update menu checkbutton
	set mShowModelAxesTicks [expr !$mShowModelAxesTicks]
    }

    dbCmd toggle_modelAxesTickEnable $pane
}

::itcl::body Archer::_toggle_view_axes {pane} {
    set currPane [dbCmd pane]
    if {$currPane == $pane} {
	# update menu checkbutton
	set mShowViewAxes [expr !$mShowViewAxes]
    }

    dbCmd toggle_viewAxesEnable $pane
}



# ------------------------------------------------------------
#                     TREE COMMANDS
# ------------------------------------------------------------
::itcl::body Archer::_alter_treenode_children {node option value} {
    foreach child [$itk_component(tree) query -children $node] {
	$itk_component(tree) alternode $child $option $value
	_alter_treenode_children $child "-color" \
	    [$itk_component(tree) query -color $child]
    }
}

::itcl::body Archer::refreshTree {{restore 1}} {
    _refresh_tree $restore
}

::itcl::body Archer::_refresh_tree {{restore 1}} {
    if {$restore == 1} {
	# get selected node
	set selNode [$itk_component(tree) query -selected]
	if {$selNode != ""} {
	    set selNodePath [$itk_component(tree) query -path $selNode]
	} else {
	    set selNodePath ""
	}

	# get current open state
	set opennodes [$itk_component(tree) opennodes "root"]
	
	set paths ""
	foreach node $opennodes {
	    lappend paths [$itk_component(tree) query -path $node]
	}
    }

    # remove all elements
    $itk_component(tree) removeall

    # re-fill tree
    _fill_tree

    if {$restore == 1} {
	set mRestoringTree 1
	# set the open state of nodes
	foreach path $paths {
	    _toggle_tree_path $path
	}

	if {$selNodePath != ""} {
	    _toggle_tree_path $selNodePath
	}
	set mRestoringTree 0
    }

    # force redraw of tree
    $itk_component(tree) redraw
}

::itcl::body Archer::_toggle_tree_path {_path} {
    set _path [file split $_path]
    if {[llength $_path] < 2} {
	#set prev ""
	set parent "root"
	set nname $_path
    } else {
	set parent [lindex $_path [expr [llength $_path] -2]]
	set nname  [lindex $_path [expr [llength $_path] -1]]
    }
	    
    set pnode [$itk_component(tree) find $parent]
	    
    set node [$itk_component(tree) find $nname $pnode]
    $itk_component(tree) toggle $node
	    
    #set prev $nname
}

::itcl::body Archer::_update_tree {} {
    # grab selection
    set select [$itk_component(tree) selection get]
    #set element [lindex [split $select ":"] 1]
    set element [split $select ":"]
    if {[llength $element] > 1} {
	set element [lindex $element 1]
    }
    set path [$itk_component(tree) query -path $element]

    # alter color of parentage
    set parent "root"
    foreach name [file split $path] {
	set node [$itk_component(tree) find $name $parent]
	set nname [$itk_component(tree) query -path $node]
	if {0 <= [dbCmd how $nname]} {
	    $itk_component(tree) alternode $node -color blue
	} else {
	    $itk_component(tree) alternode $node -color black
	}
    }

    # alter color of subnodes
    _alter_treenode_children $element "-color" \
	[$itk_component(tree) query -color $element]
}

::itcl::body Archer::_fill_tree {{node ""}} {
    switch -- $mDbType {
	"BRL-CAD" {
	    if {$node == ""} {
		# set node to root
		set node "root"

		# get toplevel objects
		set tops [dbCmd tops]
	    } else {
		set nname [$itk_component(tree) query -text $node]

		# get all its children
		set tops [_get_node_children $nname]
	    }

	    set stem "leaf"

	    foreach cname $tops {
		set cname [string trim $cname " /\\"]

		# need to get rid of any "/R" left
		set cname [string trimright $cname "/R"]
		if {$cname == "_GLOBAL"} {
		    continue
		}

		# need to add whether its a "leaf" or "branch" ... this seems to work,
		# as long as they don't change BRL-CAD
		set l [lindex [split [dbCmd l $cname] "\n"] 0]
		if {[lindex $l [expr [llength $l] -1]] == "--"} {set stem "branch"}

		# add to tree
		set cnode [$itk_component(tree) insert end $node $cname $stem]
		set cpath [$itk_component(tree) query -path $cnode]

		if {0 <= [dbCmd how $cpath]} {
		    $itk_component(tree) alternode $cnode -color blue
		} else {
		    $itk_component(tree) alternode $cnode -color black
		}

		set stem "leaf"
	    }
	}
	"IVAVIEW" {
	    set savePwd [pwd]
	    cd /
	    if {$node == ""} {
		# set node to root
		set node "root"
		set nname ""

		# get toplevel objects
		set tops [lsort -dictionary [dbCmd ls -p /]]
	    } else {
		set nname [$itk_component(tree) query -path $node]

		# get all its children
		set tops [lsort -dictionary [dbCmd ls -p $nname]]
	    }

	    if {$tops == $nname} {
		cd $savePwd
		return
	    }

	    foreach item $tops {
		set name [lindex $item 0]
		set type [lindex $item 1]

		# add to tree
		set cnode [$itk_component(tree) insert end $node $name $type]
		set cpath [$itk_component(tree) query -path $cnode]

		if {0 <= [dbCmd how $cpath]} {
		    $itk_component(tree) alternode $cnode -color blue
		} else {
		    $itk_component(tree) alternode $cnode -color black
		}
	    }

	    # This directory/group may not exist (i.e. due to being deleted).
	    catch {cd $savePwd}
	}
    }

    ::update idletasks
}

::itcl::body Archer::_select_node {tags {rflag 1}} {
    set tags [split $tags ":"]
    if {[llength $tags] > 1} {
	set element [lindex $tags 1]
    } else {
	set element $tags
    }
    if {$element == ""} {
	return
    }

    set node [$itk_component(tree) query -path $element]
    set type [$itk_component(tree) query -nodetype $element]

    set mPrevSelectedObjPath $mSelectedObjPath
    set mPrevSelectedObj $mSelectedObj
    set mSelectedObjPath $node
    set mSelectedObj [$itk_component(tree) query -text $element]

    #XXX Hack to get around the fact that somehow this
    #    routine gets randomly called by the
    #    hierarchy widget after a Load. When called its
    #    tag refers to a node in the previous database.
    if {[info exists itk_component(sdb)]} {
	set savePwd [pwd]
	cd /
    } else {
	set savePwd ""
    }

    if {[catch {dbCmd get $node} ret]} {
	if {$savePwd != ""} {
	    cd $savePwd
	}

	return
    }

    if {!$mViewOnly} {
	if {$mObjViewMode == $OBJ_ATTR_VIEW_MODE} {
	    _init_obj_attr_view
	} else {
	    if {!$mRestoringTree} {
	    _init_obj_edit_view

	    switch -- $mDefaultBindingMode \
		$OBJECT_ROTATE_MODE { \
		        beginObjRotate
	            } \
		    $OBJECT_SCALE_MODE { \
		        beginObjScale
	            } \
		    $OBJECT_TRANSLATE_MODE { \
		        beginObjTranslate 
	            } \
		    $OBJECT_CENTER_MODE { \
		        beginObjCenter
	            }
	    }
	}
    }

    # label the object if it's being drawn
    set mRenderMode [dbCmd how $node]

    if {$mShowPrimitiveLabels && 0 <= $mRenderMode} {
	dbCmd configure -primitiveLabels $node
    } else {
	dbCmd configure -primitiveLabels {}
    }

    if {$rflag} {
	dbCmd refresh
    }

    set mPrevSelectedObjPath $mSelectedObjPath
    set mPrevSelectedObj $mSelectedObj

    if {$savePwd != ""} {
	cd $savePwd
    }

    $itk_component(tree) selection clear
    $itk_component(tree) selection set $element

    if {!$mViewOnly} {
	_update_cut_mode
	_update_copy_mode
	_update_paste_mode
    }
}

::itcl::body Archer::_dbl_click {tags} {
    global App

    #set element [lindex [split $tags ":"] 1]
    set element [split $tags ":"]
    if {[llength $element] > 1} {
	set element [lindex $element 1]
    }

    set node [$itk_component(tree) query -path $element]
    set type [$itk_component(tree) query -nodetype $element]

    set mPrevSelectedObjPath $mSelectedObjPath
    set mPrevSelectedObj $mSelectedObj
    set mSelectedObjPath $node
    set mSelectedObj [$itk_component(tree) query -text $element]

    if {$mPrevSelectedObj != $mSelectedObj} {
	if {!$mViewOnly} {
	    if {$mObjViewMode == $OBJ_ATTR_VIEW_MODE} {
		_init_obj_attr_view
	    } else {
		_init_obj_edit_view
	    }
	}

	set mPrevSelectedObjPath $mSelectedObjPath
	set mPrevSelectedObj $mSelectedObj
    }

    $itk_component(tree) selection clear
    $itk_component(tree) selection set $tags

    if {!$mViewOnly} {
	_update_cut_mode
	_update_copy_mode
	_update_paste_mode
    }

    switch -- $type {
	"leaf"   -
	"branch" {_render_comp $node}
    }
}

::itcl::body Archer::_load_menu {menu snode} {
    # destroy old menu
    if [winfo exists $menu.color] {
	$menu.color delete 0 end
	destroy $menu.color
    }
    if {[winfo exists $menu.trans]} {
	$menu.trans delete 0 end
	destroy $menu.trans
    }
    $menu delete 0 end

    #set element [lindex [split $snode ":"] 1]
    set element [split $snode ":"]
    if {[llength $element] > 1} {
	set element [lindex $element 1]
    }

    set node [$itk_component(tree) query -path $element]
    set nodeType [$itk_component(tree) query -nodetype $element]

    set mRenderMode [dbCmd how $node]
    # do this in case "ev" was used from the command line
    if {2 < $mRenderMode} {
	set mRenderMode 2
    }

    set mPrevSelectedObjPath $mSelectedObjPath
    set mPrevSelectedObj $mSelectedObj
    set mSelectedObjPath $node
    set mSelectedObj [$itk_component(tree) query -text $element]

    if {$mPrevSelectedObj != $mSelectedObj} {
	if {!$mViewOnly} {
	    if {$mObjViewMode == $OBJ_ATTR_VIEW_MODE} {
		_init_obj_attr_view
	    } else {
		_init_obj_edit_view
	    }
	}
    }

    $itk_component(tree) selection clear
    $itk_component(tree) selection set $snode

    if {!$mViewOnly} {
	_update_cut_mode
	_update_copy_mode
	_update_paste_mode
    }

    if {$nodeType == "leaf"} {
	$menu add radiobutton -label "Wireframe" \
	    -indicatoron 1 -value 0 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this _render $node 0 1 1]
	
	if {[info exists itk_component(sdb)]} {
	    $menu add radiobutton -label "Shaded" \
		-indicatoron 1 -value 1 -variable [::itcl::scope mRenderMode] \
		-command [::itcl::code $this _render $node 1 1 1]
	} else {
	    $menu add radiobutton -label "Shaded (Mode 1)" \
		-indicatoron 1 -value 1 -variable [::itcl::scope mRenderMode] \
		-command [::itcl::code $this _render $node 1 1 1]
	    $menu add radiobutton -label "Shaded (Mode 2)" \
		-indicatoron 1 -value 2 -variable [::itcl::scope mRenderMode] \
		-command [::itcl::code $this _render $node 2 1 1]

	    if {$mEnableBigE} {
		$menu add radiobutton \
		    -label "Evaluated" \
		    -indicatoron 1 \
		    -value 3 \
		    -variable [::itcl::scope mRenderMode] \
		    -command [::itcl::code $this _render $node 3 1 1]
	    }
	}

	$menu add radiobutton -label "Off" \
		-indicatoron 1 -value -1 -variable [::itcl::scope mRenderMode] \
		-command [::itcl::code $this _render $node -1 1 1]
    } else {
	$menu add command -label "Wireframe" \
		-command [::itcl::code $this _render $node 0 1 1]

	if {[info exists itk_component(sdb)]} {
	    $menu add command -label "Shaded" \
		-command [::itcl::code $this _render $node 1 1 1]
	} else {
	    $menu add command -label "Shaded (Mode 1)" \
		-command [::itcl::code $this _render $node 1 1 1]
	    $menu add command -label "Shaded (Mode 2)" \
		-command [::itcl::code $this _render $node 2 1 1]

	    if {$mEnableBigE} {
		$menu add command \
		    -label "Evaluated" \
		    -command [::itcl::code $this _render $node 3 1 1]
	    }
	}

	$menu add command -label "Off" \
		-command [::itcl::code $this _render $node -1 1 1]
    }

#XXX need to copy over
#    $menu add separator
#    $menu add command -label "Copy" \
#	    -command [::itcl::code $this alter_obj "Copy" $mSelectedComp]
#    $menu add command -label "Rename" \
#	    -command [::itcl::code $this alter_obj "Rename" $mSelectedComp]
#    $menu add command -label "Delete" \
#	    -command [::itcl::code $this delete_obj $mSelectedComp]


    $menu add separator

    # Build color menu
    $menu add cascade -label "Color" \
	-menu $menu.color
    set color [menu $menu.color -tearoff 0]

    $color configure \
	-background $SystemButtonFace

    $color add command -label "Red" \
	-command [::itcl::code $this setDisplayColor $node {255 0 0}]
    $color add command -label "Orange" \
	-command [::itcl::code $this setDisplayColor $node {204 128 51}]
    $color add command -label "Yellow" \
	-command [::itcl::code $this setDisplayColor $node {219 219 112}]
    $color add command -label "Green" \
	-command [::itcl::code $this setDisplayColor $node {0 255 0}]
    $color add command -label "Blue" \
	-command [::itcl::code $this setDisplayColor $node {0 0 255}]
    $color add command -label "Indigo" \
	-command [::itcl::code $this setDisplayColor $node {0 0 128}]
    $color add command -label "Violet" \
	-command [::itcl::code $this setDisplayColor $node {128 0 128}]
    $color add separator
    $color add command -label "Default" \
	-command [::itcl::code $this setDisplayColor $node {}]
    $color add command -label "Select..." \
	-command [::itcl::code $this selectDisplayColor $node]

    if {($mDisplayType == "wgl" || $mDisplayType == "ogl") && ($nodeType != "leaf" || 0 < $mRenderMode)} {
	# Build transparency menu
	$menu add cascade -label "Transparency" \
	    -menu $menu.trans
	set trans [menu $menu.trans -tearoff 0]

	$trans configure \
	    -background $SystemButtonFace

	$trans add command -label "0%" \
		-command [::itcl::code $this setTransparency $node 1.0]
	#$trans add command -label "25%" \
	#	-command [::itcl::code $this setTransparency $node 0.75]
	#$trans add command -label "50%" \
	#	-command [::itcl::code $this setTransparency $node 0.5]
	#$trans add command -label "75%" \
	#	-command [::itcl::code $this setTransparency $node 0.25]
	$trans add command -label "10%" \
		-command [::itcl::code $this setTransparency $node 0.9]
	$trans add command -label "20%" \
		-command [::itcl::code $this setTransparency $node 0.8]
	$trans add command -label "30%" \
		-command [::itcl::code $this setTransparency $node 0.7]
	$trans add command -label "40%" \
		-command [::itcl::code $this setTransparency $node 0.6]
	$trans add command -label "50%" \
		-command [::itcl::code $this setTransparency $node 0.5]
	$trans add command -label "60%" \
		-command [::itcl::code $this setTransparency $node 0.4]
	$trans add command -label "70%" \
		-command [::itcl::code $this setTransparency $node 0.3]
	$trans add command -label "80%" \
		-command [::itcl::code $this setTransparency $node 0.2]
	$trans add command -label "90%" \
		-command [::itcl::code $this setTransparency $node 0.1]
    }

    if {[info exists itk_component(sdb)] && $nodeType == "leaf"} {
	$menu add separator
	$menu add command -label "Edit..." \
	    -command [::itcl::code $this editComponent $node]
    }

    if {[info exists itk_component(sdb)]} {
	$menu add separator
	if {$nodeType == "leaf"} {
	    if {[catch {$itk_component(sdb) attr get $node $mShowNormalsTag} mShowNormals]} {
		set mShowNormals 0
	    }
	    $menu add checkbutton -label "Show Normals" \
		-indicatoron 1 -variable [::itcl::scope mShowNormals] \
		-command [::itcl::code $this toggleCompNormals $node]
	}
	$menu add command -label "Sync Normals" \
	    -command [::itcl::code $this adjustCompNormals $node]
	$menu add command -label "Reverse Normals" \
	    -command [::itcl::code $this reverseCompNormals $node]
	$menu add separator
	$menu add command -label "Decimate" \
	    -command [::itcl::code $this decimate2 $node]
	if {[$itk_component(sdb) busy]} {
	    $menu add separator
	    $menu add command -label "Abort" \
		-command [::itcl::code $this abort]
	}
    }

    # set up bindings for status
    bind $menu <<MenuSelect>> \
	[::itcl::code $this _menu_statusCB %W]
}

# ------------------------------------------------------------
#                         GENERAL
# ------------------------------------------------------------
::itcl::body Archer::Load {type target} {
    if {$mNeedSave} {
	$itk_component(saveDialog) center [namespace tail $this]
	if {[$itk_component(saveDialog) activate]} {
	    _save_db
	} else {
	    set mNeedSave 0
	    _update_save_mode	
	}
    }

    # Get rid of any existing edit dialogs
    foreach ed $mActiveEditDialogs {
	::itcl::delete object $ed
    }
    set mActiveEditDialogs {}

    set mTarget $target
    set mDbType $type

    if {![catch {$mTarget ls}]} {
	set mDbShared 1
	set mDbReadOnly 1
    } elseif {[file exists $mTarget]} {
	if {[file writable $mTarget]} {
	    set mDbReadOnly 0
	} else {
	    set mDbReadOnly 1
	}
    } else {
	set mDbReadOnly 0
    }

    if {$mDbNoCopy || $mDbReadOnly} {
	set mTargetOldCopy $mTargetCopy
	set mTargetCopy ""
    } else {
	_create_target_copy
    }

    switch -- $type {
	"BRL-CAD" {
	    if {[info exists itk_component(sdb)]} {
		set oldSdb $itk_component(sdb)
		itk_component delete sdb
		rename $oldSdb ""
	    }

	    # load MGED database
	    if {[info exists itk_component(mged)]} {
		if {$mDbShared} {
		    $itk_component(mged) sharedDb $mTarget
		} elseif {$mDbNoCopy || $mDbReadOnly} {
		    $itk_component(mged) opendb $mTarget
		} else {
		    $itk_component(mged) opendb $mTargetCopy
		}
	    } else {
		_init_mged

		grid forget $itk_component(canvas)
		if {!$mViewOnly} {
		    grid $itk_component(mged) -row 1 -column 0 -columnspan 3 -sticky news
		    after idle "$this component cmd configure -cmd_prefix \"[namespace tail $this] cmd\""
		} else {
		    grid $itk_component(mged) -row 1 -column 0 -sticky news
		}
	    }

	    if {!$mViewOnly} {
		_update_creation_buttons 1
		dbCmd size [expr {$mGroundPlaneSize * 1.5 * [dbCmd base2local]}]
		_build_ground_plane
		_show_ground_plane
	    }
	}
	"IVAVIEW" {
	    if {[info exists itk_component(mged)]} {
		set oldMged $itk_component(mged)
		itk_component delete mged
		rename $oldMged ""

		set oldRtcntrl $itk_component(rtcntrl)
		itk_component delete rtcntrl
		rename $oldRtcntrl ""
	    }

	    # load SDB database
	    if {[info exists itk_component(sdb)]} {
		if {$mDbShared} {
		    $itk_component(sdb) sharedDb $mTarget
		} elseif {$mDbNoCopy || $mDbReadOnly} {
		    $itk_component(sdb) openDb $mTarget
		} else {
		    $itk_component(sdb) openDb $mTargetCopy
		}
	    } else {
		_init_sdb

		grid forget $itk_component(canvas)
		if {!$mViewOnly} {
		    grid $itk_component(sdb) -row 1 -column 0 -columnspan 3 -sticky news
		    after idle "$this component cmd configure -cmd_prefix \"[namespace tail $this] cmd\""
		} else {
		    grid $itk_component(sdb) -row 1 -column 0 -sticky news
		}
	    }

	    if {!$mViewOnly} {
		_update_creation_buttons 0
		_build_ground_plane
		_show_ground_plane
		_do_autoview
	    }
	}
	default {
	    return
	}
    }

    _set_color_option dbCmd -primitiveLabelColor $mPrimitiveLabelColor
    _set_color_option dbCmd -scaleColor $mScaleColor
    _set_color_option dbCmd -viewingParamsColor $mViewingParamsColor

    if {!$mViewOnly} {
	_init_db_attr_view $mTarget
	_apply_preferences
	_do_lighting
	_update_modes_menu
	_update_wizard_menu
	_update_utility_menu
	_delete_target_old_copy

	# refresh tree contents
	SetWaitCursor
	_refresh_tree 0
	SetNormalCursor
    } else {
	_apply_preferences
	_do_lighting
    }

    if {$mBindingMode == 0} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
    }
}

::itcl::body Archer::GetUserCmds {} {
    return $unwrappedDbCommands
}

::itcl::body Archer::WhatsOpen {} {
    return $mTarget
}

::itcl::body Archer::Close {} {
    if {$itk_option(-quitcmd) != {}} {
        catch {eval $itk_option(-quitcmd)}
    }    
}

::itcl::body Archer::askToSave {} {
    if {$mNeedSave} {
	$itk_component(saveDialog) center [namespace tail $this]
	if {[$itk_component(saveDialog) activate]} {
	    _save_db
	}
    }
}

::itcl::body Archer::_menu_statusCB {w} {
    if {$doStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$w entrycget active -label} op]} {
	    set op ""
	}

	switch -- $op {
	    "New..." {
		set mStatusStr "Create a new target description"
	    }
	    "Open..." {
		set mStatusStr "Open a target description"
	    }
	    "Save" {
		set mStatusStr "Save the current target description"
	    }
	    "Basic" {
		set mStatusStr "Basic user mode"
	    }
	    "Intermediate" {
		set mStatusStr "Intermediate user mode"
	    }
	    "Advanced" {
		set mStatusStr "Advanced user mode"
	    }
	    "Fastgen 4 Import..." {
		set mStatusStr "Import from a Fastgen 4 file"
	    }
	    "Fastgen 4 Export..." {
		set mStatusStr "Export to a Fastgen 4 file"
	    }
	    "Close" {
		set mStatusStr "Close the current target description"
	    }
	    "Preferences..." {
		set mStatusStr "Set application preferences"
	    }
	    "Quit" {
		set mStatusStr "Exit Archer"
	    }
	    "Reset" {
		set mStatusStr "Set view to default"
	    }
	    "Autoview" {
		set mStatusStr "Set view size and center according to what's being displayed"
	    }
	    "Center..." {
		set mStatusStr "Set the view center"
	    }
	    "Front" {
		set mStatusStr "Set view to front"
	    }
	    "Rear" {
		set mStatusStr "Set view to rear"
	    }
	    "Port" {
		set mStatusStr "Set view to port/left"
	    }
	    "Starboard" {
		set mStatusStr "Set view to starboard/right"
	    }
	    "Top" {
		set mStatusStr "Set view to top"
	    }
	    "Bottom" {
		set mStatusStr "Set view to bottom"
	    }
	    "35,25" {
		set mStatusStr "Set view to az=35, el=25"
	    }
	    "45,45" {
		set mStatusStr "Set view to az=45, el=45"
	    }
	    "Primary" {
		set mStatusStr "Toggle on/off primary toolbar"
	    }
	    "View Controls" {
		set mStatusStr "Toggle on/off view toolbar"
	    }
	    "Status Bar" {
		set mStatusStr "Toggle on/off status bar"
	    }
	    "Command Window" {
		set mStatusStr "Toggle on/off command window"
	    }
	    "About..." {
		set mStatusStr "Info about Archer"
	    }
	    "Mouse Mode Overrides..." {
		set mStatusStr "Mouse mode override definitions"
	    }
	    "Upper Left" {
		set mStatusStr "Set the active pane to the upper left pane"
	    }
	    "Upper Right" {
		set mStatusStr "Set the active pane to the upper right pane"
	    }
	    "Lower Left" {
		set mStatusStr "Set the active pane to the lower left pane"
	    }
	    "Lower Right" {
		set mStatusStr "Set the active pane to the lower right pane"
	    }
	    "Quad View" {
		set mStatusStr "Toggle between single and multiple geometry pane mode"
	    }
	    "View Axes" {
		set mStatusStr "Hide/Show view axes"
	    }
	    "Model Axes" {
		set mStatusStr "Hide/Show model axes"
	    }
	    "File" {
		set mStatusStr ""
	    }
	    "View" {
		set mStatusStr ""
	    }
	    "Modes" {
		set mStatusStr ""
	    }
	    "Help" {
		set mStatusStr ""
	    }
	    "Wireframe" {
		set mStatusStr "Draw object as wireframe"
	    }
	    "Shaded (Mode 1)" {
		set mStatusStr "Draw object as shaded if a bot or polysolid (unevalutated)"
	    }
	    "Shaded (Mode 2)" {
		set mStatusStr "Draw object as shaded (unevalutated)"
	    }
	    "Off" {
		set mStatusStr "Erase object"
	    }
	    default {
		set mStatusStr ""
	    }
	}
    }
}

::itcl::body Archer::_set_mode {{updateFractions 0}} {
    if {$updateFractions} {
	_update_hpane_fractions
	_update_vpane_fractions
    }

    switch -- $mMode {
	0 {
	    _set_basic_mode
	}
	1 {
	    _set_intermediate_mode
	}
	2 {
	    _set_advanced_mode
	}
    }
}

::itcl::body Archer::_set_basic_mode {} {
    grid forget $itk_component(attr_expand)

    #catch {$itk_component(displaymenu) delete "Command Window"}
    set itk_option(-primaryToolbar) 0
    _do_primary_toolbar

    catch {
	if {$mMultiPane} {
	    set mMultiPane 0
	    _do_multi_pane
	}
    }

    # DRH: hack to make sure it doesn't crash in Crossbow
    if {$Archer::inheritFromToplevel == 0} {
	pack forget $itk_component(menubar)
	::itcl::delete object $itk_component(menubar)
	_build_embedded_menubar
	pack $itk_component(menubar) -side top -fill x -padx 1 -before $itk_component(south)
    } else {
	_update_modes_menu
	_update_utility_menu
    }

    _update_wizard_menu

    $itk_component(hpane) hide bottomView
    $itk_component(vpane) hide attrView

    catch {
	$itk_component(viewToolbar) delete fill1
	$itk_component(viewToolbar) delete sep1
	$itk_component(viewToolbar) delete fill2
	$itk_component(viewToolbar) delete edit_rotate
	$itk_component(viewToolbar) delete edit_translate
	$itk_component(viewToolbar) delete edit_scale
	$itk_component(viewToolbar) delete edit_center
    }

    set toggle1 $mVPaneToggle1
    if {$mVPaneFraction1 == 0} {
	_toggle_tree_view "close"
    } else {
	_toggle_tree_view "open"
    }
    set mVPaneToggle1 $toggle1
    
    # How screwed up is this?
    $itk_component(vpane) fraction $mVPaneFraction1 $mVPaneFraction2
    update
    after idle $itk_component(vpane) fraction  $mVPaneFraction1 $mVPaneFraction2

    if {$mTarget != "" &&
	$mBindingMode == 0 &&
	$CENTER_MODE < $mDefaultBindingMode} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
    }
}

::itcl::body Archer::_set_intermediate_mode {} {
    grid $itk_component(attr_expand) -row 0 -column 2 -sticky e

    #catch {$itk_component(displaymenu) delete "Command Window"}
    set itk_option(-primaryToolbar) 1
    _do_primary_toolbar

    # DRH: hack to make sure it doesn't crash in Crossbow
    if {$Archer::inheritFromToplevel == 0} {
	pack forget $itk_component(menubar)
	::itcl::delete object $itk_component(menubar)
	_build_embedded_menubar
	pack $itk_component(menubar) -side top -fill x -padx 1 -before $itk_component(south)
    } else {
	_update_modes_menu
	_update_utility_menu
    }

    _update_wizard_menu
    _update_view_toolbar_for_edit

    if {$mTarget != "" &&
	$mBindingMode == 0} {
	$itk_component(viewToolbar) itemconfigure edit_rotate -state normal
	$itk_component(viewToolbar) itemconfigure edit_translate -state normal
	$itk_component(viewToolbar) itemconfigure edit_scale -state normal
	$itk_component(viewToolbar) itemconfigure edit_center -state normal
    }

    $itk_component(hpane) hide bottomView
    $itk_component(vpane) show attrView

    set toggle3 $mVPaneToggle3
    set toggle5 $mVPaneToggle5
    if {$mVPaneFraction3 == 0} {
	_toggle_tree_view "close"
    } else {
	_toggle_tree_view "open"
    }

    if {$mVPaneFraction5 == 0} {
	_toggle_attr_view "close"
    } else {
	_toggle_attr_view "open"
    }
    set mVPaneToggle3 $toggle3
    set mVPaneToggle5 $toggle5

    # How screwed up is this?
    $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
    update
    after idle $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
}

::itcl::body Archer::_set_advanced_mode {} {
    grid $itk_component(attr_expand) -row 0 -column 2 -sticky e

    set itk_option(-primaryToolbar) 1
    _do_primary_toolbar

    # DRH: hack to make sure it doesn't crash in Crossbow
    if {$Archer::inheritFromToplevel == 0} {
	pack forget $itk_component(menubar)
	::itcl::delete object $itk_component(menubar)
	_build_embedded_menubar
	pack $itk_component(menubar) -side top -fill x -padx 1 -before $itk_component(south)
    } else {
	_update_modes_menu
	_update_utility_menu
    }

    _update_wizard_menu
    _update_view_toolbar_for_edit

    if {$mTarget != "" &&
	$mBindingMode == 0} {
	$itk_component(viewToolbar) itemconfigure edit_rotate -state normal
	$itk_component(viewToolbar) itemconfigure edit_translate -state normal
	$itk_component(viewToolbar) itemconfigure edit_scale -state normal
	$itk_component(viewToolbar) itemconfigure edit_center -state normal
    }

    $itk_component(hpane) show bottomView
    $itk_component(hpane) fraction $mHPaneFraction1 $mHPaneFraction2
    $itk_component(vpane) show attrView

    set toggle3 $mVPaneToggle3
    set toggle5 $mVPaneToggle5
    if {$mVPaneFraction3 == 0} {
	_toggle_tree_view "close"
    } else {
	_toggle_tree_view "open"
    }

    if {$mVPaneFraction5 == 0} {
	_toggle_attr_view "close"
    } else {
	_toggle_attr_view "open"
    }
    set mVPaneToggle3 $toggle3
    set mVPaneToggle5 $toggle5

    # How screwed up is this?
    $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
    update
    after idle $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
}

::itcl::body Archer::_update_theme {} {
    set dir [file join $_imgdir Themes $mTheme]
    
    if {!$mViewOnly} {
    # Tree Control
    $itk_component(tree) configure \
	    -openimage [image create photo -file [file join $dir folder_open_small.png]] \
	    -closeimage [image create photo -file [file join $dir folder_closed_small.png]] \
	    -nodeimage [image create photo -file [file join $dir file_text_small.png]]
    $itk_component(tree) redraw

    # Primary Toolbar
    $itk_component(primaryToolbar) itemconfigure new \
	-image [image create photo \
		    -file [file join $dir file_new.png]]
    $itk_component(primaryToolbar) itemconfigure open \
	-image [image create photo \
		    -file [file join $dir open.png]]
    $itk_component(primaryToolbar) itemconfigure save \
	-image [image create photo \
		    -file [file join $dir save.png]]
    $itk_component(primaryToolbar) itemconfigure preferences \
	-image [image create photo \
		    -file [file join $dir configure.png]]
    $itk_component(primaryToolbar) itemconfigure cut \
	-image [image create photo \
		    -file [file join $dir edit_cut.png]]
    $itk_component(primaryToolbar) itemconfigure copy \
	-image [image create photo \
		    -file [file join $dir edit_copy.png]]
    $itk_component(primaryToolbar) itemconfigure paste \
	-image [image create photo \
		    -file [file join $dir edit_paste.png]]
    $itk_component(primaryToolbar) itemconfigure comb \
	-image [image create photo \
		    -file [file join $dir combination.png]]
    $itk_component(primaryToolbar) itemconfigure arb6 \
	-image [image create photo \
		    -file [file join $dir primitive_arb6.png]]
    $itk_component(primaryToolbar) itemconfigure arb8 \
	-image [image create photo \
		    -file [file join $dir primitive_arb8.png]]
    $itk_component(primaryToolbar) itemconfigure cone \
	-image [image create photo \
		    -file [file join $dir primitive_cone.png]]
#    $itk_component(primaryToolbar) itemconfigure pipe \
	-image [image create photo \
		    -file [file join $dir primitive_pipe.png]]
    $itk_component(primaryToolbar) itemconfigure sphere \
	-image [image create photo \
		    -file [file join $dir primitive_sph.png]]
    $itk_component(primaryToolbar) itemconfigure torus \
	-image [image create photo \
		    -file [file join $dir primitive_tor.png]]
    $itk_component(primaryToolbar) itemconfigure other \
	-image [image create photo \
		    -file [file join $dir primitive_list.png]]

    # We catch this because the item may not exist
    catch {$itk_component(primaryToolbar) itemconfigure wizards \
	       -image [image create photo \
			   -file [file join $dir wizard.png]]}

#    $itk_component(primaryToolbar) itemconfigure ehy \
	-image [image create photo \
		    -file [file join $dir primitive_ehy.png]]
#    $itk_component(primaryToolbar) itemconfigure epa \
	-image [image create photo \
		    -file [file join $dir primitive_epa.png]]
#    $itk_component(primaryToolbar) itemconfigure rpc \
	-image [image create photo \
		    -file [file join $dir primitive_rpc.png]]
#    $itk_component(primaryToolbar) itemconfigure rhc \
	-image [image create photo \
		    -file [file join $dir primitive_rhc.png]]
#    $itk_component(primaryToolbar) itemconfigure ell \
	-image [image create photo \
		    -file [file join $dir primitive_ell.png]]
#    $itk_component(primaryToolbar) itemconfigure eto \
	-image [image create photo \
		    -file [file join $dir primitive_eto.png]]
#    $itk_component(primaryToolbar) itemconfigure half \
	-image [image create photo \
		    -file [file join $dir primitive_half.png]]
#    $itk_component(primaryToolbar) itemconfigure part \
	-image [image create photo \
		    -file [file join $dir primitive_part.png]]
#    $itk_component(primaryToolbar) itemconfigure grip \
	-image [image create photo \
		    -file [file join $dir primitive_grip.png]]
#    $itk_component(primaryToolbar) itemconfigure extrude \
	-image [image create photo \
		    -file [file join $dir primitive_extrude.png]]
#    $itk_component(primaryToolbar) itemconfigure sketch \
	-image [image create photo \
		    -file [file join $dir primitive_sketch.png]]
#    $itk_component(primaryToolbar) itemconfigure bot \
	-image [image create photo \
		    -file [file join $dir primitive_bot.png]]
     }

    # View Toolbar
    $itk_component(viewToolbar) itemconfigure rotate \
	-image [image create photo \
		    -file [file join $dir view_rotate.png]]
    $itk_component(viewToolbar) itemconfigure translate \
	-image [image create photo \
		    -file [file join $dir view_translate.png]]
    $itk_component(viewToolbar) itemconfigure scale \
	-image [image create photo \
		    -file [file join $dir view_scale.png]]
    $itk_component(viewToolbar) itemconfigure center \
	-image [image create photo \
		    -file [file join $dir view_select.png]]
    $itk_component(viewToolbar) itemconfigure cpick \
	-image [image create photo \
		    -file [file join $dir compSelect.png]]
    $itk_component(viewToolbar) itemconfigure measure \
	-image [image create photo \
		    -file [file join $dir measure.png]]

    if {!$mViewOnly} {
    catch {
	$itk_component(viewToolbar) itemconfigure edit_rotate \
	    -image [image create photo \
			-file [file join $dir edit_rotate.png]]
	$itk_component(viewToolbar) itemconfigure edit_translate \
	    -image [image create photo \
			-file [file join $dir edit_translate.png]]
	$itk_component(viewToolbar) itemconfigure edit_scale \
	    -image [image create photo \
			-file [file join $dir edit_scale.png]]
	$itk_component(viewToolbar) itemconfigure edit_center \
	    -image [image create photo \
			-file [file join $dir edit_select.png]]
    }

    # Attribute View Toolbar
    $itk_component(objViewToolbar) itemconfigure objAttrView \
	-image [image create photo \
		    -file [file join $dir option_text.png]]
    $itk_component(objViewToolbar) itemconfigure objEditView \
	-image [image create photo \
		    -file [file join $dir option_tree.png]]

    # Object Edit Toolbar
    $itk_component(objEditToolbar) itemconfigure prev \
	-image [image create photo \
		    -file [file join $dir arrow_back.png]]
    $itk_component(objEditToolbar) itemconfigure next \
	-image [image create photo \
		    -file [file join $dir arrow_forward.png]]
    }
}

::itcl::body Archer::_update_creation_buttons {on} {
    if {$on} {
	$itk_component(primaryToolbar) itemconfigure arb6 -state normal
	$itk_component(primaryToolbar) itemconfigure arb8 -state normal
	$itk_component(primaryToolbar) itemconfigure cone -state normal
	$itk_component(primaryToolbar) itemconfigure sphere -state normal
	$itk_component(primaryToolbar) itemconfigure torus -state normal
#	$itk_component(primaryToolbar) itemconfigure pipe -state normal
	$itk_component(primaryToolbar) itemconfigure other -state normal
	$itk_component(primaryToolbar) itemconfigure comb -state normal
    } else {
	$itk_component(primaryToolbar) itemconfigure arb6 -state disabled
	$itk_component(primaryToolbar) itemconfigure arb8 -state disabled
	$itk_component(primaryToolbar) itemconfigure cone -state disabled
	$itk_component(primaryToolbar) itemconfigure sphere -state disabled
	$itk_component(primaryToolbar) itemconfigure torus -state disabled
#	$itk_component(primaryToolbar) itemconfigure pipe -state disabled
	$itk_component(primaryToolbar) itemconfigure other -state disabled
	$itk_component(primaryToolbar) itemconfigure comb -state disabled
    }
}

::itcl::body Archer::_update_save_mode {} {
    if {$mViewOnly} {
	return
    }

    if {!$mDbNoCopy && !$mDbReadOnly && $mNeedSave} {
	if {$Archer::inheritFromToplevel} {
	    $itk_component(filemenu) entryconfigure "Save" \
		-state normal
	} else {
	    $itk_component(menubar) menuconfigure .file.save \
		-state normal
	}

	$itk_component(primaryToolbar) itemconfigure save \
	    -state normal
    } else {
	if {$Archer::inheritFromToplevel} {
	    $itk_component(filemenu) entryconfigure "Save" \
		-state disabled
	} else {
	    $itk_component(menubar) menuconfigure .file.save \
		-state disabled
	}

	$itk_component(primaryToolbar) itemconfigure save \
	    -state disabled
    }

}

::itcl::body Archer::_update_cut_mode {} {
    if {1} {
	#XXX For now, always set to disabled
	$itk_component(primaryToolbar) itemconfigure cut \
	    -state disabled
    } else {
	if {!$mDbNoCopy && !$mDbReadOnly && $mSelectedObj != ""} {
	    $itk_component(primaryToolbar) itemconfigure cut \
		    -state normal
	} else {
	    $itk_component(primaryToolbar) itemconfigure cut \
		    -state disabled
	}
    }
}

::itcl::body Archer::_update_copy_mode {} {
    if {1} {
	#XXX For now, always set to disabled
	$itk_component(primaryToolbar) itemconfigure copy \
	    -state disabled
    } else {
	if {!$mDbNoCopy && !$mDbReadOnly && $mSelectedObj != ""} {
	    $itk_component(primaryToolbar) itemconfigure copy \
		    -state normal
	} else {
	    $itk_component(primaryToolbar) itemconfigure copy \
		    -state disabled
	}
    }
}

::itcl::body Archer::_update_paste_mode {} {
    if {1} {
	#XXX For now, always set to disabled
	$itk_component(primaryToolbar) itemconfigure paste \
	    -state disabled
    } else {
	if {!$mDbNoCopy && !$mDbReadOnly && $mSelectedObj != "" && $mPasteActive} {
	    $itk_component(primaryToolbar) itemconfigure paste \
		    -state normal
	} else {
	    $itk_component(primaryToolbar) itemconfigure paste \
		    -state disabled
	}
    }
}

::itcl::body Archer::_create_target_copy {} {
    set mTargetOldCopy $mTargetCopy
    set mTargetCopy "$mTarget~"

    set id 1
    while {[file exists $mTargetCopy]} {
	set mTargetCopy "$mTarget~$id"
	incr id
    }

    if {[file exists $mTarget]} {
	file copy $mTarget $mTargetCopy
    }
}

::itcl::body Archer::_delete_target_old_copy {} {
    if {$mTargetOldCopy != ""} {
	catch {file delete -force $mTargetOldCopy}

	# sanity
	set mTargetOldCopy ""
    }
}

::itcl::body Archer::_cut_obj {} {
    if {$mDbNoCopy || $mDbReadOnly} {
	return
    }

    set mPasteActive 1
    _update_paste_mode
}

::itcl::body Archer::_copy_obj {} {
    if {$mDbNoCopy || $mDbReadOnly} {
	return
    }

    set mPasteActive 1
    _update_paste_mode
}

::itcl::body Archer::_paste_obj {} {
    if {$mDbNoCopy || $mDbReadOnly} {
	return
    }

#    set mPasteActive 0
#    _update_paste_mode
}

::itcl::body Archer::_get_vdraw_color {color} {
    switch -- $color {
	"Grey" {
	    return "646464"
	}
	"Black" {
	    return "000000"
	}
	"Blue" {
	    return "0000ff"
	}
	"Cyan" {
	    return "00ffff"
	}
	"Green" {
	    return "00ff00"
	}
	"Magenta" {
	    return "ff00ff"
	}
	"Red" {
	    return "ff0000"
	}
	"Yellow" {
	    return "ffff00"
	}
	"White" -
	default {
	    return "ffffff"
	}
    }
}

::itcl::body Archer::_build_ground_plane {} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    catch {dbCmd vdraw vlist delete groundPlaneMajor}
    catch {dbCmd vdraw vlist delete groundPlaneMinor}

    set majorColor [_get_vdraw_color $mGroundPlaneMajorColor]
    set minorColor [_get_vdraw_color $mGroundPlaneMinorColor]
    set move 0
    set draw 1
    set Xmax [expr {$mGroundPlaneSize * 0.5}]
    set Xmin [expr {0.0 - $Xmax}]
    set Ymax [expr {$mGroundPlaneSize * 0.5}]
    set Ymin [expr {0.0 - $Ymax}]


    # build minor lines
    dbCmd vdraw open groundPlaneMinor
    dbCmd vdraw params color $minorColor

    # build minor X lines
    for {set y -$mGroundPlaneInterval} {$Ymin <= $y} {set y [expr {$y - $mGroundPlaneInterval}]} {
	dbCmd vdraw write next $move $Xmin $y 0
	dbCmd vdraw write next $draw $Xmax $y 0
    }

    for {set y $mGroundPlaneInterval} {$y <= $Ymax} {set y [expr {$y + $mGroundPlaneInterval}]} {
	dbCmd vdraw write next $move $Xmin $y 0
	dbCmd vdraw write next $draw $Xmax $y 0
    }

    # build minor Y lines
    for {set x -$mGroundPlaneInterval} {$Xmin <= $x} {set x [expr {$x - $mGroundPlaneInterval}]} {
	dbCmd vdraw write next $move $x $Ymin 0
	dbCmd vdraw write next $draw $x $Ymax 0
    }

    for {set x $mGroundPlaneInterval} {$x <= $Xmax} {set x [expr {$x + $mGroundPlaneInterval}]} {
	dbCmd vdraw write next $move $x $Ymin 0
	dbCmd vdraw write next $draw $x $Ymax 0
    }


    # build major lines
    dbCmd vdraw open groundPlaneMajor
    dbCmd vdraw params color $majorColor
    dbCmd vdraw write 0 $move $Xmin 0 0
    dbCmd vdraw write next $draw $Xmax 0 0
    dbCmd vdraw write next $move 0 $Ymin 0
    dbCmd vdraw write next $draw 0 $Ymax 0
}

::itcl::body Archer::_show_ground_plane {} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    if {[info exists itk_component(sdb)]} {
	set savePwd [pwd]
	cd /
    } else {
	set savePwd ""
    }

    if {$mShowGroundPlane} {
	dbCmd vdraw open groundPlaneMajor
	dbCmd vdraw send
	dbCmd vdraw open groundPlaneMinor
	dbCmd vdraw send
    } else {
	dbCmd erase _VDRWgroundPlaneMajor
	dbCmd erase _VDRWgroundPlaneMinor
    }

    if {$savePwd != ""} {
	cd $savePwd
    }
}

::itcl::body Archer::_show_primitive_labels {} {
    if {![info exists itk_component(mged)] &&
	![info exists itk_component(sdb)]} {
	return
    }

    _redraw_obj $mSelectedObjPath
}

::itcl::body Archer::_show_view_params {} {
    if {[info exists itk_component(mged)]} {
	$itk_component(mged) configure -showViewingParams $mShowViewingParams
    } elseif {[info exists itk_component(sdb)]} {
	$itk_component(sdb) configure -showViewingParams $mShowViewingParams
    } else {
	return
    }

    #_redraw_obj $mSelectedObjPath
    refreshDisplay
}

::itcl::body Archer::_show_scale {} {
    if {[info exists itk_component(mged)]} {
	$itk_component(mged) configure -scaleEnable $mShowScale
    } elseif {[info exists itk_component(sdb)]} {
	$itk_component(sdb) configure -scaleEnable $mShowScale
    } else {
	return
    }

    #_redraw_obj $mSelectedObjPath
    refreshDisplay
}

::itcl::body Archer::_toggle_tree_view {state} {
    # pre-move stuff
    set fractions [$itk_component(vpane) fraction]
    switch -- $state {
	"open" {
	    switch -- $mMode {
		0 {}
		default {
		    # need to get attr pane in case it has changed size
		    if {[$itk_component(attr_expand) cget -state] == "disabled"} {
			set mVPaneToggle5 [lindex $fractions 2]
		    }
		}
	    }
	}
	"close" {
	    # store fractions
	    switch -- $mMode {
		0 {set mVPaneToggle1 [lindex $fractions 0]}
		default {
		    set mVPaneToggle3 [lindex $fractions 0]
		    # need to get attr pane in case it has changed size
		    if {[$itk_component(attr_expand) cget -state] == "disabled"} {
			set mVPaneToggle5 [lindex $fractions 2]
		    }
		}
	    }
	}
    }

    switch -- $mMode {
	0 {
	    # BASIC mode
	    switch -- $state {
		"open" {
		    $itk_component(vpane) paneconfigure hierarchyView \
			    -minimum 200
		    set fraction1 $mVPaneToggle1
		    set fraction2 [expr 100 - $fraction1]
		}
		"close" {
		    $itk_component(vpane) paneconfigure hierarchyView \
			    -minimum 0
		    set fraction1 0
		    set fraction2 100
		}
	    }
	    # How screwed up is this?
	    $itk_component(vpane) fraction $fraction1 $fraction2
	    update
	    after idle $itk_component(vpane) fraction  $fraction1 $fraction2
	}
	1 -
	2 {
	    # INTERMEDIATE & ADVANCED mode
	    switch -- $state {
		"open" {
		    set fraction1 $mVPaneToggle3
		    
		    # check state of attribute pane
		    switch -- [$itk_component(attr_expand) cget -state] {
			"normal" {
			    set fraction2 [expr 100 - $fraction1]
			    set fraction3 0
			}
			"disabled" {
			    set fraction3 $mVPaneToggle5
			    set fraction2 [expr 100 - $fraction1 - $fraction3]
			}
		    }
		}
		"close" {
		    set fraction1 0

		    # check state of attribute pane
		    switch -- [$itk_component(attr_expand) cget -state] {
			"normal" {
			    set fraction2 100
			    set fraction3 0
			}
			"disabled" {
			    set fraction3 $mVPaneToggle5
			    set fraction2 [expr 100 - $fraction3]
			}
		    }
		}
	    }

	    # How screwed up is this?
	    $itk_component(vpane) fraction $fraction1 $fraction2 $fraction3
	    update
	    after idle $itk_component(vpane) fraction $fraction1 $fraction2 $fraction3
	}
    }

    # post-move stuff
    switch -- $state {
	"open" {
	    $itk_component(tree_expand) configure -image [image create photo -file \
		    [file join $_imgdir Themes $mTheme "pane_blank.png"]] \
		    -state disabled
#	    $itk_component(vpane) show hierarchyView
	}
	"close" {
	    $itk_component(tree_expand) configure -image [image create photo -file \
		    [file join $_imgdir Themes $mTheme "pane_expand.png"]] \
		    -state normal
#	    $itk_component(vpane) hide hierarchyView
	}
    }
}

::itcl::body Archer::_toggle_attr_view {state} {
    # pre-move stuff
    set fractions [$itk_component(vpane) fraction]

    switch -- $state {
	"open" {
	    if {[$itk_component(tree_expand) cget -state] == "disabled"} {
		set mVPaneToggle3 [lindex $fractions 0]
	    }
	}
	"close" {
	    # store fractions
	    set mVPaneToggle5 [lindex $fractions 2]
	    if {[$itk_component(tree_expand) cget -state] == "disabled"} {
		set mVPaneToggle3 [lindex $fractions 0]
	    }
	}
    }

    switch -- $state {
	"open" {
	    $itk_component(vpane) paneconfigure attrView \
		    -minimum 200
	    set fraction3 $mVPaneToggle5
	    switch -- [$itk_component(tree_expand) cget -state] {
		"normal" {
		    set fraction1 0
		    set fraction2 [expr 100 - $fraction3]
		}
		"disabled" {
		    set fraction1 $mVPaneToggle3
		    set fraction2 [expr 100 - $fraction1 - $fraction3]
		}
	    }
	}
	"close" {
	    # adjust minimum size to zero
	    $itk_component(vpane) paneconfigure attrView \
		    -minimum 0
	    
	    set fraction3 0
	    switch -- [$itk_component(tree_expand) cget -state] {
		"normal" {
		    set fraction1 0
		    set fraction2 100
		}
		"disabled" {
		    set fraction1 $mVPaneToggle3
		    set fraction2 [expr 100 - $fraction1]
		}
	    }
	}
    }

    # How screwed up is this?
    $itk_component(vpane) fraction $fraction1 $fraction2 $fraction3
    update
    after idle $itk_component(vpane) fraction $fraction1 $fraction2 $fraction3

    switch -- $state {
	"open" {
	    $itk_component(attr_expand) configure -image [image create photo -file \
		    [file join $_imgdir Themes $mTheme "pane_blank.png"]] \
		    -state disabled
#	    $itk_component(vpane) show attrView
	}
	"close" {
	    $itk_component(attr_expand) configure -image [image create photo -file \
		    [file join $_imgdir Themes $mTheme "pane_collapse.png"]] \
		    -state normal
#	    $itk_component(vpane) hide attrView
	}
    }
}

::itcl::body Archer::_update_toggle_mode {} {
    switch -- $mMode {
	0 {
	    set toggle1 $mVPaneToggle1

	    if {$mVPaneFraction1 == 0} {
		_toggle_tree_view "close"
	    }
	    
	    set mVPaneToggle1 $toggle1
	}
	default {
	    set toggle3 $mVPaneToggle3
	    set toggle5 $mVPaneToggle5

	    if {$mVPaneFraction3 == 0} {
		_toggle_tree_view "close"
	    }

	    if {$mVPaneFraction5 == 0} {
		_toggle_attr_view "close"
	    }
	    
	    set mVPaneToggle3 $toggle3
	    set mVPaneToggle5 $toggle5
	}
    }
}

::itcl::body Archer::editComponent {comp} {
    if {![info exists itk_component(sdb)]} {
	return
    }

    set savePwd [pwd]
    cd /
    set editDialog [SdbEditFg4 .\#auto $itk_component(sdb) $comp \
			[::itcl::code $this editApplyCallback] \
			[::itcl::code $this editDismissCallback]]
    cd $savePwd

    $editDialog center [namespace tail $this]
    $editDialog activate
    lappend mActiveEditDialogs $editDialog
}

::itcl::body Archer::editApplyCallback {comp} {
    set mNeedSave 1
    _update_save_mode	

    set savePwd [pwd]
    cd /
    set renderMode [dbCmd how -b $comp]
    cd $savePwd

    if {[llength $renderMode] == 2} {
	eval _render $comp $renderMode 1
    }
}

::itcl::body Archer::editDismissCallback {editDialog} {
    set i [lsearch $mActiveEditDialogs [namespace tail $editDialog]]
    if {$i != -1} {
	set mActiveEditDialogs [lreplace $mActiveEditDialogs $i $i]
    }
}

::itcl::body Archer::launchNirt {} {
    if {![info exists itk_component(mged)] || $mViewOnly} {
	return
    }

    $itk_component(cmd) putstring "nirt -b"
    $itk_component(cmd) putstring [$itk_component(mged) nirt -b]
}

::itcl::body Archer::launchRtApp {app size} {
    if {![info exists itk_component(mged)] || $mViewOnly} {
	return
    }

    if {![string is digit $size]} {
	set size [winfo width $itk_component(mged)]
    }

    $itk_component(mged) $app -s $size -F /dev/wgll
}

::itcl::body Archer::refreshDisplay {} {
    if {$currentDisplay == ""} {
	dbCmd refresh
    } else {
	$currentDisplay refresh
    }
}

::itcl::body Archer::mouseRay {_dm _x _y} {
    set target [$_dm screen2model $_x $_y]
    set view [$_dm screen2view $_x $_y]

    set bounds [$_dm bounds]
    set vZ [expr {[lindex $bounds 4] / -2048.0}]
    set start [$_dm v2mPoint [lindex $view 0] [lindex $view 1] $vZ]

    set partitions [shootRay $start "at" $target 1 1 0]
    set partition [lindex $partitions 0]

    if {[llength $mMouseRayCallbacks] == 0} {
	if {$partition == {}} {
	    tk_messageBox -message "Nothing hit"
	} else {
	    set region [bu_get_value_by_keyword "region" $partition]
	    tk_messageBox -message [dbCmd l $region]
	}
    } else {
	foreach callback $mMouseRayCallbacks {
	    catch {$callback $start $target $partitions}
	}
    }
}

::itcl::body Archer::shootRay {_start _op _target _prep _no_bool _onehit} {
    eval $itk_component(mged) rt_gettrees ray -i -u [$itk_component(mged) who]
    ray prep $_prep
    ray no_bool $_no_bool
    ray onehit $_onehit

    return [ray shootray $_start $_op $_target]
}

::itcl::body Archer::addMouseRayCallback {_callback} {
    lappend mMouseRayCallbacks $_callback
}

::itcl::body Archer::deleteMouseRayCallback {_callback} {
    set i [lsearch $mMouseRayCallbacks $_callback]
    if {$i != -1} {
	set mMouseRayCallbacks [lreplace $mMouseRayCallbacks $i $i]
    }
}

::itcl::body Archer::adjustCompNormals {comp} {
    if {[$itk_component(sdb) busy]} {
	return
    }

    #SetWaitCursor

    set savePwd [pwd]
    cd /

    $itk_component(sdb) normalCallback [::itcl::code $this pluginUpdateProgressBar]

    if {[catch {$itk_component(sdb) adjustNormals 0 1 1 0 $comp}]} {
	#SetNormalCursor
	pluginUpdateProgressBar 0
	return
    }

    set mNeedSave 1
    _update_save_mode	

    pluginUpdateProgressBar 0

    set renderMode [dbCmd how -b $comp]
    cd $savePwd

    if {[llength $renderMode] == 2} {
	eval _render $comp $renderMode 1
    }

    #SetNormalCursor
}

::itcl::body Archer::reverseCompNormals {comp} {
    set mNeedSave 1
    _update_save_mode	

    set savePwd [pwd]
    cd /
    eval sdbWrapper reverseNormals 0 1 1 0 $comp
    set renderMode [dbCmd how -b $comp]
    cd $savePwd

    if {[llength $renderMode] == 2} {
	eval _render $comp $renderMode 1
    }
}

::itcl::body Archer::toggleCompNormals {comp} {
    if {[catch {$itk_component(sdb) attr get $comp $mShowNormalsTag} mShowNormals]} {
	$itk_component(sdb) attr set $comp $mShowNormalsTag 1
    } else {
	if {$mShowNormals == 1} {
	    $itk_component(sdb) attr set $comp $mShowNormalsTag 0
	} else {
	    $itk_component(sdb) attr set $comp $mShowNormalsTag 1
	}
    }

    set mNeedSave 1
    _update_save_mode

    _redraw_obj $comp
}

::itcl::body Archer::launchDisplayMenuBegin {dm m x y} {
    set currentDisplay $dm
    tk_popup $m $x $y
    after idle [::itcl::code $this launchDisplayMenuEnd]
}

::itcl::body Archer::launchDisplayMenuEnd {} {
    set currentDisplay ""
}

::itcl::body Archer::getTkColor {r g b} {
    return [format \#%.2x%.2x%.2x $r $g $b]
}

::itcl::body Archer::getRgbColor {tkColor} {
    set rgb [winfo rgb $itk_interior $tkColor]
    return [list \
		[expr {[lindex $rgb 0] / 256}] \
		[expr {[lindex $rgb 1] / 256}] \
		[expr {[lindex $rgb 2] / 256}]]
}

::itcl::body Archer::setSave {} {
    if {$mDbNoCopy || $mDbReadOnly} {
	return
    }

    set mNeedSave 1
    _update_save_mode	
}

::itcl::body Archer::getLastSelectedDir {} {
    return $mLastSelectedDir
}

::itcl::body Archer::registerWizardXmlCallback {callback} {
    # Append if not already there
    if {[lsearch $wizardXmlCallbacks $callback] == -1} {
	lappend wizardXmlCallbacks $callback
    }
}

::itcl::body Archer::unRegisterWizardXmlCallback {callback} {
    set i [lsearch $wizardXmlCallbacks $callback]
    if {$i != -1} {
	set wizardXmlCallbacks [lreplace $wizardXmlCallbacks $i $i]
    }
}

##################################### Archer Commands #####################################
::itcl::body Archer::abort {args} {
    eval sdbWrapper abort 0 0 0 0 $args
}

::itcl::body Archer::adjust {args} {
    eval mgedWrapper adjust 0 1 1 1 $args
}

::itcl::body Archer::adjustNormals {args} {
    eval sdbWrapper adjustNormals 0 0 1 1 $args
}

::itcl::body Archer::blast {args} {
    eval mgedWrapper blast 0 0 0 1 $args
}

::itcl::body Archer::c {args} {
    eval mgedWrapper c 0 1 1 1 $args
}

::itcl::body Archer::cd {args} {
    if {[info exists itk_component(sdb)]} {
	return [eval $itk_component(sdb) cd $args]
    }

    eval ::cd $args
}

::itcl::body Archer::clear {args} {
    eval archerWrapper clear 0 0 0 1 $args

    if {$mShowGroundPlane} {
	_show_ground_plane
    }
}

::itcl::body Archer::comb {args} {
    eval mgedWrapper comb 0 1 1 1 $args
}

::itcl::body Archer::compact {args} {
    eval sdbWrapper compact 0 0 1 0 $args
}

::itcl::body Archer::concat {args} {
    eval mgedWrapper concat 0 0 1 1 $args
}

::itcl::body Archer::copy {args} {
    eval archerWrapper cp 0 0 1 1 $args
}

::itcl::body Archer::copyeval {args} {
    eval mgedWrapper copyeval 0 0 1 1 $args
}

::itcl::body Archer::cp {args} {
    eval archerWrapper cp 0 0 1 1 $args
}

::itcl::body Archer::dbExpand {args} {
    # parse out preceeding options
    set searchType "-glob"
    set options {}
    set objects {}
    set possibleOption 1
    foreach arg $args {
	if {$possibleOption && [regexp -- {^-[a-zA-Z]} $arg]} {
	    if {$arg == "-regexp"} {
		set searchType "-regexp"
	    } else {
		lappend options $arg
	    }
	} else {
	    set possibleOption 0
	    lappend objects $arg
	}
    }

    if {[info exists itk_component(sdb)]} {
	set tobjects {}
	foreach obj $objects {
	    set pdata [$itk_component(sdb) parsePath $obj]
	    set path [lindex $pdata 0]
	    set name [lindex $pdata 1]

	    if {$searchType == "-regexp"} {
		set name "^$name\$"
	    }

	    if {$path == {}} {
		if {$name == {}} {
		    # ignore
		    continue
		}

		set names [$itk_component(sdb) ls /]
		# find indices of matching names
		set mi [lsearch -all $searchType $names $name]

		foreach i $mi {
		    lappend tobjects "/$path/[lindex $names $i]"
#		    lappend tobjects [lindex $names $i]
		}
	    } else {
		if {[catch {$itk_component(sdb) ls "/$path"} names]} {
		    $itk_component(sdb) configure -primitiveLabels {}
		    _refresh_tree
		    SetNormalCursor

		    return ""
		}

		# find indices of matching names
		set mi [lsearch -all $searchType $names $name]

		foreach i $mi {
		    lappend tobjects "/$path/[lindex $names $i]"
		}
	    }
	}
    } else {
	set tobjects {}
	set lsItems [$itk_component(mged) ls -a]
	foreach obj $objects {
	    set pdata [split $obj /]
	    set len [llength $pdata]

	    if {$len == 1} {
		set child $obj

		if {$searchType == "-regexp"} {
		    set child "^$child\$"
		}

		# find indices of matching children
		set mi [lsearch -all $searchType $lsItems $child]

		foreach i $mi {
		    lappend tobjects [lindex $lsItems $i]
		}
	    } else {
		set path [file dirname $obj]
		incr len -1
		set child [lindex $pdata $len]
		incr len -1
		set parent [lindex $pdata $len]
		set children [_get_node_children $parent]

		if {$searchType == "-regexp"} {
		    set child "^$child\$"
		}

		# find indices of matching children
		set mi [lsearch -all $searchType $children $child]

		foreach i $mi {
		    lappend tobjects "/$path/[lindex $children $i]"
		}
	    }
	}
    }

    if {$tobjects != {}} {
	return [list $options $tobjects]
    }

    return [list $options $objects]
}

::itcl::body Archer::decimate {args} {
    eval sdbWrapper decimate 0 0 1 1 $args
    _redraw_obj [lindex $args end]
}

::itcl::body Archer::decimate2 {args} {
    eval sdbWrapper decimate2 0 0 1 1 $args
    _redraw_obj [lindex $args end]
}

::itcl::body Archer::decimateAttr {args} {
    eval sdbWrapper decimateAttr 0 0 0 1 $args
}

::itcl::body Archer::decimateAttr2 {args} {
    eval sdbWrapper decimateAttr2 0 0 0 1 $args
}

::itcl::body Archer::delete {args} {
    if {[info exists itk_component(sdb)]} {
	eval archerWrapper erase 1 0 1 1 $args
	eval archerWrapper delete 1 0 1 1 $args
    } else {
	eval archerWrapper kill 1 0 1 1 $args
    }
}

::itcl::body Archer::draw {args} {
    if {[llength $args] == 0} {
	return
    }

    set i [lsearch $args "-noWaitCursor"]
    if {$i == -1} {
	set wflag 1
    } else {
	set wflag 0
	set args [lreplace $args $i $i]
    }

    if {$wflag} {
	SetWaitCursor
    }

    set optionsAndArgs [eval dbExpand $args]
    set options [lindex $optionsAndArgs 0]
    set objects [lindex $optionsAndArgs 1]
    set tobjects ""

    # remove leading /'s to make the hierarchy widget happy
    foreach obj $objects {
	lappend tobjects [regsub {^/} $obj ""]
    }

    if {[info exists itk_component(sdb)]} {
	if {$mShowGroundPlane && [$itk_component(sdb) isBlankScreen]} {
	    # Temporarily turn off ground plane
	    set saveGroundPlane 1
	    set mShowGroundPlane 0
	    _show_ground_plane
	} else {
	    set saveGroundPlane 0
	}

	set savePwd [pwd]
	cd /

	if {[catch {eval $itk_component(sdb) draw $options $tobjects} ret]} {
	    cd $savePwd

	    # Turn ground plane back on if it was on before the draw
	    if {$saveGroundPlane} {
		set mShowGroundPlane 1
		_show_ground_plane
	    }

	    $itk_component(sdb) configure -primitiveLabels {}
	    _refresh_tree
	    SetNormalCursor

	    if {$tobjects == {}} {
		return ""
	    }

	    return $ret
	}

	cd $savePwd

	# Turn ground plane back on if it was on before the draw
	if {$saveGroundPlane} {
	    set mShowGroundPlane 1
	    _show_ground_plane
	}
    } else {
	if {[catch {eval dbCmd draw $options $tobjects} ret]} {
	    dbCmd configure -primitiveLabels {}
	    _refresh_tree
	    SetNormalCursor

	    return $ret
	}
    }

    dbCmd configure -primitiveLabels {}
    _refresh_tree
    if {$wflag} {
	SetNormalCursor
    }

    return $ret
}

::itcl::body Archer::E {args} {
    eval mgedWrapper E 1 0 0 1 $args
}

::itcl::body Archer::erase {args} {
    if {[llength $args] == 0} {
	return
    }

    SetWaitCursor

    set optionsAndArgs [eval dbExpand $args]
    set options [lindex $optionsAndArgs 0]
    set objects [lindex $optionsAndArgs 1]

    # remove leading /'s to make the hierarchy widget happy
    foreach obj $objects {
	lappend tobjects [regsub {^/} $obj ""]
    }

    if {[info exists itk_component(sdb)]} {
	set savePwd [pwd]
	cd /

	if {[catch {eval dbCmd erase $tobjects} ret]} {
	    cd $savePwd

	    dbCmd configure -primitiveLabels {}
	    _refresh_tree
	    SetNormalCursor

	    if {$tobjects == {}} {
		return ""
	    }

	    return $ret
	}

	cd $savePwd
    } else {
	if {[catch {eval dbCmd erase $tobjects} ret]} {
	    dbCmd configure -primitiveLabels {}
	    _refresh_tree
	    SetNormalCursor

	    return $ret
	}
    }

    dbCmd configure -primitiveLabels {}
    _refresh_tree
    SetNormalCursor
}

::itcl::body Archer::erase_all {args} {
    eval mgedWrapper erase_all 1 0 0 1 $args
}

::itcl::body Archer::ev {args} {
    eval mgedWrapper ev 1 0 0 1 $args
}

::itcl::body Archer::exit {args} {
    Close
}

::itcl::body Archer::exportFg4 {} {
    if {![info exists itk_component(sdb)]} {
	return
    }

    set typelist {
	{"Fastgen 4" {".fg4"}}
	{"All Files" {*}}
    }

    set target [tk_getSaveFile -parent $itk_interior \
	    -initialdir $mLastSelectedDir \
	    -title "Open Fastgen 4" \
	    -filetypes $typelist]
    
    if {$target == ""} {
	return
    } else {
	set mLastSelectedDir [file dirname $target]
    }

    SetWaitCursor

    # Make sure we're in the root directory/group
    set savePwd [pwd]
    cd /

    $itk_component(sdb) exportFg4 $target
    cd $savePwd

    SetNormalCursor
}

::itcl::body Archer::exportStl {} {
    if {![info exists itk_component(sdb)]} {
	return
    }

    set stlDir [tk_chooseDirectory -parent $itk_interior \
	    -title "Open STL Directory" \
	    -initialdir $mLastSelectedDir]

    if {$stlDir == ""} {
	return
    }

    if {![file exists $stlDir]} {
	file mkdir $stlDir
    } elseif {[file type $stlDir] != "directory"} {
	::sdialogs::Stddlgs::errordlg "STL Error" \
		"$stlDir must be a directory"
	return
    }

    set mLastSelectedDir $stlDir
    SetWaitCursor

    # Make sure we're in the root directory/group
    set savePwd [pwd]
    cd /

    $itk_component(sdb) exportStl $stlDir
    cd $savePwd

    SetNormalCursor
}

::itcl::body Archer::exportVrml {} {
    if {![info exists itk_component(sdb)]} {
	return
    }

    set typelist {
	{"VRML" {".vrml"}}
	{"All Files" {*}}
    }

    set target [tk_getSaveFile -parent $itk_interior \
	    -initialdir $mLastSelectedDir \
	    -title "Open VRML" \
	    -filetypes $typelist]
    
    if {$target == ""} {
	return
    } else {
	set mLastSelectedDir [file dirname $target]
    }

    SetWaitCursor

    # Make sure we're in the root directory/group
    set savePwd [pwd]
    cd /

    $itk_component(sdb) exportVrml $target
    cd $savePwd

    SetNormalCursor
}

::itcl::body Archer::find {args} {
    if {![info exists itk_component(mged)]} {
	return
    }

    eval $itk_component(mged) find $args
}

::itcl::body Archer::g {args} {
    eval group $args
}

::itcl::body Archer::get {args} {
    if {[info exists itk_component(mged)]} {
	return [eval $itk_component(mged) get $args]
    }

    if {[info exists itk_component(sdb)]} {
	return [eval $itk_component(sdb) get $args]
    }

    return ""
}

::itcl::body Archer::group {args} {
    if {[info exists itk_component(sdb)]} {
	eval archerWrapper group 0 1 1 1 $args
    } else {
	eval archerWrapper g 0 1 1 1 $args
    }
}

::itcl::body Archer::hide {args} {
    eval mgedWrapper hide 0 0 1 1 $args
}

::itcl::body Archer::importFg4 {} {
    if {![info exists itk_component(sdb)]} {
	return
    }

    set typelist {
	{"Fastgen 4" {".fg4"}}
	{"All Files" {*}}
    }

    set target [tk_getOpenFile -parent $itk_interior \
	    -initialdir $mLastSelectedDir \
	    -title "Import Fastgen 4" \
	    -filetypes $typelist]
    
    if {$target == ""} {
	return
    } else {
	set mLastSelectedDir [file dirname $target]
    }

    SetWaitCursor

    # Make sure we're in the root directory/group
    set savePwd [pwd]
    cd /

    set i 0
    set topGroup $mSdbTopGroup

    # Find an unused toplevel group to use for the import
    while {![catch {$itk_component(sdb) get $topGroup}]} {
	incr i
	set topGroup $mSdbTopGroup$i
    }

    group $topGroup
    $itk_component(sdb) importFg4 $target $topGroup
    $itk_component(sdb) categorizeFg4 $topGroup
    cd $savePwd

    _refresh_tree

    set mNeedSave 1
    _update_save_mode
    SetNormalCursor
}

::itcl::body Archer::importStl {importDir} {
    if {![info exists itk_component(sdb)]} {
	return
    }

    if {$importDir} {
	set stlDir [tk_chooseDirectory -parent $itk_interior \
		-title "Import STL Directory" \
		-initialdir $mLastSelectedDir]

	if {$stlDir == ""} {
	    return
	}

	if {![file exists $stlDir]} {
	    ::sdialogs::Stddlgs::errordlg "STL Error" \
		"$stlDir does not exist"
	    return
	}

	if {[file type $stlDir] != "directory"} {
	    ::sdialogs::Stddlgs::errordlg "STL Error" \
		"$stlDir must be a directory"
	    return
	}

	set mLastSelectedDir $stlDir
    } else {
	set typelist {
	    {"STL" {".stl"}}
	    {"All Files" {*}}
	}

	set stlDir [tk_getOpenFile -parent $itk_interior \
			-title "Import STL" \
			-initialdir $mLastSelectedDir \
			-filetypes $typelist]

	if {$stlDir == ""} {
	    return
	} else {
	    set mLastSelectedDir [file dirname $stlDir]
	}

	if {![file exists $stlDir]} {
	    ::sdialogs::Stddlgs::errordlg "STL Error" \
		"$stlDir does not exist"
	    return
	}
    }

    SetWaitCursor

    # Make sure we're in the root directory/group
    set savePwd [pwd]
    cd /

    set i 0
    set topGroup $mSdbTopGroup

    # Find an unused toplevel group to use for the import
    while {![catch {$itk_component(sdb) get $topGroup}]} {
	incr i
	set topGroup $mSdbTopGroup$i
    }

    group $topGroup
    $itk_component(sdb) importStl $stlDir $topGroup
    if [catch {$itk_component(sdb) get $topGroup}] {
	set msg "No STL files were imported."

	if {$importDir} {
	    append msg "\n\nPlease make sure the directory structure\n" \
		    "follows the necessary guidlines."
	}
	::sdialogs::Stddlgs::warningdlg "STL Warning" \
		$msg
    } else {
	$itk_component(sdb) categorizeFg4 $topGroup

	_refresh_tree

	set mNeedSave 1
	_update_save_mode
    }
    cd $savePwd

    SetNormalCursor
}

## - importFg4Sections
#
#
# Note - it's up to the caller to delete existing combinatons/
#        groups if desired. However, if a leaf solid already
#        exists an error will be thrown. For the moment its also
#        up to the caller to remove like-named members from any
#        relevant groups/regions.
#
::itcl::body Archer::importFg4Sections {slist wlist delta} {
    if {[expr {[llength $wlist] % 2}] != 0} {
	error "importFg4Sections: wlist must have an even number of elements"
    }

    set wi2 [lsearch $wlist WizardTop]
    if {$wi2 != -1} {
	set wi1 $wi2
	incr wi2
	set wizTop [lindex $wlist $wi2]
	set wlist [lreplace $wlist $wi1 $wi2]
    } else {
	error "importFg4Sections: wlist is missing the WizardTop key and its corresponding value"
    }

    if {[info exists itk_component(mged)]} {
	SetWaitCursor
	set savedUnits [$itk_component(mged) units]
	$itk_component(mged) units in
	$itk_component(mged) configure -autoViewEnable 0
	$itk_component(mged) detachObservers

	set pname ""
	set firstName ""
	set lastName ""
	foreach section $slist {
	    set pname [lindex $section 0]
	    set sdata [lindex $section 1]
	    set attrList [lrange $section 2 end]

	    set names [split $pname "/"]
	    set firstName [lindex $names 0]
	    set lastName [lindex $names end]
	    set gnames [lrange $names 0 end-1]

	    if {[regexp {(.*\.)r([0-9]*)$} $lastName all s1 s2]} {
		set regionName $lastName
		set solidName $s1\s$s2
	    } else {
		set regionName "$lastName\.r"
		set solidName "$lastName\.s"
	    }

	    if {![catch {$itk_component(mged) get $solidName} ret]} {
		#$itk_component(mged) attachObservers
		$itk_component(mged) units $savedUnits
		error "importFg4Sections: $solidName already exists!"
	    }

	    if {[catch {$itk_component(mged) importFg4Section $solidName $sdata} ret]} {
		#$itk_component(mged) attachObservers
		$itk_component(mged) units $savedUnits
		error "importFg4Sections: $ret"
	    }

	    eval $itk_component(mged) otranslate $solidName $delta

	    # Add to the region
	    $itk_component(mged) r $regionName u $solidName

	    if {$firstName == $lastName} {
		continue
	    }

	    # reverse the list
	    set reversedGnames {}
	    foreach item $gnames {
		set reversedGnames [linsert $reversedGnames 0 $item]
	    }

	    set gmember $regionName
	    foreach gname $reversedGnames {
		if {[catch {$itk_component(mged) get $gname} ret]} {
		    $itk_component(mged) g $gname $gmember
		} else {
		    if {[catch {$itk_component(mged) get $gname tree} tree]} {
			#$itk_component(mged) attachObservers
			$itk_component(mged) units $savedUnits
			error "importFg4Sections: $gname is not a group!"
		    }

		    # Add gmember only if its not already there
		    set tmembers [regsub -all {(\{[ul] )|([{}]+)} $tree " "]
		    if {[lsearch $tmembers $gmember] == -1} {
			$itk_component(mged) g $gname $gmember
		    }
		}

		# Add WizardTop attribute
		$itk_component(mged) attr set $gname WizardTop $wizTop

		set gmember $gname
	    }

	    # Add WizardTop attribute to the region and its solid
	    $itk_component(mged) attr set $regionName WizardTop $wizTop
	    $itk_component(mged) attr set $solidName WizardTop $wizTop

	    # Add wizard attributes
	    #foreach {key val} $wlist {
	    #$itk_component(mged) attr set $wizTop $key $val
	    #}

	    # Add other attributes that are specific to this region
	    foreach {key val} $attrList {
		$itk_component(mged) attr set $regionName $key $val
	    }

	    if {[catch {$itk_component(mged) attr get $regionName transparency} tr]} {
		set tr 1.0
	    } else {
		if {![string is double $tr] || $tr < 0.0 || 1.0 < $tr} {
		    set tr 1.0
		}
	    }

	    if {[catch {$itk_component(mged) attr get $regionName vmode} vmode]} {
		set vmode 0
	    } else {
		switch -- $vmode {
		    "shaded" {
			set vmode 2
		    }
		    default {
			set vmode 0
		    }
		}
	    }

	    _render $pname\.r $vmode $tr 0 0
	}

	# Add wizard attributes
	foreach {key val} $wlist {
	    $itk_component(mged) attr set $wizTop $key $val
	}

	_refresh_tree
#	if {$firstName != ""} {
#	    if {$firstName == $lastName} {
#		after idle [::itcl::code $this _select_node [$itk_component(tree) find $regionName] 0]
#	    } else {
#		after idle [::itcl::code $this _select_node [$itk_component(tree) find $firstName] 0]
#	    }
#	}

	set mNeedSave 1
	_update_save_mode
	$itk_component(mged) units $savedUnits
	$itk_component(mged) attachObservers
	$itk_component(mged) refreshAll
	$itk_component(mged) configure -autoViewEnable 1
	SetNormalCursor
    }
}

::itcl::body Archer::kill {args} {
    if {[info exists itk_component(sdb)]} {
	eval archerWrapper delete 1 0 1 1 $args
    } else {
	eval archerWrapper kill 1 0 1 1 $args
    }
}

::itcl::body Archer::killall {args} {
    eval mgedWrapper killall 1 0 1 1 $args
}

::itcl::body Archer::killtree {args} {
    eval mgedWrapper killtree 1 0 1 1 $args
}

::itcl::body Archer::ls {args} {
    if {[info exists itk_component(sdb)]} {
	if {$args == {}} {
	    return [eval archerWrapper ls 0 0 0 0 $args]
	} else {
	    set optionsAndArgs [eval dbExpand $args]
	    set options [lindex $optionsAndArgs 0]
	    set expandedArgs [lindex $optionsAndArgs 1]
	    set len [llength $expandedArgs]

	    if {$options == {} && $len == 1} {
		set arg [lindex $expandedArgs 0]
		if {[catch {$itk_component(sdb) get "/$arg"} odata]} {
		    return [eval archerWrapper ls 0 0 0 0 $args]
		    #return "ls: $arg - no such file or directory"
		} else {
		    set eflag [regexp {[*\[\]]} $args]
		    set type [lindex $odata 0]
		    if {$eflag == 0 && $type == "group"} {
			set results [eval archerWrapper ls 0 0 0 0 "/$arg"]

			# Remove paths
			set tmpArgs {}
			foreach arg $results {
			    lappend tmpArgs [file tail $arg]
			}

			return [lsort $tmpArgs]
		    } else {
			return $arg
		    }
		}
	    } else {
		# Remove paths
		set tmpArgs {}
		foreach arg $expandedArgs {
		    lappend tmpArgs [file tail $arg]
		}

		return [lsort $tmpArgs]
	    }
	}
    } else {
	if {$args == {}} {
	    eval archerWrapper ls 0 0 0 0 $args
	} else {
	    set optionsAndArgs [eval dbExpand $args]
	    set options [lindex $optionsAndArgs 0]
	    set expandedArgs [lindex $optionsAndArgs 1]

	    if {$options == {}} {
		return $expandedArgs
	    } else {
		return [eval archerWrapper ls 0 0 0 0 $args]
	    }
	}
    }
}

::itcl::body Archer::make_bb {args} {
    eval mgedWrapper make_bb 0 0 1 1 $args
}

::itcl::body Archer::move {args} {
    if {[info exists itk_component(sdb)]} {
	eval archerWrapper move 0 1 1 $args
    } else {
	eval archerWrapper mv 0 0 1 1 $args
    }
}

::itcl::body Archer::mv {args} {
    eval archerWrapper mv 0 0 1 1 $args
}

::itcl::body Archer::mvall {args} {
    eval mgedWrapper mvall 0 0 1 1 $args
}

::itcl::body Archer::ocenter {args} {
    if {[llength $args] == 4} {
	eval archerWrapper ocenter 0 0 1 0 $args
	#_redraw_obj $mSelectedObjPath 0
	_redraw_obj [lindex $args 0] 0
    } else {
	eval archerWrapper ocenter 0 0 0 0 $args
    }
}

::itcl::body Archer::handleObjCenter {obj x y} {
    set vcenter [dbCmd screen2view $x $y]
    if {[info exists itk_component(mged)]} {
	set ocenter [dbCmd ocenter $obj]
    } else {
	set savePwd [pwd]
	cd /
	set ocenter [dbCmd ocenter $obj]
    }
    set ovcenter [eval dbCmd m2vPoint $ocenter]

    # This is the update view center (i.e. we keep the original view Z)
    set vcenter [list [lindex $vcenter 0] [lindex $vcenter 1] [lindex $ovcenter 2]]

    set ocenter [eval dbCmd v2mPoint $vcenter]

    if {[info exists itk_component(mged)]} {
	eval archerWrapper ocenter 0 0 1 0 $obj $ocenter
    } else {
	eval archerWrapper ocenter 0 0 1 0 $obj $ocenter
	cd $savePwd
    }

    _redraw_obj $mSelectedObjPath 0
}

::itcl::body Archer::handleObjRotate {obj rx ry rz kx ky kz} {
    if {[info exists itk_component(mged)]} {
	eval archerWrapper orotate 0 0 1 0 $obj $rx $ry $rz
    } else {
	set savePwd [pwd]
	cd /
	eval archerWrapper orotate 0 0 1 0 $obj $rx $ry $rz $kx $ky $kz
	cd $savePwd
    }

    _redraw_obj $mSelectedObjPath 0
}

::itcl::body Archer::handleObjScale {obj sf kx ky kz} {
    if {[info exists itk_component(mged)]} {
	eval archerWrapper oscale 0 0 1 0 $obj $sf
    } else {
	set savePwd [pwd]
	cd /
	eval archerWrapper oscale 0 0 1 0 $obj $sf $kx $ky $kz
	cd $savePwd
    }

    _redraw_obj $mSelectedObjPath 0
}

::itcl::body Archer::handleObjTranslate {obj dx dy dz} {
    if {[info exists itk_component(mged)]} {
	eval archerWrapper otranslate 0 0 1 0 $obj $dx $dy $dz
    } else {
	set savePwd [pwd]
	cd /
	eval archerWrapper otranslate 0 0 1 0 $obj $dx $dy $dz
	cd $savePwd
    }

    _redraw_obj $mSelectedObjPath 0
}

::itcl::body Archer::orotate {obj rx ry rz kx ky kz} {
    eval archerWrapper orotate 0 0 1 0 $obj $rx $ry $rz $kx $ky $kz
    #_redraw_obj $mSelectedObjPath 0
    _redraw_obj $obj 0
}

::itcl::body Archer::oscale {obj sf kx ky kz} {
    eval archerWrapper oscale 0 0 1 0 $obj $sf $kx $ky $kz
    #_redraw_obj $mSelectedObjPath 0
    _redraw_obj $obj 0
}

::itcl::body Archer::otranslate {obj dx dy dz} {
    eval archerWrapper otranslate 0 0 1 0 $obj $dx $dy $dz
    #_redraw_obj $mSelectedObjPath 0
    _redraw_obj $obj 0
}

::itcl::body Archer::push {args} {
    eval mgedWrapper push 0 1 1 0 $args
}

::itcl::body Archer::purgeObjHistory {obj} {
    # Nothing to do
    if {[catch {dbCmd attr get $obj history} hobj]} {
	return
    }

    # Remove obj's history attribute
    $itk_component(mged) attr rm $obj history

    # March backwards in the list removing obj's history
    if {![catch {dbCmd attr get $hobj previous} prev]} {
	while {$prev != ""} {
	    if {[catch {dbCmd attr get $prev previous} pprev]} {
		set pprev ""
	    }

	    $itk_component(mged) kill $prev
	    set prev $pprev
	}
    }

    # March forward in the list removing obj's history
    if {![catch {dbCmd attr get $hobj next} next]} {
	while {$next != ""} {
	    if {[catch {dbCmd attr get $next next} nnext]} {
		set nnext ""
	    }

	    $itk_component(mged) kill $next
	    set next $nnext
	}
    }

    $itk_component(mged) kill $hobj
}

::itcl::body Archer::purgeHistory {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    foreach obj [$itk_component(mged) ls] {
	set obj [regsub {(/)|(/R)} $obj ""]
	purgeObjHistory $obj
    }

    _update_prev_obj_button ""
    _update_next_obj_button ""

    set selNode [$itk_component(tree) query -selected]
    if {$selNode != ""} {
	set selNodePath [$itk_component(tree) query -path $selNode]
	_toggle_tree_path $selNodePath
    }

    set mNeedSave 1
    _update_save_mode
}

::itcl::body Archer::put {args} {
    eval mgedWrapper put 0 0 1 1 $args
}

::itcl::body Archer::pwd {} {
    if {[info exists itk_component(sdb)]} {
	return [eval $itk_component(sdb) pwd]
    }

    ::pwd
}

::itcl::body Archer::r {args} {
    eval mgedWrapper r 0 1 1 1 $args
}

::itcl::body Archer::report {args} {
    eval mgedWrapper report 0 0 0 0 $args
}

::itcl::body Archer::reverseNormals {args} {
    eval sdbWrapper reverseNormals 0 0 1 1 $args
}

::itcl::body Archer::rm {args} {
    if {[info exists itk_component(sdb)]} {
	eval archerWrapper rm 1 0 1 1 $args
    } else {
	eval archerWrapper rm 0 0 1 1 $args
    }
}

::itcl::body Archer::track {args} {
    eval mgedWrapper track 0 0 1 1 $args
}

::itcl::body Archer::unhide {args} {
    eval mgedWrapper unhide 0 0 1 1 $args
}

::itcl::body Archer::units {args} {
    if {[info exists itk_component(sdb)]} {
	return "in"
    }

    eval mgedWrapper units 0 0 1 0 $args
}

::itcl::body Archer::packTree {data} {
    if {$data == ""} {
	return ""
    }

    set lines [split $data "\n"]
    set nlines [llength $lines]

    if {$nlines == 1} {
	set line [lindex $lines 0]

	set len [llength $line]

	if {$len == 2} {
	    return "l [lindex $line 1]"
	} elseif {$len == 3} {
	    return "l [lindex $line 1] [list [lindex $line 2]]"
	}

	error "packTree: malformed data - $data"
    }

    set tree ""

    set line [lindex $lines 0]
    set len [llength $line]
    if {$len == 2} {
	set tree "l [lindex $line 1]"
    } elseif {$len == 18} {
	set tree "l [lindex $line 1] [lrange $line 2 end]"
    } else {
#	error "packTree: malformed line - $line"
    }

    for {set n 1} {$n < $nlines} {incr n} {
	set line [string trim [lindex $lines $n]]

	# Ignore blank lines
	if {$line == ""} {
	    continue
	}

	set len [llength $line]
	if {$len == 2} {
	    set tree "[lindex $line 0] [list $tree] [list [list l [lindex $line 1]]]"
	} elseif {$len == 18} {
	    set tree "[lindex $line 0] [list $tree] [list [list l [lindex $line 1] [lrange $line 2 end]]]"
	} else {
#	    error "packTree: malformed line - $line"
	    continue
	}
    }

    return $tree
}

::itcl::body Archer::unpackTree {tree} {
    return " u [unpackTreeGuts $tree]"
}

::itcl::body Archer::unpackTreeGuts {tree} {
    if {$tree == ""} {
	return ""
    }

    if {[llength $tree] == 2} {
	return [lindex $tree 1]
    }

    if {[llength $tree] != 3} {
	error "unpackTree: tree is malformed - $tree!"
    }

    set op [lindex $tree 0]

    if {$op == "l"} {
	return "[lindex $tree 1]\t[lindex $tree 2]"
    } else {
	if {$op == "n"} {
	    set op "+"
	}

	set partA [unpackTreeGuts [lindex $tree 1]]
	set partB [unpackTreeGuts [lindex $tree 2]]

	return "$partA
 $op $partB"
    }
}

::itcl::body Archer::vdraw {args} {
    eval mgedWrapper vdraw 0 0 0 0 $args
}

::itcl::body Archer::whichid {args} {
    if {[info exists itk_component(mged)]} {
	return [eval $itk_component(mged) whichid $args]
    }

    return {}
}

::itcl::body Archer::who {args} {
    if {[info exists itk_component(sdb)] &&
	$args == ""} {
	$itk_component(sdb) who p
    } else {
	eval dbCmd who $args
    }
}

::itcl::body Archer::Z {args} {
    eval archerWrapper clear 0 0 0 1 $args
}

::itcl::body Archer::zap {args} {
    eval archerWrapper clear 0 0 0 1 $args
}

::itcl::body Archer::updateDisplaySettings {} {
    if {[info exists itk_component(mged)]} {
	set cadObj $itk_component(mged)
    } elseif {[info exists itk_component(sdb)]} {
	set cadObj $itk_component(sdb)
    } else {
	return
    }

    switch -- $mZClipMode \
	$ZCLIP_SMALL_CUBE { \
	    $cadObj zclipAll 1; \
	    $cadObj boundsAll {-4096 4095 -4096 4095 -4096 4095}; \
	} \
	$ZCLIP_MEDIUM_CUBE { \
	    $cadObj zclipAll 1; \
	    $cadObj boundsAll {-8192 8191 -8192 8191 -8192 8191}; \
	} \
	$ZCLIP_LARGE_CUBE { \
	    $cadObj zclipAll 1; \
	    $cadObj boundsAll {-16384 16363 -16384 16363 -16384 16363}; \
	} \
	$ZCLIP_NONE { \
	    $cadObj zclipAll 0; \
	}
}


##################################### Plugin Commands #####################################
::itcl::body Archer::initArcher {} {
    # Load plugins
    pluginLoader
}

::itcl::body Archer::pluginUpdateSaveMode {mode} {
    if {1 <= $mode} {
	set mode 1
    } else {
	set mode 0
    }

    set mNeedSave $mode
    _update_save_mode
}

::itcl::body Archer::pluginUpdateProgressBar {percent} {
    set mProgressBarWidth [winfo width $itk_component(progress)]
    set mProgressBarHeight [winfo height $itk_component(progress)]

    set mProgress $percent
    $itk_component(progress) delete progressBar
    if {$mProgress != 0} {
	set x [expr {$mProgressBarWidth * $mProgress}]
	$itk_component(progress) create rectangle 0 0 $x $mProgressBarHeight \
	    -tags progressBar \
	    -fill blue
    }

    ::update
}

::itcl::body Archer::pluginUpdateStatusBar {msg} {
    set mStatusStr $msg
    ::update
}

::itcl::body Archer::pluginGetMinAllowableRid {} {
    if {![info exists itk_component(mged)]} {
	return 0
    }

    set maxRid 0
    foreach {rid rname} [$itk_component(mged) rmap] {
	if {$maxRid < $rid} {
	    set maxRid $rid
	}
    }

    return [expr {$maxRid + 1}]
}

::itcl::body Archer::pluginQuery {name} {
    foreach plugin $::Archer::plugins {
	if {[$plugin get -class] == $name} {
	    return $plugin
	}
    }

    return ""
}

::itcl::body Archer::pluginRegister {majorType minorType name class file \
				 {description ""} \
				 {version ""} \
				 {developer ""} \
				 {icon ""} \
				 {tooltip ""} \
				 {action ""} \
				 {xmlAction ""}} {
    lappend ::Archer::plugins [Plugin \#auto \
				   $majorType \
				   $minorType \
				   $name \
				   $class \
				   $file \
				   $description \
				   $version \
				   $developer \
				   $icon \
				   $tooltip \
				   $action \
				   $xmlAction]
}

::itcl::body Archer::pluginUnregister {name} {
    set plugin [plugin_query $name]

    # plugin not registered
    if {$plugin == ""} {
	return
    }

    set idx [lsearch -exact $::Archer::plugins $plug]
    set ::Archer::plugins [lreplace $::Archer::plugins $idx $idx ""]
}

::itcl::body Archer::pluginLoader {} {
    global env

    set pwd [::pwd]

    # developer & user plugins
#    foreach plugindir [list [file join $env(ARCHER_HOME) $brlcadDataPath plugins archer]]
    foreach plugindir [list [file join $brlcadDataPath plugins archer]] {
	::cd $plugindir
	pluginLoadCWDFiles
    }

    ::cd $pwd
}

## - pluginLoadCWDFiles
#
# Load the current working directory's (CWD) files.
#
::itcl::body Archer::pluginLoadCWDFiles {} {
    foreach filename [lsort [glob -nocomplain *]] {
	if [file isfile $filename] {
	    set ext [file extension $filename]
	    switch -exact -- $ext {
		".tcl" -
		".itk" -
		".itcl" {
		    uplevel \#0 source $filename
		}
		".sh" {
		    # silently ignore
		}
		default {
		    # silently ignore
		}
	    }
	}
    }
}

::itcl::body Archer::pluginDialog {w} {
    set dialog [::iwidgets::dialog .\#auto \
		    -modality application \
		    -title "Plug-in Information"]

    # For some reason this hoses the centering of the dialog.
    #$dialog delete Apply
    #$dialog delete Cancel
    #$dialog delete Help
    # This does not foul up the dialog centering.
    $dialog hide 1
    $dialog hide 2
    $dialog hide 3

    $dialog configure \
	-thickness 2 \
	-buttonboxpady 0
    $dialog buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0

    # ITCL can be nasty
    set win [$dialog component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$dialog childsite]

    # scrolled frame
    set sf [::iwidgets::scrolledframe $parent.sf \
		-borderwidth 1 \
		-relief sunken \
		-width 5i \
		-height 3i \
		-hscrollmode dynamic \
		-vscrollmode dynamic \
		-labeltext "Installed Plug-ins" \
		-labelfont [list $SystemWindowFont 10 bold] \
		-labelpos n]

    set i 0
    foreach plugin $::Archer::plugins {
	# seperator
	if {$i > 0} {
	    frame [$sf childsite].sep$i -borderwidth 2 -height 2 -relief sunken
	    pack [$sf childsite].sep$i -padx 2 -pady 4 -fill x
	}

	# frame
	set frm_data [frame [$sf childsite].frm$i]

	# labels
	label $frm_data.lbl0 -text "Type:" -justify left
	label $frm_data.lbl1 -text [$plugin get -majorType] -justify left
	label $frm_data.lbl2 -text "Name:" -justify left
	label $frm_data.lbl3 -text [$plugin get -name] -justify left
	label $frm_data.lbl4 -text "Version:" -justify left
	label $frm_data.lbl5 -text [$plugin get -version] -justify left
	label $frm_data.lbl6 -text "Filename:" -justify left
	label $frm_data.lbl7 -text [$plugin get -filename] -justify left
	label $frm_data.lbl8 -text "Developer:" -justify left
	label $frm_data.lbl9 -text [$plugin get -developer] -justify left
	label $frm_data.lbl10 -text "Description:" -justify left
	label $frm_data.lbl11 -text [$plugin get -description] -justify left
#	label $frm_data.lbl12 -text "Action:" -justify left
#	label $frm_data.lbl13 -text [$plugin get -action] -justify left

	# gridding
	set row 0
	grid $frm_data.lbl0 -column 0 -row $row -sticky nw
	grid $frm_data.lbl1 -column 1 -row $row -sticky nw
	incr row
	grid $frm_data.lbl2 -column 0 -row $row -sticky nw
	grid $frm_data.lbl3 -column 1 -row $row -sticky nw
	incr row
	grid $frm_data.lbl4 -column 0 -row $row -sticky nw
	grid $frm_data.lbl5 -column 1 -row $row -sticky nw
	incr row
	grid $frm_data.lbl6 -column 0 -row $row -sticky nw
	grid $frm_data.lbl7 -column 1 -row $row -sticky nw
	incr row
	grid $frm_data.lbl8 -column 0 -row $row -sticky nw
	grid $frm_data.lbl9 -column 1 -row $row -sticky nw
	incr row
	grid $frm_data.lbl10 -column 0 -row $row -sticky nw
	grid $frm_data.lbl11 -column 1 -row $row -sticky nw
	incr row
#	grid $frm_data.lbl12 -column 0 -row $row -sticky nw
#	grid $frm_data.lbl13 -column 1 -row $row -sticky nw
	pack $frm_data -anchor w

	incr i
    }

    pack $sf -padx 2 -pady 2 -expand yes -fill both

    $dialog buttonconfigure OK -command "$dialog deactivate; destroy $dialog"
    wm protocol $dialog WM_DELETE_WINDOW "$dialog deactivate; destroy $dialog"
    wm geometry $dialog 400x400

    # Event bindings
    bind $dialog <Enter> "raise $dialog"
    bind $dialog <Configure> "raise $dialog"
    bind $dialog <FocusOut> "raise $dialog"

    $dialog center $w
    $dialog activate
}

::itcl::body Archer::pluginMged {_archer} {
    if {[catch {$_archer component mged} mged]} {
	return ""
    } else {
	return $mged
    }
}

::itcl::body Archer::pluginSdb {_archer} {
    if {[catch {$_archer component sdb} sdb]} {
	return ""
    } else {
	return $sdb
    }
}

if {$Archer::methodImpls != ""} {
    foreach impl $::Archer::methodImpls {
	eval $impl
    }
}

Archer::initArcher

if {$Archer::corePluginInit != ""} {
    foreach cpi $::Archer::corePluginInit {
	eval $cpi
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
