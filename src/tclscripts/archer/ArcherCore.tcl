#                      A R C H E R C O R E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2008 United States Government as represented by
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
#    This is a BRL-CAD Application Core mega-widget.
#

if {![info exists env(ARCHER_HOME)]} {
    if {[info exists argv0]} {
	if [catch { set env(ARCHER_HOME) [file normalize [file join [file dir $argv0] ..]] }] {
	    set env(ARCHER_HOME) .
	}
    } else {
	set env(ARCHER_HOME) .
    }
}

LoadArcherCoreLibs
package provide ArcherCore 1.0

namespace eval ArcherCore {
    if {![info exists parentClass]} {
	set parentClass itk::Toplevel
	set inheritFromToplevel 1

#	if {$tcl_platform(platform) == "windows"} {
#	    set parentClass itk::Toplevel
#	    set inheritFromToplevel 1
#	} else {
#	    set parentClass TabWindow
#	    set inheritFromToplevel 0
#	}
    }
}

::itcl::class ArcherCore {
    inherit $ArcherCore::parentClass

    itk_option define -quitcmd quitCmd Command {}
    itk_option define -master master Master "."
    itk_option define -primaryToolbar primaryToolbar PrimaryToolbar 1
    itk_option define -viewToolbar viewToolbar ViewToolbar 1
    itk_option define -statusbar statusbar Statusbar 1

    constructor {{_viewOnly 0} {_noCopy 0} args} {}
    destructor {}

    public {
	common application ""
	common splash ""
	common showWindow 0

	proc packTree              {_data}
	proc unpackTree            {_tree}

	# public window commands
	method dockArea            {{_position "south"}}
	method primaryToolbarAddBtn     {_name {args ""}}
	method primaryToolbarAddSep     {_name {args ""}}
	method primaryToolbarRemoveItem  {_index}
	method closeHierarchy {}
	method openHierarchy {{_fraction 30}}
	method refreshTree {{_restore 1}}
	method mouseRay {_dm _x _y}
	method shootRay {_start _op _target _prep _no_bool _onehit}
	method addMouseRayCallback {_callback}
	method deleteMouseRayCallback {_callback}

	# public database commands
	method dbCmd               {args}
	method cmd                 {args}
	method png                 {_filename}

	# general
	method UpdateTheme         {_theme} {set mTheme $_theme; updateTheme}
	method Load                {_filename}
	method GetUserCmds         {}
	method WhatsOpen           {}
	method Close               {}
	method askToSave           {}
	method getTkColor          {_r _g _b}
	method getRgbColor         {_tkColor}
	method setSave {}
	method getLastSelectedDir  {}
	method refreshDisplay      {}

	# Commands exposed to the user via the command line.
	# More to be added later...
	method adjust              {args}
	method arced               {args}
	method attr                {args}
	method blast               {args}
	method c                   {args}
	method cd                  {args}
	method clear               {args}
	method comb                {args}
	method comb_color          {args}
	method concat              {args}
	method copy                {args}
	method copyeval            {args}
	method cp                  {args}
	method dbExpand	           {args}
	method delete              {args}
	method draw                {args}
	method E                   {args}
	method edcomb              {args}
	method edmater             {args}
	method erase               {args}
	method erase_all           {args}
	method ev                  {args}
	method exit                {args}
	method find                {args}
	method hide                {args}
	method g                   {args}
	method get                 {args}
	method group               {args}
	method i                   {args}
	method item                {args}
	method kill                {args}
	method killall             {args}
	method killtree            {args}
	method ls                  {args}
	method make		   {args}
	method make_bb             {args}
	method make_name           {args}
	method mater               {args}
	method mirror              {args}
	method move                {args}
	method mv                  {args}
	method mvall               {args}
	method ocenter		   {args}
	method orotate		   {_obj _rx _ry _rz _kx _ky _kz}
	method oscale		   {_obj _sf _kx _ky _kz}
	method otranslate	   {_obj _dx _dy _dz}
	method push                {args}
	method put                 {args}
	method pwd                 {}
	method r                   {args}
	method report              {args}
	method rm                  {args}
	method rmater              {args}
	method shader              {args}
	method track               {args}
	method unhide              {args}
	method units               {args}
	method vdraw               {args}
	method whichid             {args}
	method who                 {args}
	method wmater              {args}
	method Z                   {args}
	method zap                 {args}

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

	common MEASURING_STICK "cad_measuring_stick"

	set brlcadDataPath [bu_brlcad_data ""]
	if {$tcl_platform(platform) != "windows"} {
	    set SystemWindowFont Helvetica
	    set SystemWindowText black
	    set SystemWindow \#d9d9d9
	    set SystemHighlight black
	    set SystemHighlightText \#ececec
	    set SystemButtonFace \#d9d9d9
	} else {
	    set SystemWindowFont SystemWindowText
	    set SystemWindowText SystemWindowText
	    set SystemWindow SystemWindow
	    set SystemHighlight SystemHighlight
	    set SystemHighlightText SystemHighlightText
	    set SystemButtonFace SystemButtonFace
	}
    }

    protected {
	proc unpackTreeGuts      {_tree}

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
	variable mVPaneFraction1 10
	variable mVPaneFraction2 90
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

	variable mCadCommands { \
				    cd clear copy cp dbExpand delete draw exit \
				    g get group kill ls move mv ocenter orotate \
				    oscale otranslate packTree pwd rm units \
				    whichid who unpackTree Z zap
	}
	variable mMgedCommands { \
				     bot2pipe \
				     adjust arced attr blast c comb comb_color concat copyeval E edcomb \
				     edmater erase_all ev find hide item killall killtree make \
				     make_bb make_name mater mirror mvall push put r rmater report \
				     shader track unhide vdraw wmater
	}
	variable mDbSpecificCommands {}
	variable mUnwrappedDbCommands {}
	variable mBannedDbCommands {
	    dbip nmg_collapse nmb_simplify
	    open move_arb_edge move_arb_face
	    shells xpush illum label qray
	    rotate_arb_face rtabort
	    shaded_mode png
	}

	variable mMouseOverrideInfo "
Translate     Shift-Right
Rotate        Shift-Left
Scale         Shift-Middle
Center        Shift-Ctrl-Right
Popup Menu    Right or Ctrl-Left
"

	variable mColorList {Grey Black Blue Cyan Green Magenta Red White Yellow Triple}
	variable mColorListNoTriple {Grey Black Blue Cyan Green Magenta Red White Yellow}
	variable mDefaultNodeColor {150 150 150}

	variable mDoStatus 1
	variable mDbName ""
	variable mDbUnits ""
	variable mDbTitle ""

	variable mCurrentDisplay ""

	variable mMouseRayCallbacks ""

	method cadWrapper {_cmd _eflag _hflag _sflag _tflag args}
	method mgedWrapper {_cmd _eflag _hflag _sflag _tflag args}

	method buildCanvasMenubar {}

	method redrawObj {_obj {_wflag 1}}

	method menuStatusCB {_w}
	method updateTheme {}
	method updateSaveMode {}
	method createTargetCopy {}
	method deleteTargetOldCopy {}

	method getVDrawColor {_color}
	method buildGroundPlane {}
	method showGroundPlane {}
	method showPrimitiveLabels {}
	method showViewParams {}
	method showScale {}

	# pane commands
	method toggleTreeView {_state}
	method toggleAttrView {_state}
	method updateToggleMode {}

	method launchNirt {}
	method launchRtApp {_app _size}
	#method refreshDisplay {}
	#method mouseRay {_dm _x _y}

	method updateDisplaySettings {}


	#XXX Everything that follows use to be private
	variable mImgDir ""
	variable mCenterX ""
	variable mCenterY ""
	variable mCenterZ ""

	variable mImages

	variable mMeasureStart
	variable mMeasureEnd

	# init functions
	method initTree          {}
	method initMged          {}
	method closeMged         {}
	method updateRtControl   {}

	# interface operations
	method closeDb           {}
	method newDb             {}
	method openDb            {}
	method saveDb            {}
	method primaryToolbarAdd        {_type _name {args ""}}
	method primaryToolbarRemove     {_index}

	# private window commands
	method doPrimaryToolbar {}
	method doViewToolbar    {}
	method doStatusBar       {}

	# tree commands
	method alterTreeNodeChildren {node option value}
	method toggleTreePath   {_path}
	method updateTree        {}
	method fillTree          {{_node ""}}
	method selectNode        {_tags {_rflag 1}}
	method dblClick          {_tags}
	method loadMenu          {_menu _snode}

	# db/display commands
	method getNodeChildren  {_node}
	method renderComp        {_node}
	method render             {_node _state _trans _updateTree {_wflag 1}}
	method selectDisplayColor  {_node}
	method setDisplayColor	   {_node _rgb}
	method setTransparency	   {_node _alpha}
	method raytracePanel    {}
	method doPng             {}
	method setActivePane    {_pane}
	method updateActivePane {_args}
	method doMultiPane      {}
	method doLighting        {}

	# view commands
	method doViewReset {}
	method doAutoview {}
	method doViewCenter {}
	method doAe {_az _el}

	method showViewAxes     {}
	method showModelAxes    {}
	method showModelAxesTicks {}
	method toggleModelAxes  {_pane}
	method toggleModelAxesTicks {_pane}
	method toggleViewAxes   {_pane}

	# private mged commands
	method alterObj          {_operation _obj}
	method deleteObj         {_obj}
	method doCopyOrMove      {_top _obj _cmd}

	method buildViewToolbar {}

	method beginViewRotate {}
	method endViewRotate {_dm}

	method beginViewScale {}
	method endViewScale {_dm}

	method beginViewTranslate {}
	method endViewTranslate {_dm}

	method initCenterMode {}

	method initCompPick {}

	method initMeasure {}
	method beginMeasure {_dm _x _y}
	method handleMeasure {_dm _x _y}
	method endMeasure {_dm}

	method initDefaultBindings {{_comp ""}}
	method initBrlcadBindings {}
	method validateDigit {_d}
	method validateDouble {_d}
	method validateTickInterval {_ti}
	method validateColorComp {_c}

	method backgroundColor {_r _g _b}

	method updateHPaneFractions {}
	method updateVPaneFractions {}

	method setColorOption {_cmd _colorOption _color {_tripleColorOption ""}}

	method addHistory {_cmd}

	# Dialogs Section
	method buildInfoDialog {_name _title _info _size _wrapOption _modality}
	method buildSaveDialog {}
	method buildViewCenterDialog {}

	# Helper Section
	method buildComboBox {_parent _name1 _name2 _varName _text _listOfChoices}
    }
}

# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------
::itcl::body ArcherCore::constructor {{_viewOnly 0} {_noCopy 0} args} {
    global env
    global tcl_platform

    set mLastSelectedDir [pwd]

    set mFontText [list $SystemWindowFont 8]
    set mFontTextBold [list $SystemWindowFont 8 bold]

    set mProgressBarHeight [expr {[font metrics $mFontText -linespace] + 1}]
    set mViewOnly $_viewOnly
    set mDbNoCopy $_noCopy

    if {$ArcherCore::inheritFromToplevel} {
	wm withdraw [namespace tail $this]
    }

    if {![info exists env(DISPLAY)]} {
	set env(DISPLAY) ":0"
    }

    set mImgDir [file join $brlcadDataPath tclscripts archer images]

    if {[llength $args] == 1} {
	set args [lindex $args 0]

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
		-textbackground $SystemWindow -prompt "ArcherCore> " \
		-prompt2 "% " -result_color black -cmd_color red
	} {}

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
	} {}
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
			[file join $mImgDir Themes $mTheme "pane_blank.png"]] \
	    -state disabled \
	    -command [::itcl::code $this toggleTreeView "open"]

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
    buildCanvasMenubar

    if {!$mViewOnly} {
	itk_component add attr_expand {
	    ::button $itk_component(canvasF).attr_expand
	} {}
	$itk_component(attr_expand) configure -relief flat \
	    -image [image create photo -file \
			[file join $mImgDir Themes $mTheme "pane_blank.png"]] \
	    -state disabled \
	    -command [::itcl::code $this toggleAttrView "open"]

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
			[file join $mImgDir Themes $mTheme "pane_expand.png"]] \
	    -command [::itcl::code $this toggleAttrView "close"]

	itk_component add attr_label {
	    ::label $itk_component(attr_title_frm).attr_label \
		-text "Attributes" -font $mFontText
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

	$itk_component(primaryToolbar) add button open \
	    -balloonstr "Open an existing geometry file" \
	    -helpstr "Open an existing geometry file" \
	    -relief flat \
	    -overrelief raised \
	    -command [::itcl::code $this openDb]

	$itk_component(primaryToolbar) add button save \
	    -balloonstr "Save the current geometry file" \
	    -helpstr "Save the current geometry file" \
	    -relief flat \
	    -overrelief raised \
	    -command [::itcl::code $this saveDb]

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
	initTree
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
	initTree
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

    buildViewToolbar

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

	updateSaveMode
    }

    backgroundColor [lindex $mBackground 0] \
	[lindex $mBackground 1] \
	[lindex $mBackground 2]

    updateTheme

    #    eval itk_initialize $args
}

# ------------------------------------------------------------
#                       DESTRUCTOR
# ------------------------------------------------------------
::itcl::body ArcherCore::destructor {} {
    # We do this to remove librt's reference
    # to $mTargetCopy now. Once this reference is
    # gone, we can successfully delete the temporary
    # file here in the destructor.
    catch {::itcl::delete object $itk_component(mged)}

    set mTargetOldCopy $mTargetCopy
    deleteTargetOldCopy
}

# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------
::itcl::configbody ArcherCore::primaryToolbar {
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

::itcl::configbody ArcherCore::viewToolbar {
    if {!$mViewOnly} {
	if {$itk_option(-viewToolbar)} {
	    pack $itk_component(viewToolbar) -expand yes -fill both
	} else {
	    pack forget $itk_component(viewToolbar)
	}
    }
}

::itcl::configbody ArcherCore::statusbar {
    if {!$mViewOnly} {
	if {$itk_option(-statusbar)} {
	    pack $itk_component(statusF) -before $itk_component(south) -side bottom -fill x
	} else {
	    pack forget $itk_component(statusF)
	}
    }
}

::itcl::body ArcherCore::cadWrapper {cmd eflag hflag sflag tflag args} {
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
	updateSaveMode
    }

    dbCmd configure -primitiveLabels {}
    if {$tflag} {
	catch {refreshTree}
    }
    SetNormalCursor

    return $ret
}

::itcl::body ArcherCore::mgedWrapper {cmd eflag hflag sflag tflag args} {
    if {![info exists itk_component(mged)]} {
	return
    }

    eval cadWrapper $cmd $eflag $hflag $sflag $tflag $args
}

