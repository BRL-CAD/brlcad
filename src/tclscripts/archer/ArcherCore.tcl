#                      A R C H E R C O R E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2010 United States Government as represented by
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
    set cursorWaitCount 0

    if {![info exists parentClass]} {
	set parentClass itk::Toplevel
	set inheritFromToplevel 1
    }
}

::itcl::class ArcherCore {
    inherit $ArcherCore::parentClass

    itk_option define -quitcmd quitCmd Command {}
    itk_option define -master master Master "."

    constructor {{_viewOnly 0} {_noCopy 0} args} {}
    destructor {}

    public {
	common application ""
	common splash ""
	common showWindow 0

	common ROTATE_MODE 0
	common TRANSLATE_MODE 1
	common SCALE_MODE 2
	common CENTER_MODE 3
	common CENTER_VIEW_OBJECT_MODE 4
	common COMP_PICK_MODE 5
	common COMP_ERASE_MODE 6
	common MEASURE_MODE 7
	common OBJECT_ROTATE_MODE 8
	common OBJECT_TRANSLATE_MODE 9
	common OBJECT_SCALE_MODE 10
	common OBJECT_CENTER_MODE 11
	common FIRST_FREE_BINDING_MODE 12

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

	common LABEL_BACKGROUND_COLOR [::ttk::style lookup label -background]

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
	method setDefaultBindingMode {_mode}

	# public database commands
	method gedCmd               {args}
	method cmd                 {args}

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
	method putString           {_str}
	method setStatusString     {_str}

	# Commands exposed to the user via the command line.
	# More to be added later...
	method 3ptarb              {args}
	method adjust              {args}
	method arced               {args}
	method attr                {args}
	method bb                  {args}
	method bev                 {args}
	method blast               {args}
	method bo                  {args}
	method bot_condense        {args}
	method bot_decimate        {args}
	method bot_face_fuse       {args}
	method bot_face_sort       {args}
	method bot_flip            {args}
	method bot_merge           {args}
	method bot_smooth          {args}
	method bot_split           {args}
	method bot_sync            {args}
	method bot_vertex_fuse     {args}
	method c                   {args}
	method cd                  {args}
	method clear               {args}
	method clone               {args}
	method color               {args}
	method comb                {args}
	method comb_color          {args}
	method copy                {args}
	method copyeval            {args}
	method copymat             {args}
	method cp                  {args}
	method cpi                 {args}
	method dbconcat            {args}
	method dbExpand	           {args}
	method decompose           {args}
	method delete              {args}
	method draw                {args}
	method E                   {args}
	method edcodes             {args}
	method edcolor             {args}
	method edcomb              {args}
	method edmater             {args}
	method erase               {args}
	method erase_all           {args}
	method ev                  {args}
	method exit                {args}
	method facetize            {args}
	method fracture            {args}
	method hide                {args}
	method human               {args}
	method g                   {args}
	method group               {args}
	method i                   {args}
	method importFg4Section    {args}
	method in                  {args}
	method inside              {args}
	method item                {args}
	method kill                {args}
	method killall             {args}
	method killrefs            {args}
	method killtree            {args}
	method ls                  {args}
	method make		   {args}
	method make_bb             {args}
	method make_pnts           {args}
	method mater               {args}
	method mirror              {args}
	method move                {args}
	method move_arb_edge       {args}
	method move_arb_face       {args}
	method mv                  {args}
	method mvall               {args}
	method nmg_collapse        {args}
	method nmg_simplify        {args}
	method ocenter		   {args}
	method orotate		   {args}
	method oscale		   {args}
	method otranslate	   {args}
	method p                   {args}
	method prefix              {args}
	method protate             {args}
	method pscale              {args}
	method ptranslate          {args}
	method push                {args}
	method put                 {args}
	method put_comb            {args}
	method putmat              {args}
	method pwd                 {}
	method r                   {args}
	method rcodes              {args}
	method red                 {args}
	method rfarb               {args}
	method rm                  {args}
	method rmater              {args}
	method rotate_arb_face     {args}
	method search		   {args}
	method shader              {args}
	method shells              {args}
	method tire                {args}
	method title               {args}
	method track               {args}
	method unhide              {args}
	method units               {args}
	method vmake               {args}
	method wmater              {args}
	method xpush               {args}
	method Z                   {args}
	method zap                 {args}

	set brlcadDataPath [bu_brlcad_data ""]
	if {$tcl_platform(platform) != "windows"} {
	    set SystemWindowFont Helvetica
	    set SystemWindowText black
	    set SystemWindow $LABEL_BACKGROUND_COLOR
	    set SystemHighlight black
	    set SystemHighlightText \#ececec
	    set SystemButtonFace $LABEL_BACKGROUND_COLOR
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
	variable mLighting 1
	variable mRenderMode -1
	variable mActivePane
	variable mActivePaneName
	variable mCurrentPaneName ""
	variable mStatusStr ""
	variable mDbType ""
	variable mDbReadOnly 0
	variable mDbNoCopy 0
	variable mDbShared 0
	variable mProgress 0
	variable mProgressBarWidth 200
	variable mProgressBarHeight ""
	#variable mProgressString ""
	variable mNeedSave 0
	variable mPrevSelectedObjPath ""
	variable mPrevSelectedObj ""
	variable mSelectedObjPath ""
	variable mSelectedObj ""
	variable mSelectedObjType ""
	variable mPasteActive 0
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

	variable mShowViewAxes 0
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

	# This is mostly a list of wrapped Ged commands. However, it also contains
	# a few commands that are implemented here in ArcherCore.
	variable mArcherCoreCommands { \
					   3ptarb adjust arced attr bb bev blast bo \
					   bot2pipe bot_condense bot_decimate bot_face_fuse \
					   bot_face_sort bot_flip bot_merge bot_smooth bot_split bot_sync bot_vertex_fuse \
					   c cd clear clone color comb comb_color copy copyeval copymat \
					   cp cpi dbconcat dbExpand decompose delete draw E edcodes edcolor edcomb \
					   edmater erase_all ev exit facetize fracture \
					   g group hide human i importFg4Section \
					   in inside item kill killall killrefs killtree ls \
					   make make_bb make_pnts mater mirror move move_arb_edge move_arb_face \
					   mv mvall nmg_collapse nmg_simplify \
					   ocenter orotate oscale otranslate p packTree prefix protate pscale ptranslate \
					   push put put_comb putmat pwd r rcodes red rfarb rm rmater \
					   rotate_arb_face search shader shells tire title track \
					   unhide units unpackTree \
					   vmake wmater xpush Z zap
	}
	variable mUnwrappedDbCommands {}
	variable mBannedDbCommands {
	    dbip open rtabort shaded_mode
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

	variable mMouseRayCallbacks ""
	variable mLastTags ""

	method handleMoreArgs {args}

	method gedWrapper {_cmd _eflag _hflag _sflag _tflag args}

	method buildCanvasMenubar {}

	method redrawObj {_obj {_wflag 1}}

	method colorMenuStatusCB {_w}
	method menuStatusCB {_w}
	method menuStatusCB_junk {_w}
	method transparencyMenuStatusCB {_w}

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

	# init functions
	method initTree          {}
	method initGed          {}
	method closeMged         {}
	method updateRtControl   {}

	# interface operations
	method closeDb           {}
	method newDb             {}
	method openDb            {}
	method saveDb            {}
	method primaryToolbarAdd        {_type _name {args ""}}
	method primaryToolbarRemove     {_index}

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

	# private mged commands
	method alterObj          {_operation _obj}
	method deleteObj         {_obj}
	method doCopyOrMove      {_top _obj _cmd}

	method buildPrimaryToolbar {}

	method beginViewRotate {}
	method endViewRotate {_pane}

	method beginViewScale {}
	method endViewScale {_pane}

	method beginViewTranslate {}
	method endViewTranslate {_pane}

	method initCenterMode {}
	method initCenterViewObjectMode {}

	method initCompErase {}
	method initCompPick {}
	method mrayCallback_cvo {_start _target _partitions}
	method mrayCallback_erase {_start _target _partitions}
	method mrayCallback_pick {_start _target _partitions}

	method initViewMeasure {}
	method endViewMeasure {_mstring}

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

	method watchVar {_name1 _name2 _op}
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
	    -showhandle 0 \
	    -background $LABEL_BACKGROUND_COLOR
    } {}

    $itk_component(hpane) add topView
    $itk_component(hpane) paneconfigure topView \
	-margin 0

    if {!$mViewOnly} {
	$itk_component(hpane) add bottomView
	$itk_component(hpane) paneconfigure bottomView \
	    -margin 0 \
	    -minimum 0

	set parent [$itk_component(hpane) childsite bottomView]
	itk_component add advancedTabs {
	    ::ttk::notebook $parent.tabs
	} {}

	itk_component add cmd {
	    Command $itk_component(advancedTabs).cmd \
		-relief sunken -borderwidth 2 \
		-hscrollmode none -vscrollmode dynamic \
		-scrollmargin 2 -visibleitems 80x15 \
		-textbackground $SystemWindow -prompt "ArcherCore> " \
		-prompt2 "% " -result_color black -cmd_color red \
		-background $LABEL_BACKGROUND_COLOR
	} {}

	itk_component add history {
	    ::iwidgets::scrolledtext $itk_component(advancedTabs).history \
		-relief sunken -borderwidth 2 \
		-hscrollmode none -vscrollmode dynamic \
		-scrollmargin 2 -visibleitems 80x15 \
		-textbackground $SystemWindow
	} {}
	[$itk_component(history) component text] configure -state disabled

	$itk_component(advancedTabs) add $itk_component(cmd) -text "Command"
	$itk_component(advancedTabs) add $itk_component(history) -text "History"
    }

    # vertical panes
    set parent [$itk_component(hpane) childsite topView]
    itk_component add vpane {
	::iwidgets::panedwindow $parent.vpane \
	    -orient vertical \
	    -thickness 5 \
	    -sashborderwidth 1 \
	    -sashcursor sb_h_double_arrow \
	    -showhandle 0 \
	    -background $LABEL_BACKGROUND_COLOR
    } {}

    $itk_component(vpane) add hierarchyView
    if {!$mViewOnly} {
	$itk_component(vpane) add geomView
	$itk_component(vpane) add attrView
	$itk_component(vpane) paneconfigure hierarchyView \
	    -margin 0 \
	    -minimum 0
	$itk_component(vpane) paneconfigure geomView \
	    -margin 0
	$itk_component(vpane) paneconfigure attrView \
	    -margin 0 \
	    -minimum 0
    } else {
	$itk_component(vpane) add geomView
	$itk_component(vpane) paneconfigure geomView \
	    -margin 0 \
	    -minimum 0
    }

    # frame for all geometry canvas's
    set parent [$itk_component(vpane) childsite geomView]
    itk_component add canvasF {
	::ttk::frame $parent.canvasF \
	    -borderwidth 1 \
	    -relief sunken
    } {}

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

    buildCanvasMenubar

    if {!$mViewOnly} {
	# dummy canvas
	itk_component add canvas {
	    ::frame $itk_component(canvasF).canvas \
		-borderwidth 0 \
		-relief flat 
	} {}
	grid $itk_component(canvas_menu) -row 0 -column 1 -sticky w
	grid $itk_component(canvas) -row 1 -column 0 -columnspan 3 -sticky news
	grid columnconfigure $itk_component(canvasF) 1 -weight 1
	grid rowconfigure $itk_component(canvasF) 1 -weight 1

	# status bar
	itk_component add statusF {
	    ::ttk::frame $itk_interior.statfrm
	} {}

	itk_component add status {
	    ::ttk::label  $itk_component(statusF).status -anchor w -relief sunken \
		-font $mFontText \
		-textvariable [::itcl::scope mStatusStr]
	} {}

	itk_component add progress {
	    ::canvas $itk_component(statusF).progress \
		-relief sunken \
		-bd 1 \
		-background $LABEL_BACKGROUND_COLOR \
		-width $mProgressBarWidth \
		-height $mProgressBarHeight
	} {}

	itk_component add editLabel {
	    ::ttk::label  $itk_component(statusF).edit -relief sunken \
		-font $mFontText \
		-width 5
	} {}

	itk_component add dbtype {
	    ::ttk::label  $itk_component(statusF).dbtype -anchor w -relief sunken \
		-font $mFontText \
		-width 8 -textvariable [::itcl::scope mDbType]
	} {}

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
		-borderwidth 0 \
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
	::ttk::frame $itk_interior.north -height 0
    } {}
    itk_component add south {
	::ttk::frame $itk_interior.south -height 0
    } {}
    set parent [$itk_component(hpane) childsite topView]
    itk_component add east {
	::ttk::frame $parent.east -width 0
    } {}
    itk_component add west {
	::ttk::frame $itk_interior.west -width 0
    } {}

    pack $itk_component(south) -side bottom -fill x
    pack $itk_component(north) -side top -fill x
    pack $itk_component(west)  -side left -fill y
    pack $itk_component(east)  -side right -fill y
    if {!$mViewOnly} {
	pack $itk_component(advancedTabs) -fill both -expand yes
	pack $itk_component(statusF) -before $itk_component(south) -side bottom -fill x
    }
    pack $itk_component(tree) -fill both -expand yes
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

    buildPrimaryToolbar

    trace add variable [::itcl::scope mMeasuringStickColor] write watchVar
    trace add variable [::itcl::scope mMeasuringStickMode] write watchVar
    trace add variable [::itcl::scope mPrimitiveLabelColor] write watchVar
    trace add variable [::itcl::scope mScaleColor] write watchVar

    trace add variable [::itcl::scope mModelAxesColor] write watchVar
    trace add variable [::itcl::scope mModelAxesLabelColor] write watchVar
    trace add variable [::itcl::scope mModelAxesTickColor] write watchVar
    trace add variable [::itcl::scope mModelAxesTickMajorColor] write watchVar

    trace add variable [::itcl::scope mViewingParamsColor] write watchVar
    trace add variable [::itcl::scope mViewAxesColor] write watchVar
    trace add variable [::itcl::scope mViewAxesLabelColor] write watchVar

    eval itk_initialize $args

    $this configure -background $LABEL_BACKGROUND_COLOR

    if {!$mViewOnly} {
	# set initial toggle variables
	set mVPaneToggle3 $mVPaneFraction3
	set mVPaneToggle5 $mVPaneFraction5

	updateSaveMode
    }

    backgroundColor [lindex $mBackground 0] \
	[lindex $mBackground 1] \
	[lindex $mBackground 2]

    updateTheme

    $itk_component(primaryToolbar) itemconfigure open -state normal

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
    catch {::itcl::delete object $itk_component(ged)}

    set mTargetOldCopy $mTargetCopy
    deleteTargetOldCopy
}

# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------




::itcl::body ArcherCore::handleMoreArgs {args} {
    eval $itk_component(cmd) print_more_args_prompt $args
    return [$itk_component(cmd) get_more_args]
}

::itcl::body ArcherCore::gedWrapper {cmd eflag hflag sflag tflag args} {
    if {![info exists itk_component(ged)]} {
	return
    }

    SetWaitCursor $this

    if {$eflag} {
	set optionsAndArgs [eval dbExpand $args]
	set options [lindex $optionsAndArgs 0]
	set expandedArgs [lindex $optionsAndArgs 1]
    } else {
	set options {}
	set expandedArgs $args
    }

    if {[catch {eval gedCmd $cmd $options $expandedArgs} ret]} {
	SetNormalCursor $this
	error $ret
    }

    if {$sflag} {
	set mNeedSave 1
	updateSaveMode
    }

    gedCmd configure -primitiveLabels {}
    if {$tflag} {
	catch {refreshTree}
    }
    SetNormalCursor $this

    return $ret
}

::itcl::body ArcherCore::buildCanvasMenubar {}  {
    if {$mViewOnly} {
	# View Menu
	$itk_component(canvas_menu) add menubutton view \
	    -text "View" -menu {
		options -tearoff 0

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
		options -tearoff 0

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
	$itk_component(canvas_menu) add menubutton raytrace \
	    -text "Raytrace" -menu {
		options -tearoff 0

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
    if {![info exists itk_component(ged)]} {
	return
    }

    if {$obj == ""} {
	return
    }

    set dlist [gedCmd who]
    foreach pelement [split $obj /] {
	set new_dlist {}
	set dlen [llength $dlist]
	foreach ditem $dlist {
	    if {[lsearch [split $ditem /] $pelement] != -1} {
		set renderData [gedCmd how -b $ditem]
		set renderMode [lindex $renderData 0]
		set renderTrans [lindex $renderData 1]
		render $ditem $renderMode $renderTrans 0 $wflag
	    } else {
		lappend new_dlist $ditem
	    }
	}

	if {$new_dlist == {}} {
	    break
	}

	set dlist $new_dlist
    }
}

::itcl::body ArcherCore::initTree {} {
    set parent [$itk_component(vpane) childsite hierarchyView]
    itk_component add tree {
	::swidgets::tree $parent.tree \
	    -background white \
	    -selectfill 1 \
	    -selectbackground black \
	    -selectforeground $SystemWindow \
	    -querycmd [::itcl::code $this fillTree %n] \
	    -selectcmd [::itcl::code $this selectNode %n] \
	    -dblselectcmd [::itcl::code $this dblClick %n] \
	    -textmenuloadcommand [::itcl::code $this loadMenu]
    } {}

    [$itk_component(tree) component popupmenu] configure \
	-background $SystemButtonFace \
	-activebackground $SystemHighlight \
	-activeforeground $SystemHighlightText
}

::itcl::body ArcherCore::initGed {} {
    itk_component add ged {
	if {$mDbNoCopy || $mDbReadOnly} {
	    set _target $mTarget
	} else {
	    set _target $mTargetCopy
	}

	cadwidgets::Ged $itk_component(canvasF).mged $_target \
	    -type $mDisplayType \
	    -showhandle 0 \
	    -sashcursor sb_v_double_arrow \
	    -hsashcursor sb_h_double_arrow \
	    -showViewingParams $mShowViewingParams \
	    -multi_pane $mMultiPane
    } {
	keep -sashwidth -sashheight -sashborderwidth
	keep -sashindent -thickness
    }
    set tmp_dbCommands [$itk_component(ged) getUserCmds]
    set mUnwrappedDbCommands {}
    foreach cmd $tmp_dbCommands {
	if {[lsearch $mArcherCoreCommands $cmd] == -1 &&
	    [lsearch $mBannedDbCommands $cmd] == -1} {
	    lappend mUnwrappedDbCommands $cmd
	}
    }

    if {!$mViewOnly} {
	$itk_component(ged) set_outputHandler "$itk_component(cmd) putstring"
    }
    $itk_component(ged) transparency_all 1
    $itk_component(ged) bounds_all "-4096 4095 -4096 4095 -4096 4095"
    $itk_component(ged) more_args_callback [::itcl::code $this handleMoreArgs]
    $itk_component(ged) history_callback [::itcl::code $this addHistory]


    # RT Control Panel
    itk_component add rtcntrl {
	RtControl $itk_interior.rtcp -mged $itk_component(ged) -tearoff 0
    } {
	usual
    }
    $itk_component(ged) fb_active 0
    $itk_component(rtcntrl) update_fb_mode
    bind $itk_component(rtcntrl) <Visibility> "raise $itk_component(rtcntrl)"
    bind $itk_component(rtcntrl) <FocusOut> "raise $itk_component(rtcntrl)"
    wm protocol $itk_component(rtcntrl) WM_DELETE_WINDOW "$itk_component(rtcntrl) deactivate"

    # create view axes control panel
    #    itk_component add vac {
    #	ViewAxesControl $itk_interior.vac -mged $itk_component(ged)
    #    } {
    #	usual
    #    }

    # create model axes control panel
    #    itk_component add mac {
    #	ModelAxesControl $itk_interior.mac -mged $itk_component(ged)
    #    } {
    #	usual
    #    }

    #    wm protocol $itk_component(vac) WM_DELETE_WINDOW "$itk_component(vac) hide"
    #    wm protocol $itk_component(mac) WM_DELETE_WINDOW "$itk_component(mac) hide"

    #    $itk_component(ged) configure -unitsCallback "$itk_component(mac) updateControlPanel"
    $itk_component(ged) configure -paneCallback [::itcl::code $this updateActivePane]

    # Other bindings for mged
    #bind $itk_component(ged) <Enter> {focus %W}

    if {$mViewOnly} {
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

    bind $itk_component(canvasF) <Configure> [::itcl::code $this updateRtControl]
    setActivePane ur
}

::itcl::body ArcherCore::closeMged {} {
    catch {delete object $itk_component(rtcntrl)}
    #    catch {delete object $itk_component(vac)}
    #    catch {delete object $itk_component(mac)}
    catch {delete object $itk_component(ged)}
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
    pack forget $itk_component(ged)
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

    ::update
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
    SetWaitCursor $this
    gedCmd kill $comp

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
	gedCmd rm $grp $comp
    }

    # remove from tree
    set parent [$itk_component(tree) query -parent $element]
    $itk_component(tree) remove $element $parent
    refreshTree
    SetNormalCursor $this
}

::itcl::body ArcherCore::doCopyOrMove {top comp cmd} {
    set mNeedSave 1
    updateSaveMode
    SetWaitCursor $this
    set comp2 [string trim [$top.entry get]]
    wm withdraw $top
    gedCmd $cmd $comp
    refreshTree
    SetNormalCursor $this
    destroy $top
}

::itcl::body ArcherCore::buildPrimaryToolbar {} {
    # tool bar
    itk_component add primaryToolbar {
	::iwidgets::toolbar $itk_interior.primarytoolbar \
	    -helpvariable [::itcl::scope mStatusStr] \
	    -balloonfont "{CG Times} 8" \
	    -balloonbackground \#ffffdd \
	    -borderwidth 1 \
	    -orient horizontal \
	    -background $LABEL_BACKGROUND_COLOR
    } {}

    $itk_component(primaryToolbar) add button open \
	-balloonstr "Open an existing geometry file" \
	-helpstr "Open an existing geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this openDb]

    if {!$mViewOnly} {
	$itk_component(primaryToolbar) add button save \
	    -balloonstr "Save the current geometry file" \
	    -helpstr "Save the current geometry file" \
	    -relief flat \
	    -overrelief raised \
	    -command [::itcl::code $this askToSave]
    }

    $itk_component(primaryToolbar) add radiobutton rotate \
	-balloonstr "Rotate view" \
	-helpstr "Rotate view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $ROTATE_MODE \
	-command [::itcl::code $this beginViewRotate]
    $itk_component(primaryToolbar) add radiobutton translate \
	-balloonstr "Translate view" \
	-helpstr "Translate view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $TRANSLATE_MODE \
	-command [::itcl::code $this beginViewTranslate] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton scale \
	-balloonstr "Scale view" \
	-helpstr "Scale view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $SCALE_MODE \
	-command [::itcl::code $this beginViewScale] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton center \
	-balloonstr "Center view" \
	-helpstr "Center view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $CENTER_MODE \
	-command [::itcl::code $this initCenterMode] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton centervo \
	-balloonstr "Center View on Object" \
	-helpstr "Center View on Object" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $CENTER_VIEW_OBJECT_MODE \
	-command [::itcl::code $this initCenterViewObjectMode] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton cpick \
	-balloonstr "Component Pick" \
	-helpstr "Component Pick" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $COMP_PICK_MODE \
	-command [::itcl::code $this initCompPick] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton cerase \
	-balloonstr "Component Erase" \
	-helpstr "Component Erase" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $COMP_ERASE_MODE \
	-command [::itcl::code $this initCompErase] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton measure \
	-balloonstr "Measuring Tool" \
	-helpstr "Measuring Tool" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $MEASURE_MODE \
	-command [::itcl::code $this initViewMeasure] \
	-state disabled

    $itk_component(primaryToolbar) itemconfigure rotate -state disabled
    $itk_component(primaryToolbar) itemconfigure translate -state disabled
    $itk_component(primaryToolbar) itemconfigure scale -state disabled
    $itk_component(primaryToolbar) itemconfigure center -state disabled
    $itk_component(primaryToolbar) itemconfigure centervo -state disabled
    $itk_component(primaryToolbar) itemconfigure cpick -state disabled
    $itk_component(primaryToolbar) itemconfigure cerase -state disabled
    $itk_component(primaryToolbar) itemconfigure measure -state disabled

    eval pack configure [pack slaves $itk_component(primaryToolbar)] -padx 2

    if {$mViewOnly} {
	grid $itk_component(primaryToolbar) \
	    -row 0 \
	    -column 0 \
	    -in $itk_component(canvasF) \
	    -sticky e
    } else {
	pack $itk_component(primaryToolbar) \
	    -before $itk_component(north) \
	    -side top \
	    -fill x \
	    -pady 2
    }
}

::itcl::body ArcherCore::beginViewRotate {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) init_view_rotate
}

::itcl::body ArcherCore::endViewRotate {_pane} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) end_view_rotate $_pane

    set ae [$itk_component(ged) pane_aet $_pane]
    addHistory "ae $ae"
}

::itcl::body ArcherCore::beginViewScale {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) init_view_scale
}

::itcl::body ArcherCore::endViewScale {_pane} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) end_view_scale $_pane

    set size [$itk_component(ged) pane_size $_pane]
    addHistory "size $size"
}

