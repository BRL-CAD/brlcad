#                      A R C H E R C O R E . T C L
# BRL-CAD
#
# Copyright (c) 2002-2011 United States Government as represented by
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

    constructor {{_viewOnly 0} {_noCopy 0} {_noTree 0} {_noToolbar 0} args} {}
    destructor {}

    public {
	common application ""
	common splash ""
	common showWindow 0

	common TREE_AFFECTED_TAG "affected"
	common TREE_FULLY_DISPLAYED_TAG "displayed"
	common TREE_PARTIALLY_DISPLAYED_TAG "pdisplayed"
	common TREE_POPUP_TAG "popup"
	common TREE_OPENED_TAG "opened"
	common TREE_PLACEHOLDER_TAG "placeholder"

	common VIEW_ROTATE_MODE 0
	common VIEW_TRANSLATE_MODE 1
	common VIEW_SCALE_MODE 2
	common VIEW_CENTER_MODE 3
	common COMP_PICK_MODE 4
	common COMP_SELECT_MODE 5
	common MEASURE_MODE 6
	common OBJECT_ROTATE_MODE 7
	common OBJECT_TRANSLATE_MODE 8
	common OBJECT_SCALE_MODE 9
	common OBJECT_CENTER_MODE 10
	common FIRST_FREE_BINDING_MODE 11

	common OBJ_EDIT_VIEW_MODE 0
	common OBJ_ATTR_VIEW_MODE 1

	common COMP_PICK_TREE_SELECT_MODE 0
	common COMP_PICK_NAME_MODE 1
	common COMP_PICK_ERASE_MODE 2
	common COMP_PICK_BOT_FLIP_MODE 3
	common COMP_PICK_BOT_SPLIT_MODE 4
	common COMP_PICK_BOT_SYNC_MODE 5

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
	method rebuildTree {}
	method rsyncTree {_pnode}
	method syncTree {}
	method shootRay {_start _op _target _prep _no_bool _onehit _bot_dflag}
	method addMouseRayCallback {_callback}
	method deleteMouseRayCallback {_callback}
	method setDefaultBindingMode {_mode}

	# public database commands
	method cmd                 {args}
	method gedCmd              {args}

	# general
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
	method edit                {args}
	method arced               {args}
	method attr                {args}
	method bb                  {args}
	method bev                 {args}
	method blast               {args}
	method bo                  {args}
	method bot                 {args}
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
	method closedb             {args}
	method color               {args}
	method comb                {args}
	method comb_color          {args}
	method combmem             {args}
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
	method man                 {args}
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
	method opendb		   {args}
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
	method q                   {args}
	method quit                {args}
	method r                   {args}
	method rcodes              {args}
	method red                 {args}
	method rfarb               {args}
	method rm                  {args}
	method rmater              {args}
	method rotate              {args}
	method rotate_arb_face     {args}
	method scale		   {args}
	method search		   {args}
	method sed		   {_prim}
	method shader              {args}
	method shells              {args}
	method tire                {args}
	method title               {args}
	method track               {args}
	method translate           {args}
	method unhide              {args}
	method units               {args}
	method vmake               {args}
	method wmater              {args}
	method xpush               {args}
	method Z                   {args}
	method zap                 {args}

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

	variable mDelayCommandViewBuild 0
	variable mRestoringTree 0
	variable mViewOnly 0
	variable mNoTree 0
	variable mNoToolbar 0
	variable mTarget ""
	variable mTargetCopy ""
	variable mTargetOldCopy ""
	variable mDisplayType
	variable mLighting 2
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

	variable mShowViewAxes 1
	variable mShowModelAxes 0
	variable mShowModelAxesTicks 1
	variable mShowGroundPlane 0
	variable mShowPrimitiveLabels 0
	variable mShowViewingParams 0
	variable mShowScale 0
	variable mShowGrid 0
	variable mSnapGrid 0
	variable mShowADC 0

	# variables for preference state
	variable mWindowGeometry ""
	variable mEnableAffectedNodeHighlight 0
	variable mEnableAffectedNodeHighlightPref ""
	variable mEnableListView 0
	variable mEnableListViewPref ""
	variable mEnableListViewAllAffected 0
	variable mEnableListViewAllAffectedPref ""
	variable mTreeAttrColumns ""
	variable mTreeAttrColumnsPref ""

	variable mSeparateCommandWindow 0
	variable mSeparateCommandWindowPref ""
	variable mSepCmdPrefix "sepcmd_"

	variable mCompPickMode $COMP_PICK_TREE_SELECT_MODE

	variable mZClipBack 100.0
	variable mZClipBackPref 100.0
	variable mZClipFront 100.0
	variable mZClipFrontPref 100.0
	variable mZClipMax 1000
	variable mZClipMaxPref 1000

	variable mBindingMode Default
	variable mBindingModePref ""
	variable mBackground "0 0 0"
	variable mBackgroundColor Navy
	variable mBackgroundColorPref ""
	variable mFBBackgroundColor Black
	variable mFBBackgroundColorPref ""
	variable mPrimitiveLabelColor Yellow
	variable mPrimitiveLabelColorPref
	variable mViewingParamsColor Yellow
	variable mViewingParamsColorPref
	variable mScaleColor Yellow
	variable mScaleColorPref ""
	variable mMeasuringStickMode 0
	variable mMeasuringStickModePref ""
	variable mMeasuringStickColor Yellow
	variable mMeasuringStickColorPref ""
	variable mMeasuringStickColorVDraw ffff00
	variable mEnableBigE 0
	variable mEnableBigEPref ""

	variable mDisplayFontSize 0
	variable mDisplayFontSizePref ""
	variable mDisplayFontSizes {}

	variable mGridAnchor "0 0 0"
	variable mGridAnchorXPref ""
	variable mGridAnchorYPref ""
	variable mGridAnchorZPref ""
	variable mGridColor White
	variable mGridColorPref ""
	variable mGridMrh 10
	variable mGridMrhPref ""
	variable mGridMrv 10
	variable mGridMrvPref ""
	variable mGridRh 1
	variable mGridRhPref ""
	variable mGridRv 1
	variable mGridRvPref ""

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
	    3ptarb adjust arced attr bb bev blast bo bot bot_condense \
	    bot_decimate bot_face_fuse bot_face_sort bot_flip \
	    bot_merge bot_smooth bot_split bot_sync bot_vertex_fuse \
	    c cd clear clone closedb color comb comb_color combmem \
	    copy copyeval copymat cp cpi dbconcat dbExpand decompose \
	    delete draw E edcodes edcolor edcomb edit edmater erase ev \
	    exit facetize fracture g group hide human i \
	    importFg4Section in inside item kill killall killrefs \
	    killtree ls make make_bb make_pnts man mater mirror move \
	    move_arb_edge move_arb_face mv mvall nmg_collapse \
	    nmg_simplify ocenter opendb orotate oscale otranslate p q \
	    quit packTree prefix protate pscale ptranslate push put \
	    put_comb putmat pwd r rcodes red rfarb rm rmater rotate \
	    rotate_arb_face scale search sed shader shells tire title \
	    track translate unhide units unpackTree vmake wmater xpush \
	    Z zap
	}

	# Commands in this list get passed directly to the Ged object
	variable mUnwrappedDbCommands {}

	variable mBannedDbCommands {
	    dbip open rtabort shaded_mode
	}

	variable mMouseOverrideInfo "\
	    \nTranslate     Shift-Right\
	    \nRotate        Shift-Left\
	    \nScale         Shift-Middle\
	    \nCenter        Shift-Ctrl-Right\
	    \nPopup Menu    Right or Ctrl-Left\
	    \n"
	variable mColorList {Grey Black Navy Blue Cyan Green Magenta Red White Yellow Triple}
	variable mColorListNoTriple {Grey Black Navy Blue Cyan Green Magenta Red White Yellow}
	variable mDefaultNodeColor {150 150 150}

	variable mDoStatus 1
	variable mDbName ""
	variable mDbUnits ""
	variable mDbTitle ""

	variable mMouseRayCallbacks ""
	variable mLastTags ""

	method handleMoreArgs {args}

	method gedWrapper {_cmd _eflag _hflag _sflag _tflag args}

	method buildCommandView {}
	method buildCanvasMenubar {}

	method redrawObj {_obj {_wflag 1}}

	method colorMenuStatusCB {_w}
	method menuStatusCB {_w}
	method menuStatusCB_junk {_w}
	method transparencyMenuStatusCB {_w}

	method updateSaveMode {}
	method createTargetCopy {}
	method deleteTargetOldCopy {}

	method getVDrawColor {_color}
	method buildGroundPlane {}
	method showGroundPlane {}
	method showPrimitiveLabels {}
	method showViewParams {}
	method showScale {}

	method launchNirt {}
	method launchRtApp {_app _size}

	method updateDisplaySettings {}
	method updateZClipPlanes {_unused}
	method calculateZClipMax {}
	method pushZClipSettings {}

	method shootRay_doit {_start _op _target _prep _no_bool _onehit _bot_dflag _objects}

	variable mImgDir ""
	variable mCenterX ""
	variable mCenterY ""
	variable mCenterZ ""

	# variables for images
	variable mImage_air ""
	variable mImage_airLabeled ""
	variable mImage_airInter ""
	variable mImage_airSub ""
	variable mImage_airUnion ""
	variable mImage_airregion ""
	variable mImage_airregionInter ""
	variable mImage_airregionSub ""
	variable mImage_airregionUnion ""
	variable mImage_comb ""
	variable mImage_combLabeled ""
	variable mImage_combInter ""
	variable mImage_combSub ""
	variable mImage_combUnion ""
	variable mImage_region ""
	variable mImage_regionLabeled ""
	variable mImage_regionInter ""
	variable mImage_regionSub ""
	variable mImage_regionUnion ""
	variable mImage_arb8 ""
	variable mImage_arb8Labeled ""
	variable mImage_arb8Inter ""
	variable mImage_arb8Sub ""
	variable mImage_arb8Union ""
	variable mImage_arb7Labeled ""
	variable mImage_arb6Labeled ""
	variable mImage_arb5Labeled ""
	variable mImage_arb4Labeled ""
	variable mImage_arbn ""
	variable mImage_arbnLabeled ""
	variable mImage_arbnInter ""
	variable mImage_arbnSub ""
	variable mImage_arbnUnion ""
	variable mImage_ars ""
	variable mImage_arsLabeled ""
	variable mImage_arsInter ""
	variable mImage_arsSub ""
	variable mImage_arsUnion ""
	variable mImage_bot ""
	variable mImage_botLabeled ""
	variable mImage_botInter ""
	variable mImage_botSub ""
	variable mImage_botUnion ""
	variable mImage_dsp ""
	variable mImage_dspLabeled ""
	variable mImage_dspInter ""
	variable mImage_dspSub ""
	variable mImage_dspUnion ""
	variable mImage_ehy ""
	variable mImage_ehyLabeled ""
	variable mImage_ehyInter ""
	variable mImage_ehySub ""
	variable mImage_ehyUnion ""
	variable mImage_ell ""
	variable mImage_ellLabeled ""
	variable mImage_ellInter ""
	variable mImage_ellSub ""
	variable mImage_ellUnion ""
	variable mImage_epa ""
	variable mImage_epaLabeled ""
	variable mImage_epaInter ""
	variable mImage_epaSub ""
	variable mImage_epaUnion ""
	variable mImage_eto ""
	variable mImage_etoLabeled ""
	variable mImage_etoInter ""
	variable mImage_etoSub ""
	variable mImage_etoUnion ""
	variable mImage_extrude ""
	variable mImage_extrudeLabeled ""
	variable mImage_extrudeInter ""
	variable mImage_extrudeSub ""
	variable mImage_extrudeUnion ""
	variable mImage_half ""
	variable mImage_halfLabeled ""
	variable mImage_halfInter ""
	variable mImage_halfSub ""
	variable mImage_halfUnion ""
	variable mImage_hyp ""
	variable mImage_hypLabeled ""
	variable mImage_hypInter ""
	variable mImage_hypSub ""
	variable mImage_hypUnion ""
	variable mImage_invalid ""
	variable mImage_invalidInter ""
	variable mImage_invalidSub ""
	variable mImage_invalidUnion ""
	variable mImage_metaball ""
	variable mImage_metaballLabeled ""
	variable mImage_metaballInter ""
	variable mImage_metaballSub ""
	variable mImage_metaballUnion ""
	variable mImage_nmg ""
	variable mImage_nmgLabeled ""
	variable mImage_nmgInter ""
	variable mImage_nmgSub ""
	variable mImage_nmgUnion ""
	variable mImage_other ""
	variable mImage_otherInter ""
	variable mImage_otherSub ""
	variable mImage_otherUnion ""
	variable mImage_partLabeled ""
	variable mImage_pipe ""
	variable mImage_pipeLabeled ""
	variable mImage_pipeInter ""
	variable mImage_pipeSub ""
	variable mImage_pipeUnion ""
	variable mImage_rhc ""
	variable mImage_rhcLabeled ""
	variable mImage_rhcInter ""
	variable mImage_rhcSub ""
	variable mImage_rhcUnion ""
	variable mImage_rpc ""
	variable mImage_rpcLabeled ""
	variable mImage_rpcInter ""
	variable mImage_rpcSub ""
	variable mImage_rpcUnion ""
	variable mImage_sketch ""
	variable mImage_sketchLabeled ""
	variable mImage_sketchInter ""
	variable mImage_sketchSub ""
	variable mImage_sketchUnion ""
	variable mImage_sph ""
	variable mImage_sphLabeled ""
	variable mImage_sphInter ""
	variable mImage_sphSub ""
	variable mImage_sphUnion ""
	variable mImage_tgc ""
	variable mImage_tgcLabeled ""
	variable mImage_tgcInter ""
	variable mImage_tgcSub ""
	variable mImage_tgcUnion ""
	variable mImage_tor ""
	variable mImage_torLabeled ""
	variable mImage_torInter ""
	variable mImage_torSub ""
	variable mImage_torUnion ""

	variable mImage_fbOff ""
	variable mImage_fbOn ""
	variable mImage_fbInterlay ""
	variable mImage_fbOverlay ""
	variable mImage_fbUnderlay ""
	variable mImage_rt ""
	variable mImage_rtAbort ""

	# variables for the new tree
	variable mCNode2PList
	variable mPNode2CList
	variable mNode2Text
	variable mText2Node
	variable mNodePDrawList
	variable mNodeDrawList
	variable mAffectedNodeList ""

	variable mCoreCmdLevel 0

	# init functions
	method initImages {}
	method initTree          {}
	method initTreeImages    {}
	method initGed           {}
	method closeMged         {}
	method updateRtControl   {}

	# interface operations
	method closeDb           {}
	method newDb             {}
	method openDb            {}
	method saveDb            {}
	method primaryToolbarAdd        {_type _name {args ""}}
	method primaryToolbarRemove     {_index}

	method bot_split2 {_bot}

	# tree commands
	method updateTreeDrawLists        {{_cflag 0}}
	method fillTree          {_pnode _ctext _flat {_allow_multiple 0}}
	method fillTreeColumns   {_cnode _ctext}
	method isRegion          {_cgdata}
	method loadMenu          {_menu _node _nodeType}
	method findTreeChildNodes {_pnode}
	method findTreeParentNodes {_cnode}
	method getCNodesFromCText {_pnode _text}
	method getTreeImage {_obj _type {_op ""} {_isregion 0}}
	method getTreeNode {_path {_cflag 0}}
	method getTreeNodes {_path {_cflag 0}}
	method getTreePath {_node {_path ""}}
	method handleTreeClose {}
	method handleTreeOpen {}
	method handleTreePopup {_x _y _X _Y}
	method handleTreeSelect {}
	method addTreeNodeTag {_node _tag}
	method removeTreeNodeTag {_node _tag}
	method addTreePlaceholder {_pnode}
	method selectTreePath {_path}
	method setTreeView {{_rflag 0}}
	method toggleTreeView {}
	method treeNodeHasBeenOpened {_node}
	method treeNodeIsOpen {_node}
	method purgeNodeData {_node}
	method updateTreeTopWithName {_name}

	# db/display commands
	method getNodeChildren  {_node}
	method getTreeFromGData  {_gdata}
	method getTreeMembers  {_tlist {_mlist {}}}
	method getTreeOp {_parent _child}
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
	method showGrid     {}
	method snapGrid     {}
	method showADC     {}

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

	method initViewCenterMode {}
	method initCompPick {}
	method initCompSelect {}
	method compSelectCallback {_mstring}

	method mrayCallback_cvo {_pane _start _target _partitions}
	method mrayCallback_pick {_pane _start _target _partitions}

	method initViewMeasure {}
	method endViewMeasure {_mstring}

	method initDefaultBindings {{_comp ""}}
	method initBrlcadBindings {}
	method validateDigit {_d}
	method validateDouble {_d}
	method validateTickInterval {_ti}
	method validateColorComp {_c}

	method backgroundColor {_color}

	method updateHPaneFractions {}
	method updateVPaneFractions {}

	method setColorOption {_cmd _colorOption _color {_tripleColorOption ""}}

	method addHistory {_cmd}

	# Dialogs Section
	method buildInfoDialog {_name _title _info _size _wrapOption _modality}
	method buildSaveDialog {}
	method buildViewCenterDialog {}
	method centerDialogOverPane {_dialog}

	# Helper Section
	method buildComboBox {_parent _name1 _name2 _varName _text _listOfChoices}

	method watchVar {_name1 _name2 _op}
    }
}

# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------
::itcl::body ArcherCore::constructor {{_viewOnly 0} {_noCopy 0} {_noTree 0} {_noToolbar 0} args} {
    global env
    global tcl_platform

    if {$tcl_platform(platform) == "windows"} {
	set mDisplayFontSizes {0 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29}
    } else {
	set mDisplayFontSizes {0 5 6 7 8 9 10 12}
    }

    set mLastSelectedDir [pwd]

    set mFontText [list $SystemWindowFont 8]
    set mFontTextBold [list $SystemWindowFont 8 bold]

    set mProgressBarHeight [expr {[font metrics $mFontText -linespace] + 1}]
    set mViewOnly $_viewOnly
    set mDbNoCopy $_noCopy
    set mNoTree $_noTree
    set mNoToolbar $_noToolbar

    if {$ArcherCore::inheritFromToplevel} {
	wm withdraw [namespace tail $this]
    }

    if {![info exists env(DISPLAY)]} {
	set env(DISPLAY) ":0"
    }

    set mImgDir [file join [bu_brlcad_data "tclscripts"] archer images]

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
	buildCommandView
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

    if {!$mNoTree} {
	$itk_component(vpane) add hierarchyView
    }

    if {!$mViewOnly} {
	$itk_component(vpane) add geomView
	$itk_component(vpane) add attrView

	if {!$mNoTree} {
	    $itk_component(vpane) paneconfigure hierarchyView \
		-margin 0 \
		-minimum 0
	}

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

    if {$mViewOnly && !$mNoToolbar} {
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
    }

    if {!$mViewOnly} {
	# dummy canvas
	itk_component add canvas {
	    ::frame $itk_component(canvasF).canvas \
		-borderwidth 0 \
		-relief flat 
	} {}

	grid $itk_component(canvas) -row 0 -column 0 -columnspan 3 -sticky news
	grid rowconfigure $itk_component(canvasF) 0 -weight 1
	grid columnconfigure $itk_component(canvasF) 1 -weight 1

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

	if {!$mNoToolbar} {
	    grid $itk_component(canvas_menu) -row 0 -column 0 -sticky w
	    grid $itk_component(canvas) -row 1 -column 0 -sticky news
	    grid rowconfigure $itk_component(canvasF) 1 -weight 1
	} else {
	    grid $itk_component(canvas) -row 0 -column 0 -sticky news
	    grid rowconfigure $itk_component(canvasF) 0 -weight 1
	}
	grid columnconfigure $itk_component(canvasF) 0 -weight 1

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
	if {!$mDelayCommandViewBuild} {
	    pack $itk_component(advancedTabs) -fill both -expand yes
	}
	pack $itk_component(statusF) -before $itk_component(south) -side bottom -fill x
    }
    if {!$mNoTree} {
	pack $itk_component(newtreeF) -fill both -expand yes
    }
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

    if {!$mNoToolbar} {
	buildPrimaryToolbar
    }

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

    trace add variable [::itcl::scope mFBBackgroundColor] write watchVar
    trace add variable [::itcl::scope mDisplayFontSize] write watchVar

    eval itk_initialize $args

    $this configure -background $LABEL_BACKGROUND_COLOR

    if {!$mViewOnly} {
	# set initial toggle variables
	set mVPaneToggle3 $mVPaneFraction3
	set mVPaneToggle5 $mVPaneFraction5

	updateSaveMode
    }

    initImages
    initTreeImages

    if {!$mViewOnly && !$mDelayCommandViewBuild} {
	after idle "$itk_component(cmd) configure -cmd_prefix \"[namespace tail $this] cmd\""
    }

#    Load ""
 
    if {!$mNoToolbar} {
	$itk_component(primaryToolbar) itemconfigure open -state normal
    }

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
    switch -- $tflag {
	0 {
	    # Do nothing
	}
	1 {
	    catch {updateTreeDrawLists}
	}
	default {
	    catch {syncTree}
	}
    }
    SetNormalCursor $this

    return $ret
}

::itcl::body ArcherCore::buildCommandView {} {
    $itk_component(hpane) add bottomView
    $itk_component(hpane) paneconfigure bottomView \
	-margin 0 \
	-minimum 0

    if {$mSeparateCommandWindow} {
	itk_component add sepcmdT {
	    ::toplevel $itk_interior.sepcmdT
	} {}

	::wm title $itk_component(sepcmdT) "Archer Command"
	set parent $itk_component(sepcmdT)
	wm protocol $itk_component(sepcmdT) WM_DELETE_WINDOW {exitArcher $::ArcherCore::application}
    } else {
	set parent [$itk_component(hpane) childsite bottomView]
    }

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

    $itk_component(cmd) component text configure -background white

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

::itcl::body ArcherCore::buildCanvasMenubar {}  {
    if {$mViewOnly && !$mNoToolbar} {
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
		    -helpstr "Set background color to black"
		command grey -label "Grey" \
		    -helpstr "Set background color to grey"
		command white -label "White" \
		    -helpstr "Set background color to white"
		command cyan -label "Cyan" \
		    -helpstr "Set background color to cyan"
		command blue -label "Light Blue" \
		    -helpstr "Set background color to blue"
		command navy -label "Navy" \
		    -helpstr "Set background color to navy"
	    }

	$itk_component(canvas_menu) menuconfigure .background.black \
	    -command [::itcl::code $this backgroundColor Black]
	$itk_component(canvas_menu) menuconfigure .background.grey \
	    -command [::itcl::code $this backgroundColor Grey]
	$itk_component(canvas_menu) menuconfigure .background.white \
	    -command [::itcl::code $this backgroundColor White]
	$itk_component(canvas_menu) menuconfigure .background.cyan \
	    -command [::itcl::code $this backgroundColor Cyan]
	$itk_component(canvas_menu) menuconfigure .background.blue \
	    -command [::itcl::code $this backgroundColor Blue]
	$itk_component(canvas_menu) menuconfigure .background.navy \
	    -command [::itcl::code $this backgroundColor Navy]

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

::itcl::body ArcherCore::initImages {} {
    set dir $mImgDir

    if {!$mViewOnly} {
	if {!$mNoToolbar} {
	    # Primary Toolbar
	    $itk_component(primaryToolbar) itemconfigure open \
		-image [image create photo \
			    -file [file join $dir open.png]]
	    $itk_component(primaryToolbar) itemconfigure save \
		-image [image create photo \
			    -file [file join $dir save.png]]
	}

	# Tree Control
#	$itk_component(tree) configure \
	    -openimage [image create photo -file [file join $dir folder_open_small.png]] \
	    -closeimage [image create photo -file [file join $dir folder_closed_small.png]] \
	    -nodeimage [image create photo -file [file join $dir file_text_small.png]]
#	$itk_component(tree) redraw

    }

    if {!$mNoToolbar} {
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
			-file [file join $dir view_center.png]]
	$itk_component(primaryToolbar) itemconfigure cpick \
	    -image [image create photo \
			-file [file join $dir component_pick.png]]
	$itk_component(primaryToolbar) itemconfigure cselect \
	    -image [image create photo \
			-file [file join $dir component_select.png]]
	$itk_component(primaryToolbar) itemconfigure measure \
	    -image [image create photo \
			-file [file join $dir measure.png]]
    }
}

::itcl::body ArcherCore::initTree {} {
    if {$mNoTree} {
	return
    }

    set parent [$itk_component(vpane) childsite hierarchyView]

    set mNode2Text() ""
    set mText2Node() ""
    set mCNode2PList() ""
    set mPNode2CList() ""

    itk_component add newtreeF {
	::frame $parent.newtreeF
    } {}

    itk_component add newtree {
	::ttk::treeview $itk_component(newtreeF).tree \
	    -selectmode browse
    } {}

    bind $itk_component(newtree) <<TreeviewSelect>> [::itcl::code $this handleTreeSelect]
    bind $itk_component(newtree) <<TreeviewOpen>> [::itcl::code $this handleTreeOpen]
    bind $itk_component(newtree) <<TreeviewClose>> [::itcl::code $this handleTreeClose]
    $itk_component(newtree) tag bind $TREE_POPUP_TAG <Button-3> [::itcl::code $this handleTreePopup %x %y %X %Y]
    $itk_component(newtree) tag configure $TREE_FULLY_DISPLAYED_TAG \
	-foreground red \
	-font TkHeadingFont
    $itk_component(newtree) tag configure $TREE_PARTIALLY_DISPLAYED_TAG \
	-foreground \#9999ff \
	-font TkHeadingFont
    $itk_component(newtree) tag configure $TREE_AFFECTED_TAG \
	-background yellow2

    itk_component add newtreepopup {
	::menu $itk_interior.newtreemenu \
	    -tearoff 0
    } {}

    itk_component add newtreehscroll {
	::ttk::scrollbar $itk_component(newtreeF).newtreehscroll \
	    -orient horizontal
    } {}

    itk_component add newtreevscroll {
	::ttk::scrollbar $itk_component(newtreeF).newtreevscroll \
	    -orient vertical
    } {}

    # Hook up scrollbars
    $itk_component(newtree) configure -xscrollcommand "$itk_component(newtreehscroll) set"
    $itk_component(newtree) configure -yscrollcommand "$itk_component(newtreevscroll) set"
    $itk_component(newtreehscroll) configure -command "$itk_component(newtree) xview"
    $itk_component(newtreevscroll) configure -command "$itk_component(newtree) yview"

    grid $itk_component(newtree) $itk_component(newtreevscroll) -sticky nsew
    grid $itk_component(newtreehscroll) - -sticky nsew
    grid columnconfigure $itk_component(newtreeF) 0 -weight 1
    grid rowconfigure $itk_component(newtreeF) 0 -weight 1

    # Arrange to have the tree column's heading be a toggle between tree and flat list views
    $itk_component(newtree) heading \#0 \
	-command [::itcl::code $this toggleTreeView] \
	-anchor w
    setTreeView
}

::itcl::body ArcherCore::initTreeImages {} {
    if {$mNoTree} {
	return
    }

    set mImage_air [image create photo -file [file join $mImgDir air.png]]
    set mImage_airLabeled [image create photo -file [file join $mImgDir air_labeled.png]]
    set mImage_airInter [image create photo -file [file join $mImgDir air_intersect.png]]
    set mImage_airSub [image create photo -file [file join $mImgDir air_subtract.png]]
    set mImage_airUnion [image create photo -file [file join $mImgDir air_union.png]]

    set mImage_airregion [image create photo -file [file join $mImgDir airregion.png]]
    set mImage_airregionInter [image create photo -file [file join $mImgDir airregion_intersect.png]]
    set mImage_airregionSub [image create photo -file [file join $mImgDir airregion_subtract.png]]
    set mImage_airregionUnion [image create photo -file [file join $mImgDir airregion_union.png]]

    set mImage_comb [image create photo -file [file join $mImgDir comb.png]]
    set mImage_combLabeled [image create photo -file [file join $mImgDir comb_labeled.png]]
    set mImage_combInter [image create photo -file [file join $mImgDir comb_intersect.png]]
    set mImage_combSub [image create photo -file [file join $mImgDir comb_subtract.png]]
    set mImage_combUnion [image create photo -file [file join $mImgDir comb_union.png]]

    set mImage_region [image create photo -file [file join $mImgDir region.png]]
    set mImage_regionLabeled [image create photo -file [file join $mImgDir region_labeled.png]]
    set mImage_regionInter [image create photo -file [file join $mImgDir region_intersect.png]]
    set mImage_regionSub [image create photo -file [file join $mImgDir region_subtract.png]]
    set mImage_regionUnion [image create photo -file [file join $mImgDir region_union.png]]

    set mImage_arb8 [image create photo -file [file join $mImgDir arb8.png]]
    set mImage_arb8Labeled [image create photo -file [file join $mImgDir arb8_labeled.png]]
    set mImage_arb8Inter [image create photo -file [file join $mImgDir arb8_intersect.png]]
    set mImage_arb8Sub [image create photo -file [file join $mImgDir arb8_subtract.png]]
    set mImage_arb8Union [image create photo -file [file join $mImgDir arb8_union.png]]

    set mImage_arb7Labeled [image create photo -file [file join $mImgDir arb7_labeled.png]]
    set mImage_arb6Labeled [image create photo -file [file join $mImgDir arb6_labeled.png]]
    set mImage_arb5Labeled [image create photo -file [file join $mImgDir arb5_labeled.png]]
    set mImage_arb4Labeled [image create photo -file [file join $mImgDir arb4_labeled.png]]

    set mImage_arbn [image create photo -file [file join $mImgDir arbn.png]]
    set mImage_arbnLabeled [image create photo -file [file join $mImgDir arbn_labeled.png]]
    set mImage_arbnInter [image create photo -file [file join $mImgDir arbn_intersect.png]]
    set mImage_arbnSub [image create photo -file [file join $mImgDir arbn_subtract.png]]
    set mImage_arbnUnion [image create photo -file [file join $mImgDir arbn_union.png]]

    set mImage_ars [image create photo -file [file join $mImgDir ars.png]]
    set mImage_arsLabeled [image create photo -file [file join $mImgDir ars_labeled.png]]
    set mImage_arsInter [image create photo -file [file join $mImgDir ars_intersect.png]]
    set mImage_arsSub [image create photo -file [file join $mImgDir ars_subtract.png]]
    set mImage_arsUnion [image create photo -file [file join $mImgDir ars_union.png]]

    set mImage_bot [image create photo -file [file join $mImgDir bot.png]]
    set mImage_botLabeled [image create photo -file [file join $mImgDir bot_labeled.png]]
    set mImage_botInter [image create photo -file [file join $mImgDir bot_intersect.png]]
    set mImage_botSub [image create photo -file [file join $mImgDir bot_subtract.png]]
    set mImage_botUnion [image create photo -file [file join $mImgDir bot_union.png]]

    set mImage_dsp [image create photo -file [file join $mImgDir dsp.png]]
    set mImage_dspLabeled [image create photo -file [file join $mImgDir dsp_labeled.png]]
    set mImage_dspInter [image create photo -file [file join $mImgDir dsp_intersect.png]]
    set mImage_dspSub [image create photo -file [file join $mImgDir dsp_subtract.png]]
    set mImage_dspUnion [image create photo -file [file join $mImgDir dsp_union.png]]

    set mImage_ehy [image create photo -file [file join $mImgDir ehy.png]]
    set mImage_ehyLabeled [image create photo -file [file join $mImgDir ehy_labeled.png]]
    set mImage_ehyInter [image create photo -file [file join $mImgDir ehy_intersect.png]]
    set mImage_ehySub [image create photo -file [file join $mImgDir ehy_subtract.png]]
    set mImage_ehyUnion [image create photo -file [file join $mImgDir ehy_union.png]]

    set mImage_ell [image create photo -file [file join $mImgDir ell.png]]
    set mImage_ellLabeled [image create photo -file [file join $mImgDir ell_labeled.png]]
    set mImage_ellInter [image create photo -file [file join $mImgDir ell_intersect.png]]
    set mImage_ellSub [image create photo -file [file join $mImgDir ell_subtract.png]]
    set mImage_ellUnion [image create photo -file [file join $mImgDir ell_union.png]]

    set mImage_epa [image create photo -file [file join $mImgDir epa.png]]
    set mImage_epaLabeled [image create photo -file [file join $mImgDir epa_labeled.png]]
    set mImage_epaInter [image create photo -file [file join $mImgDir epa_intersect.png]]
    set mImage_epaSub [image create photo -file [file join $mImgDir epa_subtract.png]]
    set mImage_epaUnion [image create photo -file [file join $mImgDir epa_union.png]]

    set mImage_eto [image create photo -file [file join $mImgDir eto.png]]
    set mImage_etoLabeled [image create photo -file [file join $mImgDir eto_labeled.png]]
    set mImage_etoInter [image create photo -file [file join $mImgDir eto_intersect.png]]
    set mImage_etoSub [image create photo -file [file join $mImgDir eto_subtract.png]]
    set mImage_etoUnion [image create photo -file [file join $mImgDir eto_union.png]]

    set mImage_extrude [image create photo -file [file join $mImgDir extrude.png]]
    set mImage_extrudeLabeled [image create photo -file [file join $mImgDir extrude_labeled.png]]
    set mImage_extrudeInter [image create photo -file [file join $mImgDir extrude_intersect.png]]
    set mImage_extrudeSub [image create photo -file [file join $mImgDir extrude_subtract.png]]
    set mImage_extrudeUnion [image create photo -file [file join $mImgDir extrude_union.png]]

    set mImage_half [image create photo -file [file join $mImgDir half.png]]
    set mImage_halfLabeled [image create photo -file [file join $mImgDir half_labeled.png]]
    set mImage_halfInter [image create photo -file [file join $mImgDir half_intersect.png]]
    set mImage_halfSub [image create photo -file [file join $mImgDir half_subtract.png]]
    set mImage_halfUnion [image create photo -file [file join $mImgDir half_union.png]]

    set mImage_hyp [image create photo -file [file join $mImgDir hyp.png]]
    set mImage_hypLabeled [image create photo -file [file join $mImgDir hyp_labeled.png]]
    set mImage_hypInter [image create photo -file [file join $mImgDir hyp_intersect.png]]
    set mImage_hypSub [image create photo -file [file join $mImgDir hyp_subtract.png]]
    set mImage_hypUnion [image create photo -file [file join $mImgDir hyp_union.png]]

    set mImage_invalid [image create photo -file [file join $mImgDir invalid.png]]
    set mImage_invalidInter [image create photo -file [file join $mImgDir invalid_intersect.png]]
    set mImage_invalidSub [image create photo -file [file join $mImgDir invalid_subtract.png]]
    set mImage_invalidUnion [image create photo -file [file join $mImgDir invalid_union.png]]

    set mImage_metaball [image create photo -file [file join $mImgDir metaball.png]]
    set mImage_metaballLabeled [image create photo -file [file join $mImgDir metaball_labeled.png]]
    set mImage_metaballInter [image create photo -file [file join $mImgDir metaball_intersect.png]]
    set mImage_metaballSub [image create photo -file [file join $mImgDir metaball_subtract.png]]
    set mImage_metaballUnion [image create photo -file [file join $mImgDir metaball_union.png]]

    set mImage_nmg [image create photo -file [file join $mImgDir nmg.png]]
    set mImage_nmgLabeled [image create photo -file [file join $mImgDir nmg_labeled.png]]
    set mImage_nmgInter [image create photo -file [file join $mImgDir nmg_intersect.png]]
    set mImage_nmgSub [image create photo -file [file join $mImgDir nmg_subtract.png]]
    set mImage_nmgUnion [image create photo -file [file join $mImgDir nmg_union.png]]

    set mImage_other [image create photo -file [file join $mImgDir other.png]]
    set mImage_otherInter [image create photo -file [file join $mImgDir other_intersect.png]]
    set mImage_otherSub [image create photo -file [file join $mImgDir other_subtract.png]]
    set mImage_otherUnion [image create photo -file [file join $mImgDir other_union.png]]

    set mImage_partLabeled [image create photo -file [file join $mImgDir part_labeled.png]]

    set mImage_pipe [image create photo -file [file join $mImgDir pipe.png]]
    set mImage_pipeLabeled [image create photo -file [file join $mImgDir pipe_labeled.png]]
    set mImage_pipeInter [image create photo -file [file join $mImgDir pipe_intersect.png]]
    set mImage_pipeSub [image create photo -file [file join $mImgDir pipe_subtract.png]]
    set mImage_pipeUnion [image create photo -file [file join $mImgDir pipe_union.png]]

    set mImage_rhc [image create photo -file [file join $mImgDir rhc.png]]
    set mImage_rhcLabeled [image create photo -file [file join $mImgDir rhc_labeled.png]]
    set mImage_rhcInter [image create photo -file [file join $mImgDir rhc_intersect.png]]
    set mImage_rhcSub [image create photo -file [file join $mImgDir rhc_subtract.png]]
    set mImage_rhcUnion [image create photo -file [file join $mImgDir rhc_union.png]]

    set mImage_rpc [image create photo -file [file join $mImgDir rpc.png]]
    set mImage_rpcLabeled [image create photo -file [file join $mImgDir rpc_labeled.png]]
    set mImage_rpcInter [image create photo -file [file join $mImgDir rpc_intersect.png]]
    set mImage_rpcSub [image create photo -file [file join $mImgDir rpc_subtract.png]]
    set mImage_rpcUnion [image create photo -file [file join $mImgDir rpc_union.png]]

    set mImage_sketch [image create photo -file [file join $mImgDir sketch.png]]
    set mImage_sketchLabeled [image create photo -file [file join $mImgDir sketch_labeled.png]]
    set mImage_sketchInter [image create photo -file [file join $mImgDir sketch_intersect.png]]
    set mImage_sketchSub [image create photo -file [file join $mImgDir sketch_subtract.png]]
    set mImage_sketchUnion [image create photo -file [file join $mImgDir sketch_union.png]]

    set mImage_sph [image create photo -file [file join $mImgDir sph.png]]
    set mImage_sphLabeled [image create photo -file [file join $mImgDir sph_labeled.png]]
    set mImage_sphInter [image create photo -file [file join $mImgDir sph_intersect.png]]
    set mImage_sphSub [image create photo -file [file join $mImgDir sph_subtract.png]]
    set mImage_sphUnion [image create photo -file [file join $mImgDir sph_union.png]]

    set mImage_tgc [image create photo -file [file join $mImgDir tgc.png]]
    set mImage_tgcLabeled [image create photo -file [file join $mImgDir tgc_labeled.png]]
    set mImage_tgcInter [image create photo -file [file join $mImgDir tgc_intersect.png]]
    set mImage_tgcSub [image create photo -file [file join $mImgDir tgc_subtract.png]]
    set mImage_tgcUnion [image create photo -file [file join $mImgDir tgc_union.png]]

    set mImage_tor [image create photo -file [file join $mImgDir tor.png]]
    set mImage_torLabeled [image create photo -file [file join $mImgDir tor_labeled.png]]
    set mImage_torInter [image create photo -file [file join $mImgDir tor_intersect.png]]
    set mImage_torSub [image create photo -file [file join $mImgDir tor_subtract.png]]
    set mImage_torUnion [image create photo -file [file join $mImgDir tor_union.png]]
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
	    -centerDotEnable $mShowViewingParams \
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
	RtControl $itk_interior.rtcp -mged $itk_component(ged)
    } {
	usual
    }
    $itk_component(ged) fb_active 0
    $itk_component(rtcntrl) updateControlPanel
    bind $itk_component(rtcntrl) <Visibility> "raise $itk_component(rtcntrl)"
    bind $itk_component(rtcntrl) <FocusOut> "raise $itk_component(rtcntrl)"
    wm protocol $itk_component(rtcntrl) WM_DELETE_WINDOW "$itk_component(rtcntrl) deactivate"

    # Other bindings for mged
    #bind $itk_component(ged) <Enter> {focus %W}

    if {$mViewOnly && !$mNoToolbar} {
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

    set mActivePane 1
    set mActivePaneName ur

    if {$mShowViewAxes} {
	showViewAxes
    }
    if {$mShowModelAxes} {
	showModelAxes
    }
    if {$mShowGroundPlane} {
	showGroundPlane
    }
    if {$mShowPrimitiveLabels} {
	showPrimitiveLabels
    }
    if {$mShowViewingParams} {
	showViewParams
    }
    if {$mShowScale} {
	showScale
    }
    if {$mLighting} {
	doLighting
    }
    if {$mShowGrid} {
	showGrid
    }
    if {$mSnapGrid} {
	snapGrid
    }
    if {$mShowADC} {
	showADC
    }

    $itk_component(ged) configure -paneCallback [::itcl::code $this updateActivePane]
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
	$itk_component(rtcntrl) updateControlPanel
    }
}

# ------------------------------------------------------------
#                 INTERFACE OPERATIONS
# ------------------------------------------------------------
::itcl::body ArcherCore::closeDb {} {
    grid forget $itk_component(ged)
    closeMged

    grid $itk_component(canvas) -row 1 -column 0 -columnspan 3 -sticky news
    set mDbType ""

    rebuildTree
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

    if {$mTarget == ""} {
	set typelist {
	    {"BRL-CAD Database" {".g"}}
	    {"All Files" {*}}
	}

	set target [tk_getSaveFile -parent $itk_interior \
			-initialdir $mLastSelectedDir \
			-title "Save the New Database" \
			-filetypes $typelist]
    } else {
	set target $mTarget
    }

    # Sanity
    if {$target == "" ||
	$mTargetCopy == "" ||
	$mDbReadOnly ||
	$mDbNoCopy} {
	return
    }

    set mTarget $target
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

##
# This method operates on one bot. It splits the bot if
# necessary and puts the pieces in a new group.
#
::itcl::body ArcherCore::bot_split2 {_bot} {
    set new_bots [gedCmd bot_split $_bot]

    set blist [lindex $new_bots 0]
    set bname [lindex $blist 0]
    set bots [lindex $blist 1]
    if {[llength $bots] == 0} {
	return ""
    }

    set backup "$bname.bak"
    if {[gedCmd exists $backup]} {
	gedCmd make_name -s 0
	set backup [gedCmd make_name $backup]
    }

    gedCmd mv $bname $backup
    eval gedCmd g $bname $bots
    gedCmd erase $bname

    return [list $bname $backup]
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
    if {!$mNoTree} {
	$itk_component(vpane) hide hierarchyView
    }
}

::itcl::body ArcherCore::openHierarchy {{fraction 30}} {
    #XXX We should check to see if fraction is between
    #    0 and 100 inclusive.
    if {!$mNoTree} {
	$itk_component(vpane) show hierarchyView
	$itk_component(vpane) fraction $fraction [expr {100 - $fraction}]
    }
}

::itcl::body ArcherCore::rebuildTree {} {
    if {$mNoTree} {
	return
    }

    foreach node [array names mNode2Text] {
	catch {$itk_component(newtree) delete $node}
    }

    $itk_component(newtree) configure -columns $mTreeAttrColumns
    set i 0
    foreach column $mTreeAttrColumns {
	$itk_component(newtree) heading $i -text $column
	incr i
    }

    # clobber the associative arrays
    unset mNode2Text
    unset mText2Node
    unset mCNode2PList
    unset mPNode2CList
    set mNode2Text() ""
    set mText2Node() ""
    set mCNode2PList() ""
    set mPNode2CList() ""
    set mNodePDrawList ""
    set mNodeDrawList ""
    set mAffectedNodeList ""

    if {$mEnableListView} {
	set items [lsort -dictionary [$itk_component(ged) ls]]
    } else {
	set items [lsort -dictionary [$itk_component(ged) tops]]
    }

    foreach item $items {
	set item [regsub {/.*} $item {}]
	fillTree {} $item $mEnableListView
    }

    updateTreeDrawLists
}

##
# This routine expects mEnableListView to be 0.
#
::itcl::body ArcherCore::rsyncTree {_pnode} {
    if {$mNoTree} {
	return
    }

    set ptext $mNode2Text($_pnode)

    # Is _pnode currently set up for children?
    if {![catch {set clists $mPNode2CList($_pnode)}]} {

	set pgdata [$itk_component(ged) get $ptext]
	set tree [getTreeFromGData $pgdata]
	set mlist [getTreeMembers $tree]

	# Reconcile clists (i.e. the tree's view)
	# with mlist (i.e. the database's view).

	set clen [llength $clists]
	switch -- $clen {
	    0 {
		# This shouldn't happen.
		puts "ArcherCore::rsyncTree - $ptext has empty child list"
	    }
	    default {
		foreach clist $clists {
		    set ctext [lindex $clist 0]
		    if {$ctext == $TREE_PLACEHOLDER_TAG} {
			if {$mlist == {}} {
			    # Since there are NO children, remove the placeholder
			    $itk_component(newtree) delete [lindex $clist 1]
			    unset mPNode2CList($_pnode)

			} else {
			    # There are children so keep the placeholder since
			    # the parent node has not been opened.

			    set mlist {}
			}

			continue
		    }

		    set cnode [lindex $clist 1]

		    set i [lsearch $mlist $ctext]
		    if {$i == -1} {
			purgeNodeData $cnode
			$itk_component(newtree) delete $cnode
			continue
		    }

		    set mlist [lreplace $mlist $i $i]

		    if {[catch {$itk_component(ged) get $ctext} cgdata]} {
			# In here, the child node refers to non-existent geometry
			# so update the image and remove any grandchildren etc.

			set op [getTreeOp $ptext $ctext]
			set img [getTreeImage $ctext "invalid" $op]
			$itk_component(newtree) item $cnode -image $img

			# Remove all grandchildren
			if {![catch {set gclists $mPNode2CList($cnode)}]} {
			    foreach gclist $gclists {
				set gctext [lindex $gclist 0]
				set gcnode [lindex $gclist 1]

				if {$gctext == $TREE_PLACEHOLDER_TAG} {
				    unset mPNode2CList($cnode)
				} else {
				    purgeNodeData $gcnode
				}

				$itk_component(newtree) delete $gcnode
			    }
			}
		    } else {
			# The child node refers to geometry that exists. The image
			# needs to be updated in case we're going from non-existent
			# to existent geometry.

			set ctype [lindex $cgdata 0]
			set isregion [isRegion $cgdata]
			set op [getTreeOp $ptext $ctext]
			set img [getTreeImage $ctext $ctype $op $isregion]

			# Also, for combinations the placeholders and
			# TREE_OPENED_TAGs might need updating.
			if {$ctype == "comb"} {
			    # Possibly add a place holder
			    if {![catch {set gclists $mPNode2CList($cnode)}]} {
				set gclen [llength $gclists]

				if {$gclen != 0} {
				    rsyncTree [lindex $clist 1]
				} else {
				    # This shouldn't happen.
				    puts "ArcherCore::rsyncTree - $ctext has empty child list"
				}

			    } else {
				set ctree [getTreeFromGData $cgdata]
				set cmlist [getTreeMembers $ctree]
				if {$cmlist != ""} {
				    removeTreeNodeTag $cnode $TREE_OPENED_TAG
				    $itk_component(newtree) item $cnode -open false
				    addTreePlaceholder $cnode
				}
			    }
			}

			$itk_component(newtree) item $cnode -image $img
		    }
		}
	    }
	}

	# Anything leftover in mlist needs to be added
	foreach member $mlist {
	    fillTree $_pnode $member $mEnableListView 1
	}
    } else {
	set pgdata [$itk_component(ged) get $ptext]
	set ptype [lindex $pgdata 0]
	if {$ptype == "comb"} {
	    set tree [getTreeFromGData $pgdata]
	    set mlist [getTreeMembers $tree]
	    if {$mlist != ""} {
		removeTreeNodeTag $_pnode $TREE_OPENED_TAG
		$itk_component(newtree) item $_pnode -open false
		addTreePlaceholder $_pnode
	    }
	}
    }
}

##
# Synchronize the tree view with the database.
#
::itcl::body ArcherCore::syncTree {} {
    if {$mNoTree} {
	return
    }

    # Get list of toplevel tree items
    if {$mEnableListView} {
	set items [lsort -dictionary [$itk_component(ged) ls]]
    } else {
	set items [lsort -dictionary [$itk_component(ged) tops]]
    }

    set scrubbed_items {}
    foreach item $items {
	set item [regsub {/.*} $item {}]
	lappend scrubbed_items  $item
    }

    # Get rid of toplevel tree nodes that are no
    # longer valid (i.e. either they don't exist or they
    # belong to atleast one combination).
    if {![catch {set clists $mPNode2CList()}]} {
	foreach clist $clists {
	    set ctext [lindex $clist 0]

	    # Checking for the existence of ctext
	    if {![$itk_component(ged) exists $ctext]} {
		# ctext doesn't exist
		set cnode [lindex $clist 1]
		purgeNodeData $cnode
		$itk_component(newtree) delete $cnode
	    } elseif {!$mEnableListView} {
		# Make sure ctext is a valid toplevel
		set i [lsearch $scrubbed_items $ctext]
		if {$i == -1} {
		    # ctext does exist, but is not a toplevel tree node
		    set cnode [lindex $clist 1]
		    purgeNodeData $cnode
		    $itk_component(newtree) delete $cnode
		}
	    }
	}
    }

    # Make sure each item has a tree node
    foreach item $scrubbed_items {
	set cnodes [getCNodesFromCText {} $item]

	if {$cnodes == {}} {
	    fillTree {} $item $mEnableListView
	} elseif {!$mEnableListView} {
	    foreach item $cnodes {
		rsyncTree $item
	    }
	}
    }

    updateTreeDrawLists
}

::itcl::body ArcherCore::shootRay {_start _op _target _prep _no_bool _onehit _bot_dflag} {
    objects [gedCmd who]
    shootRay_doit $_start $_op $_target $_prep $_no_bool $_onehit $_bot_dflag $objects
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
	$VIEW_ROTATE_MODE { \
		beginViewRotate \
		set ret 1
	} \
	$VIEW_TRANSLATE_MODE { \
		beginViewTranslate \
		set ret 1
	} \
	$VIEW_SCALE_MODE { \
		beginViewScale \
		set ret 1
	} \
	$VIEW_CENTER_MODE { \
		initViewCenterMode \
		set ret 1
	} \
	$COMP_PICK_MODE { \
		initCompPick \
		set ret 1
	} \
	$COMP_SELECT_MODE { \
		initCompSelect \
		set ret 1
	} \
	$MEASURE_MODE { \
		initViewMeasure \
		set ret 1
	}

    return $ret
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

#    set select [$itk_component(tree) selection get]
    #set element [lindex [split $select ":"] 1]
    set element [split $select ":"]
    if {[llength $element] > 1} {
	set element [lindex $element 1]
    }

#    set node [$itk_component(tree) query -path $element]

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
#    set parent [$itk_component(tree) query -parent $element]
#    $itk_component(tree) remove $element $parent
#    rebuildTree
    SetNormalCursor $this
}

::itcl::body ArcherCore::doCopyOrMove {top comp cmd} {
    set mNeedSave 1
    updateSaveMode
    SetWaitCursor $this
    set comp2 [string trim [$top.entry get]]
    wm withdraw $top
    gedCmd $cmd $comp
    syncTree
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
	-value $VIEW_ROTATE_MODE \
	-command [::itcl::code $this beginViewRotate]
    $itk_component(primaryToolbar) add radiobutton translate \
	-balloonstr "Translate view" \
	-helpstr "Translate view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $VIEW_TRANSLATE_MODE \
	-command [::itcl::code $this beginViewTranslate] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton scale \
	-balloonstr "Scale view" \
	-helpstr "Scale view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $VIEW_SCALE_MODE \
	-command [::itcl::code $this beginViewScale] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton center \
	-balloonstr "Center view" \
	-helpstr "Center view" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $VIEW_CENTER_MODE \
	-command [::itcl::code $this initViewCenterMode] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton cpick \
	-balloonstr "Component Pick" \
	-helpstr "Component Pick" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $COMP_PICK_MODE \
	-command [::itcl::code $this initCompPick] \
	-state disabled
    $itk_component(primaryToolbar) add radiobutton cselect \
	-balloonstr "Component Select Mode" \
	-helpstr "Component Select Mode" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $COMP_SELECT_MODE \
	-command [::itcl::code $this initCompSelect] \
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
    $itk_component(primaryToolbar) itemconfigure cpick -state disabled
    $itk_component(primaryToolbar) itemconfigure cselect -state disabled
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
    $itk_component(ged) init_view_rotate 1
    $itk_component(ged) init_button_no_op 2

    $itk_component(ged) rect lwidth 0
}

::itcl::body ArcherCore::endViewRotate {_pane} {
    $itk_component(ged) end_view_rotate $_pane

    set ae [$itk_component(ged) pane_aet $_pane]
    addHistory "ae $ae"
}

::itcl::body ArcherCore::beginViewScale {} {
    $itk_component(ged) init_view_scale 1
    $itk_component(ged) init_button_no_op 2

    $itk_component(ged) rect lwidth 0
}

::itcl::body ArcherCore::endViewScale {_pane} {
    $itk_component(ged) end_view_scale $_pane

    set size [$itk_component(ged) pane_size $_pane]
    addHistory "size $size"
}

::itcl::body ArcherCore::beginViewTranslate {} {
    $itk_component(ged) init_view_translate 1
    $itk_component(ged) init_button_no_op 2

    $itk_component(ged) rect lwidth 0
}

::itcl::body ArcherCore::endViewTranslate {_pane} {
    $itk_component(ged) end_view_translate $_pane

    set center [$itk_component(ged) pane_center $_pane]
    addHistory "center $center"
}

::itcl::body ArcherCore::initViewCenterMode {} {
    $itk_component(ged) init_view_center 1
    $itk_component(ged) init_button_no_op 2

    $itk_component(ged) clear_mouse_ray_callback_list
    $itk_component(ged) add_mouse_ray_callback [::itcl::code $this mrayCallback_cvo]
    $itk_component(ged) init_comp_pick 2

    $itk_component(ged) rect lwidth 0
}

::itcl::body ArcherCore::initCompPick {} {
    $itk_component(ged) clear_mouse_ray_callback_list
    $itk_component(ged) add_mouse_ray_callback [::itcl::code $this mrayCallback_pick]
    $itk_component(ged) init_comp_pick 1
    $itk_component(ged) init_button_no_op 2

    $itk_component(ged) rect lwidth 0
}

::itcl::body ArcherCore::initCompSelect {} {
    $itk_component(ged) clear_view_rect_callback_list
    $itk_component(ged) add_view_rect_callback [::itcl::code $this compSelectCallback]
    $itk_component(ged) init_view_rect 1
    $itk_component(ged) init_button_no_op 2

    # The rect lwidth should be a preference
    $itk_component(ged) rect lwidth 1
}

::itcl::body ArcherCore::compSelectCallback {_mstring} {
    putString $_mstring
}

::itcl::body ArcherCore::mrayCallback_cvo {_pane _start _target _partitions} {
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
    $itk_component(ged) pane_center $_pane $point
}

::itcl::body ArcherCore::mrayCallback_pick {_pane _start _target _partitions} {
    set partition [lindex $_partitions 0]
    if {$partition == {}} {
	putString "Missed!"
	set mStatusStr "Missed!"
    } else {
	set in [bu_get_value_by_keyword "in" $partition]
	set path [bu_get_value_by_keyword "path" $in]
	set path [regsub {^/} $path {}]
	set pathlist [split $path /]
	set last [lindex $pathlist end]
	switch -- $mCompPickMode \
	    $COMP_PICK_TREE_SELECT_MODE { \
		selectTreePath $path
	    } \
	    $COMP_PICK_NAME_MODE { \
		putString "Hit $path, last - $last"
	    } \
	    $COMP_PICK_ERASE_MODE { \
		gedCmd erase $path
		if {!$mEnableListView} {
		    getTreeNode $path 1
		}
		updateTreeDrawLists
		putString "erase $path"
		set mStatusStr "erase $path"
	    } \
	    $COMP_PICK_BOT_FLIP_MODE { \
		catch {bot_flip $last}
		redrawObj $path
	    } \
	    $COMP_PICK_BOT_SPLIT_MODE {
		set how [gedCmd how $path]
		if {![catch {bot_split2 $last} bgroup]} {
		    set dirname [file dirname $path]
		    if {$dirname != "."} {
			set drawitem $dirname
		    } else {
			set drawitem $bgroup
		    }
		    gedCmd draw -m$how $drawitem

		    syncTree
		    setSave
		}
	    } \
	    $COMP_PICK_BOT_SYNC_MODE { \
		catch {bot_sync $last}
		redrawObj $path
	    }
    }
}

::itcl::body ArcherCore::initViewMeasure {} {
    $itk_component(ged) clear_view_measure_callback_list
    $itk_component(ged) add_view_measure_callback [::itcl::code $this endViewMeasure]
    $itk_component(ged) init_view_measure
    $itk_component(ged) init_button_no_op 2

    $itk_component(ged) rect lwidth 0
}

::itcl::body ArcherCore::endViewMeasure {_mstring} {
    putString $_mstring
    set mStatusStr $_mstring
}

::itcl::body ArcherCore::initDefaultBindings {{_comp ""}} {
    if {!$mNoToolbar} {
	$itk_component(primaryToolbar) itemconfigure rotate -state normal
	$itk_component(primaryToolbar) itemconfigure translate -state normal
	$itk_component(primaryToolbar) itemconfigure scale -state normal
	$itk_component(primaryToolbar) itemconfigure center -state normal
	$itk_component(primaryToolbar) itemconfigure cpick -state normal
	$itk_component(primaryToolbar) itemconfigure cselect -state normal
	$itk_component(primaryToolbar) itemconfigure measure -state normal
    }

    $itk_component(ged) init_view_bindings

    # Initialize rotate mode
    set mDefaultBindingMode $VIEW_ROTATE_MODE
    beginViewRotate
}

::itcl::body ArcherCore::initBrlcadBindings {} {
    if {!$mNoToolbar} {
	$itk_component(primaryToolbar) itemconfigure rotate -state disabled
	$itk_component(primaryToolbar) itemconfigure translate -state disabled
	$itk_component(primaryToolbar) itemconfigure scale -state disabled
	$itk_component(primaryToolbar) itemconfigure center -state disabled
	$itk_component(primaryToolbar) itemconfigure cpick -state disabled
	$itk_component(primaryToolbar) itemconfigure cselect -state disabled
	$itk_component(primaryToolbar) itemconfigure measure -state disabled
    }

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


::itcl::body ArcherCore::backgroundColor {_color} {
    set mCurrentPaneName ""
    set mBackgroundColor $_color
    set mBackground [::cadwidgets::Ged::get_rgb_color $mBackgroundColor]

    if {[info exists itk_component(ged)]} {
	eval $itk_component(ged) bg_all $mBackground
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
	"Navy" {
	    $cmd configure $colorOption {0 0 50}
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

::itcl::body ArcherCore::cmd {args} {
    set cmd [lindex $args 0]
    if {$cmd == ""} {
	return
    }

    if {$cmd == "info"} {
	set arg1 [lindex $args 1]
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
	return [uplevel $mCoreCmdLevel $args]
    }

    set i [lsearch -exact $mUnwrappedDbCommands $cmd]
    if {$i != -1} {
	addHistory $args
	return [eval gedCmd $args]
    }

    error "ArcherCore::cmd: unrecognized command - $args, check source code"
}

::itcl::body ArcherCore::gedCmd {args} {
    return [eval $itk_component(ged) $args]
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

    return [getTreeMembers $tlist]
}

::itcl::body ArcherCore::getTreeFromGData {_gdata} {
    set ti [lsearch $_gdata tree]
    if {$ti != -1} {
	incr ti
	return [lindex $_gdata $ti]
    }

    return {}
}

::itcl::body ArcherCore::getTreeMembers {_tlist {_mlist {}}} {
    set len [llength $_tlist]
    set op [lindex $_tlist 0]
    if {$op == "l"} {
	set name [lindex $_tlist 1]
	lappend _mlist $name
	return $_mlist
    }

    if {$len == 3} {
	set _mlist [getTreeMembers [lindex $_tlist 1] $_mlist]
	set _mlist [getTreeMembers [lindex $_tlist 2] $_mlist]
	return $_mlist
    }

    puts "ArcherCore::getTreeMembers: faulty tree - $_tlist"
    return $_mlist
}


::itcl::body ArcherCore::getTreeOp {_parent _child} {
    if {$_parent == ""} {
	return ""
    }

    #XXX The quick and dirty solution calls "l". There should be an option to
    #    get a clean list of the members and their respective operators.
    set ldata [split [$itk_component(ged) l $_parent] "\n"]
    foreach line [lrange $ldata 1 end] {
	if {[catch {lindex $line 1} member]} {
	    continue
	}

	if {$member == $_child} {
	    switch -- [lindex $line 0] {
		"-" {
		    return "Sub"
		}
		"+" {
		    return "Inter"
		}
		default {
		    return "Union"
		}
		
	    }
	    return 
	}
    }

    return ""
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
	updateTreeDrawLists
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
	#XXX The png command below needs to be modified to draw
	#XXX into an off screen buffer to avoid occlusion
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
    set mShowModelAxesTicks [gedCmd cget -modelAxesTickEnable]
    set mShowViewAxes [gedCmd cget -viewAxesEnable]
    set mShowGrid [gedCmd cget -gridEnable]
    set mSnapGrid [gedCmd cget -gridSnap]
    set mShowADC [gedCmd cget -adcEnable]
}

::itcl::body ArcherCore::doMultiPane {} {
    gedCmd configure -multi_pane $mMultiPane
}

::itcl::body ArcherCore::doLighting {} {
    SetWaitCursor $this

    # Leave this off for now.
    gedCmd zclip_all 0

    gedCmd zbuffer_all $mLighting
    gedCmd light_all $mLighting

    SetNormalCursor $this
}

::itcl::body ArcherCore::doViewReset {} {
    set mCurrentPaneName ""
    gedCmd autoview_all
    gedCmd default_views
}

::itcl::body ArcherCore::doAutoview {} {
    if {$mCurrentPaneName == ""} {
	set pane $mActivePaneName
    } else {
	set pane $mCurrentPaneName
    }
    set mCurrentPaneName ""

    $itk_component(ged) pane_autoview $pane
}

::itcl::body ArcherCore::doViewCenter {} {
    set pane [centerDialogOverPane $itk_component(centerDialog)]

    set center [$itk_component(ged) pane_center $pane]
    set mCenterX [lindex $center 0]
    set mCenterY [lindex $center 1]
    set mCenterZ [lindex $center 2]

    set mDbUnits [gedCmd units -s]
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
    catch {gedCmd configure -modelAxesTickEnable $mShowModelAxesTicks}
}

::itcl::body ArcherCore::showGrid {} {
    catch {gedCmd configure -gridEnable $mShowGrid}
}

::itcl::body ArcherCore::snapGrid {} {
    catch {gedCmd configure -gridSnap $mSnapGrid}
}

::itcl::body ArcherCore::showADC {} {
    catch {gedCmd configure -adcEnable $mShowADC}
}


# ------------------------------------------------------------
#                     TREE COMMANDS
# ------------------------------------------------------------

::itcl::body ArcherCore::updateTreeDrawLists {{_cflag 0}} {
    if {$mNoTree} {
	return
    }

    foreach node $mNodePDrawList {
	removeTreeNodeTag $node $TREE_PARTIALLY_DISPLAYED_TAG
    }
    foreach node $mNodeDrawList {
	removeTreeNodeTag $node $TREE_FULLY_DISPLAYED_TAG
    }

    set mNodePDrawList ""
    set mNodeDrawList ""

    foreach ditem [gedCmd who] {
	if {$mEnableListView} {
	    set ditem [regsub {^/} $ditem {}]
	    set dlist [split $ditem /]
	    set dlen [llength $dlist]
	    if {$dlen == 1} {
		eval lappend mNodeDrawList [lindex [lindex $mText2Node($ditem) 0] 0]
	    } else {
		eval lappend mNodePDrawList [lindex [lindex $mText2Node([lindex $dlist 0]) 0] 0]
	    }
	} else {
	    set nodesList [getTreeNodes $ditem $_cflag]
	    eval lappend mNodePDrawList [lindex $nodesList 0]
	    eval lappend mNodeDrawList [lindex $nodesList 1]
	}
    }

    set mNodePDrawList [lsort -unique $mNodePDrawList]
    set mNodeDrawList [lsort -unique $mNodeDrawList]

    foreach node $mNodePDrawList {
	addTreeNodeTag $node $TREE_PARTIALLY_DISPLAYED_TAG
    }
    foreach node $mNodeDrawList {
	addTreeNodeTag $node $TREE_FULLY_DISPLAYED_TAG
    }
}

::itcl::body ArcherCore::fillTree {_pnode _ctext _flat {_allow_multiple 0}} {
    global no_tree_decorate

    set cnodes [getCNodesFromCText $_pnode $_ctext]

    # Atleast one node for _pnode/_ctext already exists
    if {!$_allow_multiple && $cnodes != {}} {
	return
    }

    if {[catch {$itk_component(ged) get $_ctext} cgdata]} {
	set ctype invalid
	set isregion 0
    } else {
	set ctype [lindex $cgdata 0]
	set isregion [isRegion $cgdata]
    }

    set ptext $mNode2Text($_pnode)
    if {[info exists no_tree_decorate] && $no_tree_decorate} {
	set cnode [$itk_component(newtree) insert $_pnode end \
		       -tags $TREE_POPUP_TAG \
		       -text $_ctext]
    } else {
	set op [getTreeOp $ptext $_ctext]
	set img [getTreeImage $_ctext $ctype $op $isregion]
	set cnode [$itk_component(newtree) insert $_pnode end \
		       -tags $TREE_POPUP_TAG \
		       -text $_ctext \
		       -image $img]
    }
    fillTreeColumns $cnode $_ctext

    if {!$_flat && $ctype == "comb"} {
	set tree [getTreeFromGData $cgdata]
	set mlist [getTreeMembers $tree]
	if {$mlist != ""} {
	    removeTreeNodeTag $cnode $TREE_OPENED_TAG
	    $itk_component(newtree) item $cnode -open false
	    addTreePlaceholder $cnode
	}
    }

    lappend mText2Node($_ctext) [list $cnode $_pnode]
    set mNode2Text($cnode) $_ctext
    lappend mPNode2CList($_pnode) [list $_ctext $cnode]
    set mCNode2PList($cnode) [list $ptext $_pnode]
}

::itcl::body ArcherCore::fillTreeColumns {_cnode _ctext} {
    if {$mTreeAttrColumns != {}} {
	set vals {}

	if {[catch {gedCmd attr get $_ctext} alist]} {
	    set alist {}
	}

	foreach attr $mTreeAttrColumns {
	    set ai [lsearch -index 0 $alist $attr]
	    if {$ai != -1} {
		incr ai
		lappend vals [lindex $alist $ai]
	    } else {
		lappend vals {}
	    }
	}

	$itk_component(newtree) item $_cnode -values $vals
    }
}

::itcl::body ArcherCore::isRegion {_cgdata} {
    set ri [lsearch $_cgdata region]
    if {$ri == -1} {
	return 0
    }

    incr ri
    set isregion [lindex $_cgdata $ri]

    if {$isregion} {
	# Check for rid
	set ii [lsearch $_cgdata id]
	if {$ii != -1} {
	    incr ii
	    set hasId [lindex $_cgdata $ii]
	} else {
	    set hasId 0
	}

	# Check for air
	set ai [lsearch $_cgdata air]

	if {$ai != -1} {
	    set hasAir 1
	} else {
	    set hasAir 0
	}

	if {($hasId && $hasAir) || (!$hasId && !$hasAir)} {
	    set isregion 3
	} else {
	    if {$hasId} {
		set isregion 1
	    } else {
		set isregion 2
	    }
	}

	return $isregion
    }

    return 0
}

::itcl::body ArcherCore::loadMenu {_menu _node _nodeType} {
    # destroy old menu
    if [winfo exists $_menu.color] {
	$_menu.color delete 0 end
	destroy $_menu.color
    }
    if {[winfo exists $_menu.trans]} {
	$_menu.trans delete 0 end
	destroy $_menu.trans
    }
    $_menu delete 0 end

    set mRenderMode [gedCmd how $_node]
    # do this in case "ev" was used from the command line
    if {!$mEnableBigE && 2 < $mRenderMode} {
	set mRenderMode 0
    }

    if {$_nodeType == "leaf"} {
	$_menu add radiobutton -label "Wireframe" \
	    -indicatoron 1 -value 0 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $_node 0 1 1]

	$_menu add radiobutton -label "Shaded" \
	    -indicatoron 1 -value 1 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $_node 1 1 1]
	$_menu add radiobutton -label "Hidden Line" \
	    -indicatoron 1 -value 2 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $_node 4 1 1]

	if {$mEnableBigE} {
	    $_menu add radiobutton \
		-label "Evaluated" \
		-indicatoron 1 \
		-value 3 \
		-variable [::itcl::scope mRenderMode] \
		-command [::itcl::code $this render $_node 3 1 1]
	}

	$_menu add radiobutton -label "Off" \
	    -indicatoron 1 -value -1 -variable [::itcl::scope mRenderMode] \
	    -command [::itcl::code $this render $_node -1 1 1]
    } else {
	$_menu add command -label "Wireframe" \
	    -command [::itcl::code $this render $_node 0 1 1]

	$_menu add command -label "Shaded" \
	    -command [::itcl::code $this render $_node 1 1 1]
	$_menu add command -label "Hidden Line" \
	    -command [::itcl::code $this render $_node 4 1 1]

	if {$mEnableBigE} {
	    $_menu add command \
		-label "Evaluated" \
		-command [::itcl::code $this render $_node 3 1 1]
	}

	$_menu add command -label "Off" \
	    -command [::itcl::code $this render $_node -1 1 1]
    }

    #XXX need to copy over
    #    $_menu add separator
    #    $_menu add command -label "Copy" \
	#	    -command [::itcl::code $this alterObj "Copy" $mSelectedComp]
    #    $_menu add command -label "Rename" \
	#	    -command [::itcl::code $this alterObj "Rename" $mSelectedComp]
    #    $_menu add command -label "Delete" \
	#	    -command [::itcl::code $this deleteObj $mSelectedComp]


    $_menu add separator

    # Build color menu
    $_menu add cascade -label "Color" \
	-menu $_menu.color
    set color [menu $_menu.color -tearoff 0]

    $color configure \
	-background $SystemButtonFace

    $color add command -label "Red" \
	-command [::itcl::code $this setDisplayColor $_node {255 0 0}]
    $color add command -label "Orange" \
	-command [::itcl::code $this setDisplayColor $_node {204 128 51}]
    $color add command -label "Yellow" \
	-command [::itcl::code $this setDisplayColor $_node {219 219 112}]
    $color add command -label "Green" \
	-command [::itcl::code $this setDisplayColor $_node {0 255 0}]
    $color add command -label "Blue" \
	-command [::itcl::code $this setDisplayColor $_node {0 0 255}]
    $color add command -label "Indigo" \
	-command [::itcl::code $this setDisplayColor $_node {0 0 128}]
    $color add command -label "Violet" \
	-command [::itcl::code $this setDisplayColor $_node {128 0 128}]
    $color add separator
    $color add command -label "Default" \
	-command [::itcl::code $this setDisplayColor $_node {}]
    $color add command -label "Select..." \
	-command [::itcl::code $this selectDisplayColor $_node]

    if {($mDisplayType == "wgl" || $mDisplayType == "ogl") && ($_nodeType != "leaf" || 0 < $mRenderMode)} {
	# Build transparency menu
	$_menu add cascade -label "Transparency" \
	    -menu $_menu.trans
	set trans [menu $_menu.trans -tearoff 0]

	$trans configure \
	    -background $SystemButtonFace

	$trans add command -label "None" \
	    -command [::itcl::code $this setTransparency $_node 1.0]
#	#$trans add command -label "25%" \
#	    #	-command [::itcl::code $this setTransparency $_node 0.75]
#	#$trans add command -label "50%" \
#	    #	-command [::itcl::code $this setTransparency $_node 0.5]
#	#$trans add command -label "75%" \
#	    #	-command [::itcl::code $this setTransparency $_node 0.25]
#	$trans add command -label "10%" \
#	    -command [::itcl::code $this setTransparency $_node 0.9]
#	$trans add command -label "20%" \
#	    -command [::itcl::code $this setTransparency $_node 0.8]
#	$trans add command -label "30%" \
#	    -command [::itcl::code $this setTransparency $_node 0.7]
#	$trans add command -label "40%" \
#	    -command [::itcl::code $this setTransparency $_node 0.6]
#	$trans add command -label "50%" \
#	    -command [::itcl::code $this setTransparency $_node 0.5]
#	$trans add command -label "60%" \
#	    -command [::itcl::code $this setTransparency $_node 0.4]
#	$trans add command -label "70%" \
#	    -command [::itcl::code $this setTransparency $_node 0.3]
	$trans add command -label "80%" \
	    -command [::itcl::code $this setTransparency $_node 0.2]
	$trans add command -label "85%" \
	    -command [::itcl::code $this setTransparency $_node 0.15]
	$trans add command -label "90%" \
	    -command [::itcl::code $this setTransparency $_node 0.1]
	$trans add command -label "95%" \
	    -command [::itcl::code $this setTransparency $_node 0.05]
	$trans add command -label "97%" \
	    -command [::itcl::code $this setTransparency $_node 0.03]
	$trans add command -label "99%" \
	    -command [::itcl::code $this setTransparency $_node 0.01]

	# set up bindings for transparency status
	bind $trans <<MenuSelect>> \
	    [::itcl::code $this transparencyMenuStatusCB %W]
    }

    # set up bindings for status
    bind $_menu <<MenuSelect>> \
	[::itcl::code $this menuStatusCB %W]
    bind $color <<MenuSelect>> \
	[::itcl::code $this colorMenuStatusCB %W]
}

::itcl::body ArcherCore::findTreeChildNodes {_pnode} {
    if {![info exists mPNode2CList($_pnode)]} {
	 return
    }

    set cnodes {}
    foreach clist $mPNode2CList($_pnode) {
	set ctext [lindex $clist 0]
	if {$ctext == $TREE_PLACEHOLDER_TAG} {
	    continue
	}

	set cnode [lindex $clist 1]
	lappend cnodes $cnode
	eval lappend cnodes [findTreeChildNodes $cnode]
    }

    return $cnodes
}

::itcl::body ArcherCore::findTreeParentNodes {_cnode} {
    set plist $mCNode2PList($_cnode)
    set pnode [lindex $plist 1]

    if {$pnode != {}} {
	lappend pnodes $pnode
	eval lappend pnodes [findTreeParentNodes $pnode]
	return $pnodes
    }

    return $pnode
}

::itcl::body ArcherCore::getCNodesFromCText {_pnode _text} {
    if {[catch {set clists $mPNode2CList($_pnode)}]} {
	return ""
    }

    set cnodes {}

    set ilist [lsearch -all -index 0 $clists $_text]
    if {$ilist != {}} {
	foreach i $ilist {
	    lappend cnodes [lindex [lindex $clists $i] 1]
	}
    }

    return $cnodes
}

::itcl::body ArcherCore::getTreeImage {_obj _type {_op ""} {_isregion 0}} {
    switch -- $_type {
	comb {
	    switch -- $_isregion {
		1 {
		    return [subst $[subst mImage_region$_op]]
		}
		2 {
		    return [subst $[subst mImage_air$_op]]
		}
		3 {
		    return [subst $[subst mImage_airregion$_op]]
		}
		0 -
		default {
		    return [subst $[subst mImage_comb$_op]]
		}
	    }
	}
	arb8 -
	arbn -
	ars -
	bot -
	dsp -
	ehy -
	ell -
	epa -
	eto -
	extrude -
	half -
	hyp -
	invalid -
	metaball -
	nmg -
	pipe -
	rhc -
	rpc -
	sketch -
	sph -
	tgc -
	tor {
	    return [subst $[subst mImage_$_type$_op]]
	}
	default {
	    return [subst $[subst mImage_other$_op]]
	}
    }
}

::itcl::body ArcherCore::getTreeNode {_path {_cflag 0}} {
    set items [split $_path /]
    set len [llength $items]

    if {$len < 1} {
	return {}
    }

    set pnode {}
    set ptext [lindex $items 0]

    if {![info exists mText2Node($ptext)]} {
	return $pnode
    }

    foreach sublist $mText2Node($ptext) {
	set gpnode [lindex $sublist 1]

	if {$gpnode == {}} {
	    set pnode [lindex $sublist 0]

	    if {$_cflag} {
		if {![$itk_component(newtree) item $pnode -open]} {
		    $itk_component(newtree) item $pnode -open true
		    $itk_component(newtree) focus $pnode
		    handleTreeOpen
		} else {
		    $itk_component(newtree) focus $pnode
		}
	    }

	    break
	}
    }

    foreach item [lrange $items 1 end] {
	set found 0

	if {![info exists mText2Node($item)]} {
	    if {$_cflag} {
		if {![$itk_component(newtree) item $pnode -open]} {
		    $itk_component(newtree) item $pnode -open true
		    $itk_component(newtree) focus $pnode
		    handleTreeOpen
		} else {
		    $itk_component(newtree) focus $pnode
		}
	    } else {
		return $pnode
	    }
	}

	foreach sublist $mText2Node($item) {
	    set cnode [lindex $sublist 0]

	    if {$pnode == [lindex $sublist 1]} {
		set pnode $cnode
		set found 1

		if {$_cflag} {
		    if {![$itk_component(newtree) item $cnode -open]} {
			$itk_component(newtree) item $cnode -open true
			$itk_component(newtree) focus $cnode
			handleTreeOpen
		    } else {
			$itk_component(newtree) focus $cnode
		    }
		}

		continue
	    }
	}

	if {!$found} {
	    return $pnode
	}
    }

    return $pnode
}

::itcl::body ArcherCore::getTreeNodes {_path {_cflag 0}} {
    set nlist_partial {}
    set nlist_full {}
    set cnode [getTreeNode $_path $_cflag]

    if {$cnode == {}} {
	return [list $nlist_partial $nlist_full]
    }

    lappend nlist_full $cnode

    set nlist_partial [findTreeParentNodes $cnode]
    eval lappend nlist_full [findTreeChildNodes $cnode]

    return [list $nlist_partial $nlist_full]
}

::itcl::body ArcherCore::getTreePath {_node {_path ""}} {
    if {$_node == ""} {
	return ""
    }

    if {$_path == ""} {
	set _path $mNode2Text($_node)
    }

    set parent [lindex $mCNode2PList($_node) end]
    if {$parent == {}} {
	return $_path
    }

    set text $mNode2Text($parent)
    set _path "$text/$_path"

    return [getTreePath $parent $_path]
}

::itcl::body ArcherCore::handleTreeClose {} {
}

::itcl::body ArcherCore::handleTreeOpen {} {
    if {$mEnableListView} {
	return
    }

    SetWaitCursor $this

    set cnode [$itk_component(newtree) focus]
    set ctext [$itk_component(newtree) item $cnode -text]
    set cgdata [$itk_component(ged) get $ctext]
    set ctype [lindex $cgdata 0]

    if {$ctype == "comb"} {
	# If this node has never been opened ...
	if {[addTreeNodeTag $cnode $TREE_OPENED_TAG]} {
	    # Remove placeholder
	    set placeholder [lindex [lindex $mPNode2CList($cnode) 0] 1]
	    $itk_component(newtree) delete $placeholder
	    unset mPNode2CList($cnode)

	    set tree [getTreeFromGData $cgdata]
	    foreach gctext [getTreeMembers $tree] {
		if {[catch {$itk_component(ged) get $gctext} gcgdata]} {
		    set op [getTreeOp $ctext $gctext]
		    set img [getTreeImage $gctext "invalid" $op]

		    set gcnode [$itk_component(newtree) insert $cnode end \
				    -tags $TREE_POPUP_TAG \
				    -text $gctext \
				    -image $img]

		    fillTreeColumns $gcnode $gctext

		    lappend mText2Node($gctext) [list $gcnode $cnode]
		    set mNode2Text($gcnode) $gctext
		    lappend mPNode2CList($cnode) [list $gctext $gcnode]
		    set mCNode2PList($gcnode) [list $ctext $cnode]

		    continue
		}

		# Add gchild members
		fillTree $cnode $gctext $mEnableListView 1
	    }
	}
    }

    updateTreeDrawLists
    SetNormalCursor $this
}

::itcl::body ArcherCore::handleTreePopup {_x _y _X _Y} {
    set item [$itk_component(newtree) identify row $_x $_y]
    set text [$itk_component(newtree) item $item -text]
    set img [$itk_component(newtree) item $item -image]

    if {$img == $mImage_comb} {
	set nodeType "branch"
    } else {
	set nodeType "leaf"
    }

    set path [getTreePath $item $text]

    loadMenu $itk_component(newtreepopup) $path $nodeType
    tk_popup $itk_component(newtreepopup) $_X $_Y
}

::itcl::body ArcherCore::handleTreeSelect {} {
    foreach anode $mAffectedNodeList {
	removeTreeNodeTag $anode $TREE_AFFECTED_TAG
    }

    set mAffectedNodeList ""
    set snode [$itk_component(newtree) selection]

    if {$snode == ""} {
	return
    }

    set mPrevSelectedObjPath $mSelectedObjPath
    set mPrevSelectedObj $mSelectedObj
    set mSelectedObjPath [getTreePath $snode]
    set mSelectedObj $mNode2Text($snode)

    # label the object if it's being drawn
    set mRenderMode [gedCmd how $mSelectedObjPath]

    if {$mShowPrimitiveLabels} {
	if {0 <= $mRenderMode} {
	    gedCmd configure -primitiveLabels $mSelectedObjPath
	} else {
	    gedCmd configure -primitiveLabels {}
	}
    }

    if {!$mEnableAffectedNodeHighlight} {
	return
    }

    if {$mEnableListView} {
	if {$mEnableListViewAllAffected} {
	    foreach path [string trim [gedCmd search / -name $mSelectedObj]] {
		set path [regsub {^/} $path {}]
		foreach obj [split $path /] {
		    if {$obj == $mSelectedObj} {
			continue
		    }

		    set cnode [lindex [lindex $mText2Node($obj) 0] 0]
		    lappend mAffectedNodeList $cnode
		    addTreeNodeTag $cnode $TREE_AFFECTED_TAG
		}
	    }
	} else {
	    foreach obj [gedCmd dbfind $mSelectedObj] {
		set cnode [lindex [lindex $mText2Node($obj) 0] 0]
		lappend mAffectedNodeList $cnode
		addTreeNodeTag $cnode $TREE_AFFECTED_TAG
	    }
	}
    } else {
	foreach path [string trim [gedCmd search / -name $mSelectedObj]] {
	    set path [regsub {^/} $path {}]
	    set pathNodes [getTreeNodes $path]
#	    set pnodes [lreverse [lindex $pathNodes 0]]
	    set pnodes [lindex $pathNodes 0]
	    set cnodes [lindex $pathNodes 1]
	    set found 0
	    foreach pnode $pnodes {
		if {[$itk_component(newtree) item $pnode -open]} {
		    lappend mAffectedNodeList $pnode
		    set found 1
		    addTreeNodeTag $pnode $TREE_AFFECTED_TAG
		    break
		}
	    }

	    if {!$found} {
		set cnode [lindex $cnodes 0]
		if {$cnode != $snode} {
		    lappend mAffectedNodeList $cnode
		    addTreeNodeTag $cnode $TREE_AFFECTED_TAG
		}
	    }
	}
    }
}

::itcl::body ArcherCore::addTreeNodeTag {_node _tag} {
    set tags [$itk_component(newtree) item $_node -tags]
    set ai [lsearch $tags $_tag]
    if {$ai == -1} {
	lappend tags $_tag
	$itk_component(newtree) item $_node -tags $tags

	return 1
    }

    return 0
}

::itcl::body ArcherCore::removeTreeNodeTag {_node _tag} {
    set tags [$itk_component(newtree) item $_node -tags]
    set ai [lsearch $tags $_tag]
    if {$ai != -1} {
	set tags [lreplace $tags $ai $ai]
	$itk_component(newtree) item $_node -tags $tags
    }
}

::itcl::body ArcherCore::addTreePlaceholder {_pnode} {
    set cnode [$itk_component(newtree) insert $_pnode end \
		   -text $TREE_PLACEHOLDER_TAG \
		   -tags $TREE_PLACEHOLDER_TAG]

    set mPNode2CList($_pnode) [list [list $TREE_PLACEHOLDER_TAG $cnode]]
}

::itcl::body ArcherCore::selectTreePath {_path} {
    if {$_path == {}} {
	return
    }

    set obj [lindex [split $_path /] end]

    if {$mEnableListView} {
	set mSelectedObj $obj
	set mSelectObjPath $obj
	$itk_component(newtree) selection set [lindex [lindex $mText2Node($obj) 0] 0]
	$itk_component(newtree) see [lindex [lindex $mText2Node($obj) 0] 0]
    } else {
	getTreeNode $_path 1
	set snode [$itk_component(newtree) focus]
	set mSelectedObjPath ""

	if {$snode == {}} {
	    set mSelectedObj ""
	    putString $_path
	} else {
	    set mSelectedObj $obj
	    $itk_component(newtree) selection set $snode
	    $itk_component(newtree) see $snode
	    foreach pnode [lreverse [findTreeParentNodes $snode]] {
		append mSelectedObjPath $mNode2Text($pnode) "/"
	    }
	    append mSelectedObjPath $mNode2Text($snode)
	}
    }
}

::itcl::body ArcherCore::setTreeView {{_rflag 0}} {
    SetWaitCursor $this

    if {$mEnableListView} {
	set text "Show Tree"
    } else {
	set text "Show List"
    }

    $itk_component(newtree) heading \#0 -text $text

    if {$_rflag} {
	rebuildTree

	if {$mEnableListView} {
	    selectTreePath $mSelectedObj
	} else {
	    set paths [gedCmd search / -name $mSelectedObj]
	    if {[llength $paths]} {
		selectTreePath [lindex $paths 0]
	    }
	}
    }

    SetNormalCursor $this
}

::itcl::body ArcherCore::toggleTreeView {} {
    if {$mEnableListView} {
	set mEnableListView 0
    } else {
	set mEnableListView 1
    }

    setTreeView 1
}

::itcl::body ArcherCore::treeNodeHasBeenOpened {_node} {
    set tags [$itk_component(newtree) item $_node -tags]
    set ai [lsearch $tags $TREE_OPENED_TAG]
    if {$ai == -1} {
	return 0
    }

    return 1
}

::itcl::body ArcherCore::treeNodeIsOpen {_node} {
    return [$itk_component(newtree) item $_node -open]
}

#
# Delete any use of _node and its descendents in the data
# variables that are used to interact with the tree viewer.
#
::itcl::body ArcherCore::purgeNodeData {_node} {
    if {$mNoTree} {
	return
    }

    if {[info exists mPNode2CList($_node)]} {
	foreach sublist $mPNode2CList($_node) {
	    if {[lindex $sublist 0] != $TREE_PLACEHOLDER_TAG} {
		purgeNodeData [lindex $sublist 1]
	    }
	}
    }

    set name $mNode2Text($_node)
    if {[info exists mText2Node($name)]} {
	set leftovers {}
	foreach sublist $mText2Node($name) {
	    if {$_node != [lindex $sublist 0]} {
		lappend leftovers $sublist
	    }
	}

	if {$leftovers != {}} {
	    set mText2Node($name) $leftovers
	} else {
	    unset mText2Node($name)
	}
    }

    set pnode [lindex $mCNode2PList($_node) 1]
    if {[info exists mPNode2CList($pnode)]} {
	set leftovers {}
	foreach sublist $mPNode2CList($pnode) {
	    if {$_node != [lindex $sublist 1]} {
		lappend leftovers $sublist
	    }
	}

	if {$leftovers != {}} {
	    set mPNode2CList($pnode) $leftovers
	} else {
	    unset mPNode2CList($pnode)
	}
    }

    unset mNode2Text($_node)
    unset mCNode2PList($_node)

    set i [lsearch $mNodePDrawList $_node]
    if {$i != -1} {
	set mNodePDrawList [lreplace $mNodePDrawList $i $i]
    }

    set i [lsearch $mNodeDrawList $_node]
    if {$i != -1} {
	set mNodeDrawList [lreplace $mNodeDrawList $i $i]
    }
}

#
# Note -_name is expected to exist in the database.
#
::itcl::body ArcherCore::updateTreeTopWithName {_name} {
    # Check to see if its okay to add a toplevel node
    set toplist {}
    foreach item [$itk_component(ged) tops] {
	lappend toplist [regsub {/.*} $item {}]
    }

    set i [lsearch $toplist $_name]

    # Possibly add a toplevel node to the tree
    if {$i != -1} {
	# Add _name if not already there.
	if {[catch {lsearch -index 0 $mPNode2CList() $_name} j] || $j == -1} {
	    fillTree {} $_name $mEnableListView
	}
    } else {
	# Not found in call to tops, so possibly need to update _name as a member
	# of some other combination(s).
	if {![catch {set tlists $mText2Node($_name)}]} {
	    foreach tlist $tlists {
		set cnode [lindex $tlist 0]
		set pnode [lindex $tlist 1]
		set ptext $mNode2Text($pnode)

		if {[catch {$itk_component(ged) get $_name} cgdata]} {
		    # Shouldn't happen at this point.
		} else {
		    set ctype [lindex $cgdata 0]

		    if {!$mEnableListView} {
			set isregion [isRegion $cgdata]
			set op [getTreeOp $ptext $_name]
			set img [getTreeImage $_name $ctype $op $isregion]

			if {$ctype == "comb"} {
			    # Possibly add a place holder
			    if {![catch {set gclists $mPNode2CList($cnode)}]} {
				set gclen [llength $gclists]

				# This will probably never happen
				if {$gclen == 0} {
				    addTreePlaceholder $cnode
				}
			    } else {
				addTreePlaceholder $cnode
			    }
			}

			$itk_component(newtree) item $cnode -image $img
		    }
		}
	    }
	}
    }
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
	    after idle "$itk_component(cmd) configure -cmd_prefix \"[namespace tail $this] cmd\""
	} else {
	    if {!$mNoToolbar} {
		grid $itk_component(ged) -row 1 -column 0 -sticky news
	    } else {
		grid $itk_component(ged) -row 0 -column 0 -sticky news
	    }
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

	# rebuild tree contents
	rebuildTree
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

::itcl::body ArcherCore::refreshDisplay {} {
    if {$mCurrentPaneName == ""} {
	set pane $mActivePaneName
    } else {
	set pane $mCurrentPaneName
    }
    set mCurrentPaneName ""

#    $itk_component(ged) pane_refresh $pane
    $itk_component(ged) refresh_all
}

::itcl::body ArcherCore::putString {_str} {
    $itk_component(cmd) putstring $_str
}

::itcl::body ArcherCore::setStatusString {_str} {
    set mStatusStr $_str
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

::itcl::body ArcherCore::updateSaveMode {} {
    if {$mViewOnly} {
	return
    }

    if {!$mNoToolbar} {
	if {!$mDbNoCopy && !$mDbReadOnly && $mNeedSave} {
	    $itk_component(primaryToolbar) itemconfigure save \
		-state normal
	} else {
	    $itk_component(primaryToolbar) itemconfigure save \
		-state disabled
	}
    }
}

::itcl::body ArcherCore::createTargetCopy {} {
    if {$mTarget == ""} {
	set target "BBBBogusArcherTargetCopy"
    } else {
	set target $mTarget
    }

    set mTargetOldCopy $mTargetCopy
    set mTargetCopy "$target~"

    set id 1
    while {[file exists $mTargetCopy]} {
	set mTargetCopy "$target~$id"
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
	"Navy" {
	    return "000032"
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
    if {!$mShowPrimitiveLabels} {
	gedCmd configure -primitiveLabels {}
    }

    handleTreeSelect
}

::itcl::body ArcherCore::showViewParams {} {
    $itk_component(ged) configure -showViewingParams $mShowViewingParams
    $itk_component(ged) configure -centerDotEnable $mShowViewingParams
    refreshDisplay
}

::itcl::body ArcherCore::showScale {} {
    $itk_component(ged) configure -scaleEnable $mShowScale
    refreshDisplay
}

::itcl::body ArcherCore::launchNirt {} {
    putString "nirt -b"
    putString [$itk_component(ged) nirt -b]
}

::itcl::body ArcherCore::launchRtApp {app size} {
    global tcl_platform

    if {![string is digit $size]} {
	set size [winfo width $itk_component(ged)]
    }

    if {$tcl_platform(platform) == "windows"} {
	$itk_component(ged) $app -s $size -F /dev/wgll
    } {
	$itk_component(ged) $app -s $size -F /dev/ogll
    }
}

::itcl::body ArcherCore::updateDisplaySettings {} {
    updateZClipPlanes 0
}

::itcl::body ArcherCore::updateZClipPlanes {_unused} {
    set near [expr {0.01 * $mZClipFrontPref * $mZClipMaxPref}]
    set far [expr {0.01 * $mZClipBackPref * $mZClipMaxPref}]
    $itk_component(ged) bounds_all "-1.0 1.0 -1.0 1.0 -$near $far"
    $itk_component(ged) refresh_all
}

::itcl::body ArcherCore::calculateZClipMax {} {
    set size [$itk_component(ged) size]
    set autoview_l [$itk_component(ged) get_autoview]
    set asize [lindex $autoview_l end]

    set max [expr {($asize / $size) * 0.5}]
    set maxSq [expr {$max * $max}]

    # set mZClipMaxPref to the length of the diagonal
    set mZClipMaxPref [expr {sqrt($maxSq + $maxSq)}]

    updateZClipPlanes 0
}

::itcl::body ArcherCore::pushZClipSettings {} {
    set mZClipMaxPref $mZClipMax
    set mZClipBackPref $mZClipBack
    set mZClipFrontPref $mZClipFront
    updateDisplaySettings
}

::itcl::body ArcherCore::shootRay_doit {_start _op _target _prep _no_bool _onehit _bot_dflag _objects} {
    eval $itk_component(ged) rt_gettrees ray -i -u $_objects
    ray prep $_prep
    ray set no_bool $_no_bool
    ray set onehit $_onehit
    ray set bot_reverse_normal_disabled $_bot_dflag

    return [ray shootray $_start $_op $_target]
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
    eval gedWrapper attr 0 0 1 2 $args
}

::itcl::body ArcherCore::bb {args} {
    eval gedWrapper bb 0 0 1 2 $args
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

::itcl::body ArcherCore::bot {args} {
    eval gedWrapper bot 0 0 1 1 $args
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
    eval gedWrapper bot_split 0 0 1 2 $args
}

::itcl::body ArcherCore::bot_sync {args} {
    eval gedWrapper bot_sync 1 0 1 0 $args
}

::itcl::body ArcherCore::bot_vertex_fuse {args} {
    eval gedWrapper bot_vertex_fuse 0 0 1 1 $args
}

::itcl::body ArcherCore::c {args} {
    eval gedWrapper c 0 1 1 2 $args
}

::itcl::body ArcherCore::cd {args} {
    eval ::cd $args
}

::itcl::body ArcherCore::clear {args} {
    $itk_component(cmd) clear
}

::itcl::body ArcherCore::clone {args} {
    eval gedWrapper clone 0 0 1 2 $args
}

::itcl::body ArcherCore::closedb {args} {
    Load ""

    if {[llength $args] != 0} {
        return "Usage: closedb\nWarning - ignored unsupported argument(s) \"$args\""
    }
}

::itcl::body ArcherCore::color {args} {
    eval gedWrapper color 0 0 1 0 $args
}

::itcl::body ArcherCore::comb {args} {
    eval gedWrapper comb 0 1 1 2 $args
}

::itcl::body ArcherCore::comb_color {args} {
    eval gedWrapper comb_color 0 1 1 1 $args
}

::itcl::body ArcherCore::combmem {args} {
    eval gedWrapper combmem 0 1 1 1 $args
}

::itcl::body ArcherCore::copy {args} {
    eval gedWrapper cp 0 0 1 2 $args
}

::itcl::body ArcherCore::copyeval {args} {
    eval gedWrapper copyeval 0 0 1 2 $args
}

::itcl::body ArcherCore::copymat {args} {
    eval gedWrapper copymat 0 0 1 0 $args
}

::itcl::body ArcherCore::cp {args} {
    eval gedWrapper cp 0 0 1 2 $args
}

::itcl::body ArcherCore::cpi {args} {
    eval gedWrapper cpi 0 0 1 2 $args
}

::itcl::body ArcherCore::dbconcat {args} {
    eval gedWrapper dbconcat 0 0 1 2 $args
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
    eval gedWrapper kill 1 0 1 2 $args
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
	updateTreeDrawLists
	SetNormalCursor $this

	return $ret
    }

    gedCmd configure -primitiveLabels {}
    updateTreeDrawLists
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
    eval gedWrapper edcomb 0 0 1 2 $args
}

::itcl::body ArcherCore::edit {args} {
    eval gedWrapper edit 0 0 1 0 $args

    #FIXME: not right at all; we need to redraw all edited objects
    #set len [llength $args]
    #if {$len > 2} {
	#redrawObj [lindex $args end] 0
    #}
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
    set tobjects {}
    foreach obj $objects {
	lappend tobjects [regsub {^/} $obj ""]
    }

    if {[catch {eval gedCmd erase $options $tobjects} ret]} {
	gedCmd configure -primitiveLabels {}
	updateTreeDrawLists
	SetNormalCursor $this

	return $ret
    }

    gedCmd configure -primitiveLabels {}
    updateTreeDrawLists
    SetNormalCursor $this
}

::itcl::body ArcherCore::ev {args} {
    eval gedWrapper ev 1 0 0 1 $args
}

::itcl::body ArcherCore::exit {args} {
    Close
}

::itcl::body ArcherCore::facetize {args} {
    eval gedWrapper facetize 0 0 1 2 $args
}

::itcl::body ArcherCore::fracture {args} {
    eval gedWrapper fracture 0 1 1 2 $args
}

::itcl::body ArcherCore::g {args} {
    eval group $args
}

::itcl::body ArcherCore::group {args} {
    eval gedWrapper g 0 1 1 2 $args
}

::itcl::body ArcherCore::hide {args} {
    eval gedWrapper hide 0 0 1 2 $args
}

::itcl::body ArcherCore::human {args} {
    eval gedWrapper human 0 0 1 2 $args
}


::itcl::body ArcherCore::i {args} {
    eval gedWrapper i 0 1 1 1 $args
}

::itcl::body ArcherCore::importFg4Section {args} {
    eval gedWrapper importFg4Section 0 0 1 1 $args
}

::itcl::body ArcherCore::in {args} {
    eval gedWrapper in 0 0 1 2 $args
}

::itcl::body ArcherCore::inside {args} {
    eval gedWrapper inside 0 0 1 2 $args
}

::itcl::body ArcherCore::item {args} {
    eval gedWrapper item 0 0 1 0 $args
}

::itcl::body ArcherCore::kill {args} {
    eval gedWrapper kill 1 0 1 2 $args
}

::itcl::body ArcherCore::killall {args} {
    eval gedWrapper killall 1 0 1 2 $args
}

::itcl::body ArcherCore::killrefs {args} {
    eval gedWrapper killrefs 1 0 1 2 $args
}

::itcl::body ArcherCore::killtree {args} {
    eval gedWrapper killtree 1 0 1 2 $args
}

::itcl::body ArcherCore::ls {args} {
    eval gedWrapper ls 1 0 0 0 $args
}

::itcl::body ArcherCore::make {args} {
    eval gedWrapper make 0 0 1 2 $args
}

::itcl::body ArcherCore::make_bb {args} {
    eval gedWrapper make_bb 0 0 1 1 $args
}

::itcl::body ArcherCore::make_pnts {args} {
    eval gedWrapper make_pnts 0 1 1 1 $args
}

::itcl::body ArcherCore::man {args} {
    set archerMan $itk_interior.archerMan

    set len [llength $args]
    if {$len != 0 && $len != 1} {
	return "Usage: man cmdName"
    }

    if {$args != {}} {
	set page $args
        if {![$archerMan select $page]} {
            error "couldn't find manual page \"$page\""
        }
    }
    $archerMan activate
}

::itcl::body ArcherCore::mater {args} {
    eval gedWrapper mater 0 1 1 1 $args
}

::itcl::body ArcherCore::mirror {args} {
    eval gedWrapper mirror 0 0 1 2 $args
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
    eval gedWrapper mv 0 0 1 2 $args
}

::itcl::body ArcherCore::mvall {args} {
    eval gedWrapper mvall 0 0 1 2 $args
}

::itcl::body ArcherCore::nmg_collapse {args} {
    eval gedWrapper nmg_collapse 0 0 1 2 $args
}

::itcl::body ArcherCore::nmg_simplify {args} {
    eval gedWrapper nmg_simplify 0 0 1 2 $args
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

::itcl::body ArcherCore::opendb {args} {
    set ret ""

    switch [llength $args] {
        0 {set ret $mTarget}
        1 {Load [lindex $args 0]}
        default {set ret "Usage: opendb \[database.g\]"}
    }

    return $ret
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
    eval gedWrapper put 0 0 1 2 $args
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

::itcl::body ArcherCore::q {args} {
    Close
}

::itcl::body ArcherCore::quit {args} {
    Close
}

::itcl::body ArcherCore::r {args} {
    eval gedWrapper r 0 1 1 2 $args
}

::itcl::body ArcherCore::rcodes {args} {
    eval gedWrapper rcodes 0 0 1 0 $args
}

::itcl::body ArcherCore::red {args} {
    eval gedWrapper red 0 0 1 2 $args
}

::itcl::body ArcherCore::rfarb {args} {
    eval gedWrapper rfarb 0 0 1 1 $args
}

::itcl::body ArcherCore::rm {args} {
    eval gedWrapper rm 0 0 1 2 $args
}

::itcl::body ArcherCore::rmater {args} {
    eval gedWrapper rmater 0 0 1 1 $args
}

::itcl::body ArcherCore::rotate {args} {
    set args [linsert $args 0 "rotate"]
    eval gedWrapper edit 0 0 1 0 $args
}

::itcl::body ArcherCore::rotate_arb_face {args} {
    eval gedWrapper rotate_arb_face 0 0 1 0 $args
}

::itcl::body ArcherCore::scale {args} {
    set args [linsert $args 0 "scale"]
    eval gedWrapper edit 0 0 1 0 $args
}

::itcl::body ArcherCore::search {args} {
    if {$args == {}} {
	return [gedCmd search]
    } else {
	return [eval gedCmd search $args]
    }
}

::itcl::body ArcherCore::sed {_prim} {
    if {$_prim == ""} {
	return "Usage: sed prim"
    }

    set paths [gedCmd search / -name $_prim]
#    $itk_component(tree) selectpaths $paths

#    $itk_component(tree) selectitem $_prim
}

::itcl::body ArcherCore::shader {args} {
    eval gedWrapper shader 0 0 1 0 $args
}

::itcl::body ArcherCore::shells {args} {
    eval gedWrapper shells 0 0 1 1 $args
}

::itcl::body ArcherCore::tire {args} {
    eval gedWrapper tire 0 0 1 2 $args
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

::itcl::body ArcherCore::translate {args} {
    set args [linsert $args 0 "translate"]
    eval gedWrapper edit 0 0 1 0 $args
}

::itcl::body ArcherCore::unhide {args} {
    eval gedWrapper unhide 0 0 1 2 $args
}

::itcl::body ArcherCore::units {args} {
    if {$args == {}} {
	return [gedCmd units]
    }

    set arg0 [lindex $args 0]
    if {[llength $args] == 1 && ($arg0 == "-s" || $arg0 == "-t")} {
	return [gedCmd units $arg0]
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
    eval gedWrapper vmake 0 0 1 2 $args
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

::itcl::body ArcherCore::centerDialogOverPane {_dialog} {
    if {$mCurrentPaneName == ""} {
	set pane $mActivePaneName
    } else {
	set pane $mCurrentPaneName
    }

    set mCurrentPaneName ""
    $_dialog center [$itk_component(ged) pane_win_name $pane]

    return $pane
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
	mFBBackgroundColor {
	    $itk_component(rtcntrl) configure -color [cadwidgets::Ged::get_rgb_color $mFBBackgroundColor]
	}
	mDisplayFontSize {
	    $itk_component(ged) fontsize $mDisplayFontSize
	}
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