::itcl::body ArcherCore::buildCanvasMenubar {}  {
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
	-command [::itcl::code $this doAe 0 0] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.rear \
	-command [::itcl::code $this doAe 180 0] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.port \
	-command [::itcl::code $this doAe 90 0] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.starboard \
	-command [::itcl::code $this doAe -90 0] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.top \
	-command [::itcl::code $this doAe 0 90] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.bottom \
	-command [::itcl::code $this doAe 0 -90] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.35,25 \
	-command [::itcl::code $this doAe 35 25] \
	-state disabled
    $itk_component(canvas_menu) menuconfigure .view.45,45 \
	-command [::itcl::code $this doAe 45 45] \
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
	-command [::itcl::code $this backgroundColor 0 0 0]
    $itk_component(canvas_menu) menuconfigure .background.grey \
	-command [::itcl::code $this backgroundColor 100 100 100]
    $itk_component(canvas_menu) menuconfigure .background.white \
	-command [::itcl::code $this backgroundColor 255 255 255]
    $itk_component(canvas_menu) menuconfigure .background.lblue \
	-command [::itcl::code $this backgroundColor 0 198 255]
    $itk_component(canvas_menu) menuconfigure .background.dblue \
	-command [::itcl::code $this backgroundColor 0 0 160]

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

::itcl::body ArcherCore::redrawObj {obj {wflag 1}} {
    if {![info exists itk_component(mged)]} {
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

    render $obj $renderMode $renderTrans 0 $wflag
}


::itcl::body ArcherCore::initTree {} {
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
			[file join $mImgDir Themes $mTheme "pane_collapse.png"]] \
	    -command [::itcl::code $this toggleTreeView "close"]
    }

    itk_component add tree {
	::swidgets::tree $itk_component(tree_frm).tree \
	    -background white \
	    -selectfill 1 \
	    -selectbackground black \
	    -selectforeground $SystemWindow \
	    -querycmd [::itcl::code $this fillTree %n] \
	    -selectcmd [::itcl::code $this selectNode %n] \
	    -dblselectcmd [::itcl::code $this dblClick %n] \
	    -textmenuloadcommand [::itcl::code $this loadMenu]
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

::itcl::body ArcherCore::initMged {} {
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
    set mUnwrappedDbCommands {}
    foreach cmd $tmp_dbCommands {
	if {[lsearch $mBannedDbCommands $cmd] == -1} {
	    lappend mUnwrappedDbCommands $cmd
	}
    }

    set mDbSpecificCommands $mMgedCommands


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
    $itk_component(mged) fb_active 0
    $itk_component(rtcntrl) update_fb_mode
    bind $itk_component(rtcntrl) <Visibility> "raise $itk_component(rtcntrl)"
    bind $itk_component(rtcntrl) <FocusOut> "raise $itk_component(rtcntrl)"
    wm protocol $itk_component(rtcntrl) WM_DELETE_WINDOW "$itk_component(rtcntrl) deactivate"

    # create view axes control panel
    #    itk_component add vac {
    #	ViewAxesControl $itk_interior.vac -mged $itk_component(mged)
    #    } {
    #	usual
    #    }

    # create model axes control panel
    #    itk_component add mac {
    #	ModelAxesControl $itk_interior.mac -mged $itk_component(mged)
    #    } {
    #	usual
    #    }

    #    wm protocol $itk_component(vac) WM_DELETE_WINDOW "$itk_component(vac) hide"
    #    wm protocol $itk_component(mac) WM_DELETE_WINDOW "$itk_component(mac) hide"

    #    $itk_component(mged) configure -unitsCallback "$itk_component(mac) updateControlPanel"
    $itk_component(mged) configure -paneCallback [::itcl::code $this updateActivePane]

    # Override axes hot keys in the Mged widget
    #    bind [$itk_component(mged) component ul component dm] m [::itcl::code $this toggleModelAxes ul]
    #    bind [$itk_component(mged) component ur component dm] m [::itcl::code $this toggleModelAxes ur]
    #    bind [$itk_component(mged) component ll component dm] m [::itcl::code $this toggleModelAxes ll]
    #    bind [$itk_component(mged) component lr component dm] m [::itcl::code $this toggleModelAxes lr]
    #    bind [$itk_component(mged) component ul component dm] T [::itcl::code $this toggleModelAxesTicks ul]
    #    bind [$itk_component(mged) component ur component dm] T [::itcl::code $this toggleModelAxesTicks ur]
    #    bind [$itk_component(mged) component ll component dm] T [::itcl::code $this toggleModelAxesTicks ll]
    #    bind [$itk_component(mged) component lr component dm] T [::itcl::code $this toggleModelAxesTicks lr]
    #    bind [$itk_component(mged) component ul component dm] v [::itcl::code $this toggleViewAxes ul]
    #    bind [$itk_component(mged) component ur component dm] v [::itcl::code $this toggleViewAxes ur]
    #    bind [$itk_component(mged) component ll component dm] v [::itcl::code $this toggleViewAxes ll]
    #    bind [$itk_component(mged) component lr component dm] v [::itcl::code $this toggleViewAxes lr]


    # Other bindings for mged
    #bind $itk_component(mged) <Enter> {focus %W}

    if {!$mViewOnly} {
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

    bind $itk_component(canvasF) <Configure> [::itcl::code $this updateRtControl]
}

::itcl::body ArcherCore::closeMged {} {
    catch {delete object $itk_component(rtcntrl)}
    #    catch {delete object $itk_component(vac)}
    #    catch {delete object $itk_component(mac)}
    catch {delete object $itk_component(mged)}
}

::itcl::body ArcherCore::updateRtControl {} {
    ::update
    if {[info exists itk_component(rtcntrl)]} {
	$itk_component(rtcntrl) configure -size "Size of Pane"
    }
}

# ------------------------------------------------------------
#                 INTERFACE OPERATIONS
# ------------------------------------------------------------
::itcl::body ArcherCore::closeDb {} {
    pack forget $itk_component(mged)
    closeMged

    grid $itk_component(canvas) -row 1 -column 0 -columnspan 3 -sticky news
    set mDbType ""

    refreshTree 0
}

::itcl::body ArcherCore::newDb {} {
    set typelist {
	{"BRL-CAD Database" {".g"}}
	{"All Files" {*}}
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

    switch -- [file extension $target] {
	".g"   {
	    set db [Db \#auto $target]
	    ::itcl::delete object $db
	}
	default {
	    return
	}
    }

    Load $target
}

::itcl::body ArcherCore::openDb {} {
    set typelist {
	{"BRL-CAD Database" {".g"}}
	{"All Files" {*}}
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

    Load $target
}

::itcl::body ArcherCore::saveDb {} {
    set mNeedSave 0
    updateSaveMode

    # Sanity
    if {$mTarget == "" ||
	$mTargetCopy == "" ||
	$mDbReadOnly ||
	$mDbNoCopy} {
	return
    }

    file copy -force $mTargetCopy $mTarget
}

::itcl::body ArcherCore::primaryToolbarAdd {type name {args ""}} {
    if {[llength $args] > 1} {
	eval $itk_component(primaryToolbar) add $type $name $args
    } else {
	eval $itk_component(primaryToolbar) add $type $name [lindex $args 0]
    }
    return [$itk_component(primaryToolbar) index $name]
}

::itcl::body ArcherCore::primaryToolbarRemove {index} {
    eval $itk_component(primaryToolbar) delete $index
}

# ------------------------------------------------------------
#                    WINDOW COMMANDS
# ------------------------------------------------------------
::itcl::body ArcherCore::doPrimaryToolbar {} {
    configure -primaryToolbar $itk_option(-primaryToolbar)
}

::itcl::body ArcherCore::doViewToolbar {} {
    configure -viewToolbar $itk_option(-viewToolbar)
}

::itcl::body ArcherCore::doStatusBar {} {
    configure -statusbar $itk_option(-statusbar)
}

::itcl::body ArcherCore::dockArea {{position "south"}} {
    switch -- $position {
	"north" -
	"south" -
	"east"  -
	"west"  {return $itk_component($position)}
	default {
	    error "ArcherCore::dockArea: unrecognized area `$position'"
	}
    }
}

::itcl::body ArcherCore::primaryToolbarAddBtn {name {args ""}} {
    if [catch {primaryToolbarAdd button $name $args} err] {error $err}
    return $err
}

::itcl::body ArcherCore::primaryToolbarAddSep {name {args ""}} {
    if [catch {primaryToolbarAdd frame $name $args} err] {error $err}
    return $err
}

::itcl::body ArcherCore::primaryToolbarRemoveItem  {index} {
    if [catch {primaryToolbarRemove $index} err] {error $err}
}

::itcl::body ArcherCore::closeHierarchy {} {
    $itk_component(vpane) hide hierarchyView
}

::itcl::body ArcherCore::openHierarchy {{fraction 30}} {
    #XXX We should check to see if fraction is between
    #    0 and 100 inclusive.
    $itk_component(vpane) show hierarchyView
    $itk_component(vpane) fraction $fraction [expr {100 - $fraction}]
}

# ------------------------------------------------------------
#                     MGED COMMANDS
# ------------------------------------------------------------
::itcl::body ArcherCore::alterObj {operation comp} {
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
    set ok [button $top.ok -text "OK" -width 7 -command [::itcl::code $this doCopyOrMove $top $comp $cmd]]
    pack $cancel -side right -anchor e -padx 3 -pady 2
    pack $ok -side right -anchor e -padx 3 -pady 2

    set_focus $top $entry
    tkwait window $top
}

::itcl::body ArcherCore::deleteObj {comp} {
    if {[do_question "Are you sure you wish to delete `$comp'."] == "no"} {
	return
    }

    set mNeedSave 1
    updateSaveMode
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
    refreshTree
    SetNormalCursor
}

::itcl::body ArcherCore::doCopyOrMove {top comp cmd} {
    set mNeedSave 1
    updateSaveMode
    SetWaitCursor
    set comp2 [string trim [$top.entry get]]
    wm withdraw $top
    dbCmd $cmd $comp
    refreshTree
    SetNormalCursor
    destroy $top
}

::itcl::body ArcherCore::buildViewToolbar {} {
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
	-command [::itcl::code $this initCenterMode]
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

::itcl::body ArcherCore::beginViewRotate {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm rotate_mode %x %y; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endViewRotate $dm]; break"
    }
}

::itcl::body ArcherCore::endViewRotate {dsp} {
    $dsp idle_mode

    if {![info exists itk_component(mged)]} {
	return
    }

    set ae [$itk_component(mged) ae]
    addHistory "ae $ae"
}

::itcl::body ArcherCore::beginViewScale {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm scale_mode %x %y; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endViewScale $dm]; break"
    }
}

::itcl::body ArcherCore::endViewScale {dsp} {
    $dsp idle_mode

    if {![info exists itk_component(mged)]} {
	return
    }

    set size [$itk_component(mged) size]
    addHistory "size $size"
}

::itcl::body ArcherCore::beginViewTranslate {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm translate_mode %x %y; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endViewTranslate $dm]; break"
    }
}

::itcl::body ArcherCore::endViewTranslate {dsp} {
    $dsp idle_mode

    if {![info exists itk_component(mged)]} {
	return
    }

    set center [$itk_component(mged) center]
    addHistory "center $center"
}

::itcl::body ArcherCore::initCenterMode {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm slew %x %y; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endViewTranslate $dm]; break"
    }
}