::itcl::body ArcherCore::beginViewTranslate {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) init_view_translate
}

::itcl::body ArcherCore::endViewTranslate {_pane} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) end_view_translate $_pane

    set center [$itk_component(ged) pane_center $_pane]
    addHistory "center $center"
}

::itcl::body ArcherCore::initCenterMode {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) init_view_center
}

::itcl::body ArcherCore::initCenterViewObjectMode {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) clear_mouse_ray_callback_list
    $itk_component(ged) add_mouse_ray_callback [::itcl::code $this mrayCallback_cvo]
    $itk_component(ged) init_comp_pick
}

::itcl::body ArcherCore::initCompErase {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) clear_mouse_ray_callback_list
    $itk_component(ged) add_mouse_ray_callback [::itcl::code $this mrayCallback_erase]
    $itk_component(ged) init_comp_pick
}

::itcl::body ArcherCore::initCompPick {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) clear_mouse_ray_callback_list
    $itk_component(ged) add_mouse_ray_callback [::itcl::code $this mrayCallback_pick]
    $itk_component(ged) init_comp_pick
}

::itcl::body ArcherCore::mrayCallback_cvo {_start _target _partitions} {
    if {$_partitions == ""} {
	set rpos [$itk_component(ged) lastMouseRayPos]
	eval $itk_component(ged) vslew $rpos
	return
    }

    set partition [lindex $_partitions 0]

    if {[catch {bu_get_value_by_keyword in $partition} in]} {
	putString "Partition does not contain an \"in\""
	putString "$in"
	return
    }

    if {[catch {bu_get_value_by_keyword point $in} point]} {
	putString "Partition does not contain an \"in\" point"
	putString "$point"
	return
    }

    set point [vscale $point [$itk_component(ged) base2local]]
    $itk_component(ged) center $point
}

