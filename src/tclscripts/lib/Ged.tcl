#                          G E D . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
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
# Description -
#	The Ged class wraps LIBTCLCAD's Geometry Edit (GED) object.
#

option add *Pane*margin 0 widgetDefault
option add *Ged.width 400 widgetDefault
option add *Ged.height 400 widgetDefault

package provide cadwidgets::Ged 1.0

::itcl::class cadwidgets::Ged {
    inherit iwidgets::Panedwindow

    #XXX Temporarily needed by Archer
    itk_option define -autoViewEnable autoViewEnable AutoViewEnable 1
    itk_option define -usePhony usePhony UsePhony 0
    itk_option define -rscale rscale Rscale 0.4
    itk_option define -sscale sscale Sscale 2.0

    itk_option define -pane pane Pane ur
    itk_option define -multi_pane multi_pane Multi_Pane 1
    itk_option define -paneCallback paneCallback PaneCallback ""

    itk_option define -bg bg Bg {0 0 0}
    itk_option define -debug debug Debug 0
    itk_option define -depthMask depthMask DepthMask 1
    itk_option define -light light Light 0
    itk_option define -linestyle linestyle Linestyle 0
    itk_option define -linewidth linewidth Linewidth 1
    itk_option define -perspective perspective Perspective 0
    itk_option define -transparency transparency Transparency 0
    itk_option define -type type Type wgl
    itk_option define -zbuffer zbuffer Zbuffer 0
    itk_option define -zclip zclip Zclip 0

    itk_option define -modelAxesColor modelAxesColor AxesColor White
    itk_option define -modelAxesEnable modelAxesEnable AxesEnable 0
    itk_option define -modelAxesLabelColor modelAxesLabelColor AxesLabelColor Yellow
    itk_option define -modelAxesLineWidth modelAxesLineWidth AxesLineWidth 0
    itk_option define -modelAxesPosition modelAxesPosition AxesPosition {0 0 0}
    itk_option define -modelAxesSize modelAxesSize AxesSize 2.0
    itk_option define -modelAxesTripleColor modelAxesTripleColor AxesTripleColor 0

    itk_option define -modelAxesTickColor modelAxesTickColor AxesTickColor Yellow
    itk_option define -modelAxesTickEnable modelAxesTickEnable AxesTickEnable 1
    itk_option define -modelAxesTickInterval modelAxesTickInterval AxesTickInterval 100
    itk_option define -modelAxesTickLength modelAxesTickLength AxesTickLength 4
    itk_option define -modelAxesTickMajorColor modelAxesTickMajorColor AxesTickMajorColor Red
    itk_option define -modelAxesTickMajorLength modelAxesTickMajorLength AxesTickMajorLength 8
    itk_option define -modelAxesTicksPerMajor modelAxesTicksPerMajor AxesTicksPerMajor 10
    itk_option define -modelAxesTickThreshold modelAxesTickThreshold AxesTickThreshold 8

    itk_option define -viewAxesColor viewAxesColor AxesColor White
    itk_option define -viewAxesEnable viewAxesEnable AxesEnable 0
    itk_option define -viewAxesLabelColor viewAxesLabelColor AxesLabelColor Yellow
    itk_option define -viewAxesLineWidth viewAxesLineWidth AxesLineWidth 0
    itk_option define -viewAxesPosition viewAxesPosition AxesPosition {-0.85 -0.85 0}
    itk_option define -viewAxesPosOnly viewAxesPosOnly AxesPosOnly 1
    itk_option define -viewAxesSize viewAxesSize AxesSize 0.2
    itk_option define -viewAxesTripleColor viewAxesTripleColor AxesTripleColor 1

    itk_option define -adcEnable adcEnable AdcEnable 0
    itk_option define -centerDotColor centerDotColor CenterDotColor Yellow
    itk_option define -centerDotEnable centerDotEnable CenterDotEnable 1
    itk_option define -gridEnable gridEnable GridEnable 0
    itk_option define -gridSnap gridSnap GridSnap 0
    itk_option define -measuringStickColor measuringStickColor MeasuringStickColor Yellow
    itk_option define -measuringStickMode measuringStickMode MeasuringStickMode 0
    itk_option define -primitiveLabelColor primitiveLabelColor PrimitiveLabelColor Yellow
    itk_option define -primitiveLabels primitiveLabels PrimitiveLabels {}
    itk_option define -scaleColor scaleColor ScaleColor Yellow
    itk_option define -scaleEnable scaleEnable ScaleEnable 0
    itk_option define -showViewingParams showViewingParams ShowViewingParams 0
    itk_option define -viewingParamsColor viewingParamsColor ViewingParamsColor Yellow

    constructor {_gedOrFile args} {}
    destructor {}

    public {
	common MEASURING_STICK "cad_measuring_stick"

	variable mGedFile ""

	method 3ptarb {args}
	method adc {args}
 	method adjust {args}
	method ae {args}
	method ae2dir {args}
	method aet {args}
	method analyze {args}
	method arb {args}
	method arced {args}
	method arot {args}
	method attr {args}
	method autoview {args}
	method autoview_all {args}
	method base2local {}
	method bb {args}
	method bev {args}
	method bg {args}
	method bg_all {args}
	method blast {args}
	method bo {args}
	method bot {args}
	method bot_condense {args}
	method bot_decimate {args}
	method bot_dump {args}
	method bot_face_fuse {args}
	method bot_face_sort {args}
	method bot_flip {args}
	method bot_merge {args}
	method bot_smooth {args}
	method bot_split {args}
	method bot_sync {args}
	method bot_vertex_fuse {args}
	method bounds {args}
	method bounds_all {args}
	method c {args}
	method cat {args}
	method center {args}
	method clear {args}
	method clone {args}
	method color {args}
	method comb {args}
	method comb_color {args}
	method combmem {args}
	method configure_win {args}
	method constrain_rmode {args}
	method constrain_tmode {args}
	method copyeval {args}
	method copymat {args}
	method cp {args}
	method cpi {args}
	method data_arrows {args}
	method data_axes {args}
	method data_labels {args}
	method data_lines {args}
	method data_move {args}
	method data_pick {args}
	method dbconcat {args}
	method dbfind {args}
	method dbip {args}
	method dbot_dump {args}
	method decompose {args}
	method delay {args}
	method dir2ae {args}
	method draw {args}
	method dump {args}
	method dup {args}
	method E {args}
	method eac {args}
	method echo {args}
	method edcodes {args}
	method edcolor {args}
	method edcomb {args}
	method edit {args}
	method edmater {args}
	method erase {args}
	method ev {args}
	method expand {args}
	method eye {args}
	method eye_pos {args}
	method exists {args}
	method faceplate {args}
	method facetize {args}
	method fb2pix {args}
	method find_pipept {args}
	method fontsize {args}
	method form {args}
	method fracture {args}
	method g {args}
	method get {args}
	method get_autoview {args}
	method get_comb {args}
	method get_eyemodel {args}
	method get_type {args}
	method glob {args}
	method gqa {args}
	method grid {args}
	method handle_expose {args}
	method hide {args}
	method how {args}
	method human {args}
	method i {args}
	method idents {args}
	method idle_mode {args}
	method illum {args}
	method importFg4Section {args}
	method in {args}
	method inside {args}
	method isize {args}
	method item {args}
	method keep {args}
	method keypoint {args}
	method kill {args}
	method killall {args}
	method killrefs {args}
	method killtree {args}
	method l {args}
	method lastMouseRayPos {}
	method light {args}
	method light_all {args}
	method list_views {args}
	method listen {args}
	method listeval {args}
	method loadview {args}
	method local2base {}
	method log {args}
	method lookat {args}
	method ls {args}
	method lt {args}
	method m2v_point {args}
	method make {args}
	method make_bb {name args}
	method make_name {args}
	method make_pnts {args}
	method match {args}
	method mater {args}
	method mirror {args}
	method model2view {args}
	method model_axes {args}
	method more_args_callback {args}
	method mouse_append_pipept {args}
	method mouse_constrain_rot {args}
	method mouse_constrain_trans {args}
	method mouse_find_pipept {args}
	method mouse_move_arb_edge {args}
	method mouse_move_arb_face {args}
	method mouse_move_pipept {args}
	method mouse_orotate {args}
	method mouse_oscale {args}
	method mouse_otranslate {args}
	method mouse_prepend_pipept {args}
	method mouse_rect {args}
	method mouse_rot {args}
	method mouse_rotate_arb_face {args}
	method mouse_scale {args}
	method mouse_protate {args}
	method mouse_pscale {args}
	method mouse_ptranslate {args}
	method mouse_trans {args}
	method move_arb_edge {args}
	method move_arb_edge_mode {args}
	method move_arb_face {args}
	method move_arb_face_mode {args}
	method move_pipept {args}
	method move_pipept_mode {args}
	method mv {args}
	method mvall {args}
	method nirt {args}
	method nmg_collapse {args}
	method nmg_simplify {args}
	method ocenter {args}
	method open {args}
	method opendb {args}
	method orient {args}
	method orotate {args}
	method orotate_mode {args}
	method oscale {args}
	method oscale_mode {args}
	method otranslate {args}
	method otranslate_mode {args}
	method overlay {args}
	method paint_rect_area {args}
	method pane_adc {_pane args}
	method pane_ae {_pane args}
	method pane_aet {_pane args}
	method pane_arot {_pane args}
	method pane_autoview {_pane args}
	method pane_center {_pane args}
	method pane_constrain_rmode {_pane args}
	method pane_constrain_tmode {_pane args}
	method pane_eye {_pane args}
	method pane_eye_pos {_pane args}
	method pane_fb2pix {_pane args}
	method pane_find_pipept {args}
	method pane_fontsize {_pane args}
	method pane_get_eyemodel {_pane args}
	method pane_grid {_pane args}
	method pane_idle_mode {_pane args}
	method pane_isize {_pane args}
	method pane_keypoint {_pane args}
	method pane_light {_pane args}
	method pane_listen {_pane args}
	method pane_loadview {_pane args}
	method pane_lookat {_pane args}
	method pane_m2v_point {_pane args}
	method pane_model2view {_pane args}
	method pane_move_arb_edge_mode {_pane args}
	method pane_move_arb_face_mode {_pane args}
	method pane_move_pipept_mode {_pane args}
	method pane_mouse_append_pipept {_pane args}
	method pane_mouse_constrain_rot {_pane args}
	method pane_mouse_constrain_trans {_pane args}
	method pane_mouse_find_pipept {_pane args}
	method pane_mouse_move_arb_edge {_pane args}
	method pane_mouse_move_arb_face {_pane args}
	method pane_mouse_move_pipept {_pane args}
	method pane_mouse_orotate {_pane args}
	method pane_mouse_oscale {_pane args}
	method pane_mouse_otranslate {_pane args}
	method pane_mouse_prepend_pipept {_pane args}
	method pane_mouse_rect {_pane args}
	method pane_mouse_rot {_pane args}
	method pane_mouse_rotate_arb_face {_pane args}
	method pane_mouse_scale {_pane args}
	method pane_mouse_protate {_pane args}
	method pane_mouse_pscale {_pane args}
	method pane_mouse_ptranslate {_pane args}
	method pane_mouse_trans {_pane args}
	method pane_nirt {_pane args}
	method pane_orient {_pane args}
	method pane_orotate_mode {_pane args}
	method pane_oscale_mode {_pane args}
	method pane_otranslate_mode {_pane args}
	method pane_paint_rect_area {_pane args}
	method pane_perspective {_pane args}
	method pane_pix2fb {_pane args}
	method pane_plot {_pane args}
	method pane_pmat {_pane args}
	method pane_pmodel2view {_pane args}
	method pane_png {_pane args}
	method pane_pngwf {_pane args}
	method pane_pov {_pane args}
	method pane_preview {_pane args}
	method pane_protate_mode {_pane args}
	method pane_ps {_pane args}
	method pane_pscale_mode {_pane args}
	method pane_ptranslate_mode {_pane args}
	method pane_quat {_pane args}
	method pane_qvrot {_pane args}
	method pane_rect {_pane args}
	method pane_rect_mode {_pane args}
	method pane_refresh {_pane args}
	method pane_rmat {_pane args}
	method pane_rot {_pane args}
	method pane_rot_about {_pane args}
	method pane_rot_point {_pane args}
	method pane_rotate_mode {_pane args}
	method pane_rotate_arb_face_mode {_pane args}
	method pane_rrt {_pane args}
	method pane_rt {_pane args}
	method pane_rtarea {_pane args}
	method pane_rtcheck {_pane args}
	method pane_rtedge {_pane args}
	method pane_rtweight {_pane args}
	method pane_savekey {_pane args}
	method pane_saveview {_pane args}
	method pane_sca {_pane args}
	method pane_screengrab {_pane args}
	method pane_scale_mode {_pane args}
	method pane_screen2view {args}
	method pane_set_coord {_pane args}
	method pane_set_fb_mode {_pane args}
	method pane_setview {_pane args}
	method pane_size {_pane args}
	method pane_slew {_pane args}
	method pane_snap_view {_pane args}
	method pane_tra {_pane args}
	method pane_translate_mode {_pane args}
	method pane_v2m_point {_pane args}
	method pane_view {_pane args}
	method pane_view2model {_pane args}
	method pane_view_callback {_pane args}
	method pane_viewdir {_pane args}
	method pane_vmake {_pane args}
	method pane_vnirt {_pane args}
	method pane_vslew {_pane args}
	method pane_ypr {_pane args}
	method pane_zbuffer {_pane args}
	method pane_zclip {_pane args}
	method pane_zoom {_pane args}
	method pane_win_name {_pane}
	method pane_win_size {_pane args}
	method pathlist {args}
	method paths {args}
	method perspective {args}
	method pix2fb {args}
	method plot {args}
	method pmat {args}
	method pmodel2view {args}
	method png {args}
	method pngwf {args}
	method pov {args}
	method prcolor {args}
	method prefix {args}
	method preview {args}
	method prim_label {args}
	method ps {args}
	method push {args}
	method put {args}
	method put_comb {args}
	method putmat {args}
	method qray {args}
	method quat {args}
	method qvrot {args}
	method r {args}
	method rcodes {args}
	method rect {args}
	method rect_mode {args}
	method red {args}
	method redraw {args}
	method refresh {args}
	method refresh_all {args}
	method refresh_off {}
	method refresh_on {}
	method regdef {args}
	method regions {args}
	method report {args}
	method rfarb {args}
	method rm {args}
	method rmap {args}
	method rmat {args}
	method rmater {args}
	method rot {args}
	method rot_about {args}
	method rot_point {args}
	method rotate {args}
	method rotate_arb_face {args}
	method rotate_arb_face_mode {args}
	method rotate_mode {args}
	method rrt {args}
	method rselect {args}
	method rt {args}
	method rt_end_callback {args}
	method rt_gettrees {args}
	method rtabort {args}
	method rtarea {args}
	method rtcheck {args}
	method rtedge {args}
	method rtweight {args}
	method savekey {args}
	method saveview {args}
	method sca {args}
	method screengrab {args}
	method protate {args}
	method protate_mode {args}
	method pscale {args}
	method pscale_mode {args}
	method ptranslate {args}
	method ptranslate_mode {args}
	method scale {args}
	method scale_mode {args}
	method screen2view {args}
	method sdata_arrows {args}
	method sdata_axes {args}
	method sdata_labels {args}
	method sdata_lines {args}
	method search {args}
	method select {args}
	method set_coord {args}
	method set_fb_mode {args}
	method set_output_script {args}
	method set_transparency {args}
	method set_uplotOutputMode {args}
	method setview {args}
	method shaded_mode {args}
	method shader {args}
	method shareGed {_ged}
	method shells {args}
	method showmats {args}
	method size {args}
	method slew {args}
	method snap_view {args}
	method solids {args}
	method solids_on_ray {args}
	method summary {args}
	method sync {args}
	method tire {args}
	method title {args}
	method tol {args}
	method tops {args}
	method tra {args}
	method track {args}
	method translate {args}
	method translate_mode {args}
	method transparency {args}
	method transparency_all {args}
	method tree {args}
	method unhide {args}
	method units {args}
	method v2m_point {args}
	method vdraw {args}
	method version {args}
	method view {args}
	method view2model {args}
	method view_axes {args}
	method view_callback {args}
	method view_callback_all {args}
	method viewdir {args}
	method vmake {args}
	method vnirt {args}
	method vslew {args}
	method wcodes {args}
	method whatid {args}
	method which_shader {args}
	method whichair {args}
	method whichid {args}
	method who {args}
	method win_size {args}
	method wmater {args}
	method xpush {args}
	method ypr {args}
	method zap {args}
	method zbuffer {args}
	method zbuffer_all {args}
	method zclip {args}
	method zclip_all {args}
	method zoom {args}

	method ? {}
	method apropos {args}
	method begin_data_arrow {_pane _x _y}
	method begin_data_line {_pane _x _y}
	method begin_data_move {_pane _x _y}
	method begin_view_measure {_pane _part1_button _part1_button _x _y}
	method begin_view_measure_part2 {_pane _button _x _y}
	method default_views {}
	method delete_pipept {args}
	method end_data_arrow {_pane}
	method end_data_line {_pane}
	method end_data_move {_pane}
	method end_view_measure {_pane _part1_button _part2_button}
	method end_view_measure_part2 {_pane _button}
	method end_view_rect {_pane {_button 1}}
	method end_view_rotate {_pane}
	method end_view_scale {_pane}
	method end_view_translate {_pane}
	method getUserCmds {}
	method handle_data_move {_pane _dtype _dindex _x _y}
	method handle_view_measure {_pane _x _y}
	method handle_view_measure_part2 {_pane _x _y}
	method help {args}
	method history_callback {args}
	method init_append_pipept {_obj {_button 1} {_callback {}}}
	method init_button_no_op {{_button 1}}
	method init_comp_pick {{_button 1}}
	method init_data_arrow {{_button 1}}
	method init_data_label {{_button 1}}
	method init_data_line {{_button 1}}
	method init_data_move {{_button 1}}
	method init_data_pick {{_button 1}}
	method init_find_pipept {_obj {_button 1} {_callback {}}}
	method init_prepend_pipept {_obj {_button 1} {_callback {}}}
	method init_view_bindings {{_type default}}
	method init_view_center {{_button 1}}
	method init_view_measure {{_button 1} {_part2_button 2}}
	method init_view_measure_part2 {_button}
	method init_view_rect {{_button 1}}
	method init_view_rotate {{_button 1}}
	method init_view_scale {{_button 1}}
	method init_view_translate {{_button 1}}
	method center_ray {{_pflag 0}}
	method mouse_ray {_x _y {_pflag 0}}
	method pane_mouse_3dpoint {_pane _x _y {_vflag 1}}
	method pane_mouse_data_label {_pane _x _y}
	method pane_mouse_data_pick {_pane _x _y}
	method pane_mouse_ray {_pane _x _y {_pflag 0}}
	method pane {args}
	method shoot_ray {_start _op _target _prep _no_bool _onehit}

	method add_begin_data_arrow_callback {_callback}
	method clear_begin_data_arrow_callback_list {}
	method delete_begin_data_arrow_callback {_callback}

	method add_end_data_arrow_callback {_callback}
	method clear_end_data_arrow_callback_list {}
	method delete_end_data_arrow_callback {_callback}

	method add_begin_data_line_callback {_callback}
	method clear_begin_data_line_callback_list {}
	method delete_begin_data_line_callback {_callback}

	method add_end_data_line_callback {_callback}
	method clear_end_data_line_callback_list {}
	method delete_end_data_line_callback {_callback}

	method add_begin_data_move_callback {_callback}
	method clear_begin_data_move_callback_list {}
	method delete_begin_data_move_callback {_callback}

	method add_end_data_move_callback {_callback}
	method clear_end_data_move_callback_list {}
	method delete_end_data_move_callback {_callback}

	method add_mouse_data_callback {_callback}
	method clear_mouse_data_callback_list {}
	method delete_mouse_data_callback {_callback}

	method add_mouse_ray_callback {_callback}
	method clear_mouse_ray_callback_list {}
	method delete_mouse_ray_callback {_callback}

	method add_data_label_callback {_callback}
	method clear_data_label_callback_list {}
	method delete_data_label_callback {_callback}

	method add_data_move_callback {_callback}
	method clear_data_move_callback_list {}
	method delete_data_move_callback {_callback}

	method add_view_measure_callback {_callback}
	method clear_view_measure_callback_list {}
	method delete_view_measure_callback {_callback}

	method add_view_rect_callback {_callback}
	method clear_view_rect_callback_list {}
	method delete_view_rect_callback {_callback}

	method set_data_point_callback {_callback}
 
	#XXX Still needs to be resolved
	method set_outputHandler {args}
	method fb_active {args}

	proc get_ged_color {_color}
	proc get_rgb_color {_color}
	proc get_vdraw_color {_color}
	proc rgb_to_tk {_r _g _b}
	proc tk_to_rgb {_tkcolor}
    }

    protected {
	variable mGed ""
	variable mSharedGed 0
	variable mHistoryCallback ""
	variable mPipePointCallback ""
	variable mMeasuringStickColorVDraw2D ff00ff
	variable mMeasuringStickColorVDraw3D ffff00
	variable mMeasuringStick3D 1
	variable mMeasuringStick3DCurrent 1
	variable mRefreshOn 1
	variable mLastDataType ""
	variable mLastDataIndex ""
	variable mLastMouseRayPos ""
	variable mLastMouseRayStart ""
	variable mLastMouseRayTarget ""
	variable mLastMousePos ""
	variable mBegin3DPoint ""
	variable mMiddle3DPoint ""
	variable mEnd3DPoint ""
	variable mMeasureLineActive 0

	variable mBeginDataArrowCallbacks ""
	variable mBeginDataLineCallbacks ""
	variable mBeginDataMoveCallbacks ""
	variable mDataLabelCallbacks ""
	variable mDataMoveCallbacks ""
	variable mDataPointCallback ""
	variable mEndDataArrowCallbacks ""
	variable mEndDataLineCallbacks ""
	variable mEndDataMoveCallbacks ""
	variable mMouseDataCallbacks ""
	variable mMouseRayCallbacks ""
	variable mViewMeasureCallbacks ""
	variable mViewRectCallbacks ""

	variable mRay "ray"
	variable mRayCurrWho ""
	variable mRayLastWho ""

	method init_button_no_op_prot {{_button 1}}
	method measure_line_erase {}
	method multi_pane {args}
	method new_view {args}
	method toggle_multi_pane {}
    }

    private {
	variable mPrivPane ur
	variable mPrivMultiPane 1
	variable help

	method help_init {}
    }
}