::itcl::body ArcherCore::initCompPick {} {
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

::itcl::body ArcherCore::initMeasure {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "[::itcl::code $this beginMeasure $dm %x %y]; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endMeasure $dm]; break"
    }
}

::itcl::body ArcherCore::beginMeasure {_dm _x _y} {
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

    set mMeasuringStickColorVDraw [getVDrawColor $mMeasuringStickColor]
}

::itcl::body ArcherCore::handleMeasure {_dm _x _y} {
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

::itcl::body ArcherCore::endMeasure {_dm} {
    $_dm idle_mode

    catch {dbCmd vdraw vlist delete $MEASURING_STICK}
    dbCmd erase _VDRW$MEASURING_STICK

    set diff [vsub2 $mMeasureEnd $mMeasureStart]
    set delta [expr {[magnitude $diff] * [dbCmd base2local]}]
    tk_messageBox -title "Measured Distance" \
	-icon info \
	-message "Measured distance:  $delta [dbCmd units]"
}


::itcl::body ArcherCore::initDefaultBindings {{_comp ""}} {
    if {$_comp == ""} {
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
    }

    $itk_component(viewToolbar) configure -state normal
    $itk_component(viewToolbar) itemconfigure cpick \
	-state normal

    catch {eval [$itk_component(viewToolbar) itemcget $mDefaultBindingMode -command]}
}

::itcl::body ArcherCore::initBrlcadBindings {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    $itk_component(viewToolbar) configure -state disabled
    $itk_component(mged) resetBindingsAll
}


::itcl::body ArcherCore::validateDigit {d} {
    if {[string is digit $d]} {
	return 1
    }

    return 0
}

::itcl::body ArcherCore::validateDouble {d} {
    if {$d == "-" || [string is double $d]} {
	return 1
    }

    return 0
}

::itcl::body ArcherCore::validateTickInterval {ti} {
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

::itcl::body ArcherCore::validateColorComp {c} {
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


::itcl::body ArcherCore::backgroundColor {r g b} {
    set mBackground [list $r $g $b]

    if {![info exists itk_component(mged)]} {
	set color [getTkColor \
		       [lindex $mBackground 0] \
		       [lindex $mBackground 1] \
		       [lindex $mBackground 2]]
	$itk_component(canvas) configure -background $color
    } else {
	eval dbCmd bgAll $mBackground
    }
}


::itcl::body ArcherCore::updateHPaneFractions {} {
    if {$mViewOnly} {
	return
    }

    set fractions [$itk_component(hpane) fraction]

    if {[llength $fractions] == 2} {
	set mHPaneFraction1 [lindex $fractions 0]
	set mHPaneFraction2 [lindex $fractions 1]
    }
}

::itcl::body ArcherCore::updateVPaneFractions {} {
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

::itcl::body ArcherCore::setColorOption {cmd colorOption color {tripleColorOption ""}} {
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

::itcl::body ArcherCore::addHistory {cmd} {
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

::itcl::body ArcherCore::dbCmd {args} {
    if {![info exists itk_component(mged)]} {
	return
    }

    return [eval $itk_component(mged) $args]
}

::itcl::body ArcherCore::cmd {args} {
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
		    if {[lsearch $subcmd $mUnwrappedDbCommands] == -1 &&
			[lsearch $subcmd $mDbSpecificCommands] == -1 &&
			[lsearch $subcmd $mCadCommands] == -1} {
			error "ArcherCore::cmd: unrecognized command - $subcmd"
		    } else {
			return
		    }
		} else {
		    return [eval list $mCadCommands $mDbSpecificCommands $mUnwrappedDbCommands]
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

    set i [lsearch $mCadCommands $cmd]
    if {$i != -1} {
	addHistory $args
	return [eval $args]
    }

    set i [lsearch $mDbSpecificCommands $cmd]
    if {$i != -1} {
	addHistory $args
	return [eval $args]
    }

    set i [lsearch $mUnwrappedDbCommands $cmd]
    if {$i != -1} {
	addHistory $args
	return [eval dbCmd $args]
    }

    error "ArcherCore::cmd: unrecognized command - $args, check source code"
}

::itcl::body ArcherCore::png {filename} {
    SetWaitCursor
    dbCmd sync
    dbCmd png $filename
    SetNormalCursor
}

# ------------------------------------------------------------
#                  DB/DISPLAY COMMANDS
# ------------------------------------------------------------
::itcl::body ArcherCore::getNodeChildren {node} {
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


::itcl::body ArcherCore::renderComp {_node} {
    set renderMode [dbCmd how $_node]
    if {$renderMode < 0} {
	render $_node 0 1 1
    } else {
	render $_node -1 1 1
    }
}


::itcl::body ArcherCore::render {node state trans updateTree {wflag 1}} {
    if {$wflag} {
	SetWaitCursor
    }

    set savePwd ""
    set tnode [file tail $node]
    set saveGroundPlane 0

    if {$mShowPrimitiveLabels} {
	set plnode $node
    } else {
	set plnode {}
    }

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

    # Change back to previous directory
    if {$savePwd != ""} {
	cd $savePwd
    }

    # Turn ground plane back on if it was on before the draw
    if {$saveGroundPlane} {
	set mShowGroundPlane 1
	showGroundPlane
    }

    if {$updateTree} {
	updateTree
    }

    if {$wflag} {
	SetNormalCursor
    }
}

::itcl::body ArcherCore::selectDisplayColor {node} {
    set tnode [file tail $node]

    if {[catch {dbCmd attr get \
		    $tnode displayColor} displayColor] &&
	[catch {dbCmd attr get \
		    $tnode rgb} displayColor]} {
	set displayColor [eval format "%d/%d/%d" $mDefaultNodeColor]
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

::itcl::body ArcherCore::setDisplayColor {node rgb} {
    set tnode [file tail $node]
    set savePwd ""

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
	render $node [lindex $drawState 0] \
	    [lindex $drawState 1] 0 1
    }

    set mNeedSave 1
    updateSaveMode
}

::itcl::body ArcherCore::setTransparency {node alpha} {
    dbCmd set_transparency $node $alpha
}

::itcl::body ArcherCore::raytracePanel {} {
    $itk_component(rtcntrl) configure -size "Size of Pane"
    $itk_component(rtcntrl) activate
}

::itcl::body ArcherCore::doPng {} {
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

::itcl::body ArcherCore::setActivePane {pane} {
    $itk_component(mged) pane $pane
    updateActivePane $pane
}

::itcl::body ArcherCore::updateActivePane {args} {
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

::itcl::body ArcherCore::doMultiPane {} {
    dbCmd configure -multi_pane $mMultiPane
}

::itcl::body ArcherCore::doLighting {} {
    SetWaitCursor

    if {$mZClipMode != $ZCLIP_NONE} {
	dbCmd zclipAll $mLighting
    }

    dbCmd zbufferAll $mLighting
    dbCmd lightAll $mLighting

    SetNormalCursor
}

::itcl::body ArcherCore::doViewReset {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    dbCmd autoviewAll
    dbCmd default_views
}

::itcl::body ArcherCore::doAutoview {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    dbCmd autoviewAll
}

::itcl::body ArcherCore::doViewCenter {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    if {$mCurrentDisplay == ""} {
	set dm dbCmd
    } else {
	set dm $mCurrentDisplay
    }

    set center [$dm center]
    set mCenterX [lindex $center 0]
    set mCenterY [lindex $center 1]
    set mCenterZ [lindex $center 2]

    set mDbUnits [dbCmd units]
    $itk_component(centerDialog) center [namespace tail $this]
    if {[$itk_component(centerDialog) activate]} {
	$dm center $mCenterX $mCenterY $mCenterZ
    }
}

::itcl::body ArcherCore::doAe {az el} {
    if {$mCurrentDisplay == ""} {
	dbCmd ae $az $el
    } else {
	$mCurrentDisplay ae $az $el
    }

    addHistory "ae $az $el"
}

::itcl::body ArcherCore::showViewAxes {} {
    catch {dbCmd setViewAxesEnable $mShowViewAxes}
}

::itcl::body ArcherCore::showModelAxes {} {
    catch {dbCmd setModelAxesEnable $mShowModelAxes}
}

::itcl::body ArcherCore::showModelAxesTicks {} {
    catch {dbCmd setModelAxesTickEnable $mShowModelAxesTicks}
}

::itcl::body ArcherCore::toggleModelAxes {pane} {
    set currPane [dbCmd pane]
    if {$currPane == $pane} {
	# update menu checkbutton
	set mShowModelAxes [expr !$mShowModelAxes]
    }

    dbCmd toggle_modelAxesEnable $pane
}

::itcl::body ArcherCore::toggleModelAxesTicks {pane} {
    set currPane [dbCmd pane]
    if {$currPane == $pane} {
	# update menu checkbutton
	set mShowModelAxesTicks [expr !$mShowModelAxesTicks]
    }

    dbCmd toggle_modelAxesTickEnable $pane
}

::itcl::body ArcherCore::toggleViewAxes {pane} {
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
::itcl::body ArcherCore::alterTreeNodeChildren {node option value} {
    foreach child [$itk_component(tree) query -children $node] {
	$itk_component(tree) alternode $child $option $value
	alterTreeNodeChildren $child "-color" \
	    [$itk_component(tree) query -color $child]
    }
}

::itcl::body ArcherCore::refreshTree {{restore 1}} {
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
    fillTree

    if {$restore == 1} {
	set mRestoringTree 1
	# set the open state of nodes
	foreach path $paths {
	    toggleTreePath $path
	}

	if {$selNodePath != ""} {
	    toggleTreePath $selNodePath
	}
	set mRestoringTree 0
    }

    # force redraw of tree
    $itk_component(tree) redraw
}

::itcl::body ArcherCore::toggleTreePath {_path} {
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

::itcl::body ArcherCore::updateTree {} {
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
    alterTreeNodeChildren $element "-color" \
	[$itk_component(tree) query -color $element]
}

::itcl::body ArcherCore::fillTree {{node ""}} {
    if {$node == ""} {
	# set node to root
	set node "root"

	# get toplevel objects
	set tops [dbCmd tops]
    } else {
	set nname [$itk_component(tree) query -text $node]

	# get all its children
	set tops [getNodeChildren $nname]
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

    ::update idletasks
}

::itcl::body ArcherCore::selectNode {tags {rflag 1}} {
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
    set savePwd ""

    if {[catch {dbCmd get_type $node} ret]} {
	if {$savePwd != ""} {
	    cd $savePwd
	}

	return
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
}

::itcl::body ArcherCore::dblClick {tags} {
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
	set mPrevSelectedObjPath $mSelectedObjPath
	set mPrevSelectedObj $mSelectedObj
    }

    $itk_component(tree) selection clear
    $itk_component(tree) selection set $tags

    switch -- $type {
	"leaf"   -
	"branch" {renderComp $node}
    }
}

::itcl::body ArcherCore::loadMenu {menu snode} {
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

    $itk_component(tree) selection clear
    $itk_component(tree) selection set $snode

    if {$nodeType == "leaf"} {
	$menu add radiobutton -label "Wireframe" \
	    -indicatoron 1 -value 0 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $node 0 1 1]

	$menu add radiobutton -label "Shaded (Mode 1)" \
	    -indicatoron 1 -value 1 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $node 1 1 1]
	$menu add radiobutton -label "Shaded (Mode 2)" \
	    -indicatoron 1 -value 2 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $node 2 1 1]

	if {$mEnableBigE} {
	    $menu add radiobutton \
		-label "Evaluated" \
		-indicatoron 1 \
		-value 3 \
		-variable [::itcl::scope mRenderMode] \
		-command [::itcl::code $this render $node 3 1 1]
	}

	$menu add radiobutton -label "Off" \
	    -indicatoron 1 -value -1 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $node -1 1 1]
    } else {
	$menu add command -label "Wireframe" \
	    -command [::itcl::code $this render $node 0 1 1]

	$menu add command -label "Shaded (Mode 1)" \
	    -command [::itcl::code $this render $node 1 1 1]
	$menu add command -label "Shaded (Mode 2)" \
	    -command [::itcl::code $this render $node 2 1 1]

	if {$mEnableBigE} {
	    $menu add command \
		-label "Evaluated" \
		-command [::itcl::code $this render $node 3 1 1]
	}

	$menu add command -label "Off" \
	    -command [::itcl::code $this render $node -1 1 1]
    }

    #XXX need to copy over
    #    $menu add separator
    #    $menu add command -label "Copy" \
	#	    -command [::itcl::code $this alterObj "Copy" $mSelectedComp]
    #    $menu add command -label "Rename" \
	#	    -command [::itcl::code $this alterObj "Rename" $mSelectedComp]
    #    $menu add command -label "Delete" \
	#	    -command [::itcl::code $this deleteObj $mSelectedComp]


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

    # set up bindings for status
    bind $menu <<MenuSelect>> \
	[::itcl::code $this menuStatusCB %W]
}

# ------------------------------------------------------------
#                         GENERAL
# ------------------------------------------------------------
::itcl::body ArcherCore::Load {target} {
    if {$mNeedSave} {
	$itk_component(saveDialog) center [namespace tail $this]
	if {[$itk_component(saveDialog) activate]} {
	    saveDb
	} else {
	    set mNeedSave 0
	    updateSaveMode
	}
    }

    set mTarget $target
    set mDbType "BRL-CAD"

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
	createTargetCopy
    }

    # Load MGED database
    if {[info exists itk_component(mged)]} {
	if {$mDbShared} {
	    $itk_component(mged) sharedDb $mTarget
	} elseif {$mDbNoCopy || $mDbReadOnly} {
	    $itk_component(mged) opendb $mTarget
	} else {
	    $itk_component(mged) opendb $mTargetCopy
	}
    } else {
	initMged

	grid forget $itk_component(canvas)
	if {!$mViewOnly} {
	    grid $itk_component(mged) -row 1 -column 0 -columnspan 3 -sticky news
	    after idle "$this component cmd configure -cmd_prefix \"[namespace tail $this] cmd\""
	} else {
	    grid $itk_component(mged) -row 1 -column 0 -sticky news
	}
    }

    set mDbTitle [$itk_component(mged) title]
    set mDbUnits [$itk_component(mged) units]
    set mPrevObjViewMode $OBJ_ATTR_VIEW_MODE
    set mPrevSelectedObjPath ""
    set mPrevSelectedObj ""
    set mSelectedObjPath ""
    set mSelectedObj ""
    set mSelectedObjType ""
    set mPasteActive 0
    set mPendingEdits 0

    if {!$mViewOnly} {
	dbCmd size [expr {$mGroundPlaneSize * 1.5 * [dbCmd base2local]}]
	buildGroundPlane
	showGroundPlane
    }

    setColorOption dbCmd -primitiveLabelColor $mPrimitiveLabelColor
    setColorOption dbCmd -scaleColor $mScaleColor
    setColorOption dbCmd -viewingParamsColor $mViewingParamsColor

    if {!$mViewOnly} {
	doLighting
	deleteTargetOldCopy

	# refresh tree contents
	SetWaitCursor
	refreshTree 0
	SetNormalCursor
    } else {
	doLighting
    }

    if {$mBindingMode == 0} {
	initDefaultBindings $itk_component(mged)
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
    }
}

::itcl::body ArcherCore::GetUserCmds {} {
    return $mUnwrappedDbCommands
}

::itcl::body ArcherCore::WhatsOpen {} {
    return $mTarget
}

::itcl::body ArcherCore::Close {} {
    if {$itk_option(-quitcmd) != {}} {
	catch {eval $itk_option(-quitcmd)}
    } else {
	::exit
    }
}

::itcl::body ArcherCore::askToSave {} {
    if {$mNeedSave} {
	$itk_component(saveDialog) center [namespace tail $this]
	if {[$itk_component(saveDialog) activate]} {
	    saveDb
	}
    }
}

::itcl::body ArcherCore::menuStatusCB {w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$w entrycget active -label} op]} {
	    set op ""
	}

	switch -- $op {
	    "Open..." {
		set mStatusStr "Open a target description"
	    }
	    "Save" {
		set mStatusStr "Save the current target description"
	    }
	    "Quit" {
		set mStatusStr "Exit ArcherCore"
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

::itcl::body ArcherCore::updateTheme {} {
    set dir [file join $mImgDir Themes $mTheme]

    if {!$mViewOnly} {
	# Tree Control
	$itk_component(tree) configure \
	    -openimage [image create photo -file [file join $dir folder_open_small.png]] \
	    -closeimage [image create photo -file [file join $dir folder_closed_small.png]] \
	    -nodeimage [image create photo -file [file join $dir file_text_small.png]]
	$itk_component(tree) redraw

	# Primary Toolbar
	$itk_component(primaryToolbar) itemconfigure open \
	    -image [image create photo \
			-file [file join $dir open.png]]
	$itk_component(primaryToolbar) itemconfigure save \
	    -image [image create photo \
			-file [file join $dir save.png]]
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
}


::itcl::body ArcherCore::updateSaveMode {} {
    if {$mViewOnly} {
	return
    }

    if {!$mDbNoCopy && !$mDbReadOnly && $mNeedSave} {
	$itk_component(primaryToolbar) itemconfigure save \
	    -state normal
    } else {
	$itk_component(primaryToolbar) itemconfigure save \
	    -state disabled
    }

}

::itcl::body ArcherCore::createTargetCopy {} {
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

::itcl::body ArcherCore::deleteTargetOldCopy {} {
    if {$mTargetOldCopy != ""} {
	catch {file delete -force $mTargetOldCopy}

	# sanity
	set mTargetOldCopy ""
    }
}


::itcl::body ArcherCore::getVDrawColor {color} {
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

::itcl::body ArcherCore::buildGroundPlane {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    catch {dbCmd vdraw vlist delete groundPlaneMajor}
    catch {dbCmd vdraw vlist delete groundPlaneMinor}

    set majorColor [getVDrawColor $mGroundPlaneMajorColor]
    set minorColor [getVDrawColor $mGroundPlaneMinorColor]
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

::itcl::body ArcherCore::showGroundPlane {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    set savePwd ""

    if {$mShowGroundPlane} {
	dbCmd vdraw open groundPlaneMajor
	dbCmd vdraw send
	dbCmd vdraw open groundPlaneMinor
	dbCmd vdraw send
    } else {
	set phonyList [dbCmd who p]
	if {[lsearch $phonyList _VDRWgroundPlaneMajor] != -1} {
	    dbCmd erase _VDRWgroundPlaneMajor _VDRWgroundPlaneMinor
	}
    }

    if {$savePwd != ""} {
	cd $savePwd
    }
}

::itcl::body ArcherCore::showPrimitiveLabels {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    redrawObj $mSelectedObjPath
}

::itcl::body ArcherCore::showViewParams {} {
    if {[info exists itk_component(mged)]} {
	$itk_component(mged) configure -showViewingParams $mShowViewingParams
    } else {
	return
    }

    refreshDisplay
}

::itcl::body ArcherCore::showScale {} {
    if {[info exists itk_component(mged)]} {
	$itk_component(mged) configure -scaleEnable $mShowScale
    } else {
	return
    }

    refreshDisplay
}

::itcl::body ArcherCore::toggleTreeView {state} {
    # pre-move stuff
    set fractions [$itk_component(vpane) fraction]
    switch -- $state {
	"open" {
	    # need to get attr pane in case it has changed size
	    if {[$itk_component(attr_expand) cget -state] == "disabled"} {
		set mVPaneToggle5 [lindex $fractions 2]
	    }
	}
	"close" {
	    # store fractions
	    set mVPaneToggle3 [lindex $fractions 0]
	    # need to get attr pane in case it has changed size
	    if {[$itk_component(attr_expand) cget -state] == "disabled"} {
		set mVPaneToggle5 [lindex $fractions 2]
	    }
	}
    }

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
    ::update
    after idle $itk_component(vpane) fraction $fraction1 $fraction2 $fraction3


    # post-move stuff
    switch -- $state {
	"open" {
	    $itk_component(tree_expand) configure -image [image create photo -file \
							      [file join $mImgDir Themes $mTheme "pane_blank.png"]] \
		-state disabled
	    #	    $itk_component(vpane) show hierarchyView
	}
	"close" {
	    $itk_component(tree_expand) configure -image [image create photo -file \
							      [file join $mImgDir Themes $mTheme "pane_expand.png"]] \
		-state normal
	    #	    $itk_component(vpane) hide hierarchyView
	}
    }
}

::itcl::body ArcherCore::toggleAttrView {state} {
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
    ::update
    after idle $itk_component(vpane) fraction $fraction1 $fraction2 $fraction3

    switch -- $state {
	"open" {
	    $itk_component(attr_expand) configure -image [image create photo -file \
							      [file join $mImgDir Themes $mTheme "pane_blank.png"]] \
		-state disabled
	}
	"close" {
	    $itk_component(attr_expand) configure -image [image create photo -file \
							      [file join $mImgDir Themes $mTheme "pane_collapse.png"]] \
		-state normal
	}
    }
}

::itcl::body ArcherCore::updateToggleMode {} {
    switch -- $mMode {
	0 {
	    set toggle1 $mVPaneToggle1

	    if {$mVPaneFraction1 == 0} {
		toggleTreeView "close"
	    }

	    set mVPaneToggle1 $toggle1
	}
	default {
	    set toggle3 $mVPaneToggle3
	    set toggle5 $mVPaneToggle5

	    if {$mVPaneFraction3 == 0} {
		toggleTreeView "close"
	    }

	    if {$mVPaneFraction5 == 0} {
		toggleAttrView "close"
	    }

	    set mVPaneToggle3 $toggle3
	    set mVPaneToggle5 $toggle5
	}
    }
}


::itcl::body ArcherCore::launchNirt {} {
    if {![info exists itk_component(mged)] || $mViewOnly} {
	return
    }

    $itk_component(cmd) putstring "nirt -b"
    $itk_component(cmd) putstring [$itk_component(mged) nirt -b]
}

::itcl::body ArcherCore::launchRtApp {app size} {
    global tcl_platform

    if {![info exists itk_component(mged)] || $mViewOnly} {
	return
    }

    if {![string is digit $size]} {
	set size [winfo width $itk_component(mged)]
    }

    if {$tcl_platform(platform) == "windows"} {
	$itk_component(mged) $app -s $size -F /dev/wgll
    } {
	$itk_component(mged) $app -s $size -F /dev/ogll
    }
}

::itcl::body ArcherCore::refreshDisplay {} {
    if {$mCurrentDisplay == ""} {
	dbCmd refresh
    } else {
	$mCurrentDisplay refresh
    }
}

::itcl::body ArcherCore::mouseRay {_dm _x _y} {
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

::itcl::body ArcherCore::shootRay {_start _op _target _prep _no_bool _onehit} {
    eval $itk_component(mged) rt_gettrees ray -i -u [$itk_component(mged) who]
    ray prep $_prep
    ray no_bool $_no_bool
    ray onehit $_onehit

    return [ray shootray $_start $_op $_target]
}

::itcl::body ArcherCore::addMouseRayCallback {_callback} {
    lappend mMouseRayCallbacks $_callback
}

::itcl::body ArcherCore::deleteMouseRayCallback {_callback} {
    set i [lsearch $mMouseRayCallbacks $_callback]
    if {$i != -1} {
	set mMouseRayCallbacks [lreplace $mMouseRayCallbacks $i $i]
    }
}

::itcl::body ArcherCore::getTkColor {r g b} {
    return [format \#%.2x%.2x%.2x $r $g $b]
}

::itcl::body ArcherCore::getRgbColor {tkColor} {
    set rgb [winfo rgb $itk_interior $tkColor]
    return [list \
		[expr {[lindex $rgb 0] / 256}] \
		[expr {[lindex $rgb 1] / 256}] \
		[expr {[lindex $rgb 2] / 256}]]
}

::itcl::body ArcherCore::setSave {} {
    if {$mDbNoCopy || $mDbReadOnly} {
	return
    }

    set mNeedSave 1
    updateSaveMode
}

::itcl::body ArcherCore::getLastSelectedDir {} {
    return $mLastSelectedDir
}

##################################### ArcherCore Commands #####################################
::itcl::body ArcherCore::adjust {args} {
    eval mgedWrapper adjust 0 1 1 1 $args
}

::itcl::body ArcherCore::arced {args} {
    eval mgedWrapper arced 0 0 1 0 $args
}

::itcl::body ArcherCore::attr {args} {
    eval mgedWrapper attr 0 0 1 0 $args
}

::itcl::body ArcherCore::blast {args} {
    eval mgedWrapper blast 0 0 0 1 $args
}

::itcl::body ArcherCore::c {args} {
    eval mgedWrapper c 0 1 1 1 $args
}

::itcl::body ArcherCore::cd {args} {
    eval ::cd $args
}

::itcl::body ArcherCore::clear {args} {
    eval cadWrapper clear 0 0 0 1 $args

    if {$mShowGroundPlane} {
	showGroundPlane
    }
}

::itcl::body ArcherCore::comb {args} {
    eval mgedWrapper comb 0 1 1 1 $args
}

::itcl::body ArcherCore::comb_color {args} {
    eval mgedWrapper comb_color 0 1 1 1 $args
}

::itcl::body ArcherCore::concat {args} {
    eval mgedWrapper concat 0 0 1 1 $args
}

::itcl::body ArcherCore::copy {args} {
    eval cadWrapper cp 0 0 1 1 $args
}

::itcl::body ArcherCore::copyeval {args} {
    eval mgedWrapper copyeval 0 0 1 1 $args
}

::itcl::body ArcherCore::cp {args} {
    eval cadWrapper cp 0 0 1 1 $args
}

::itcl::body ArcherCore::dbExpand {args} {
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
	    set children [getNodeChildren $parent]

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

    if {$tobjects != {}} {
	return [list $options $tobjects]
    }

    return [list $options $objects]
}

::itcl::body ArcherCore::delete {args} {
    eval cadWrapper kill 1 0 1 1 $args
}

::itcl::body ArcherCore::draw {args} {
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

    if {[catch {eval dbCmd draw $options $tobjects} ret]} {
	dbCmd configure -primitiveLabels {}
	refreshTree
	SetNormalCursor

	return $ret
    }

    dbCmd configure -primitiveLabels {}
    refreshTree
    if {$wflag} {
	SetNormalCursor
    }

    return $ret
}

::itcl::body ArcherCore::E {args} {
    eval mgedWrapper E 1 0 0 1 $args
}

::itcl::body ArcherCore::edcomb {args} {
    eval mgedWrapper edcomb 0 0 1 1 $args
}

::itcl::body ArcherCore::edmater {args} {
    eval mgedWrapper edmater 0 0 1 1 $args
}

::itcl::body ArcherCore::erase {args} {
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

    if {[catch {eval dbCmd erase $tobjects} ret]} {
	dbCmd configure -primitiveLabels {}
	refreshTree
	SetNormalCursor

	return $ret
    }

    dbCmd configure -primitiveLabels {}
    refreshTree
    SetNormalCursor
}

::itcl::body ArcherCore::erase_all {args} {
    eval mgedWrapper erase_all 1 0 0 1 $args
}

::itcl::body ArcherCore::ev {args} {
    eval mgedWrapper ev 1 0 0 1 $args
}

::itcl::body ArcherCore::exit {args} {
    Close
}

::itcl::body ArcherCore::find {args} {
    if {![info exists itk_component(mged)]} {
	return
    }

    eval $itk_component(mged) find $args
}

::itcl::body ArcherCore::g {args} {
    eval group $args
}

::itcl::body ArcherCore::get {args} {
    if {[info exists itk_component(mged)]} {
	return [eval $itk_component(mged) get $args]
    }

    return ""
}

::itcl::body ArcherCore::group {args} {
    eval cadWrapper g 0 1 1 1 $args
}

::itcl::body ArcherCore::hide {args} {
    eval mgedWrapper hide 0 0 1 1 $args
}


::itcl::body ArcherCore::item {args} {
    eval mgedWrapper item 0 0 1 1 $args
}

::itcl::body ArcherCore::kill {args} {
    eval cadWrapper kill 1 0 1 1 $args
}

::itcl::body ArcherCore::killall {args} {
    eval mgedWrapper killall 1 0 1 1 $args
}

::itcl::body ArcherCore::killtree {args} {
    eval mgedWrapper killtree 1 0 1 1 $args
}

::itcl::body ArcherCore::ls {args} {
    if {$args == {}} {
	eval cadWrapper ls 0 0 0 0 $args
    } else {
	set optionsAndArgs [eval dbExpand $args]
	set options [lindex $optionsAndArgs 0]
	set expandedArgs [lindex $optionsAndArgs 1]

	if {$options == {}} {
	    return $expandedArgs
	} else {
	    return [eval cadWrapper ls 0 0 0 0 $args]
	}
    }
}

::itcl::body ArcherCore::make {args} {
    eval mgedWrapper make 0 0 1 1 $args
}

::itcl::body ArcherCore::make_bb {args} {
    eval mgedWrapper make_bb 0 0 1 1 $args
}

::itcl::body ArcherCore::make_name {args} {
    eval mgedWrapper make_name 0 0 0 0 $args
}

::itcl::body ArcherCore::mater {args} {
    eval mgedWrapper mater 0 0 1 1 $args
}

::itcl::body ArcherCore::mirror {args} {
    eval mgedWrapper mirror 0 0 1 1 $args
}

::itcl::body ArcherCore::move {args} {
    eval cadWrapper mv 0 0 1 1 $args
}

::itcl::body ArcherCore::mv {args} {
    eval cadWrapper mv 0 0 1 1 $args
}

::itcl::body ArcherCore::mvall {args} {
    eval mgedWrapper mvall 0 0 1 1 $args
}

::itcl::body ArcherCore::ocenter {args} {
    if {[llength $args] == 4} {
	set obj [lindex $args 0]

	eval cadWrapper ocenter 0 0 1 0 $args
	redrawObj $obj 0
    } else {
	eval cadWrapper ocenter 0 0 0 0 $args
    }
}

::itcl::body ArcherCore::orotate {obj rx ry rz kx ky kz} {
    eval cadWrapper orotate 0 0 1 0 $obj $rx $ry $rz $kx $ky $kz
    redrawObj $obj 0
}

::itcl::body ArcherCore::oscale {obj sf kx ky kz} {
    eval cadWrapper oscale 0 0 1 0 $obj $sf $kx $ky $kz
    redrawObj $obj 0
}

::itcl::body ArcherCore::otranslate {obj dx dy dz} {
    eval cadWrapper otranslate 0 0 1 0 $obj $dx $dy $dz
    redrawObj $obj 0
}

::itcl::body ArcherCore::push {args} {
    eval mgedWrapper push 0 1 1 0 $args
}

::itcl::body ArcherCore::put {args} {
    eval mgedWrapper put 0 0 1 1 $args
}

::itcl::body ArcherCore::pwd {} {
    ::pwd
}

::itcl::body ArcherCore::r {args} {
    eval mgedWrapper r 0 1 1 1 $args
}

::itcl::body ArcherCore::report {args} {
    eval mgedWrapper report 0 0 0 0 $args
}

::itcl::body ArcherCore::rm {args} {
    eval cadWrapper rm 1 0 1 1 $args
}

::itcl::body ArcherCore::rmater {args} {
    eval mgedWrapper rmater 0 0 1 1 $args
}

::itcl::body ArcherCore::shader {args} {
    eval mgedWrapper shader 0 0 1 1 $args
}

::itcl::body ArcherCore::track {args} {
    eval mgedWrapper track 0 0 1 1 $args
}

::itcl::body ArcherCore::unhide {args} {
    eval mgedWrapper unhide 0 0 1 1 $args
}

::itcl::body ArcherCore::units {args} {
    eval mgedWrapper units 0 0 1 0 $args
}

::itcl::body ArcherCore::wmater {args} {
    eval mgedWrapper wmater 0 0 1 1 $args
}

::itcl::body ArcherCore::packTree {data} {
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

::itcl::body ArcherCore::unpackTree {tree} {
    return " u [unpackTreeGuts $tree]"
}

::itcl::body ArcherCore::unpackTreeGuts {tree} {
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

::itcl::body ArcherCore::vdraw {args} {
    eval mgedWrapper vdraw 0 0 0 0 $args
}

::itcl::body ArcherCore::whichid {args} {
    return [eval $itk_component(mged) whichid $args]
}

::itcl::body ArcherCore::who {args} {
    eval dbCmd who $args
}

::itcl::body ArcherCore::Z {args} {
    eval cadWrapper clear 0 0 0 1 $args
}

::itcl::body ArcherCore::zap {args} {
    eval cadWrapper clear 0 0 0 1 $args
}

::itcl::body ArcherCore::updateDisplaySettings {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    switch -- $mZClipMode \
	$ZCLIP_SMALL_CUBE { \
				$itk_component(mged) zclipAll 1; \
				$itk_component(mged) boundsAll {-4096 4095 -4096 4095 -4096 4095}; \
			    } \
	$ZCLIP_MEDIUM_CUBE { \
				 $itk_component(mged) zclipAll 1; \
				 $itk_component(mged) boundsAll {-8192 8191 -8192 8191 -8192 8191}; \
			     } \
	$ZCLIP_LARGE_CUBE { \
				$itk_component(mged) zclipAll 1; \
				$itk_component(mged) boundsAll {-16384 16363 -16384 16363 -16384 16363}; \
			    } \
	$ZCLIP_NONE { \
			  $itk_component(mged) zclipAll 0; \
		      }
}


################################### Dialogs Section ###################################

::itcl::body ArcherCore::buildInfoDialog {name title info size wrapOption modality} {
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

::itcl::body ArcherCore::buildSaveDialog {} {
    buildInfoDialog saveDialog \
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

::itcl::body ArcherCore::buildViewCenterDialog {} {
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
	    -textvariable [::itcl::scope mCenterX] \
	    -validate key \
	    -vcmd [::itcl::code $this validateDouble %P]
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
	    -textvariable [::itcl::scope mCenterY] \
	    -validate key \
	    -vcmd [::itcl::code $this validateDouble %P]
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
	    -textvariable [::itcl::scope mCenterZ] \
	    -validate key \
	    -vcmd [::itcl::code $this validateDouble %P]
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


################################### Helper Section ###################################

::itcl::body ArcherCore::buildComboBox {parent name1 name2 varName text listOfChoices} {
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


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