::itcl::body ArcherCore::mrayCallback_erase {_start _target _partitions} {
    if {$_partitions == ""} {
	return
    }

    set partition [lindex $_partitions 0]

    if {[catch {bu_get_value_by_keyword in $partition} in]} {
	putString "Partition does not contain an \"in\""
	putString "$in"
	return
    }

    if {[catch {bu_get_value_by_keyword path $in} path]} {
	putString "Partition does not contain an \"in\" path"
	putString "[subst $[subst pt_$i]]"
	return
    }

    erase $path
    putString "erase $path"
    set mStatusStr "erase $path"
}

::itcl::body ArcherCore::mrayCallback_pick {_start _target _partitions} {
    set partition [lindex $_partitions 0]
    if {$partition == {}} {
	putString "Missed!"
	set mStatusStr "Missed!"
    } else {
	set region [bu_get_value_by_keyword "region" $partition]
	putString "$region"
	set mStatusStr "$region"
    }
}

::itcl::body ArcherCore::initViewMeasure {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) clear_view_measure_callback_list
    $itk_component(ged) add_view_measure_callback [::itcl::code $this endViewMeasure]
    $itk_component(ged) init_view_measure
}

::itcl::body ArcherCore::endViewMeasure {_mstring} {
    putString $_mstring
    set mStatusStr $_mstring
}