::itcl::body cadwidgets::Ged::constructor {_gedOrFile args} {
    global tcl_platform

    if {[catch {$_gedOrFile ls}]} {
	set mGedFile $_gedOrFile
	set mGed [subst $this]_ged
	go_open $mGed db $mGedFile
    } else {
	set mGedFile ""
	set mGed $_gedOrFile
	set mSharedGed 1
    }
    iwidgets::Panedwindow::add upper
    iwidgets::Panedwindow::add lower

    # create two more panedwindows
    itk_component add upw {
	::iwidgets::Panedwindow [childsite upper].pw -orient vertical
    } {
	usual
	keep -sashwidth -sashheight
	keep -sashborderwidth -sashindent
	keep -thickness -showhandle
	rename -sashcursor -hsashcursor hsashcursor HSashCursor
    }

    itk_component add lpw {
	::iwidgets::Panedwindow [childsite lower].pw -orient vertical
    } {
	usual
	keep -sashwidth -sashheight
	keep -sashborderwidth -sashindent
	keep -thickness -showhandle
	rename -sashcursor -hsashcursor hsashcursor HSashCursor
    }

    $itk_component(upw) add ulp
    $itk_component(upw) add urp
    $itk_component(lpw) add llp
    $itk_component(lpw) add lrp

    if {$tcl_platform(platform) == "windows"} {
	set dmType wgl
    } else {
	set dmType ogl
    }

    # create four views
    itk_component add ul {
	new_view [$itk_component(upw) childsite ulp].view $dmType -t 0
    } {}

    itk_component add ur {
	new_view [$itk_component(upw) childsite urp].view $dmType -t 0
    } {}

    itk_component add ll {
	new_view [$itk_component(lpw) childsite llp].view $dmType -t 0
    } {}

    itk_component add lr {
	new_view [$itk_component(lpw) childsite lrp].view $dmType -t 0
    } {}

    # initialize the views
    default_views

    foreach dm {ur ul ll lr} {
	pack $itk_component($dm) -fill both -expand yes
    }

    pack $itk_component(upw) -fill both -expand yes
    pack $itk_component(lpw) -fill both -expand yes

    catch {eval itk_initialize $args}

    cadwidgets::Ged::help_init
}

::itcl::body cadwidgets::Ged::destructor {} {
    if {!$mSharedGed} {
	catch {rename $mRay ""}
	rename $mGed ""
    }
}



############################### Configuration Options ###############################

::itcl::configbody cadwidgets::Ged::centerDotEnable {
    eval faceplate center_dot draw $itk_option(-centerDotEnable)
}

::itcl::configbody cadwidgets::Ged::adcEnable {
    adc draw $itk_option(-adcEnable)
}

::itcl::configbody cadwidgets::Ged::gridEnable {
    grid draw $itk_option(-gridEnable)
}

::itcl::configbody cadwidgets::Ged::gridSnap {
    grid snap $itk_option(-gridSnap)
}

::itcl::configbody cadwidgets::Ged::mGedFile {
    cadwidgets::Ged::open $mGedFile
}

::itcl::configbody cadwidgets::Ged::modelAxesColor {
    eval model_axes axes_color [get_rgb_color $itk_option(-modelAxesColor)]
}

::itcl::configbody cadwidgets::Ged::modelAxesEnable {
    model_axes draw $itk_option(-modelAxesEnable)
}

::itcl::configbody cadwidgets::Ged::modelAxesLabelColor {
    eval model_axes label_color [get_rgb_color $itk_option(-modelAxesLabelColor)]
}

::itcl::configbody cadwidgets::Ged::modelAxesLineWidth {
    model_axes line_width $itk_option(-modelAxesLineWidth)
}

::itcl::configbody cadwidgets::Ged::modelAxesPosition {
    eval model_axes axes_pos $itk_option(-modelAxesPosition)
}

::itcl::configbody cadwidgets::Ged::modelAxesSize {
    model_axes axes_size $itk_option(-modelAxesSize)
}

::itcl::configbody cadwidgets::Ged::modelAxesTickColor {
    eval model_axes tick_color [get_rgb_color $itk_option(-modelAxesTickColor)]
}

::itcl::configbody cadwidgets::Ged::modelAxesTickEnable {
    model_axes tick_enable $itk_option(-modelAxesTickEnable)
}

::itcl::configbody cadwidgets::Ged::modelAxesTickInterval {
    model_axes tick_interval $itk_option(-modelAxesTickInterval)
}

::itcl::configbody cadwidgets::Ged::modelAxesTickLength {
    model_axes tick_length $itk_option(-modelAxesTickLength)
}

::itcl::configbody cadwidgets::Ged::modelAxesTickMajorColor {
    eval model_axes tick_major_color [get_rgb_color $itk_option(-modelAxesTickMajorColor)]
}

::itcl::configbody cadwidgets::Ged::modelAxesTickMajorLength {
    model_axes tick_major_length $itk_option(-modelAxesTickMajorLength)
}

::itcl::configbody cadwidgets::Ged::modelAxesTicksPerMajor {
    model_axes ticks_per_major $itk_option(-modelAxesTicksPerMajor)
}

::itcl::configbody cadwidgets::Ged::modelAxesTickThreshold {
    model_axes tick_threshold $itk_option(-modelAxesTickThreshold)
}

::itcl::configbody cadwidgets::Ged::modelAxesTripleColor {
    model_axes triple_color $itk_option(-modelAxesTripleColor)
}

::itcl::configbody cadwidgets::Ged::multi_pane {
    multi_pane $itk_option(-multi_pane)
}

::itcl::configbody cadwidgets::Ged::pane {
    pane $itk_option(-pane)
}

::itcl::configbody cadwidgets::Ged::primitiveLabelColor {
    eval faceplate prim_labels color [get_rgb_color $itk_option(-primitiveLabelColor)]
}

::itcl::configbody cadwidgets::Ged::primitiveLabels {
    eval prim_label $itk_option(-primitiveLabels)
}

::itcl::configbody cadwidgets::Ged::scaleColor {
    eval faceplate view_scale color [get_rgb_color $itk_option(-scaleColor)]
}

::itcl::configbody cadwidgets::Ged::scaleEnable {
    faceplate view_scale draw $itk_option(-scaleEnable)
}

::itcl::configbody cadwidgets::Ged::showViewingParams {
    faceplate view_params draw $itk_option(-showViewingParams)
}

::itcl::configbody cadwidgets::Ged::viewAxesColor {
    eval view_axes axes_color [get_rgb_color $itk_option(-viewAxesColor)]
}

::itcl::configbody cadwidgets::Ged::viewAxesEnable {
    view_axes draw $itk_option(-viewAxesEnable)
}

::itcl::configbody cadwidgets::Ged::viewAxesLabelColor {
    eval view_axes label_color [get_rgb_color $itk_option(-viewAxesLabelColor)]
}

::itcl::configbody cadwidgets::Ged::viewAxesLineWidth {
    eval view_axes line_width $itk_option(-viewAxesLineWidth)
}

::itcl::configbody cadwidgets::Ged::viewAxesPosition {
    eval view_axes axes_pos $itk_option(-viewAxesPosition)
}

::itcl::configbody cadwidgets::Ged::viewAxesSize {
    eval view_axes axes_size $itk_option(-viewAxesSize)
}

::itcl::configbody cadwidgets::Ged::viewAxesTripleColor {
    view_axes triple_color $itk_option(-viewAxesTripleColor)
}

::itcl::configbody cadwidgets::Ged::viewingParamsColor {
    eval faceplate view_params color [get_rgb_color $itk_option(-viewingParamsColor)]
}


############################### Public Methods Wrapping Ged Objects ###############################

::itcl::body cadwidgets::Ged::3ptarb {args} {
    eval $mGed 3ptarb $args
}