::itcl::body ArcherCore::initDefaultBindings {{_comp ""}} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(primaryToolbar) itemconfigure rotate -state normal
    $itk_component(primaryToolbar) itemconfigure translate -state normal
    $itk_component(primaryToolbar) itemconfigure scale -state normal
    $itk_component(primaryToolbar) itemconfigure center -state normal
    $itk_component(primaryToolbar) itemconfigure centervo -state normal
    $itk_component(primaryToolbar) itemconfigure cpick -state normal
    $itk_component(primaryToolbar) itemconfigure cerase -state normal
    $itk_component(primaryToolbar) itemconfigure measure -state normal

    $itk_component(ged) init_view_bindings

    # Initialize rotate mode
    set mDefaultBindingMode $ROTATE_MODE
    beginViewRotate
}

::itcl::body ArcherCore::initBrlcadBindings {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(primaryToolbar) itemconfigure rotate -state disabled
    $itk_component(primaryToolbar) itemconfigure translate -state disabled
    $itk_component(primaryToolbar) itemconfigure scale -state disabled
    $itk_component(primaryToolbar) itemconfigure center -state disabled
    $itk_component(primaryToolbar) itemconfigure centervo -state disabled
    $itk_component(primaryToolbar) itemconfigure cpick -state disabled
    $itk_component(primaryToolbar) itemconfigure cerase -state disabled
    $itk_component(primaryToolbar) itemconfigure measure -state disabled

    $itk_component(ged) init_view_bindings brlcad
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
    set mCurrentPaneName ""
    set mBackground [list $r $g $b]

    if {![info exists itk_component(ged)]} {
	set color [getTkColor \
		       [lindex $mBackground 0] \
		       [lindex $mBackground 1] \
		       [lindex $mBackground 2]]
	$itk_component(canvas) configure -background $color
    } else {
	eval gedCmd bg_all $mBackground
    }
}


::itcl::body ArcherCore::updateHPaneFractions {} {
    if {$mViewOnly} {
	return
    }

    if {[catch {$itk_component(hpane) fraction} fractions]} {
	return
    }

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

::itcl::body ArcherCore::gedCmd {args} {
    if {![info exists itk_component(ged)]} {
	return
    }

    return [eval $itk_component(ged) $args]
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
		    if {[lsearch $mArcherCoreCommands $subcmd] == -1 &&
			[lsearch $mUnwrappedDbCommands $subcmd] == -1} {
			error "ArcherCore::cmd: unrecognized command - $subcmd"
		    } else {
			return $subcmd
		    }
		} else {
		    return [eval list $mArcherCoreCommands $mUnwrappedDbCommands]
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

    set i [lsearch -exact $mArcherCoreCommands $cmd]
    if {$i != -1} {
	addHistory $args
	return [eval $args]
    }

    set i [lsearch -exact $mUnwrappedDbCommands $cmd]
    if {$i != -1} {
	addHistory $args
	return [eval gedCmd $args]
    }

    error "ArcherCore::cmd: unrecognized command - $args, check source code"
}

# ------------------------------------------------------------
#                  DB/DISPLAY COMMANDS
# ------------------------------------------------------------
::itcl::body ArcherCore::getNodeChildren {node} {
    if {$node == ""} {
	return {}
    }

    if {[catch {gedCmd get $node tree} tlist]} {
	return {}
    }

    # first remove any matrices
    regsub -all -- {\{-?[0-9]+[^\}]+-?[0-9]+\}} $tlist "" tlist

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
    set renderMode [gedCmd how $_node]
    if {$renderMode < 0} {
	render $_node 0 1 1
    } else {
	render $_node -1 1 1
    }
}


::itcl::body ArcherCore::render {node state trans updateTree {wflag 1}} {
    if {$wflag} {
	SetWaitCursor $this
    }

    set tnode [file tail $node]
    set saveGroundPlane 0

    if {$mShowPrimitiveLabels} {
	set plnode $node
    } else {
	set plnode {}
    }

    $itk_component(ged) refresh_off

    catch {
    if {[catch {gedCmd attr get \
		    $tnode displayColor} displayColor]} {
	switch -exact -- $state {
	    "0" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd draw -m0 -x$trans $node
	    }
	    "1" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd draw -m1 -x$trans $node
	    }
	    "2" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd draw -m2 -x$trans $node
	    }
	    "3" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd E $node
	    }
	    "4" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd draw -h $node
	    }
	    "-1" {
		gedCmd configure -primitiveLabels {}
		gedCmd erase $node
	    }
	}
    } else {
	switch -exact -- $state {
	    "0" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd draw -m0 -x$trans \
		    -C$displayColor $node
	    }
	    "1" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd draw -m1 -x$trans \
		    -C$displayColor $node
	    }
	    "2" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd draw -m2 -x$trans \
		    -C$displayColor $node
	    }
	    "3" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd E -C$displayColor $node
	    }
	    "4" {
		gedCmd configure -primitiveLabels $plnode
		gedCmd draw -h -C$displayColor $node
	    }
	    "-1" {
		gedCmd configure -primitiveLabels {}
		gedCmd erase $node
	    }
	}
    }
    }

    $itk_component(ged) refresh_on
    $itk_component(ged) refresh_all
 
    # Turn ground plane back on if it was on before the draw
    if {$saveGroundPlane} {
	set mShowGroundPlane 1
	showGroundPlane
    }

    if {$updateTree} {
	updateTree
    }

    if {$wflag} {
	SetNormalCursor $this
    }
}

::itcl::body ArcherCore::selectDisplayColor {node} {
    set tnode [file tail $node]

    if {[catch {gedCmd attr get \
		    $tnode displayColor} displayColor] &&
	[catch {gedCmd attr get \
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
	gedCmd attr rm $tnode displayColor
    } else {
	gedCmd attr set $tnode \
	    displayColor "[lindex $rgb 0]/[lindex $rgb 1]/[lindex $rgb 2]"
    }

    set drawState [gedCmd how -b $node]

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
    gedCmd set_transparency $node $alpha
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

::itcl::body ArcherCore::setActivePane {_pane} {
    $itk_component(ged) pane $_pane
}

::itcl::body ArcherCore::updateActivePane {args} {
    # update active pane radiobuttons
    switch -- $args {
	ul {
	    set mActivePane 0
	    set mActivePaneName ul
	}
	ur {
	    set mActivePane 1
	    set mActivePaneName ur
	}
	ll {
	    set mActivePane 2
	    set mActivePaneName ll
	}
	lr {
	    set mActivePane 3
	    set mActivePaneName lr
	}
    }

    set mShowModelAxes [gedCmd cget -modelAxesEnable]
    set mShowModelAxesTicks [gedCmd cget -modelAxesTickEnabled]
    set mShowViewAxes [gedCmd cget -viewAxesEnable]
}

::itcl::body ArcherCore::doMultiPane {} {
    gedCmd configure -multi_pane $mMultiPane
}

::itcl::body ArcherCore::doLighting {} {
    SetWaitCursor $this

    if {$mZClipMode != $ZCLIP_NONE} {
	gedCmd zclip_all $mLighting
    }

    gedCmd zbuffer_all $mLighting
    gedCmd light_all $mLighting

    SetNormalCursor $this
}

::itcl::body ArcherCore::doViewReset {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    set mCurrentPaneName ""
    gedCmd autoview_all
    gedCmd default_views
}

::itcl::body ArcherCore::doAutoview {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    if {$mCurrentPaneName == ""} {
	set pane $mActivePaneName
    } else {
	set pane $mCurrentPaneName
    }
    set mCurrentPaneName ""

    $itk_component(ged) pane_autoview $pane
}

::itcl::body ArcherCore::doViewCenter {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    if {$mCurrentPaneName == ""} {
	set pane $mActivePaneName
    } else {
	set pane $mCurrentPaneName
    }
    set mCurrentPaneName ""

    set center [$itk_component(ged) pane_center $pane]
    set mCenterX [lindex $center 0]
    set mCenterY [lindex $center 1]
    set mCenterZ [lindex $center 2]

    set mDbUnits [gedCmd units -s]
    $itk_component(centerDialog) center [namespace tail $this]
    ::update
    if {[$itk_component(centerDialog) activate]} {
	$itk_component(ged) pane_center $pane $mCenterX $mCenterY $mCenterZ
    }
}

::itcl::body ArcherCore::doAe {_az _el} {
    if {$mCurrentPaneName == ""} {
	set pane $mActivePaneName
    } else {
	set pane $mCurrentPaneName
    }
    set mCurrentPaneName ""

    $itk_component(ged) pane_ae $pane $_az $_el

    addHistory "ae $_az $_el"
}

::itcl::body ArcherCore::showViewAxes {} {
    catch {gedCmd configure -viewAxesEnable $mShowViewAxes}
}

::itcl::body ArcherCore::showModelAxes {} {
    catch {gedCmd configure -modelAxesEnable $mShowModelAxes}
}

::itcl::body ArcherCore::showModelAxesTicks {} {
    catch {gedCmd configure -modelAxesTickEnabled $mShowModelAxesTicks}
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

	catch {
	    # set the open state of nodes
	    foreach path $paths {
		toggleTreePath $path
	    }

	    if {$selNodePath != ""} {
		toggleTreePath $selNodePath
	    }
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
	if {0 <= [gedCmd how $nname]} {
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
	set tops [gedCmd tops]
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
	set l [lindex [split [gedCmd l $cname] "\n"] 0]
	if {[lindex $l [expr [llength $l] -1]] == "--"} {set stem "branch"}

	# add to tree
	set cnode [$itk_component(tree) insert end $node $cname $stem]
	set cpath [$itk_component(tree) query -path $cnode]

	if {0 <= [gedCmd how $cpath]} {
	    $itk_component(tree) alternode $cnode -color blue
	} else {
	    $itk_component(tree) alternode $cnode -color black
	}

	set stem "leaf"
    }

    ::update idletasks
}

::itcl::body ArcherCore::selectNode {tags {rflag 1}} {
    set mLastTags $tags
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

    if {[catch {gedCmd get_type $node} ret]} {
	if {$savePwd != ""} {
	    cd $savePwd
	}

	return
    }

    # label the object if it's being drawn
    set mRenderMode [gedCmd how $node]

    if {$mShowPrimitiveLabels} {
	if {0 <= $mRenderMode} {
	    gedCmd configure -primitiveLabels $node
	} else {
	    gedCmd configure -primitiveLabels {}
	}
    }

#    if {$mShowPrimitiveLabels && 0 <= $mRenderMode} {
#	gedCmd configure -primitiveLabels $node
#    } else {
#	gedCmd configure -primitiveLabels {}
#    }
#
#    if {$rflag} {
#	gedCmd refresh
#    }

    set mPrevSelectedObjPath $mSelectedObjPath
    set mPrevSelectedObj $mSelectedObj

    if {$savePwd != ""} {
	cd $savePwd
    }

    $itk_component(tree) selection clear
    $itk_component(tree) selection set $element
}

::itcl::body ArcherCore::dblClick {tags} {
    set mLastTags $tags
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

    set mRenderMode [gedCmd how $node]
    # do this in case "ev" was used from the command line
    if {2 < $mRenderMode} {
	set mRenderMode 2
    }

    if {$nodeType == "leaf"} {
	$menu add radiobutton -label "Wireframe" \
	    -indicatoron 1 -value 0 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $node 0 1 1]

	$menu add radiobutton -label "Shaded" \
	    -indicatoron 1 -value 1 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $node 1 1 1]
#	$menu add radiobutton -label "Shaded (Mode 2)" \
#	    -indicatoron 1 -value 2 -variable [::itcl::scope mRenderMode] \
#	    -command [::itcl::code $this render $node 2 1 1]
	$menu add radiobutton -label "Hidden Line)" \
	    -indicatoron 1 -value 2 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $node 4 1 1]

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

	$menu add command -label "Shaded" \
	    -command [::itcl::code $this render $node 1 1 1]
#	$menu add command -label "Shaded (Mode 2)" \
#	    -command [::itcl::code $this render $node 2 1 1]
	$menu add command -label "Hidden Line" \
	    -command [::itcl::code $this render $node 4 1 1]

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

	$trans add command -label "None" \
	    -command [::itcl::code $this setTransparency $node 1.0]
#	#$trans add command -label "25%" \
#	    #	-command [::itcl::code $this setTransparency $node 0.75]
#	#$trans add command -label "50%" \
#	    #	-command [::itcl::code $this setTransparency $node 0.5]
#	#$trans add command -label "75%" \
#	    #	-command [::itcl::code $this setTransparency $node 0.25]
#	$trans add command -label "10%" \
#	    -command [::itcl::code $this setTransparency $node 0.9]
#	$trans add command -label "20%" \
#	    -command [::itcl::code $this setTransparency $node 0.8]
#	$trans add command -label "30%" \
#	    -command [::itcl::code $this setTransparency $node 0.7]
#	$trans add command -label "40%" \
#	    -command [::itcl::code $this setTransparency $node 0.6]
#	$trans add command -label "50%" \
#	    -command [::itcl::code $this setTransparency $node 0.5]
#	$trans add command -label "60%" \
#	    -command [::itcl::code $this setTransparency $node 0.4]
#	$trans add command -label "70%" \
#	    -command [::itcl::code $this setTransparency $node 0.3]
	$trans add command -label "80%" \
	    -command [::itcl::code $this setTransparency $node 0.2]
	$trans add command -label "85%" \
	    -command [::itcl::code $this setTransparency $node 0.15]
	$trans add command -label "90%" \
	    -command [::itcl::code $this setTransparency $node 0.1]
	$trans add command -label "95%" \
	    -command [::itcl::code $this setTransparency $node 0.05]
	$trans add command -label "97%" \
	    -command [::itcl::code $this setTransparency $node 0.03]
	$trans add command -label "99%" \
	    -command [::itcl::code $this setTransparency $node 0.01]

	# set up bindings for transparency status
	bind $trans <<MenuSelect>> \
	    [::itcl::code $this transparencyMenuStatusCB %W]
    }

    # set up bindings for status
    bind $menu <<MenuSelect>> \
	[::itcl::code $this menuStatusCB %W]
    bind $color <<MenuSelect>> \
	[::itcl::code $this colorMenuStatusCB %W]
}

# ------------------------------------------------------------
#                         GENERAL
# ------------------------------------------------------------
::itcl::body ArcherCore::Load {target} {
    SetWaitCursor $this
    if {$mNeedSave} {
	if {![askToSave]} {
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
    if {[info exists itk_component(ged)]} {
	if {$mDbShared} {
	    $itk_component(ged) sharedGed $mTarget
	} elseif {$mDbNoCopy || $mDbReadOnly} {
	    $itk_component(ged) open $mTarget
	} else {
	    $itk_component(ged) open $mTargetCopy
	}
    } else {
	initGed

	grid forget $itk_component(canvas)
	if {!$mViewOnly} {
	    grid $itk_component(ged) -row 1 -column 0 -columnspan 3 -sticky news
	    after idle "$this component cmd configure -cmd_prefix \"[namespace tail $this] cmd\""
	} else {
	    grid $itk_component(ged) -row 1 -column 0 -sticky news
	}
    }

    set mDbTitle [$itk_component(ged) title]
    set mDbUnits [$itk_component(ged) units -s]
    set mPrevObjViewMode $OBJ_ATTR_VIEW_MODE
    set mPrevSelectedObjPath ""
    set mPrevSelectedObj ""
    set mSelectedObjPath ""
    set mSelectedObj ""
    set mSelectedObjType ""
    set mPasteActive 0

    if {!$mViewOnly} {
	gedCmd size [expr {$mGroundPlaneSize * 1.5 * [gedCmd base2local]}]
	buildGroundPlane
	showGroundPlane
    }

    if {!$mViewOnly} {
	doLighting
	deleteTargetOldCopy

	# refresh tree contents
	refreshTree 0
    } else {
	doLighting
    }

    if {$mBindingMode == 0} {
	initDefaultBindings $itk_component(ged)
    }
    SetNormalCursor $this
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
    if {!$mNeedSave} {
	return 0
    }

    $itk_component(saveDialog) center [namespace tail $this]
    ::update
    if {[$itk_component(saveDialog) activate]} {
	saveDb
	return 1
    }

    return 0
}

::itcl::body ArcherCore::colorMenuStatusCB {_w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$_w entrycget active -label} op]} {
	    set op ""
	}

	switch -- $op {
	    "Red" {
		set mStatusStr "Set this object's color to red"
	    }
	    "Orange" {
		set mStatusStr "Set this object's color to orange"
	    }
	    "Yellow" {
		set mStatusStr "Set this object's color to yellow"
	    }
	    "Green" {
		set mStatusStr "Set this object's color to green"
	    }
	    "Blue" {
		set mStatusStr "Set this object's color to blue"
	    }
	    "Indigo" {
		set mStatusStr "Set this object's color to indigo"
	    }
	    "Violet" {
		set mStatusStr "Set this object's color to violet"
	    }
	    "Default" {
		set mStatusStr "Set this object's color to the default color"
	    }
	    "Select..." {
		set mStatusStr "Set this object's color to the selected color"
	    }
	    default {
		set mStatusStr ""
	    }
	}
    }
}