::itcl::body cadwidgets::Ged::adc {args} {
    eval $mGed adc $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::adjust {args} {
    eval $mGed adjust $args
}

::itcl::body cadwidgets::Ged::ae {args} {
    eval aet $args
}

::itcl::body cadwidgets::Ged::ae2dir {args} {
    eval $mGed ae2dir $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::aet {args} {
    eval $mGed aet $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::analyze {args} {
    eval $mGed analyze $args
}

::itcl::body cadwidgets::Ged::arb {args} {
    eval $mGed arb $args
}

::itcl::body cadwidgets::Ged::arced {args} {
    eval $mGed arced $args
}

::itcl::body cadwidgets::Ged::arot {args} {
    eval $mGed arot $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::attr {args} {
    eval $mGed attr $args
}

::itcl::body cadwidgets::Ged::autoview {args} {
    eval $mGed autoview $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::autoview_all {args} {
    foreach dm {ur ul ll lr} {
	eval $mGed autoview $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::base2local {} {
    eval $mGed base2local
}

::itcl::body cadwidgets::Ged::bb {args} {
    eval $mGed bb $args
}

::itcl::body cadwidgets::Ged::bev {args} {
    eval $mGed bev $args
}

::itcl::body cadwidgets::Ged::bg {args} {
    eval $mGed bg $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::bg_all {args} {
    foreach dm {ur ul ll lr} {
	eval $mGed bg $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::blast {args} {
    eval $mGed blast $args
}

::itcl::body cadwidgets::Ged::bo {args} {
    eval $mGed bo $args
}

::itcl::body cadwidgets::Ged::bot {args} {
    eval $mGed bot $args
}

::itcl::body cadwidgets::Ged::bot_condense {args} {
    eval $mGed bot_condense $args
}

::itcl::body cadwidgets::Ged::bot_decimate {args} {
    eval $mGed bot_decimate $args
}

::itcl::body cadwidgets::Ged::bot_dump {args} {
    eval $mGed bot_dump $args
}

::itcl::body cadwidgets::Ged::bot_face_fuse {args} {
    eval $mGed bot_face_fuse $args
}

::itcl::body cadwidgets::Ged::bot_face_sort {args} {
    eval $mGed bot_face_sort $args
}

::itcl::body cadwidgets::Ged::bot_flip {args} {
    eval $mGed bot_flip $args
}

::itcl::body cadwidgets::Ged::bot_merge {args} {
    eval $mGed bot_merge $args
}

::itcl::body cadwidgets::Ged::bot_smooth {args} {
    eval $mGed bot_smooth $args
}

::itcl::body cadwidgets::Ged::bot_split {args} {
    eval $mGed bot_split $args
}

::itcl::body cadwidgets::Ged::bot_sync {args} {
    eval $mGed bot_sync $args
}

::itcl::body cadwidgets::Ged::bot_vertex_fuse {args} {
    eval $mGed bot_vertex_fuse $args
}

::itcl::body cadwidgets::Ged::bounds {args} {
    eval $mGed bounds $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::bounds_all {args} {
    foreach dm {ur ul ll lr} {
	eval $mGed bounds $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::c {args} {
    eval $mGed c $args
}

::itcl::body cadwidgets::Ged::cat {args} {
    eval $mGed cat $args
}

::itcl::body cadwidgets::Ged::center {args} {
    eval $mGed center $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::clear {args} {
    eval $mGed clear $args
}

::itcl::body cadwidgets::Ged::clone {args} {
    eval $mGed clone $args
}

::itcl::body cadwidgets::Ged::color {args} {
    eval $mGed color $args
}

::itcl::body cadwidgets::Ged::comb {args} {
    eval $mGed comb $args
}

::itcl::body cadwidgets::Ged::comb_color {args} {
    eval $mGed comb_color $args
}

::itcl::body cadwidgets::Ged::combmem {args} {
    eval $mGed combmem $args
}

::itcl::body cadwidgets::Ged::configure_win {args} {
    eval $mGed configure $args
}

::itcl::body cadwidgets::Ged::constrain_rmode {args} {
    eval $mGed constrain_rmode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::constrain_tmode {args} {
    eval $mGed constrain_tmode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::copyeval {args} {
    eval $mGed copyeval $args
}

::itcl::body cadwidgets::Ged::copymat {args} {
    eval $mGed copymat $args
}

::itcl::body cadwidgets::Ged::cp {args} {
    eval $mGed cp $args
}

::itcl::body cadwidgets::Ged::cpi {args} {
    eval $mGed cpi $args
}

::itcl::body cadwidgets::Ged::data_arrows {args} {
    set len [llength $args]
    if {$len < 2} {
	return [eval $mGed data_arrows $itk_component($itk_option(-pane)) $args]
    }
 
    foreach dm {ur ul ll lr} {
	eval $mGed data_arrows $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::data_axes {args} {
    set len [llength $args]
    if {$len < 2} {
	return [eval $mGed data_axes $itk_component($itk_option(-pane)) $args]
    }

    foreach dm {ur ul ll lr} {
	eval $mGed data_axes $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::data_labels {args} {
    set len [llength $args]
    if {$len < 2} {
	return [eval $mGed data_labels $itk_component($itk_option(-pane)) $args]
    }

    foreach dm {ur ul ll lr} {
	eval $mGed data_labels $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::data_lines {args} {
    set len [llength $args]
    if {$len < 2} {
	return [eval $mGed data_lines $itk_component($itk_option(-pane)) $args]
    }

    foreach dm {ur ul ll lr} {
	eval $mGed data_lines $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::data_move {args} {
    eval $mGed data_move $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::data_pick {args} {
    eval $mGed data_pick $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::dbconcat {args} {
    eval $mGed dbconcat $args
}

::itcl::body cadwidgets::Ged::dbfind {args} {
    eval $mGed dbfind $args
}

::itcl::body cadwidgets::Ged::dbip {args} {
    eval $mGed dbip $args
}

::itcl::body cadwidgets::Ged::dbot_dump {args} {
    eval $mGed dbot_dump $args
}

::itcl::body cadwidgets::Ged::decompose {args} {
    eval $mGed decompose $args
}

::itcl::body cadwidgets::Ged::delay {args} {
    eval $mGed delay $args
}

::itcl::body cadwidgets::Ged::dir2ae {args} {
    eval $mGed dir2ae $args
}

::itcl::body cadwidgets::Ged::draw {args} {
    eval $mGed draw $args
}

::itcl::body cadwidgets::Ged::dump {args} {
    eval $mGed dump $args
}

::itcl::body cadwidgets::Ged::dup {args} {
    eval $mGed dup $args
}

::itcl::body cadwidgets::Ged::E {args} {
    eval $mGed E $args
}

::itcl::body cadwidgets::Ged::eac {args} {
    eval $mGed eac $args
}

::itcl::body cadwidgets::Ged::echo {args} {
    eval $mGed echo $args
}

::itcl::body cadwidgets::Ged::edcodes {args} {
    eval $mGed edcodes $args
}

::itcl::body cadwidgets::Ged::edcolor {args} {
    eval $mGed edcolor $args
}

::itcl::body cadwidgets::Ged::edcomb {args} {
    eval $mGed edcomb $args
}

::itcl::body cadwidgets::Ged::edit {args} {
    eval $mGed edit $args
}

::itcl::body cadwidgets::Ged::edmater {args} {
    eval $mGed edmater $args
}

::itcl::body cadwidgets::Ged::erase {args} {
    eval $mGed erase $args
}

::itcl::body cadwidgets::Ged::ev {args} {
    eval $mGed ev $args
}

::itcl::body cadwidgets::Ged::expand {args} {
    eval $mGed expand $args
}

::itcl::body cadwidgets::Ged::eye {args} {
    eval $mGed eye $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::eye_pos {args} {
    eval $mGed eye_pos $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::exists {args} {
    eval $mGed exists $args
}

::itcl::body cadwidgets::Ged::faceplate {args} {
    foreach dm {ur ul ll lr} {
	set ret [eval $mGed faceplate $itk_component($dm) $args]
    }

    return $ret
}

::itcl::body cadwidgets::Ged::facetize {args} {
    eval $mGed facetize $args
}

::itcl::body cadwidgets::Ged::fb2pix {args} {
    eval $mGed fb2pix $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::find_pipept {args} {
    eval $mGed find_pipept $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::fontsize {args} {
    eval $mGed fontsize $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::form {args} {
    eval $mGed form $args
}

::itcl::body cadwidgets::Ged::fracture {args} {
    eval $mGed fracture $args
}

::itcl::body cadwidgets::Ged::g {args} {
    eval $mGed g $args
}

::itcl::body cadwidgets::Ged::get {args} {
    eval $mGed get $args
}

::itcl::body cadwidgets::Ged::get_autoview {args} {
    eval $mGed get_autoview $args
}

::itcl::body cadwidgets::Ged::get_comb {args} {
    eval $mGed get_comb $args
}

::itcl::body cadwidgets::Ged::get_eyemodel {args} {
    eval $mGed get_eyemodel $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::lastMouseRayPos {} {
    return $mLastMouseRayPos
}

::itcl::body cadwidgets::Ged::get_type {args} {
    eval $mGed get_type $args
}

::itcl::body cadwidgets::Ged::glob {args} {
    eval $mGed glob $args
}

::itcl::body cadwidgets::Ged::gqa {args} {
    eval $mGed gqa $args
}

::itcl::body cadwidgets::Ged::grid {args} {
    foreach dm {ur ul ll lr} {
	eval $mGed grid $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::handle_expose {args} {
    eval $mGed handle_expose $args
}

::itcl::body cadwidgets::Ged::hide {args} {
    eval $mGed hide $args
}

::itcl::body cadwidgets::Ged::how {args} {
    eval $mGed how $args
}

::itcl::body cadwidgets::Ged::human {args} {
    eval $mGed human $args
}

::itcl::body cadwidgets::Ged::i {args} {
    eval $mGed i $args
}

::itcl::body cadwidgets::Ged::idents {args} {
    eval $mGed idents $args
}

::itcl::body cadwidgets::Ged::idle_mode {args} {
    eval $mGed idle_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::illum {args} {
    eval $mGed illum $args
}

::itcl::body cadwidgets::Ged::importFg4Section {args} {
    eval $mGed importFg4Section $args
}

::itcl::body cadwidgets::Ged::in {args} {
    eval $mGed in $args
}

::itcl::body cadwidgets::Ged::inside {args} {
    eval $mGed inside $args
}

::itcl::body cadwidgets::Ged::isize {args} {
    eval $mGed isize $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::item {args} {
    eval $mGed item $args
}

::itcl::body cadwidgets::Ged::keep {args} {
    eval $mGed keep $args
}

::itcl::body cadwidgets::Ged::keypoint {args} {
    eval $mGed $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::kill {args} {
    eval $mGed kill $args
}

::itcl::body cadwidgets::Ged::killall {args} {
    eval $mGed killall $args
}

::itcl::body cadwidgets::Ged::killrefs {args} {
    eval $mGed killrefs $args
}

::itcl::body cadwidgets::Ged::killtree {args} {
    eval $mGed killtree $args
}

::itcl::body cadwidgets::Ged::l {args} {
    eval $mGed l $args
}

::itcl::body cadwidgets::Ged::light {args} {
    eval $mGed light $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::light_all {args} {
    foreach dm {ur ul ll lr} {
	eval $mGed light $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::list_views {args} {
    eval $mGed list_views $args
}

::itcl::body cadwidgets::Ged::listen {args} {
    eval $mGed listen $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::listeval {args} {
    eval $mGed listeval $args
}

::itcl::body cadwidgets::Ged::loadview {args} {
    eval $mGed loadview $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::local2base {} {
    eval $mGed local2base
}

::itcl::body cadwidgets::Ged::log {args} {
    eval $mGed log $args
}

::itcl::body cadwidgets::Ged::lookat {args} {
    eval $mGed lookat $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::ls {args} {
    eval $mGed ls $args
}

::itcl::body cadwidgets::Ged::lt {args} {
    eval $mGed lt $args
}

::itcl::body cadwidgets::Ged::m2v_point {args} {
    eval $mGed m2v_point $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::make {args} {
    eval $mGed make $args
}

::itcl::body cadwidgets::Ged::make_bb {name args} {
    eval $mGed make_bb $name $args
}

::itcl::body cadwidgets::Ged::make_name {args} {
    eval $mGed make_name $args
}

::itcl::body cadwidgets::Ged::make_pnts {args} {
    eval $mGed make_pnts $args
}

::itcl::body cadwidgets::Ged::match {args} {
    eval $mGed match $args
}

::itcl::body cadwidgets::Ged::mater {args} {
    eval $mGed mater $args
}

::itcl::body cadwidgets::Ged::mirror {args} {
    eval $mGed mirror $args
}

::itcl::body cadwidgets::Ged::model2view {args} {
    eval $mGed model2view $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::model_axes {args} {
    foreach dm {ur ul ll lr} {
	set ret [eval $mGed model_axes $itk_component($dm) $args]
    }

    return $ret
}

::itcl::body cadwidgets::Ged::more_args_callback {args} {
    eval $mGed more_args_callback $args
}

::itcl::body cadwidgets::Ged::mouse_append_pipept {args} {
    eval $mGed mouse_append_pipept $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_constrain_rot {args} {
    eval $mGed mouse_constrain_rot $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_constrain_trans {args} {
    eval $mGed mouse_constrain_trans $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_find_pipept {args} {
    set i [eval $mGed mouse_find_pipept $itk_component($itk_option(-pane)) $args]
}

::itcl::body cadwidgets::Ged::mouse_move_arb_edge {args} {
    eval $mGed mouse_move_arb_edge $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_move_arb_face {args} {
    eval $mGed mouse_move_arb_face $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_move_pipept {args} {
    puts "cadwidgets::Ged::pane_mouse_move_pipept $_pane $args"
    eval $mGed mouse_move_pipept $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_orotate {args} {
    eval $mGed mouse_orotate $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_oscale {args} {
    eval $mGed mouse_oscale $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_otranslate {args} {
    eval $mGed mouse_otranslate $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_prepend_pipept {args} {
    eval $mGed mouse_prepend_pipept $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_rect {args} {
    eval $mGed mouse_rect $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_rot {args} {
    eval $mGed mouse_rot $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_rotate_arb_face {args} {
    eval $mGed mouse_rotate_arb_face $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_scale {args} {
    eval $mGed mouse_scale $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_protate {args} {
    eval $mGed mouse_protate $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_pscale {args} {
    eval $mGed mouse_pscale $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_ptranslate {args} {
    eval $mGed mouse_ptranslate $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mouse_trans {args} {
    eval $mGed mouse_trans $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::move_arb_edge {args} {
    eval $mGed move_arb_edge $args
}

::itcl::body cadwidgets::Ged::move_arb_edge_mode {args} {
    eval $mGed move_arb_edge_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::move_arb_face {args} {
    eval $mGed move_arb_face $args
}

::itcl::body cadwidgets::Ged::move_arb_face_mode {args} {
    eval $mGed move_arb_face_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::move_pipept {args} {
    eval $mGed move_pipept $args
}

::itcl::body cadwidgets::Ged::move_pipept_mode {args} {
    eval $mGed move_pipept_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::mv {args} {
    eval $mGed mv $args
}

::itcl::body cadwidgets::Ged::mvall {args} {
    eval $mGed mvall $args
}

::itcl::body cadwidgets::Ged::nirt {args} {
    eval $mGed nirt $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::nmg_collapse {args} {
    eval $mGed nmg_collapse $args
}

::itcl::body cadwidgets::Ged::nmg_simplify {args} {
    eval $mGed nmg_simplify $args
}

::itcl::body cadwidgets::Ged::ocenter {args} {
    eval $mGed ocenter $args
}

::itcl::body cadwidgets::Ged::open {args} {
    catch {rename $mRay ""}
    set $mGedFile [eval $mGed open $args]
}

::itcl::body cadwidgets::Ged::opendb {args} {
    eval open $args
}

::itcl::body cadwidgets::Ged::orient {args} {
    eval $mGed orient $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::orotate {args} {
    eval $mGed orotate $args
}

::itcl::body cadwidgets::Ged::orotate_mode {args} {
    eval $mGed orotate_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::oscale {args} {
    eval $mGed oscale $args
}

::itcl::body cadwidgets::Ged::oscale_mode {args} {
    eval $mGed oscale_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::otranslate {args} {
    eval $mGed otranslate $args
}

::itcl::body cadwidgets::Ged::otranslate_mode {args} {
    eval $mGed otranslate_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::overlay {args} {
    eval $mGed overlay $args
}

::itcl::body cadwidgets::Ged::paint_rect_area {args} {
    eval $mGed paint_rect_area $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::pane_adc {_pane args} {
    eval $mGed adc $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_ae {_pane args} {
    eval $mGed aet $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_aet {_pane args} {
    eval $mGed aet $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_arot {_pane args} {
    eval $mGed arot $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_autoview {_pane args} {
    eval $mGed autoview $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_center {_pane args} {
    eval $mGed center $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_constrain_rmode {_pane args} {
    eval $mGed constrain_rmode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_constrain_tmode {_pane args} {
    eval $mGed constrain_tmode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_eye {_pane args} {
    eval $mGed eye $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_eye_pos {_pane args} {
    eval $mGed eye_pos $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_fb2pix {_pane args} {
    eval $mGed fb2pix $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_find_pipept {_pane args} {
    eval $mGed find_pipept $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_fontsize {_pane args} {
    eval $mGed fontsize $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_get_eyemodel {_pane args} {
    eval $mGed get_eyemodel $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_grid {_pane args} {
    eval $mGed grid $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_idle_mode {_pane args} {
    eval $mGed idle_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_isize {_pane args} {
    eval $mGed isize $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_keypoint {_pane args} {
    eval $mGed keypoint $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_light {_pane args} {
    eval $mGed light $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_listen {_pane args} {
    eval $mGed listen $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_loadview {_pane args} {
    eval $mGed loadview $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_lookat {_pane args} {
    eval $mGed lookat $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_m2v_point {_pane args} {
    eval $mGed m2v_point $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_model2view {_pane args} {
    eval $mGed model2view $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_move_arb_edge_mode {_pane args} {
    eval $mGed move_arb_edge_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_move_arb_face_mode {_pane args} {
    eval $mGed move_arb_face_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_move_pipept_mode {_pane args} {
    eval $mGed move_pipept_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_append_pipept {_pane args} {
    eval $mGed mouse_append_pipept $itk_component($_pane) $args

    if {$mPipePointCallback != ""} {
	catch {$mPipePointCallback}
    }
}

::itcl::body cadwidgets::Ged::pane_mouse_constrain_rot {_pane args} {
    eval $mGed mouse_constrain_rot $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_constrain_trans {_pane args} {
    eval $mGed mouse_constrain_trans $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_find_pipept {_pane args} {
    set i [eval $mGed mouse_find_pipept $itk_component($_pane) $args]

    if {$mPipePointCallback != ""} {
	catch {$mPipePointCallback $i}
    }
}

::itcl::body cadwidgets::Ged::pane_mouse_move_arb_edge {_pane args} {
    eval $mGed mouse_move_arb_edge $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_move_arb_face {_pane args} {
    eval $mGed mouse_move_arb_face $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_move_pipept {_pane args} {
    puts "cadwidgets::Ged::pane_mouse_move_pipept $_pane $args"
    eval $mGed mouse_move_pipept $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_orotate {_pane args} {
    eval $mGed mouse_orotate $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_oscale {_pane args} {
    eval $mGed mouse_oscale $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_otranslate {_pane args} {
    eval $mGed mouse_otranslate $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_prepend_pipept {_pane args} {
    eval $mGed mouse_prepend_pipept $itk_component($_pane) $args

    if {$mPipePointCallback != ""} {
	catch {$mPipePointCallback}
    }
}

::itcl::body cadwidgets::Ged::pane_mouse_rect {_pane args} {
    eval $mGed mouse_rect $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_rot {_pane args} {
    eval $mGed mouse_rot $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_rotate_arb_face {_pane args} {
    eval $mGed mouse_rotate_arb_face $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_scale {_pane args} {
    eval $mGed mouse_scale $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_protate {_pane args} {
    eval $mGed mouse_protate $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_pscale {_pane args} {
    eval $mGed mouse_pscale $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_ptranslate {_pane args} {
    eval $mGed mouse_ptranslate $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_mouse_trans {_pane args} {
    eval $mGed mouse_trans $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_nirt {_pane args} {
    eval $mGed nirt $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_orient {_pane args} {
    eval $mGed orient $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_orotate_mode {_pane args} {
    eval $mGed orotate_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_oscale_mode {_pane args} {
    eval $mGed oscale_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_otranslate_mode {_pane args} {
    eval $mGed otranslate_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_paint_rect_area {_pane args} {
    eval $mGed paint_rect_area $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_perspective {_pane args} {
    eval $mGed perspective $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_pix2fb {_pane args} {
    eval $mGed pix2fb $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_plot {_pane args} {
    eval $mGed plot $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_pmat {_pane args} {
    eval $mGed pmat $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_pmodel2view {_pane args} {
    eval $mGed pmodel2view $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_png {_pane args} {
    eval $mGed png $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_pngwf {_pane args} {
    eval $mGed pngwf $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_pov {_pane args} {
    eval $mGed pov $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_preview {_pane args} {
    eval $mGed preview $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_protate_mode {_pane args} {
    eval $mGed protate_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_ps {_pane args} {
    eval $mGed ps $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_pscale_mode {_pane args} {
    eval $mGed pscale_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_ptranslate_mode {_pane args} {
    eval $mGed ptranslate_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_quat {_pane args} {
    eval $mGed quat $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_qvrot {_pane args} {
    eval $mGed qvrot $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rect {_pane args} {
    eval $mGed rect $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rect_mode {_pane args} {
    eval $mGed rect_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_refresh {_pane args} {
    eval $mGed refresh $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rmat {_pane args} {
    eval $mGed rmat $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rot {_pane args} {
    eval $mGed rot $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rot_about {_pane args} {
    eval $mGed rot_about $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rot_point {_pane args} {
    eval $mGed rot_point $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rotate_mode {_pane args} {
    eval $mGed rotate_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rotate_arb_face_mode {_pane args} {
    eval $mGed rotate_arb_face_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rrt {_pane args} {
    eval $mGed rrt $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rt {_pane args} {
    eval $mGed rt $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rtarea {_pane args} {
    eval $mGed rtarea $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rtcheck {_pane args} {
    eval $mGed rtcheck $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rtedge {_pane args} {
    eval $mGed rtedge $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_rtweight {_pane args} {
    eval $mGed rtweight $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_savekey {_pane args} {
    eval $mGed savekey $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_saveview {_pane args} {
    eval $mGed saveview $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_sca {_pane args} {
    eval $mGed sca $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_screengrab {_pane args} {
    eval $mGed screengrab $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_scale_mode {_pane args} {
    eval $mGed scale_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_scale_mode {_pane args} {
    eval $mGed scale_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_screen2view {_pane args} {
    eval $mGed screen2view $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_set_coord {_pane args} {
    eval $mGed set_coord $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_set_fb_mode {_pane args} {
    eval $mGed set_fb_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_setview {_pane args} {
    eval $mGed setview $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_size {_pane args} {
    eval $mGed size $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_slew {_pane args} {
    eval $mGed slew $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_snap_view {_pane args} {
    eval $mGed snap_view $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_tra {_pane args} {
    eval $mGed tra $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_translate_mode {_pane args} {
    eval $mGed translate_mode $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_v2m_point {_pane args} {
    eval $mGed v2m_point $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_view {_pane args} {
    eval $mGed view $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_view2model {_pane args} {
    eval $mGed view2model $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_view_callback {_pane args} {
    eval $mGed view_callback $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_viewdir {_pane args} {
    eval $mGed viewdir $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_vmake {_pane args} {
    eval $mGed vmake $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_vnirt {_pane args} {
    eval $mGed vnirt $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_vslew {_pane args} {
    eval $mGed vslew $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_ypr {_pane args} {
    eval $mGed ypr $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_zbuffer {_pane args} {
    eval $mGed zbuffer $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_zclip {_pane args} {
    eval $mGed zclip $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_zoom {_pane args} {
    eval $mGed zoom $itk_component($_pane) $args
}

::itcl::body cadwidgets::Ged::pane_win_name {_pane} {
    return $itk_component($_pane)
}

::itcl::body cadwidgets::Ged::pane_win_size {_pane args} {
    set nargs [llength $args]

    # get the view window size for the specified pane
    if {$nargs == 0} {
	return [$mGed view_win_size $itk_component($_pane)]
    }

    if {$nargs == 1} {
	set w $args
	set h $args
    } elseif {$nargs == 2} {
	set w [lindex $args 0]
	set h [lindex $args 1]
    } else {
	error "size: bad size - $args"
    }

    eval $mGed view_win_size $itk_component($_pane) $w $h
}

::itcl::body cadwidgets::Ged::pathlist {args} {
    eval $mGed pathlist $args
}

::itcl::body cadwidgets::Ged::paths {args} {
    eval $mGed paths $args
}

::itcl::body cadwidgets::Ged::perspective {args} {
    eval $mGed perspective $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::pix2fb {args} {
    eval $mGed pix2fb $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::plot {args} {
    eval $mGed plot $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::pmat {args} {
    eval $mGed pmat $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::pmodel2view {args} {
    eval $mGed pmodel2view $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::png {args} {
    eval $mGed png $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::pngwf {args} {
    eval $mGed pngwf $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::pov {args} {
    eval $mGed pov $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::prcolor {args} {
    eval $mGed prcolor $args
}

::itcl::body cadwidgets::Ged::prefix {args} {
    eval $mGed prefix $args
}

::itcl::body cadwidgets::Ged::preview {args} {
    eval $mGed preview $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::prim_label {args} {
    eval $mGed prim_label $args

    if {[llength $args]} {
	faceplate prim_labels draw 1
    } else {
	faceplate prim_labels draw 0
    }
}

::itcl::body cadwidgets::Ged::ps {args} {
    eval $mGed ps $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::push {args} {
    eval $mGed push $args
}

::itcl::body cadwidgets::Ged::put {args} {
    eval $mGed put $args
}

::itcl::body cadwidgets::Ged::put_comb {args} {
    eval $mGed put_comb $args
}

::itcl::body cadwidgets::Ged::putmat {args} {
    eval $mGed putmat $args
}

::itcl::body cadwidgets::Ged::qray {args} {
    eval $mGed qray $args
}

::itcl::body cadwidgets::Ged::quat {args} {
    eval $mGed quat $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::qvrot {args} {
    eval $mGed qvrot $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::r {args} {
    eval $mGed r $args
}

::itcl::body cadwidgets::Ged::rcodes {args} {
    eval $mGed rcodes $args
}

::itcl::body cadwidgets::Ged::rect {args} {
    eval $mGed rect $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rect_mode {args} {
    eval $mGed rect_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::red {args} {
    eval $mGed red $args
}

::itcl::body cadwidgets::Ged::redraw {args} {
    eval $mGed redraw $args
}

::itcl::body cadwidgets::Ged::refresh {args} {
    eval $mGed refresh $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::refresh_all {args} {
    eval $mGed refresh_all $args
}

::itcl::body cadwidgets::Ged::refresh_off {} {
    incr mRefreshOn -1

    if {$mRefreshOn == 0} {
	eval $mGed refresh_on 0
    }
}

::itcl::body cadwidgets::Ged::refresh_on {} {
    incr mRefreshOn 1

    if {$mRefreshOn == 1} {
	eval $mGed refresh_on 1
    }
}

::itcl::body cadwidgets::Ged::regdef {args} {
    eval $mGed regdef $args
}

::itcl::body cadwidgets::Ged::regions {args} {
    eval $mGed regions $args
}

::itcl::body cadwidgets::Ged::report {args} {
    eval $mGed report $args
}

::itcl::body cadwidgets::Ged::rfarb {args} {
    eval $mGed rfarb $args
}

::itcl::body cadwidgets::Ged::rm {args} {
    eval $mGed rm $args
}

::itcl::body cadwidgets::Ged::rmap {args} {
    eval $mGed rmap $args
}

::itcl::body cadwidgets::Ged::rmat {args} {
    eval $mGed rmat $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rmater {args} {
    eval $mGed rmater $args
}

::itcl::body cadwidgets::Ged::rot {args} {
    eval $mGed rot $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rot_about {args} {
    eval $mGed rot_about $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rot_point {args} {
    eval $mGed rot_point $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rotate_arb_face {args} {
    eval $mGed rotate_arb_face $args
}

::itcl::body cadwidgets::Ged::rotate_arb_face_mode {args} {
    eval $mGed rotate_arb_face_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rotate_mode {args} {
    eval $mGed rotate_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rrt {args} {
    eval $mGed rrt $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rselect {args} {
    eval $mGed rselect $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rt {args} {
    eval $mGed rt $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rt_end_callback {args} {
    eval $mGed rt_end_callback $args
}

::itcl::body cadwidgets::Ged::rt_gettrees {args} {
    eval $mGed rt_gettrees $args
}

::itcl::body cadwidgets::Ged::rtabort {args} {
    eval $mGed rtabort $args
}

::itcl::body cadwidgets::Ged::rtarea {args} {
    eval $mGed rtarea $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rtcheck {args} {
    eval $mGed rtcheck $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rtedge {args} {
    eval $mGed rtedge $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::rtweight {args} {
    eval $mGed rtweight $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::savekey {args} {
    eval $mGed savekey $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::saveview {args} {
    eval $mGed saveview $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::sca {args} {
    eval $mGed sca $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::screengrab {args} {
    eval $mGed screengrab $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::screen2view {args} {
    eval $mGed screen2view $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::protate {args} {
    eval $mGed protate $args
}

::itcl::body cadwidgets::Ged::protate_mode {args} {
    eval $mGed protate_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::pscale {args} {
    eval $mGed pscale $args
}

::itcl::body cadwidgets::Ged::pscale_mode {args} {
    eval $mGed pscale_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::ptranslate {args} {
    eval $mGed ptranslate $args
}

::itcl::body cadwidgets::Ged::ptranslate_mode {args} {
    eval $mGed ptranslate_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::scale_mode {args} {
    eval $mGed scale_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::sdata_arrows {args} {
    set len [llength $args]
    if {$len < 2} {
	return [eval $mGed sdata_arrows $itk_component($itk_option(-pane)) $args]
    }

    foreach dm {ur ul ll lr} {
	eval $mGed sdata_arrows $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::sdata_axes {args} {
    set len [llength $args]
    if {$len < 2} {
	return [eval $mGed sdata_axes $itk_component($itk_option(-pane)) $args]
    }

    foreach dm {ur ul ll lr} {
	eval $mGed sdata_axes $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::sdata_labels {args} {
    set len [llength $args]
    if {$len < 2} {
	return [eval $mGed sdata_labels $itk_component($itk_option(-pane)) $args]
    }

    foreach dm {ur ul ll lr} {
	eval $mGed sdata_labels $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::sdata_lines {args} {
    set len [llength $args]
    if {$len < 2} {
	return [eval $mGed sdata_lines $itk_component($itk_option(-pane)) $args]
    }

    foreach dm {ur ul ll lr} {
	eval $mGed sdata_lines $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::search {args} {
    eval $mGed search $args
}

::itcl::body cadwidgets::Ged::select {args} {
    eval $mGed select $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::set_coord {args} {
    eval $mGed set_coord $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::set_fb_mode {args} {
    eval $mGed set_fb_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::set_output_script {args} {
    eval $mGed set_output_script $args
}

::itcl::body cadwidgets::Ged::set_transparency {args} {
    eval $mGed set_transparency $args
}

::itcl::body cadwidgets::Ged::set_uplotOutputMode {args} {
    eval $mGed set_uplotOutputMode $args
}

::itcl::body cadwidgets::Ged::setview {args} {
    eval $mGed setview $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::shaded_mode {args} {
    eval $mGed shaded_mode $args
}

::itcl::body cadwidgets::Ged::shader {args} {
    eval $mGed shader $args
}

::itcl::body cadwidgets::Ged::shareGed {_ged} {
    if {!$mSharedGed} {
	rename $mGed ""
    }

    set mGed $_ged
    set mSharedGed 1
}

::itcl::body cadwidgets::Ged::shells {args} {
    eval $mGed shells $args
}

::itcl::body cadwidgets::Ged::showmats {args} {
    eval $mGed showmats $args
}

::itcl::body cadwidgets::Ged::size {args} {
    eval $mGed size $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::slew {args} {
    eval $mGed slew $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::snap_view {args} {
    eval $mGed snap_view $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::solids {args} {
    eval $mGed solids $args
}

::itcl::body cadwidgets::Ged::solids_on_ray {args} {
    eval $mGed solids_on_ray $args
}

::itcl::body cadwidgets::Ged::summary {args} {
    eval $mGed summary $args
}

::itcl::body cadwidgets::Ged::sync {args} {
    eval $mGed sync $args
}

::itcl::body cadwidgets::Ged::tire {args} {
    eval $mGed tire $args
}

::itcl::body cadwidgets::Ged::title {args} {
    eval $mGed title $args
}

::itcl::body cadwidgets::Ged::tol {args} {
    eval $mGed tol $args
}

::itcl::body cadwidgets::Ged::tops {args} {
    eval $mGed tops $args
}

::itcl::body cadwidgets::Ged::tra {args} {
    eval $mGed tra $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::track {args} {
    eval $mGed track $args
}

::itcl::body cadwidgets::Ged::translate {args} {
    eval $mGed translate $args
}

::itcl::body cadwidgets::Ged::translate_mode {args} {
    eval $mGed translate_mode $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::transparency {args} {
    eval $mGed transparency $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::transparency_all {args} {
    foreach dm {ur ul ll lr} {
	eval $mGed transparency $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::tree {args} {
    eval $mGed tree $args
}

::itcl::body cadwidgets::Ged::unhide {args} {
    eval $mGed unhide $args
}

::itcl::body cadwidgets::Ged::units {args} {
    eval $mGed units $args
}

::itcl::body cadwidgets::Ged::v2m_point {args} {
    eval $mGed v2m_point $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::vdraw {args} {
    eval $mGed vdraw $args
}

::itcl::body cadwidgets::Ged::version {args} {
    eval $mGed version $args
}

::itcl::body cadwidgets::Ged::view {args} {
    eval $mGed view $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::view2model {args} {
    eval $mGed view2model $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::view_axes {args} {
    foreach dm {ur ul ll lr} {
	set ret [eval $mGed view_axes $itk_component($dm) $args]
    }

    return $ret
}

::itcl::body cadwidgets::Ged::view_callback {args} {
    eval $mGed view_callback $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::view_callback_all {args} {
    foreach dm {ur ul ll lr} {
	eval $mGed view_callback $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::viewdir {args} {
    eval $mGed viewdir $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::vmake {args} {
    eval $mGed vmake $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::vnirt {args} {
    eval $mGed vnirt $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::vslew {args} {
    eval $mGed vslew $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::wcodes {args} {
    eval $mGed wcodes $args
}

::itcl::body cadwidgets::Ged::whatid {args} {
    eval $mGed whatid $args
}

::itcl::body cadwidgets::Ged::which_shader {args} {
    eval $mGed which_shader $args
}

::itcl::body cadwidgets::Ged::whichair {args} {
    eval $mGed whichair $args
}

::itcl::body cadwidgets::Ged::whichid {args} {
    eval $mGed whichid $args
}

::itcl::body cadwidgets::Ged::who {args} {
    eval $mGed who $args
}

::itcl::body cadwidgets::Ged::wmater {args} {
    eval $mGed wmater $args
}

::itcl::body cadwidgets::Ged::xpush {args} {
    eval $mGed xpush $args
}

::itcl::body cadwidgets::Ged::ypr {args} {
    eval $mGed ypr $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::zap {args} {
    eval $mGed zap $args
}

::itcl::body cadwidgets::Ged::zbuffer {args} {
    eval $mGed zbuffer $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::zbuffer_all {args} {
    foreach dm {ur ul ll lr} {
	eval $mGed zbuffer $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::zclip {args} {
    eval $mGed zclip $itk_component($itk_option(-pane)) $args
}

::itcl::body cadwidgets::Ged::zclip_all {args} {
    foreach dm {ur ul ll lr} {
	eval $mGed zclip $itk_component($dm) $args
    }
}

::itcl::body cadwidgets::Ged::zoom {args} {
    eval $mGed zoom $itk_component($itk_option(-pane)) $args
}


############################### Public Methods Specific to cadwidgets::Ged ###############################

::itcl::body cadwidgets::Ged::? {} {
    return [$help ? 20 8]
}

::itcl::body cadwidgets::Ged::apropos {args} {
    return [eval $help apropos $args]
}

# Create a new arrow with both points the same.
# Go into data move mode for this arrow and it's second point.
#
::itcl::body cadwidgets::Ged::begin_data_arrow {_pane _x _y} {
    set mLastMousePos "$_x $_y"
    set point [pane_mouse_3dpoint $_pane $_x $_y]

    if {$point == ""} {
	return
    }

    foreach callback $mBeginDataArrowCallbacks {
	catch {$callback $point}
    }

    #XXX Temporarily depend on callbacks to create the arrow
    set points [$mGed data_arrows $itk_component($_pane) points]
    set dindex [llength $points]
    incr dindex -1

    if {$dindex < 0} {
	return
    }

    # start receiving motion events
    bind $itk_component($_pane) <Motion> "[::itcl::code $this handle_data_move $_pane data_arrows $dindex %x %y]; break"
}

::itcl::body cadwidgets::Ged::begin_data_line {_pane _x _y} {
    set mLastMousePos "$_x $_y"
    set point [pane_mouse_3dpoint $_pane $_x $_y]

    if {$point == ""} {
	return
    }

    foreach callback $mBeginDataLineCallbacks {
	catch {$callback $point}
    }

    #XXX Temporarily depend on callbacks to create the line
    set points [$mGed data_lines $itk_component($_pane) points]
    set dindex [llength $points]
    incr dindex -1

    if {$dindex < 0} {
	return
    }

    # start receiving motion events
    bind $itk_component($_pane) <Motion> "[::itcl::code $this handle_data_move $_pane data_lines $dindex %x %y]; break"
}

::itcl::body cadwidgets::Ged::begin_data_move {_pane _x _y} {
    set mLastMousePos "$_x $_y"
    set data [$mGed data_pick $itk_component($_pane) $_x $_y]
    set mLastDataType ""

    if {$data == ""} {
	return
    }

    foreach callback $mBeginDataMoveCallbacks {
	catch {$callback $point}
    }

    set mLastDataType [lindex $data 0]
    set mLastDataIndex [lindex $data 1]

    # start receiving motion events
    bind $itk_component($_pane) <Motion> "[::itcl::code $this handle_data_move $_pane $mLastDataType $mLastDataIndex %x %y]; break"
}

::itcl::body cadwidgets::Ged::begin_view_measure {_pane _part1_button _part2_button _x _y} {
    measure_line_erase

    set mBegin3DPoint [pane_mouse_3dpoint $_pane $_x $_y]
    set mMiddle3DPoint $mBegin3DPoint
    set mMeasuringStick3D $mMeasuringStick3DCurrent

    # start receiving motion events
    bind $itk_component($_pane) <Motion> "[::itcl::code $this handle_view_measure $_pane %x %y]; break"

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <ButtonRelease-$_part1_button> "[::itcl::code $this end_view_measure $dm $_part1_button $_part2_button]; break"
    }

    set mMeasuringStickColorVDraw3D [get_vdraw_color $itk_option(-measuringStickColor)]
}

::itcl::body cadwidgets::Ged::begin_view_measure_part2 {_pane _button _x _y} {
    if {$mMeasuringStick3D} {
	set mMeasuringStick3D $mMeasuringStick3DCurrent
    }

    set mEnd3DPoint [pane_mouse_3dpoint $_pane $_x $_y]
    set pt $mEnd3DPoint

    if {[vnear_zero [vsub2 $mEnd3DPoint $mMiddle3DPoint] 0.0001]} {
	return
    }

    $mGed vdraw open $MEASURING_STICK
    if {$mMeasuringStick3D && $mMeasuringStick3DCurrent} {
	$mGed vdraw params color $mMeasuringStickColorVDraw3D
    } else {
	$mGed vdraw params color $mMeasuringStickColorVDraw2D
    }
    eval $mGed vdraw write next 1 $pt
    $mGed vdraw send

    # start receiving motion events
    bind $itk_component($_pane) <Motion> "[::itcl::code $this handle_view_measure_part2 $_pane %x %y]; break"
}

::itcl::body cadwidgets::Ged::default_views {} {
    $mGed aet $itk_component(ul) 0 90 0
    $mGed aet $itk_component(ur) 35 25 0
    $mGed aet $itk_component(ll) 0 0 0
    $mGed aet $itk_component(lr) 90 0 0
}

::itcl::body cadwidgets::Ged::delete_pipept {args} {
    eval $mGed delete_pipept $args
}

::itcl::body cadwidgets::Ged::end_data_arrow {_pane} {
    $mGed idle_mode $itk_component($_pane)

    if {$mLastMousePos == ""} {
	return
    }

    refresh_off
    $mGed data_arrows $itk_component($_pane) draw 0
    set point [eval pane_mouse_3dpoint $_pane $mLastMousePos]
    $mGed data_arrows $itk_component($_pane) draw 1
    set mLastMousePos ""

    # replace last point
    set points [$mGed data_arrows $itk_component($_pane) points]
    set points [lreplace $points end end $point]
    $mGed data_arrows $itk_component($_pane) points $points

    foreach callback $mEndDataArrowCallbacks {
	catch {$callback $point}
    }

    refresh_on
    refresh_all
}

::itcl::body cadwidgets::Ged::end_data_line {_pane} {
    $mGed idle_mode $itk_component($_pane)

    if {$mLastMousePos == ""} {
	return
    }

    refresh_off
    $mGed data_lines $itk_component($_pane) draw 0
    set point [eval pane_mouse_3dpoint $_pane $mLastMousePos]
    $mGed data_lines $itk_component($_pane) draw 1
    set mLastMousePos ""

    # replace last point
    set points [$mGed data_lines $itk_component($_pane) points]
    set points [lreplace $points end end $point]
    $mGed data_lines $itk_component($_pane) points $points

    foreach callback $mEndDataLineCallbacks {
	catch {$callback $point}
    }

    refresh_on
    refresh_all
}

::itcl::body cadwidgets::Ged::end_data_move {_pane} {
    $mGed idle_mode $itk_component($_pane)

    if {$mLastMousePos == "" || $mLastDataType == ""} { 
	return
    }

    set mLastMouseRayTarget ""
    refresh_off
    $mGed $mLastDataType $itk_component($_pane) draw 0

    # This call returns a point that is either a hit point
    # on some geometry object or a "Data" point (i.e. an axes,
    # line, arrow or label). Points on the view plane are NOT
    # returned.
    set point [eval pane_mouse_3dpoint $_pane $mLastMousePos 0]

    $mGed $mLastDataType $itk_component($_pane) draw 1
    set mLastMousePos ""

    # If a point has not been selected via the pane_mouse_3dpoint call
    # above (i.e. neither a geometry object nor a data point was hit)
    # and gridSnap is active, apply snap to grid to the data point
    # currently being moved.
    if {$point == "" && $itk_option(-gridSnap)} {
	# First, get the data point being moved.
	if {$mLastDataType == "data_labels"} {
	    set labels [$mGed data_labels $itk_component($_pane) labels]
	    set label [lindex $labels $mLastDataIndex]
	    set point [lindex $label 1]
	} else {
	    set points [$mGed $mLastDataType $itk_component($_pane) points]
	    set point [lindex $points $mLastDataIndex]
	}

	# Convert point to view coordinates and call snap_view. Then convert
	# back to model coordinates. Note - vZ is saved so that the movement
	# stays in a plane parallel to the view plane.
	set view [pane_m2v_point $_pane $point]
	set vZ [lindex $view 2]
	set view [$mGed snap_view $itk_component($_pane) [lindex $view 0] [lindex $view 1]]
	lappend view $vZ
	set point [pane_v2m_point $_pane $view]
    }

    # Replace the mLastDataIndex point with this point
    if {$point != ""} {
	if {$mLastDataType == "data_labels" || $mLastDataType == "sdata_labels"} {
	    set labels [$mGed $mLastDataType $itk_component($_pane) labels]
	    set label [lindex $labels $mLastDataIndex]
	    set label [lreplace $label 1 1 $point]
	    set labels [lreplace $labels $mLastDataIndex $mLastDataIndex $label]
	    $mLastDataType labels $labels
	} else {
	    set points [$mGed $mLastDataType $itk_component($_pane) points]
	    set points [lreplace $points $mLastDataIndex $mLastDataIndex $point]
	    $mLastDataType points $points
	}
    }

    foreach callback $mEndDataMoveCallbacks {
	catch {$callback $mLastDataType}
    }
    refresh_on
    refresh_all
}

::itcl::body cadwidgets::Ged::end_view_measure {_pane _part1_button _part2_button} {
    $mGed idle_mode $itk_component($_pane)

    # Add specific bindings to eliminate bleed through from measure tool bindings
    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <Control-ButtonRelease-$_part1_button> "$mGed idle_mode $itk_component($dm); break"
	bind $itk_component($dm) <Shift-ButtonRelease-$_part1_button> "$mGed idle_mode $itk_component($dm); break"
    }

    refresh_off

    set diff [vsub2 $mMiddle3DPoint $mBegin3DPoint]
    set delta [expr {[magnitude $diff] * [$mGed base2local $itk_component($_pane)]}]

    if {[expr {abs($delta) > 0.0001}]} {
	set mMeasureLineActive 1
	init_view_measure_part2 $_part2_button

	# Add specific bindings to eliminate bleed through from measure tool bindings
	foreach dm {ur ul ll lr} {
	    bind $itk_component($dm) <Control-ButtonRelease-$_part2_button> "$mGed idle_mode $itk_component($dm); break"
	    bind $itk_component($dm) <Shift-ButtonRelease-$_part2_button> "$mGed idle_mode $itk_component($dm); break"
	}
    } else {
	init_button_no_op_prot $_part2_button
	refresh_on
	return
    }

    set mstring "Measured Distance (Leg 1):  $delta [$mGed units -s]"

    if {[llength $mViewMeasureCallbacks] == 0} {
	tk_messageBox -title "Measured Distance" \
	    -icon info \
	    -message $mstring
    } else {
	foreach callback $mViewMeasureCallbacks {
	    catch {$callback $mstring}
	}
    }

    refresh_on
#    refresh_all
}

::itcl::body cadwidgets::Ged::end_view_measure_part2 {_pane _button} {
    $mGed idle_mode $itk_component($_pane)

    # Calculate length of leg 2
    set diff [vsub2 $mEnd3DPoint $mMiddle3DPoint]
    set delta [expr {[magnitude $diff] * [$mGed base2local $itk_component($_pane)]}]

    set ret [catch {
	set A [vunitize [vsub2 $mBegin3DPoint $mMiddle3DPoint]]
	set B [vunitize [vsub2 $mEnd3DPoint $mMiddle3DPoint]]
    }]

    if {$ret} {
	return
    }

    set cos [vdot $A $B]
    set angle [format "%.2f" [expr {acos($cos) * (180.0 / 3.141592653589793)}]]

    if {[llength $mViewMeasureCallbacks] == 0} {
	set mstring "Measured Distance (Leg 2):  $delta [$mGed units -s]\nMeasured Angle:  $angle"
	tk_messageBox -title "Measured Angle" \
	    -icon info \
	    -message $mstring
    } else {
	# For some reason, having a newline in the string causes the geometry window to flash ????
	# So, split the string into two pieces.
	set mstring "Measured Distance (Leg 2):  $delta [$mGed units -s]"
	foreach callback $mViewMeasureCallbacks {
	    catch {$callback $mstring}
	}

	set mstring "Measured Angle:  $angle"
	foreach callback $mViewMeasureCallbacks {
	    catch {$callback $mstring}
	}
    }

    set mBegin3DPoint {0 0 0}
    set mMiddle3DPoint {0 0 0}
    set mEndDPoint {0 0 0}

    init_button_no_op_prot $_button
}

::itcl::body cadwidgets::Ged::end_view_rect {_pane {_button 1}} {
    $mGed idle_mode $itk_component($_pane)

#    # Add specific bindings to eliminate bleed through from rectangle mode
#    foreach dm {ur ul ll lr} {
#	bind $itk_component($dm) <Control-ButtonRelease-$_button> "$mGed idle_mode $itk_component($dm); break"
#	bind $itk_component($dm) <Shift-ButtonRelease-$_button> "$mGed idle_mode $itk_component($dm); break"
#    }

    if {[llength $mViewRectCallbacks] == 0} {
	tk_messageBox -message [$mGed rselect $itk_component($_pane)]
    } else {
	foreach callback $mViewRectCallbacks {
	    catch {$callback [$mGed rselect $itk_component($_pane)]}
	}
    }
}

::itcl::body cadwidgets::Ged::end_view_rotate {_pane} {
    $mGed idle_mode $itk_component($_pane)

    if {$mHistoryCallback != ""} {
	eval $mHistoryCallback [list [list aet [pane_aet $_pane]]]
    }
}

::itcl::body cadwidgets::Ged::end_view_scale {_pane} {
    $mGed idle_mode $itk_component($_pane)

    if {$mHistoryCallback != ""} {
	eval $mHistoryCallback [list [list size [pane_size $_pane]]]
    }
}

::itcl::body cadwidgets::Ged::end_view_translate {_pane} {
    $mGed idle_mode $itk_component($_pane)

    if {$mHistoryCallback != ""} {
	eval $mHistoryCallback [list [list center [pane_center $_pane]]]
    }
}

::itcl::body cadwidgets::Ged::getUserCmds {} {
    return [$help getCmds]
}

::itcl::body cadwidgets::Ged::handle_data_move {_pane _dtype _dindex _x _y} {
    refresh_off
    set mLastMousePos "$_x $_y"
    $mGed data_move $itk_component($_pane) $_dtype $_dindex $_x $_y

    if {$_dtype == "data_labels" || $_dtype == "sdata_labels"} {
	set labels [$mGed $_dtype $itk_component($_pane) labels]

	foreach dm {ur ul ll lr} {
	    if {$dm == $_pane} {
		continue
	    }

	    $mGed $_dtype $itk_component($dm) labels $labels
	}
    } else {
	set points [$mGed $_dtype $itk_component($_pane) points]

	foreach dm {ur ul ll lr} {
	    if {$dm == $_pane} {
		continue
	    }

	    $mGed $_dtype $itk_component($dm) points $points
	}
    }
    refresh_on
    refresh_all
}

::itcl::body cadwidgets::Ged::handle_view_measure {_pane _x _y} {
    catch {$mGed vdraw vlist delete $MEASURING_STICK}

    set mMiddle3DPoint [pane_mouse_3dpoint $_pane $_x $_y]

    set move 0
    set draw 1
    $mGed vdraw open $MEASURING_STICK
    if {$mMeasuringStick3D && $mMeasuringStick3DCurrent} {
	$mGed vdraw params color $mMeasuringStickColorVDraw3D
    } else {
	$mGed vdraw params color $mMeasuringStickColorVDraw2D
    }
    eval $mGed vdraw write next $move $mBegin3DPoint
    eval $mGed vdraw write next $draw $mMiddle3DPoint
    $mGed vdraw send
}

::itcl::body cadwidgets::Ged::handle_view_measure_part2 {_pane _x _y} {
    set mEnd3DPoint [pane_mouse_3dpoint $_pane $_x $_y]

    $mGed vdraw open $MEASURING_STICK
    if {$mMeasuringStick3D && $mMeasuringStick3DCurrent} {
	$mGed vdraw params color $mMeasuringStickColorVDraw3D
    } else {
	$mGed vdraw params color $mMeasuringStickColorVDraw2D
    }

    # Replace the end point
    $mGed vdraw delete 2
    eval $mGed vdraw write next 1 $mEnd3DPoint
    $mGed vdraw send
}

::itcl::body cadwidgets::Ged::help {args} {
    return [eval $help get $args]
}

::itcl::body cadwidgets::Ged::history_callback {args} {
    if {[llength $args]} {
	set mHistoryCallback $args
    }

    return $mHistoryCallback
}

::itcl::body cadwidgets::Ged::init_append_pipept {_obj {_button 1} {_callback {}}} {
    measure_line_erase

    set mPipePointCallback $_callback

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this pane_mouse_append_pipept $dm $_obj %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> ""
    }
}

::itcl::body cadwidgets::Ged::init_button_no_op {{_button 1}} {
    measure_line_erase
    init_button_no_op_prot $_button
}

::itcl::body cadwidgets::Ged::init_comp_pick {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this pane_mouse_ray $dm %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> ""
    }
}

::itcl::body cadwidgets::Ged::init_data_arrow {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this begin_data_arrow $dm %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> "[::itcl::code $this end_data_arrow $dm]; break"
    }
}

::itcl::body cadwidgets::Ged::init_data_label {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this pane_mouse_data_label $dm %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> ""
    }
}

::itcl::body cadwidgets::Ged::init_data_line {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this begin_data_line $dm %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> "[::itcl::code $this end_data_line $dm]; break"
    }
}

::itcl::body cadwidgets::Ged::init_data_move {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this begin_data_move $dm %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> "[::itcl::code $this end_data_move $dm]; break"
    }
}

::itcl::body cadwidgets::Ged::init_data_pick {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this pane_mouse_data_pick $dm %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> ""
    }
}

::itcl::body cadwidgets::Ged::init_find_pipept {_obj {_button 1} {_callback {}}} {
    measure_line_erase

    set mPipePointCallback $_callback

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this pane_mouse_find_pipept $dm $_obj %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> ""
    }
}

::itcl::body cadwidgets::Ged::init_prepend_pipept {_obj {_button 1} {_callback {}}} {
    measure_line_erase

    set mPipePointCallback $_callback

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this pane_mouse_prepend_pipept $dm $_obj %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> ""
    }
}

::itcl::body cadwidgets::Ged::init_view_bindings {{_type default}} {
    global tcl_platform

    switch -- $_type {
	brlcad {
	    foreach pane {ul ur ll lr} {
		$mGed init_view_bindings $itk_component($pane)
	    }	    
	}
	default {
	    foreach pane {ul ur ll lr} {
		$mGed init_view_bindings $itk_component($pane)
		set win $itk_component($pane)

		# Turn off a few mouse bindings
		bind $win <1> {}
		bind $win <2> {}
		bind $win <3> {}
	    }
	}
    }

    # Turn off <Enter> bindings. This fixes the problem where various
    # various dialogs disappear when the mouse enters the geometry
    # window when on the "windows" platform.
    if {$tcl_platform(platform) == "windows"} {
	foreach dm {ur ul ll lr} {
	    bind $itk_component($dm) <Enter> {}
	}
    }
}

::itcl::body cadwidgets::Ged::init_view_center {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "$mGed vslew $itk_component($dm) %x %y; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> "[::itcl::code $this end_view_translate $dm]; break"
    }
}

::itcl::body cadwidgets::Ged::init_view_measure {{_part1_button 1} {_part2_button 2}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_part1_button> "[::itcl::code $this begin_view_measure $dm $_part1_button $_part2_button %x %y]; focus %W; break"
    }
}

::itcl::body cadwidgets::Ged::init_view_measure_part2 {_button} {
    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "[::itcl::code $this begin_view_measure_part2 $dm $_button %x %y]; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> "[::itcl::code $this end_view_measure_part2 $dm $_button]; break"
    }
}

::itcl::body cadwidgets::Ged::init_view_rect {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "$mGed rect_mode $itk_component($dm) %x %y; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> "[::itcl::code $this end_view_rect $dm]; break"
    }
}

::itcl::body cadwidgets::Ged::init_view_rotate {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "$mGed rotate_mode $itk_component($dm) %x %y; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> "[::itcl::code $this end_view_rotate $dm]; break"
    }
}

::itcl::body cadwidgets::Ged::init_view_scale {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "$mGed scale_mode $itk_component($dm) %x %y; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> "[::itcl::code $this end_view_scale $dm]; break"
    }
}

::itcl::body cadwidgets::Ged::init_view_translate {{_button 1}} {
    measure_line_erase

    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> "$mGed translate_mode $itk_component($dm) %x %y; focus %W; break"
	bind $itk_component($dm) <ButtonRelease-$_button> "[::itcl::code $this end_view_translate $dm]; break"
    }
}

::itcl::body cadwidgets::Ged::center_ray {{_pflag 0}} {
    set wsize [$mGed view_win_size $itk_component($itk_option(-pane))]
    set x [expr {[lindex $wsize 0] * 0.5}]
    set y [expr {[lindex $wsize 1] * 0.5}]
    mouse_ray $x $y $_pflag
}

::itcl::body cadwidgets::Ged::mouse_ray {_x _y {_pflag 0}} {
    pane_mouse_ray $itk_option(-pane) $_x $_y $_pflag
}

## pane_mouse_3dpoint
#
# First, try to pick a data point.
# If that fails, try to pick a point on an object.
# Lastly, if all else fails, pick a point on the view plane.
#
::itcl::body cadwidgets::Ged::pane_mouse_3dpoint {_pane _x _y {_vflag 1}} {
    set mMeasuringStick3DCurrent 1
    set pdata [$mGed data_pick $itk_component($_pane) $_x $_y]

    if {$pdata == ""} {
	set partitions [pane_mouse_ray $_pane $_x $_y 1]

	if {$partitions == ""} {
	    if {!$_vflag} {
		return
	    }

	    set mMeasuringStick3DCurrent 0
	    set point $mLastMouseRayTarget
	} else {
	    if {$mDataPointCallback != ""} {
		if {![catch {$mDataPointCallback $mLastMouseRayStart $mLastMouseRayTarget $partitions} point]} {
		    return $point
		}
	    }

	    set partition [lindex $partitions 0]

	    if {[catch {bu_get_value_by_keyword in $partition} in]} {
		set mMeasuringStick3DCurrent 0
		#		putString "Partition does not contain an \"in\""
		#		putString "$in"
		return $mLastMouseRayTarget
	    }

	    if {[catch {bu_get_value_by_keyword point $in} point]} {
		set mMeasuringStick3DCurrent 0
		#		putString "Partition does not contain an \"in\" point"
		#		putString "$point"
		return $mLastMouseRayTarget
	    }
	}
    } else {
	set dtype [lindex $pdata 0]
	set dindex [lindex $pdata 1]

	if {$dtype == "data_labels"} {
	    set labels [$mGed data_labels $itk_component($_pane) labels]
	    set label [lindex $labels $dindex]
	    set point [lindex $label 1]
	} else {
	    set points [$mGed $dtype $itk_component($_pane) points]
	    set point [lindex $points $dindex]
	}
    }

    return $point
}

::itcl::body cadwidgets::Ged::pane_mouse_data_label {_pane _x _y} {
    set mLastMousePos "$_x $_y"
    set point [pane_mouse_3dpoint $_pane $_x $_y]

    if {$point == ""} {
	return
    }

    #XXX Temporarily depend on callbacks to create the label
    # should prompt the user for a label via the callback, then create

    foreach callback $mDataLabelCallbacks {
	catch {$callback $point}
    }
}

::itcl::body cadwidgets::Ged::pane_mouse_data_pick {_pane _x _y} {
    set pdata [$mGed data_pick $itk_component($_pane) $_x $_y]

    if {[llength $mMouseDataCallbacks] == 0} {
	tk_messageBox -message "pdata - $pdata"
    } else {
	foreach callback $mMouseDataCallbacks {
	    catch {$callback $pdata} msg
	}
    }
}

::itcl::body cadwidgets::Ged::pane_mouse_ray {_pane _x _y {_pflag 0}} {
    set mLastMouseRayPos "$_x $_y"

    set view [$mGed screen2view $itk_component($_pane) $_x $_y]
    set view [$mGed snap_view $itk_component($_pane) [lindex $view 0] [lindex $view 1]]

    set mRayCurrWho [$mGed who]

    set bounds [$mGed bounds $itk_component($_pane)]
    set vZ [lindex $bounds 5]
    set mLastMouseRayStart [$mGed v2m_point $itk_component($_pane) [lindex $view 0] [lindex $view 1] $vZ]
    set mLastMouseRayTarget [$mGed v2m_point $itk_component($_pane) [lindex $view 0] [lindex $view 1] 0]

    if {[catch {shoot_ray $mLastMouseRayStart "at" $mLastMouseRayTarget 1 1 0} partitions]} {
	return $partitions
    }

    if {$_pflag} {
	return $partitions
    }

    # mMouseRayCallbacks is not currently active
    if {[llength $mMouseRayCallbacks] == 0} {
	set partition [lindex $partitions 0]

	if {$partition == {}} {
	    tk_messageBox -message "Nothing hit"
	} else {
	    set region [bu_get_value_by_keyword "region" $partition]
	    tk_messageBox -message [$mGed l $region]
	}
    } else {
	foreach callback $mMouseRayCallbacks {
	    catch {$callback $_pane $mLastMouseRayStart $mLastMouseRayTarget $partitions}
	}
    }
}

::itcl::body cadwidgets::Ged::pane {args} {
    # get the active pane
    if {$args == ""} {
	return $itk_option(-pane)
    }

    # set the active pane
    switch -- $args {
	ul -
	ur -
	ll -
	lr {
	    set itk_option(-pane) $args
	}
	default {
	    return -code error "pane: bad value - $args"
	}
    }

    if {!$itk_option(-multi_pane)} {
	# nothing to do
	if {$mPrivPane == $itk_option(-pane)} {
	    if {$itk_option(-paneCallback) != ""} {
		catch {eval $itk_option(-paneCallback) $args}
	    }

	    return
	}

	switch -- $mPrivPane {
	    ul {
		switch -- $itk_option(-pane) {
		    ur {
			$itk_component(upw) hide ulp
			$itk_component(upw) show urp
		    }
		    ll {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show urp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) show llp
			$itk_component(lpw) hide lrp
		    }
		    lr {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show urp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    ur {
		switch -- $itk_option(-pane) {
		    ul {
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ll {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show ulp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) show llp
			$itk_component(lpw) hide lrp
		    }
		    lr {
			iwidgets::Panedwindow::hide upper
			$itk_component(upw) show ulp
			iwidgets::Panedwindow::show lower
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    ll {
		switch -- $itk_option(-pane) {
		    ul {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show lrp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ur {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show lrp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide ulp
			$itk_component(upw) show urp
		    }
		    lr {
			$itk_component(lpw) hide llp
			$itk_component(lpw) show lrp
		    }
		}
	    }
	    lr {
		switch -- $itk_option(-pane) {
		    ul {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show llp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide urp
			$itk_component(upw) show ulp
		    }
		    ur {
			iwidgets::Panedwindow::hide lower
			$itk_component(lpw) show llp
			iwidgets::Panedwindow::show upper
			$itk_component(upw) hide ulp
			$itk_component(upw) show urp
		    }
		    ll {
			$itk_component(lpw) hide lrp
			$itk_component(lpw) show llp
		    }
		}
	    }
	}
    }

    set mPrivPane $itk_option(-pane)

    if {$itk_option(-paneCallback) != ""} {
	catch {eval $itk_option(-paneCallback) $args}
    }
}

::itcl::body cadwidgets::Ged::shoot_ray {_start _op _target _prep _no_bool _onehit} {
    SetWaitCursor $this

    set result ""
    catch {
	if {$mRayCurrWho != $mRayLastWho} {
	    eval $mGed rt_gettrees $mRay -i -u $mRayCurrWho
	    $mRay prep $_prep
	    $mRay no_bool $_no_bool
	    $mRay onehit $_onehit
	    set mRayLastWho $mRayCurrWho
	}

	set result [$mRay shootray $_start $_op $_target]
    }

    SetNormalCursor $this

    return $result
}

::itcl::body cadwidgets::Ged::add_begin_data_arrow_callback {_callback} {
    set i [lsearch $mBeginDataArrowCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mBeginDataArrowCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_begin_data_arrow_callback_list {} {
    set mBeginDataArrowCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_begin_data_arrow_callback {_callback} {
    set i [lsearch $mBeginDataArrowCallbacks $_callback]
    if {$i != -1} {
	set mBeginDataArrowCallbacks [lreplace $mBeginDataArrowCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_end_data_arrow_callback {_callback} {
    set i [lsearch $mEndDataArrowCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mEndDataArrowCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_end_data_arrow_callback_list {} {
    set mEndDataArrowCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_end_data_arrow_callback {_callback} {
    set i [lsearch $mEndDataArrowCallbacks $_callback]
    if {$i != -1} {
	set mEndDataArrowCallbacks [lreplace $mEndDataArrowCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_begin_data_line_callback {_callback} {
    set i [lsearch $mBeginDataLineCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mBeginDataLineCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_begin_data_line_callback_list {} {
    set mBeginDataLineCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_begin_data_line_callback {_callback} {
    set i [lsearch $mBeginDataLineCallbacks $_callback]
    if {$i != -1} {
	set mBeginDataLineCallbacks [lreplace $mBeginDataLineCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_end_data_line_callback {_callback} {
    set i [lsearch $mEndDataLineCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mEndDataLineCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_end_data_line_callback_list {} {
    set mEndDataLineCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_end_data_line_callback {_callback} {
    set i [lsearch $mEndDataLineCallbacks $_callback]
    if {$i != -1} {
	set mEndDataLineCallbacks [lreplace $mEndDataLineCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_begin_data_move_callback {_callback} {
    set i [lsearch $mBeginDataMoveCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mBeginDataMoveCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_begin_data_move_callback_list {} {
    set mBeginDataMoveCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_begin_data_move_callback {_callback} {
    set i [lsearch $mBeginDataMoveCallbacks $_callback]
    if {$i != -1} {
	set mBeginDataMoveCallbacks [lreplace $mBeginDataMoveCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_end_data_move_callback {_callback} {
    set i [lsearch $mEndDataMoveCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mEndDataMoveCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_end_data_move_callback_list {} {
    set mEndDataMoveCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_end_data_move_callback {_callback} {
    set i [lsearch $mEndDataMoveCallbacks $_callback]
    if {$i != -1} {
	set mEndDataMoveCallbacks [lreplace $mEndDataMoveCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_mouse_ray_callback {_callback} {
    set i [lsearch $mMouseRayCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mMouseRayCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_mouse_ray_callback_list {} {
    set mMouseRayCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_mouse_ray_callback {_callback} {
    set i [lsearch $mMouseRayCallbacks $_callback]
    if {$i != -1} {
	set mMouseRayCallbacks [lreplace $mMouseRayCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_mouse_data_callback {_callback} {
    set i [lsearch $mMouseDataCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mMouseDataCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_mouse_data_callback_list {} {
    set mMouseDataCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_mouse_data_callback {_callback} {
    set i [lsearch $mMouseDataCallbacks $_callback]
    if {$i != -1} {
	set mMouseDataCallbacks [lreplace $mMouseDataCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_data_label_callback {_callback} {
    set i [lsearch $mDataLabelCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mDataLabelCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_data_label_callback_list {} {
    set mDataLabelCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_data_label_callback {_callback} {
    set i [lsearch $mDataLabelCallbacks $_callback]
    if {$i != -1} {
	set mDataLabelCallbacks [lreplace $mDataLabelCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_data_move_callback {_callback} {
    set i [lsearch $mDataMoveCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mDataMoveCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_data_move_callback_list {} {
    set mDataMoveCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_data_move_callback {_callback} {
    set i [lsearch $mDataMoveCallbacks $_callback]
    if {$i != -1} {
	set mDataMoveCallbacks [lreplace $mDataMoveCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_view_measure_callback {_callback} {
    set i [lsearch $mViewMeasureCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mViewMeasureCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_view_measure_callback_list {} {
    set mViewMeasureCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_view_measure_callback {_callback} {
    set i [lsearch $mViewMeasureCallbacks $_callback]
    if {$i != -1} {
	set mViewMeasureCallbacks [lreplace $mViewMeasureCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::add_view_rect_callback {_callback} {
    set i [lsearch $mViewRectCallbacks $_callback]

    # Add if not already in list
    if {$i == -1} {
	lappend mViewRectCallbacks $_callback
    }
}

::itcl::body cadwidgets::Ged::clear_view_rect_callback_list {} {
    set mViewRectCallbacks {}
}

::itcl::body cadwidgets::Ged::delete_view_rect_callback {_callback} {
    set i [lsearch $mViewRectCallbacks $_callback]
    if {$i != -1} {
	set mViewRectCallbacks [lreplace $mViewRectCallbacks $i $i]
    }
}

::itcl::body cadwidgets::Ged::set_data_point_callback {_callback} {
    set mDataPointCallback $_callback
}

::itcl::body cadwidgets::Ged::get_ged_color {_color} {
    switch -- $_color {
	"Grey" {
	    return "64/64/64"
	}
	"Black" {
	    return "0/0/0"
	}
	"Navy" {
	    return "0/0/50"
	}
	"Blue" {
	    return "0/0/255"
	}
	"Cyan" {
	    return "0/255/255"
	}
	"Green" {
	    return "0/255/0"
	}
	"Magenta" {
	    return "255/0/255"
	}
	"Red" {
	    return "255/0/0"
	}
	"Yellow" {
	    return "255/255/0"
	}
	"White" -
	default {
	    return "255/255/255"
	}
    }
}

::itcl::body cadwidgets::Ged::get_rgb_color {_color} {
    switch -- $_color {
	"Grey" {
	    return "64 64 64"
	}
	"Black" {
	    return "0 0 0"
	}
	"Navy" {
	    return "0 0 50"
	}
	"Blue" {
	    return "0 0 255"
	}
	"Cyan" {
	    return "0 255 255"
	}
	"Green" {
	    return "0 255 0"
	}
	"Magenta" {
	    return "255 0 255"
	}
	"Red" {
	    return "255 0 0"
	}
	"Yellow" {
	    return "255 255 0"
	}
	"White" -
	default {
	    return "255 255 255"
	}
    }
}

::itcl::body cadwidgets::Ged::get_vdraw_color {_color} {
    switch -- $_color {
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

::itcl::body cadwidgets::Ged::tk_to_rgb {_tkcolor} {
    if {![regexp {^\#([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})$} $_tkcolor all r g b]} {
	return {128 128 128}
    }

    return [list [expr int(0x$r)] [expr int(0x$g)] [expr int(0x$b)]]
}

::itcl::body cadwidgets::Ged::rgb_to_tk {_r _g _b} {
    return [format \#%.2x%.2x%.2x $_r $_g $_b]
}



############################### Commands that still need to be resolved ###############################
::itcl::body cadwidgets::Ged::set_outputHandler {args} {
   eval set_output_script $args
}

::itcl::body cadwidgets::Ged::fb_active {args} {
    eval set_fb_mode $args
}


############################### Protected Methods ###############################

::itcl::body cadwidgets::Ged::init_button_no_op_prot {{_button 1}} {
    foreach dm {ur ul ll lr} {
	bind $itk_component($dm) <$_button> ""
	bind $itk_component($dm) <ButtonRelease-$_button> ""
#	$mGed rect $itk_component($dm) draw 0
    }
}

::itcl::body cadwidgets::Ged::measure_line_erase {} {
    if {$mMeasureLineActive} {
	catch {$mGed vdraw vlist delete $MEASURING_STICK}
	catch {$mGed erase _VDRW$MEASURING_STICK}
	set mMeasureLineActive 0
    }
}

::itcl::body cadwidgets::Ged::multi_pane {args} {
    # get multi_pane
    if {$args == ""} {
	return $itk_option(-multi_pane)
    }

    # nothing to do
    if {$args == $mPrivMultiPane} {
	return
    }

    switch -- $args {
	0 {
	    set mPrivMultiPane 1
	    toggle_multi_pane
	}
	1 {
	    set mPrivMultiPane 0
	    toggle_multi_pane
	}
	default {
	    return -code error "mult_pane: bad value - $args"
	}
    }
}

::itcl::body cadwidgets::Ged::new_view {args} {
    eval $mGed new_view $args
}

::itcl::body cadwidgets::Ged::toggle_multi_pane {} {
    if {$mPrivMultiPane} {
	set itk_option(-multi_pane) 0
	set mPrivMultiPane 0

	switch -- $itk_option(-pane) {
	    ul {
		iwidgets::Panedwindow::hide lower
		$itk_component(upw) hide urp
	    }
	    ur {
		iwidgets::Panedwindow::hide lower
		$itk_component(upw) hide ulp
	    }
	    ll {
		iwidgets::Panedwindow::hide upper
		$itk_component(lpw) hide lrp
	    }
	    lr {
		iwidgets::Panedwindow::hide upper
		$itk_component(lpw) hide llp
	    }
	}
    } else {
	set itk_option(-multi_pane) 1
	set mPrivMultiPane 1

	switch -- $itk_option(-pane) {
	    ul {
		iwidgets::Panedwindow::show lower
		$itk_component(upw) show urp
	    }
	    ur {
		iwidgets::Panedwindow::show lower
		$itk_component(upw) show ulp
	    }
	    ll {
		iwidgets::Panedwindow::show upper
		$itk_component(lpw) show lrp
	    }
	    lr {
		iwidgets::Panedwindow::show upper
		$itk_component(lpw) show llp
	    }
	}
    }
}


############################### Private Methods ###############################

::itcl::body cadwidgets::Ged::help_init {} {
    set help [cadwidgets::Help \#auto]

    $help add ?			{{} {list available commands}}
    $help add apropos		{{key} {list relevent commands given a keyword}}
    $help add 3ptarb		{{name x1 y1 z1 x2 y2 z2 x3 y3 z3 coord c1 c2 th} {creates an arb}}
    $help add adc		{{args} {set/get adc attributes}}
    $help add adjust		{{} {adjust database object parameters}}
    $help add ae2dir		{{[-i] az el} {return the view direction}}
    $help add aet		{{["az el tw"]} {set/get the azimuth, elevation and twist}}
    $help add analyze		{{object(s)} {analyze objects}}
    $help add arb		{{name rot fb} {creates an arb}}
    $help add arced     	{{a/b anim_cmd ...} {edit the matrix, etc., along an arc}}
    $help add arot		{{x y z angle} {rotate about axis x,y,z by angle (degrees)}}
    $help add attr      	{{{set|get|rm|append} object [args]}
	      		{set, get, remove or append to attribute values for the specified object.
	    		for the "set" subcommand, the arguments are attribute name/value pairs
			for the "get" subcommand, the arguments are attribute names
	    		for the "rm" subcommand, the arguments are attribute names
	    		for the "append" subcommand, the arguments are attribute name/value pairs}}
    $help add autoview		{{view_obj} {set the view object's size and center}}
    $help add bb		{{object} {Report the size of the bounding box (rpp) containing the specified object}}
    $help add bev		{{[P|t] new_obj obj1 op obj2 ...} {boolean evaluation of objects via NMG's}}
    $help add blast		{{"-C#/#/# <objects>"} {clear screen, draw objects}}
    $help add bo		{{(-i|-o) major_type minor_type dest source}
	      		{manipulate opaque objects.
	    		Must specify one of -i (for creating or adjusting objects (input))
	    		or -o for extracting objects (output).
	    		If the major type is "u" the minor type must be one of:
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
	    		Only uniform array binary objects (major_type=u) are currently supported}}
    $help add bot_condense	{{new_bot old_bot} {create a new bot by condensing the old bot}}
    $help add bot_decimate	{{[options] new_bot old_bot} {create a new bot by decimating the old bot}}
    $help add bot_dump	{{[-b] [-m directory] [-o file] [-t dxf|obj|sat|stl] [-u units] [bot1 bot2 ...]\n} {dump the specified bots}}
    $help add bot_face_fuse	{{new_bot old_bot} {eliminate duplicate faces in a BOT solid}}
    $help add bot_face_sort	{{triangles_per_piece bot_solid1 [bot_solid2 bot_solid3 ...]} {sort the facelist of BOT solids to optimize ray trace performance for a particular number of triangles per raytrace piece}}
    $help add bot_merge		{{bot_dest bot1_src [botn_src]} {merge the specified bots into bot_dest}}
    $help add bot_smooth	{{[-t norm_tolerance_degrees] new_bot old_bot} {calculate vertex normals for BOT primitive}}
    $help add bot_split		{{bot} {split the bot}}
    $help add bot_vertex_fuse	{{new_bot old_bot} {}}
    $help add c		{{[-gr] comb_name <boolean_expr>} {create or extend a combination using standard notation}}
    $help add cat	{{<objects>} {list attributes (brief)}}
    $help add center		{{["x y z"]} {set/get the view center}}
    $help add clear		{{} {clear screen}}
    $help add clone		{{[options] object} {clone the specified object}}
    $help add coord		{{[m|v]} {set/get the coodinate system}}
    $help add color		{{low high r g b str} {make color entry}}
    $help add comb		{{comb_name <operation solid>} {create or extend combination w/booleans}}
    $help add comb_color 	{{comb R G B} {set combination's color}}
    $help add combmem		{{comb_name <op name az el tw tx ty tz sa sx sy sz ...>} {set/get comb members}}
    $help add copyeval		{{new_solid path_to_old_solid}	{copy an 'evaluated' path solid}}
    $help add copymat		{{a/b c/d}	{copy matrix from one combination's arc to another's}}
    $help add cp		{{from to} {copy [duplicate] object}}
    $help add cpi		{{from to}	{copy cylinder and position at end of original cylinder}}
    $help add dbconcat		{{[-t] [-u] [-c] [-s|-p] file [prefix]} {concatenate 'file' onto end of present database.  Run 'dup file' first.}}
    $help add dbfind		{{[-s] <objects>} {find all references to objects}}
    $help add dbip		{{} {get dbip}}
    $help add dbot_dump	{{[-b] [-m directory] [-o file] [-t dxf|obj|sat|stl] [-u units] \n} {dump the displayed bots}}
    $help add decompose		{{nmg_solid [prefix]}	{decompose nmg_solid into maximally connected shells}}
    $help add delay		{{sec usec} {delay processing for the specified amount of time}}
    $help add dir2ae		{{az el} {returns a direction vector given the azimuth and elevation}}
    $help add draw		{{"-C#/#/# <objects>"} {draw objects}}
    $help add dump		{{file} {write current state of database object to file}}
    $help add dup		{{file [prefix]} {check for dup names in 'file'}}
    $help add E			{{[-s] <objects>} {evaluated edit of objects. Option 's' provides a slower, but better fidelity evaluation}}
    $help add eac		{{air_code(s)} {draw objects with the specified air codes}}
    $help add echo		{{args} {echo the specified args to the command window}}
    $help add edcodes		{{object(s)} {edit the various codes for the specified objects}}
    $help add edcolor		{{} {edit the color table}}
    $help add edcomb		{{comb rflag rid air los mid} {modify combination record information}}
    $help add edit		{{[help] subcmd args} {edit objects via subcommands}}
    $help add edmater		{{comb1 [comb2 ...]} {edit combination materials}}
    $help add erase		{{<objects>} {remove objects from the screen}}
    $help add ev		{{"[-dfnqstuvwT] [-P #] <objects>"} {evaluate objects via NMG tessellation}}
    $help add expand		{{expression} {globs expression against database objects}}
    $help add eye		{{mx my mz} {set eye point to given model coordinates}}
    $help add eye_pos		{{mx my mz} {set eye position to given model coordinates}}
    $help add exists		{{object} {check for the existence of object}}
    $help add facetize		{{[-m] [-n] [-t] [-T] new_obj old_obj [old_obj2 old_obj3 ...]} {create a new bot object by facetizing the specified objects}}
    $help add form		{{objType} {returns form of objType}}
    $help add fracture		{{} {}}
    $help add g			{{groupname <objects>} {group objects}}
    $help add get		{{obj ?attr?} {get obj attributes}}
    $help add get_autoview	{{} {get view parameters that shows drawn geometry}}
    $help add get_comb		{{comb} {get the specified combination as a list}}
    $help add get_eyemodel	{{} {get the viewsize, orientation and eye_pt of the current view}}
    $help add get_type		{{object} {returns the object type}}
    $help add glob		{{expression} {returns a list of objects specified by expression}}
    $help add gqa		{{options object(s)} {perform quantitative analysis checks on geometry}}
    $help add grid		{{color|draw|help|mrh|mrv|rh|rv|snap|vars|vsnap [args]} {get/set grid attributes}}
    $help add help		{{cmd} {returns a help string for cmd}}
    $help add hide		{{[objects]} {set the "hidden" flag for the specified objects so they do not appear in a "t" or "ls" command output}}
    $help add how		{{obj} {returns how an object is being displayed}}
    $help add i			{{obj combination [operation]} {add instance of obj to comb}}
    $help add idents		{{file object(s)} {dump the idents for the specified objects to file}}
    $help add illum		{{name} {illuminate object}}
    $help add importFg4Section	{{obj section} {create an object by importing the specified section}}
    $help add in		{{args} {creates a primitive by prompting the user for input}}
    $help add inside		{{out_prim in_prim th(s)} {Creates in_prim as the inside of out_prim}}
    $help add isize		{{} {returns the inverse of view size}}
    $help add item		{{region ident [air [material [los]]]} {set region ident codes}}
    $help add keep		{{keep_file object(s)} {save named objects in specified file}}
    $help add keypoint		{{[point]} {set/get the keypoint}}
    $help add kill		{{[-f] <objects>} {delete object[s] from file}}
    $help add killall		{{<objects>} {kill object[s] and all references}}
    $help add killrefs		{{} {}}
    $help add killtree		{{<object>} {kill complete tree[s] - BE CAREFUL}}
    $help add l			{{[-r] <object(s)>} {list attributes (verbose). Objects may be paths}}
    $help add label		{{[-n] obj} {label objects}}
    $help add light		{{[0|1]} {get/set the light mode}}
    $help add listen		{{[n]} {get/set the port to listen on for rt applications}}
    $help add listeval		{{} {lists 'evaluated' path solids}}
    $help add loadview		{{file} {loads a view from file}}
    $help add log		{{get|start|stop} {used to control logging}}
    $help add lookat		{{x y z} {adjust view to look at given coordinates}}
    $help add ls		{{[-a -c -r -s]} {table of contents}}
    $help add lt		{{object} {return first level tree as list of operator/member pairs}}
    $help add m2v_point		{{x y z} {convert xyz in model space to xyz in view space}}
    $help add make		{{-t | object type} {make an object/primitive of the specified type}}
    $help add make_bb		{{bbname object(s)} {make a bounding box (rpp) around the specified objects}}
    $help add make_name		{{template | -s [num]} {make a unique name}}
    $help add make_pnts		{{object_name path_and_filename file_format units_or_conv_factor default_diameter} {creates a point-cloud}}
    $help add match		{{exp} {returns all database objects matching the given expression}}
    $help add mater		{{region shader R G B inherit} {modify region's material information}}
    $help add mirror		{{[-p point] [-d dir] [-x] [-y] [-z] [-o offset] old new}	{mirror object along the specified axis}}
    $help add model2view	{{} {returns the model2view matrix}}
    $help add move_arb_edge	{{arb edge pt} {move an arb's edge through pt}}
    $help add move_arb_face	{{arb face pt} {move an arb's face through pt}}
    $help add mv		{{old new} {rename object}}
    $help add mvall		{{old new} {rename object everywhere}}
    $help add nirt		{{[nirt(1) options] [x y z]}	{trace a single ray from current view}}
    $help add nmg_collapse    	{{nmg_solid new_solid maximum_error_distance [minimum_allowed_angle]}	{decimate NMG solid via edge collapse}}
    $help add nmg_simplify    	{{[arb|tgc|ell|poly] new_solid nmg_solid}	{simplify nmg_solid, if possible}}
    $help add ocenter 		{{obj [x y z]} {get/set center for obj}}
    $help add orient		{{x y z w} {set view direction from quaternion}}
    $help add orotate		{{x y z} {rotate object}}
    $help add oscale		{{sf} {scale object}}
    $help add otranslate 	{{x y z} {translate object}}
    $help add overlay		{{file.pl [name]} {overlay the specified 2D/3D UNIX plot file}}
    $help add pathlist		{{name(s)}	{list all paths from name(s) to leaves}}
    $help add paths		{{pattern} {lists all paths matching input path}}
    $help add perspective	{{[angle]} {set/get the perspective angle}}
    $help add plot		{{file [2|3] [f] [g] [z]} {creates a plot file of the current view}}
    $help add pmat		{{} {get the perspective matrix}}
    $help add pmodel2view	{{} {get the pmodel2view matrix}}
    $help add png		{{[-c r/g/b] [-s size] file} {creates a png file of the current view (wirefram only)}}
    $help add pov		{{args}	{experimental:  set point-of-view}}
    $help add prcolor		{{} {print color and material table}}
    $help add prefix		{{new_prefix object(s)} {prefix each occurrence of object name(s)}}
    $help add preview		{{[-v] [-d sec_delay] [-D start frame] [-K last frame] rt_script_file} {preview new style RT animation script}}
    $help add ps		{{[-f font] [-t title] [-c creator] [-s size in inches] [-l linewidth] file} {creates a postscript file of the current view}}
    $help add push		{{object[s]} {pushes object's path transformations to solids}}
    $help add put		{{object data} {creates an object}}
    $help add put_comb		{{comb_name is_Region id air material los color shader inherit boolean_expr} {create a combination}}
    $help add putmat		{{a/b I|m0 m1 ... m15} {put the specified matrix on a/b}}
    $help add qray		{{subcommand}	{get/set query_ray characteristics}}
    $help add quat		{{[a b c d]} {get/set the view orientation as a quaternion}}
    $help add qvrot		{{x y z angle} {set the view given a direction vector and an angle of rotation}}
    $help add r			{{region <operation solid>} {create or extend a Region combination}}
    $help add rcodes		{{file} {read codes from file}}
    $help add red		{{comb} {edit comb}}
    $help add regdef		{{item air los mat} {get/set region defaults}}
    $help add regions		{{file object(s)} {returns an ascii summary of regions}}
    $help add report		{{[lvl]} {print solid table & vector list}}
    $help add rfarb		{{} {makes an arb given a point, 2 coords of 3 points, rot, fb and thickness}}
    $help add rm		{{comb <members>} {remove members from comb}}
    $help add rmap		{{} {returns a region ids to region(s) mapping}}
    $help add rmat		{{} {get/set the rotation matrix}}
    $help add rmater		{{file} {read material properties from a file}}
    $help add rot		{{"x y z"} {rotate the view}}
    $help add rot_about		{{[e|k|m|v]} {set/get the rotate about point}}
    $help add rot_point		{{x y z} {rotate xyz by the current view rotation matrix}}
    $help add rotate_arb_face	{{arb face pt} {rotate an arb's face through pt}}
    $help add rrt		{{rt_application [options]} {run the specified rt application}}
    $help add rselect		{{} {select objects within a previously define rectangle}}
    $help add rt		{{[options] [-- objects]} {do raytrace of view or specified objects}}
    $help add rt_gettrees      	{{[-i] [-u] pname object} {create a raytracing object}}
    $help add rtabort		{{} {abort the associated raytraces}}
    $help add rtarea		{{[options] [-- objects]} {calculate area of specified objects}}
    $help add rtcheck		{{[options]} {check for overlaps in current view}}
    $help add rtedge		{{[options] [-- objects]} {do raytrace of view or specified objects yielding only edges}}
    $help add rtweight		{{[options] [-- objects]} {calculate weight of specified objects}}
    $help add savekey		{{file [time]} {save key frame data to file}}
    $help add saveview		{{[-e] [-i] [-l] [-o] filename [args]} {save the current view to file}}
    $help add sca		{{sfactor} {scale by sfactor}}
    $help add screengrab	{{imagename.ext}	{output active graphics window to image file typed by extension(i.e. mged> screengrab imagename.png)\n");}}
    $help add search		{{options} {see search man page}}
    $help add select		{{vx vy {vr | vw vh}} {select objects within the specified circle or rectangle}}
    $help add setview		{{x y z} {set the view given angles x, y, and z in degrees}}
    $help add shaded_mode	{{[0|1|2]}	{get/set shaded mode}}
    $help add shader		{{comb shader_material [shader_args]} {command line version of the mater command}}
    $help add shells		{{nmg_model}	{breaks model into seperate shells}}
    $help add showmats		{{path}	{show xform matrices along path}}
    $help add size		{{vsize} {set/get the view size}}
    $help add slew		{{"x y"} {slew the view}}
    $help add snap_view		{{vx vy} {snap the view to grid}}
    $help add solids		{{file object(s)} {returns an ascii summary of solids}}
    $help add summary		{{[s r g]}	{count/list solid/reg/groups}}
    $help add sync		{{} {sync the in memory database to disk}}
    $help add tire		{{[options] tire_top} {create a tire}}
    $help add title		{{?string?} {print or change the title}}
    $help add tol		{{"[abs #] [rel #] [norm #] [dist #] [perp #]"} {show/set tessellation and calculation tolerances}}
    $help add tops		{{} {find all top level objects}}
    $help add tra		{{[-v|-m] "x y z"} {translate the view}}
    $help add track		{{args} {create a track}}
    $help add tree		{{[-c] [-i n] [-d n] [-o outfile] object(s)} {print out a tree of all members of an object, or all members to depth n in the tree if n -d option is supplied}}
    $help add unhide		{{[objects]} {unset the "hidden" flag for the specified objects so they will appear in a "t" or "ls" command output}}
    $help add units		{{[mm|cm|m|in|ft|...]}	{change units}}
    $help add v2m_point		{{x y z} {convert xyz in view space to xyz in model space}}
    $help add vdraw		{{write|insert|delete|read|length|show [args]} {vector drawing (cnuzman)}}
    $help add version		{{} {return the database version}}
    $help add view		{{quat|ypr|aet|center|eye|size [args]} {get/set view parameters}}
    $help add view2model	{{} {returns the view to model matrix}}
    $help add viewDir		{{[-i]} {return the view direction}}
    $help add vmake		{{pname ptype} {make a primtive of ptype and size it according to the view}}
    $help add vnirt		{{options vX vY} {trace a single ray aimed at (vX, vY) in view coordinates}}
    $help add wcodes		{{file object(s)} {write codes to file for the specified object(s)}}
    $help add whatid		{{region_name} {display ident number for region}}
    $help add which_shader	{{[-s] shader(s)} {returns a list of objects that make use of the specified shader(s)}}
    $help add whichair		{{air_codes(s)} {lists all regions with given air code}}
    $help add whichid		{{[-s] ident(s)} {lists all regions with given ident code}}
    $help add who		{{[r(eal)|p(hony)|b(oth)]} {list the top-level objects currently being displayed}}
    $help add wmater		{{file comb1 [comb2 ...]} {write material properties to a file for the specified combinations}}
    $help add xpush		{{object} {Experimental Push Command}}
    $help add ypr		{{yaw pitch roll} {set the view orientation given the yaw, pitch and roll}}
    $help add zap		{{} {clear screen}}
    $help add zoom		{{sf} {zoom view by specified scale factor}}
    $help add data_arrows	{{} {}}
    $help add data_axes		{{} {}}
    $help add data_labels	{{} {}}
    $help add data_lines	{{} {}}
    $help add data_move 	{{} {}}
    $help add data_pick 	{{} {}}
    $help add sdata_arrows	{{} {}}
    $help add sdata_axes	{{} {}}
    $help add sdata_labels	{{} {}}
    $help add sdata_lines	{{} {}}
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