::itcl::body ArcherCore::menuStatusCB {_w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$_w entrycget active -label} op]} {
	    set op ""
	}

	switch -- $op {
	    "Wireframe" {
		set mStatusStr "Draw object as wireframe"
	    }
	    "Shaded" {
		set mStatusStr "Draw object as shaded if a bot or polysolid (unevalutated)"
	    }
	    "Hidden Line" {
		set mStatusStr "Draw object as hidden line"
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

::itcl::body ArcherCore::menuStatusCB_junk {_w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$_w entrycget active -label} op]} {
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
	    "Shaded" {
		set mStatusStr "Draw object as shaded if a bot or polysolid (unevalutated)"
	    }
	    "Hidden Line" {
		set mStatusStr "Draw object as hidden line"
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

::itcl::body ArcherCore::transparencyMenuStatusCB {_w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$_w entrycget active -label} op]} {
	    set op ""
	}

	switch -- $op {
	    "None" {
		set mStatusStr "Set this object's transparency to 0%"
	    }
	    "80%" {
		set mStatusStr "Set this object's transparency to 80%"
	    }
	    "85%" {
		set mStatusStr "Set this object's transparency to 85%"
	    }
	    "90%" {
		set mStatusStr "Set this object's transparency to 90%"
	    }
	    "95%" {
		set mStatusStr "Set this object's transparency to 95%"
	    }
	    "97%" {
		set mStatusStr "Set this object's transparency to 97%"
	    }
	    "99%" {
		set mStatusStr "Set this object's transparency to 99%"
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
    $itk_component(primaryToolbar) itemconfigure rotate \
	-image [image create photo \
		    -file [file join $dir view_rotate.png]]
    $itk_component(primaryToolbar) itemconfigure translate \
	-image [image create photo \
		    -file [file join $dir view_translate.png]]
    $itk_component(primaryToolbar) itemconfigure scale \
	-image [image create photo \
		    -file [file join $dir view_scale.png]]
    $itk_component(primaryToolbar) itemconfigure center \
	-image [image create photo \
		    -file [file join $dir view_select.png]]
    $itk_component(primaryToolbar) itemconfigure centervo \
	-image [image create photo \
		    -file [file join $dir view_obj_select.png]]
    $itk_component(primaryToolbar) itemconfigure cpick \
	-image [image create photo \
		    -file [file join $dir compSelect.png]]
    $itk_component(primaryToolbar) itemconfigure cerase \
	-image [image create photo \
		    -file [file join $dir compErase.png]]
    $itk_component(primaryToolbar) itemconfigure measure \
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

#XXXX Needs more flexibility (i.e. position and orientation)
::itcl::body ArcherCore::buildGroundPlane {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    catch {gedCmd vdraw vlist delete groundPlaneMajor}
    catch {gedCmd vdraw vlist delete groundPlaneMinor}

    set majorColor [getVDrawColor $mGroundPlaneMajorColor]
    set minorColor [getVDrawColor $mGroundPlaneMinorColor]
    set move 0
    set draw 1
    set Xmax [expr {$mGroundPlaneSize * 0.5}]
    set Xmin [expr {0.0 - $Xmax}]
    set Ymax [expr {$mGroundPlaneSize * 0.5}]
    set Ymin [expr {0.0 - $Ymax}]


    # build minor lines
    gedCmd vdraw open groundPlaneMinor
    gedCmd vdraw params color $minorColor

    # build minor X lines
    for {set y -$mGroundPlaneInterval} {$Ymin <= $y} {set y [expr {$y - $mGroundPlaneInterval}]} {
	gedCmd vdraw write next $move $Xmin $y 0
	gedCmd vdraw write next $draw $Xmax $y 0
    }

    for {set y $mGroundPlaneInterval} {$y <= $Ymax} {set y [expr {$y + $mGroundPlaneInterval}]} {
	gedCmd vdraw write next $move $Xmin $y 0
	gedCmd vdraw write next $draw $Xmax $y 0
    }

    # build minor Y lines
    for {set x -$mGroundPlaneInterval} {$Xmin <= $x} {set x [expr {$x - $mGroundPlaneInterval}]} {
	gedCmd vdraw write next $move $x $Ymin 0
	gedCmd vdraw write next $draw $x $Ymax 0
    }

    for {set x $mGroundPlaneInterval} {$x <= $Xmax} {set x [expr {$x + $mGroundPlaneInterval}]} {
	gedCmd vdraw write next $move $x $Ymin 0
	gedCmd vdraw write next $draw $x $Ymax 0
    }


    # build major lines
    gedCmd vdraw open groundPlaneMajor
    gedCmd vdraw params color $majorColor
    gedCmd vdraw write 0 $move $Xmin 0 0
    gedCmd vdraw write next $draw $Xmax 0 0
    gedCmd vdraw write next $move 0 $Ymin 0
    gedCmd vdraw write next $draw 0 $Ymax 0
}

::itcl::body ArcherCore::showGroundPlane {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    set savePwd ""

    if {$mShowGroundPlane} {
	gedCmd vdraw open groundPlaneMajor
	gedCmd vdraw send
	gedCmd vdraw open groundPlaneMinor
	gedCmd vdraw send
    } else {
	set phonyList [gedCmd who p]
	if {[lsearch $phonyList _VDRWgroundPlaneMajor] != -1} {
	    gedCmd erase _VDRWgroundPlaneMajor _VDRWgroundPlaneMinor
	}
    }

    if {$savePwd != ""} {
	cd $savePwd
    }
}

::itcl::body ArcherCore::showPrimitiveLabels {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    if {!$mShowPrimitiveLabels} {
	gedCmd configure -primitiveLabels {}
    }

    selectNode $mLastTags
#    redrawObj $mSelectedObjPath
}

::itcl::body ArcherCore::showViewParams {} {
    if {[info exists itk_component(ged)]} {
	$itk_component(ged) configure -showViewingParams $mShowViewingParams
    } else {
	return
    }

    refreshDisplay
}

::itcl::body ArcherCore::showScale {} {
    if {[info exists itk_component(ged)]} {
	$itk_component(ged) configure -scaleEnable $mShowScale
    } else {
	return
    }

    refreshDisplay
}

::itcl::body ArcherCore::updateToggleMode {} {
    set toggle3 $mVPaneToggle3
    set toggle5 $mVPaneToggle5
    set mVPaneToggle3 $toggle3
    set mVPaneToggle5 $toggle5
}


::itcl::body ArcherCore::launchNirt {} {
    if {![info exists itk_component(ged)] || $mViewOnly} {
	return
    }

    putString "nirt -b"
    putString [$itk_component(ged) nirt -b]
}

::itcl::body ArcherCore::launchRtApp {app size} {
    global tcl_platform

    if {![info exists itk_component(ged)]} {
	return
    }

    if {![string is digit $size]} {
	set size [winfo width $itk_component(ged)]
    }

    if {$tcl_platform(platform) == "windows"} {
	$itk_component(ged) $app -s $size -F /dev/wgll
    } {
	$itk_component(ged) $app -s $size -F /dev/ogll
    }
}

::itcl::body ArcherCore::refreshDisplay {} {
    if {$mCurrentPaneName == ""} {
	set pane $mActivePaneName
    } else {
	set pane $mCurrentPaneName
    }
    set mCurrentPaneName ""

    $itk_component(ged) pane_refresh $pane
}

::itcl::body ArcherCore::putString {_str} {
    $itk_component(cmd) putstring $_str
}

::itcl::body ArcherCore::setStatusString {_str} {
    set mStatusStr $_str
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
	    tk_messageBox -message [gedCmd l $region]
	}
    } else {
	foreach callback $mMouseRayCallbacks {
	    catch {$callback $start $target $partitions}
	}
    }
}

::itcl::body ArcherCore::shootRay {_start _op _target _prep _no_bool _onehit} {
    eval $itk_component(ged) rt_gettrees ray -i -u [gedCmd who]
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

::itcl::body ArcherCore::setDefaultBindingMode {_mode} {
    set mDefaultBindingMode $_mode

    set ret 0
    switch -- $mDefaultBindingMode \
	$ROTATE_MODE { \
		beginViewRotate \
		set ret 1
	} \
	$TRANSLATE_MODE { \
		beginViewTranslate \
		set ret 1
	} \
	$SCALE_MODE { \
		beginViewScale \
		set ret 1
	} \
	$CENTER_MODE { \
		initCenterMode \
		set ret 1
	} \
	$CENTER_VIEW_OBJECT_MODE { \
		initCenterViewObjectMode \
		set ret 1
	} \
	$COMP_ERASE_MODE { \
		initCompErase \
		set ret 1
	} \
	$COMP_PICK_MODE { \
		initCompPick \
		set ret 1
	} \
	$MEASURE_MODE { \
		initViewMeasure \
		set ret 1
	}

    return $ret
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
::itcl::body ArcherCore::3ptarb {args} {
    eval gedWrapper 3ptarb 0 1 1 1 $args
}

::itcl::body ArcherCore::adjust {args} {
    eval gedWrapper adjust 0 1 1 1 $args
}

::itcl::body ArcherCore::arced {args} {
    eval gedWrapper arced 0 0 1 0 $args
}

::itcl::body ArcherCore::attr {args} {
    eval gedWrapper attr 0 0 1 0 $args
}

::itcl::body ArcherCore::bb {args} {
    eval gedWrapper bb 0 0 1 1 $args
}

::itcl::body ArcherCore::bev {args} {
    eval gedWrapper bev 0 0 1 1 $args
}

::itcl::body ArcherCore::blast {args} {
    eval gedWrapper blast 0 0 0 1 $args
}

::itcl::body ArcherCore::bo {args} {
    eval gedWrapper bo 0 0 1 1 $args
}

::itcl::body ArcherCore::bot_condense {args} {
    eval gedWrapper bot_condense 0 0 1 1 $args
}

::itcl::body ArcherCore::bot_decimate {args} {
    eval gedWrapper bot_decimate 0 0 1 1 $args
}

::itcl::body ArcherCore::bot_face_fuse {args} {
    eval gedWrapper bot_face_fuse 0 0 1 1 $args
}

::itcl::body ArcherCore::bot_face_sort {args} {
    eval gedWrapper bot_face_sort 1 0 1 1 $args
}

::itcl::body ArcherCore::bot_flip {args} {
    eval gedWrapper bot_flip 1 0 1 0 $args
}

::itcl::body ArcherCore::bot_merge {args} {
    eval gedWrapper bot_merge 1 0 1 1 $args
}

::itcl::body ArcherCore::bot_smooth {args} {
    eval gedWrapper bot_smooth 0 0 1 1 $args
}

::itcl::body ArcherCore::bot_split {args} {
    eval gedWrapper bot_split 0 0 1 1 $args
}

::itcl::body ArcherCore::bot_sync {args} {
    eval gedWrapper bot_sync 1 0 1 0 $args
}

::itcl::body ArcherCore::bot_vertex_fuse {args} {
    eval gedWrapper bot_vertex_fuse 0 0 1 1 $args
}

::itcl::body ArcherCore::c {args} {
    eval gedWrapper c 0 1 1 1 $args
}

::itcl::body ArcherCore::cd {args} {
    eval ::cd $args
}

::itcl::body ArcherCore::clear {args} {
    $itk_component(cmd) clear
}

::itcl::body ArcherCore::clone {args} {
    eval gedWrapper clone 0 0 1 1 $args
}

::itcl::body ArcherCore::color {args} {
    eval gedWrapper color 0 0 1 0 $args
}

::itcl::body ArcherCore::comb {args} {
    eval gedWrapper comb 0 1 1 1 $args
}

::itcl::body ArcherCore::comb_color {args} {
    eval gedWrapper comb_color 0 1 1 1 $args
}

::itcl::body ArcherCore::copy {args} {
    eval gedWrapper cp 0 0 1 1 $args
}

::itcl::body ArcherCore::copyeval {args} {
    eval gedWrapper copyeval 0 0 1 1 $args
}

::itcl::body ArcherCore::copymat {args} {
    eval gedWrapper copymat 0 0 1 0 $args
}

::itcl::body ArcherCore::cp {args} {
    eval gedWrapper cp 0 0 1 1 $args
}

::itcl::body ArcherCore::cpi {args} {
    eval gedWrapper cpi 0 0 1 1 $args
}

::itcl::body ArcherCore::dbconcat {args} {
    eval gedWrapper dbconcat 0 0 1 1 $args
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
    set lsItems [$itk_component(ged) ls -a]
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

	    if {[llength $mi] == 0} {
		lappend tobjects $obj
	    } else {
		foreach i $mi {
		    lappend tobjects [lindex $lsItems $i]
		}
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

	    if {[llength $mi] == 0} {
		lappend tobjects $obj
	    } else {
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

::itcl::body ArcherCore::decompose {args} {
    eval gedWrapper decompose 0 1 1 0 $args
}

::itcl::body ArcherCore::delete {args} {
    eval gedWrapper kill 1 0 1 1 $args
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
	SetWaitCursor $this
    }

    set optionsAndArgs [eval dbExpand $args]
    set options [lindex $optionsAndArgs 0]
    set objects [lindex $optionsAndArgs 1]
    set tobjects ""

    # remove leading /'s to make the hierarchy widget happy
    foreach obj $objects {
	lappend tobjects [regsub {^/} $obj ""]
    }

    if {[catch {eval gedCmd draw $options $tobjects} ret]} {
	gedCmd configure -primitiveLabels {}
	refreshTree
	SetNormalCursor $this

	return $ret
    }

    gedCmd configure -primitiveLabels {}
    refreshTree
    if {$wflag} {
	SetNormalCursor $this
    }

    return $ret
}

::itcl::body ArcherCore::E {args} {
    eval gedWrapper E 1 0 0 1 $args
}

::itcl::body ArcherCore::edcodes {args} {
    eval gedWrapper edcodes 0 0 1 0 $args
}

::itcl::body ArcherCore::edcolor {args} {
    eval gedWrapper edcolor 0 0 1 0 $args
}

::itcl::body ArcherCore::edcomb {args} {
    eval gedWrapper edcomb 0 0 1 1 $args
}

::itcl::body ArcherCore::edmater {args} {
    eval gedWrapper edmater 0 0 1 0 $args
}

::itcl::body ArcherCore::erase {args} {
    if {[llength $args] == 0} {
	return
    }

    SetWaitCursor $this

    set optionsAndArgs [eval dbExpand $args]
    set options [lindex $optionsAndArgs 0]
    set objects [lindex $optionsAndArgs 1]

    # remove leading /'s to make the hierarchy widget happy
    foreach obj $objects {
	lappend tobjects [regsub {^/} $obj ""]
    }

    if {[catch {eval gedCmd erase $options $tobjects} ret]} {
	gedCmd configure -primitiveLabels {}
	refreshTree
	SetNormalCursor $this

	return $ret
    }

    gedCmd configure -primitiveLabels {}
    refreshTree
    SetNormalCursor $this
}

::itcl::body ArcherCore::erase_all {args} {
    eval gedWrapper erase_all 1 0 0 1 $args
}

::itcl::body ArcherCore::ev {args} {
    eval gedWrapper ev 1 0 0 1 $args
}

::itcl::body ArcherCore::exit {args} {
    Close
}

::itcl::body ArcherCore::facetize {args} {
    eval gedWrapper facetize 0 0 1 1 $args
}

::itcl::body ArcherCore::fracture {args} {
    eval gedWrapper fracture 0 1 1 1 $args
}

::itcl::body ArcherCore::g {args} {
    eval group $args
}

::itcl::body ArcherCore::group {args} {
    eval gedWrapper g 0 1 1 1 $args
}

::itcl::body ArcherCore::hide {args} {
    eval gedWrapper hide 0 0 1 1 $args
}

::itcl::body ArcherCore::human {args} {
    eval gedWrapper human 0 0 1 1 $args
}


::itcl::body ArcherCore::i {args} {
    eval gedWrapper i 0 1 1 1 $args
}

::itcl::body ArcherCore::importFg4Section {args} {
    eval gedWrapper importFg4Section 0 0 1 1 $args
}

::itcl::body ArcherCore::in {args} {
    eval gedWrapper in 0 0 1 1 $args
}

::itcl::body ArcherCore::inside {args} {
    eval gedWrapper inside 0 0 1 1 $args
}

::itcl::body ArcherCore::item {args} {
    eval gedWrapper item 0 0 1 0 $args
}

::itcl::body ArcherCore::kill {args} {
    eval gedWrapper kill 1 0 1 1 $args
}

::itcl::body ArcherCore::killall {args} {
    eval gedWrapper killall 1 0 1 1 $args
}

::itcl::body ArcherCore::killrefs {args} {
    eval gedWrapper killrefs 1 0 1 1 $args
}

::itcl::body ArcherCore::killtree {args} {
    eval gedWrapper killtree 1 0 1 1 $args
}

::itcl::body ArcherCore::ls {args} {
    if {$args == {}} {
	return [gedCmd ls]
    } else {
	set optionsAndArgs [eval dbExpand $args]
	set options [lindex $optionsAndArgs 0]
	set expandedArgs [lindex $optionsAndArgs 1]

	if {$options == {}} {
	    return $expandedArgs
	} else {
	    return [eval gedCmd ls $args]
	}
    }
}

::itcl::body ArcherCore::make {args} {
    eval gedWrapper make 0 0 1 1 $args
}

::itcl::body ArcherCore::make_bb {args} {
    eval gedWrapper make_bb 0 0 1 1 $args
}

::itcl::body ArcherCore::make_pnts {args} {
    eval gedWrapper make_pnts 0 1 1 1 $args
}

::itcl::body ArcherCore::mater {args} {
    eval gedWrapper mater 0 1 1 1 $args
}

::itcl::body ArcherCore::mirror {args} {
    eval gedWrapper mirror 0 0 1 1 $args
}

::itcl::body ArcherCore::move {args} {
    eval gedWrapper mv 0 0 1 1 $args
}

::itcl::body ArcherCore::move_arb_edge {args} {
    eval gedWrapper move_arb_edge 0 0 1 0 $args
}

::itcl::body ArcherCore::move_arb_face {args} {
    eval gedWrapper move_arb_face 0 0 1 0 $args
}

::itcl::body ArcherCore::mv {args} {
    eval gedWrapper mv 0 0 1 1 $args
}

::itcl::body ArcherCore::mvall {args} {
    eval gedWrapper mvall 0 0 1 1 $args
}

::itcl::body ArcherCore::nmg_collapse {args} {
    eval gedWrapper nmg_collapse 0 0 1 1 $args
}

::itcl::body ArcherCore::nmg_simplify {args} {
    eval gedWrapper nmg_simplify 0 0 1 1 $args
}

::itcl::body ArcherCore::ocenter {args} {
    if {[llength $args] == 4} {
	set obj [lindex $args 0]

	eval gedWrapper ocenter 0 0 1 0 $args
	redrawObj $obj 0
    } else {
	eval gedCmd ocenter $args
    }
}

::itcl::body ArcherCore::orotate {args} {
    set result [eval gedWrapper orotate 0 0 1 0 $args]

    set len [llength $args]
    if {$len == 4 || $len == 7} {
	redrawObj [lindex $args 0] 0
    }

    return $result
}

::itcl::body ArcherCore::oscale {args} {
    set result [eval gedWrapper oscale 0 0 1 0 $args]

    if {[llength $args] == 5} {
	redrawObj [lindex $args 0] 0
    }

    return $result
}

::itcl::body ArcherCore::otranslate {args} {
    set result [eval gedWrapper otranslate 0 0 1 0 $args]

    if {[llength $args] == 4} {
	redrawObj [lindex $args 0] 0
    }

    return $result
}

::itcl::body ArcherCore::p {args} {
    # Nothing for now
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

::itcl::body ArcherCore::prefix {args} {
    eval gedWrapper prefix 0 0 1 1 $args
}

::itcl::body ArcherCore::protate {args} {
    eval gedWrapper protate 0 0 1 0 $args
}

::itcl::body ArcherCore::pscale {args} {
    eval gedWrapper pscale 0 0 1 0 $args
}

::itcl::body ArcherCore::ptranslate {args} {
    eval gedWrapper ptranslate 0 0 1 0 $args
}

::itcl::body ArcherCore::push {args} {
    eval gedWrapper push 0 1 1 0 $args
}

::itcl::body ArcherCore::put {args} {
    eval gedWrapper put 0 0 1 1 $args
}

::itcl::body ArcherCore::put_comb {args} {
    eval gedWrapper put_comb 0 0 1 1 $args
}

::itcl::body ArcherCore::putmat {args} {
    eval gedWrapper putmat 0 0 1 0 $args
}

::itcl::body ArcherCore::pwd {} {
    ::pwd
}

::itcl::body ArcherCore::r {args} {
    eval gedWrapper r 0 1 1 1 $args
}

::itcl::body ArcherCore::rcodes {args} {
    eval gedWrapper rcodes 0 0 1 0 $args
}

::itcl::body ArcherCore::red {args} {
    eval gedWrapper red 0 0 1 1 $args
}

::itcl::body ArcherCore::rfarb {args} {
    eval gedWrapper rfarb 0 0 1 1 $args
}

::itcl::body ArcherCore::rm {args} {
    eval gedWrapper rm 0 0 1 1 $args
}

::itcl::body ArcherCore::rmater {args} {
    eval gedWrapper rmater 0 0 1 1 $args
}

::itcl::body ArcherCore::rotate_arb_face {args} {
    eval gedWrapper rotate_arb_face 0 0 1 0 $args
}

::itcl::body ArcherCore::search {args} {
     if {$args == {}} {
	return [gedCmd search]
    } else {
	return [eval gedCmd search $args]
    }
}


::itcl::body ArcherCore::shader {args} {
    eval gedWrapper shader 0 0 1 0 $args
}

::itcl::body ArcherCore::shells {args} {
    eval gedWrapper shells 0 0 1 1 $args
}

::itcl::body ArcherCore::tire {args} {
    eval gedWrapper tire 0 0 1 1 $args
}

::itcl::body ArcherCore::title {args} {
    if {$args == {}} {
	return [gedCmd title]
    }

    eval gedWrapper title 0 0 1 0 $args
}

::itcl::body ArcherCore::track {args} {
    eval gedWrapper track 0 0 1 1 $args
}

::itcl::body ArcherCore::unhide {args} {
    eval gedWrapper unhide 0 0 1 1 $args
}

::itcl::body ArcherCore::units {args} {
    if {$args == {}} {
	return [gedCmd units]
    }

    if {[llength $args] == 1 && [lindex $args 0] == "-s"} {
	return [gedCmd units -s]
    }

    eval gedWrapper units 0 0 1 0 $args
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

::itcl::body ArcherCore::vmake {args} {
    eval gedWrapper vmake 0 0 1 1 $args
}

::itcl::body ArcherCore::wmater {args} {
    eval gedWrapper wmater 0 0 0 0 $args
}

::itcl::body ArcherCore::xpush {args} {
    eval gedWrapper xpush 0 0 1 0 $args
}

::itcl::body ArcherCore::Z {args} {
    eval gedWrapper clear 0 0 0 1 $args
}

::itcl::body ArcherCore::zap {args} {
    eval gedWrapper clear 0 0 0 1 $args
}

::itcl::body ArcherCore::updateDisplaySettings {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    switch -- $mZClipMode \
	$ZCLIP_SMALL_CUBE { \
				$itk_component(ged) zclip_all 1; \
				$itk_component(ged) bounds_all {-4096 4095 -4096 4095 -4096 4095}; \
			    } \
	$ZCLIP_MEDIUM_CUBE { \
				 $itk_component(ged) zclip_all 1; \
				 $itk_component(ged) bounds_all {-8192 8191 -8192 8191 -8192 8191}; \
			     } \
	$ZCLIP_LARGE_CUBE { \
				$itk_component(ged) zclip_all 1; \
				$itk_component(ged) bounds_all {-16384 16363 -16384 16363 -16384 16363}; \
			    } \
	$ZCLIP_NONE { \
			  $itk_component(ged) zclip_all 0; \
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
	"Save Database?" \
	"Do you wish to save the current database?" \
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
	::ttk::label $parent.xl \
	    -text "X:"
    } {}

    itk_component add centerDialogXE {
	::ttk::entry $parent.xe \
	    -width 12 \
	    -textvariable [::itcl::scope mCenterX] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add centerDialogXUL {
	::ttk::label $parent.xul \
	    -textvariable [::itcl::scope mDbUnits]
    } {}
    itk_component add centerDialogYL {
	::ttk::label $parent.yl \
	    -text "Y:"
    } {}
    itk_component add centerDialogYE {
	::ttk::entry $parent.ye \
	    -width 12 \
	    -textvariable [::itcl::scope mCenterY] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add centerDialogYUL {
	::ttk::label $parent.yul \
	    -textvariable [::itcl::scope mDbUnits]
    } {}
    itk_component add centerDialogZL {
	::ttk::label $parent.zl \
	    -text "Z:"
    } {}
    itk_component add centerDialogZE {
	::ttk::entry $parent.ze \
	    -width 12 \
	    -textvariable [::itcl::scope mCenterZ] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add centerDialogZUL {
	::ttk::label $parent.zul \
	    -textvariable [::itcl::scope mDbUnits]
    } {}

    after idle "$itk_component(centerDialog) center"

    $itk_component(centerDialog) configure -background $LABEL_BACKGROUND_COLOR

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
	::ttk::label $parent.$name2\L \
	    -text $text
    } {}

    itk_component add $name1\F {
	::ttk::frame $parent.$name2\F \
	    -relief sunken
    } {}

    itk_component add $name1\CB {
	::ttk::combobox $itk_component($name1\F).$name2\CB \
	    -state readonly \
	    -textvariable [::itcl::scope $varName] \
	    -values $listOfChoices
    } {}

    pack $itk_component($name1\CB) -expand yes -fill both
}

::itcl::body ArcherCore::watchVar {_name1 _name2 _op} {
    if {![info exists itk_component(ged)]} {
	return
    }

    switch -- $_name1 {
	mMeasuringStickColor {
	    $itk_component(ged) configure -measuringStickColor $mMeasuringStickColor
	}
	mMeasuringStickMode {
	    $itk_component(ged) configure -measuringStickMode $mMeasuringStickMode 
	}
	mModelAxesColor {
	    if {$mModelAxesColor == "Triple"} {
		$itk_component(ged) configure -modelAxesTripleColor 1
	    } else {
		$itk_component(ged) configure -modelAxesTripleColor 0
		$itk_component(ged) configure -modelAxesColor $mModelAxesColor
	    }
	}
	mModelAxesLabelColor {
	    $itk_component(ged) configure -modelAxesLabelColor $mModelAxesLabelColor
	}
	mModelAxesTickColor {
	    $itk_component(ged) configure -modelAxesTickColor $mModelAxesTickColor
	}
	mModelAxesTickMajorColor {
	    $itk_component(ged) configure -modelAxesTickMajorColor $mModelAxesTickMajorColor
	}
	mPrimitiveLabelColor {
	    $itk_component(ged) configure -primitiveLabelColor $mPrimitiveLabelColor
	}
	mScaleColor {
	    $itk_component(ged) configure -scaleColor $mScaleColor
	}
	mViewAxesColor {
	    if {$mViewAxesColor == "Triple"} {
		$itk_component(ged) configure -viewAxesTripleColor 1
	    } else {
		$itk_component(ged) configure -viewAxesTripleColor 0
		$itk_component(ged) configure -viewAxesColor $mViewAxesColor
	    }
	}
	mViewAxesLabelColor {
	    $itk_component(ged) configure -viewAxesLabelColor $mViewAxesLabelColor
	}
	mViewingParamsColor {
	    $itk_component(ged) configure -viewingParamsColor $mViewingParamsColor
	}
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
