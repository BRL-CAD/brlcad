#                      A R C H E R . T C L
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
#    Archer megawidget.
#

namespace eval Archer {
    if {![info exists debug]} {
	set debug 0
    }

    set methodDecls ""
    set methodImpls ""
    set extraMgedCommands ""
    set corePluginInit ""
    set pluginsdir [file join $env(ARCHER_HOME) plugins archer]
    if {![file exists $pluginsdir]} {
	set pluginsdir [file join [bu_brlcad_data "plugins"] archer]
    }
    if {![file exists $pluginsdir]} {
	set pluginsdir [file join [bu_brlcad_data "src"] archer plugins]
    }

    if {[file exists [file join $pluginsdir Core]]} {
	set savePwd [pwd]
	cd [file join $pluginsdir Core]
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
    if {[file exists [file join $pluginsdir Commands]]} {
	set savePwd [pwd]
	cd [file join $pluginsdir Commands]
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

LoadArcherLibs
package require ArcherCore 1.0
package provide Archer 1.0

::itcl::class Archer {
    inherit ArcherCore

    constructor {{_viewOnly 0} {_noCopy 0} args} {
	ArcherCore::constructor $_viewOnly $_noCopy
    } {}
    destructor {}

    public {
	# Public Class Variables
	common plugins ""
	common pluginMajorTypeCore "Core"
   	common pluginMajorTypeCommand "Command"
   	common pluginMajorTypeWizard "Wizard"
   	common pluginMajorTypeUtility "Utility"
   	common pluginMinorTypeMged "Mged"
   	common pluginMinorTypeAll "All"

	# Plugins Section
	proc initArcher {}
	proc pluginDialog {_w}
	proc pluginLoadCWDFiles {}
	proc pluginLoader {}
	proc pluginMged {_archer}
	proc pluginQuery {_name}
	proc pluginRegister {_majorType _minorType _name _class _file \
				 {_description ""} \
				 {_version ""} \
				 {_developer ""} \
				 {_icon ""} \
				 {_tooltip ""} \
				 {_action ""} \
				 {_xmlAction ""}}
	proc pluginUnregister {_name}

	method pluginGetMinAllowableRid {}
   	method pluginUpdateProgressBar {percent}
	method pluginUpdateSaveMode {mode}
	method pluginUpdateStatusBar {msg}

	method importFg4Sections   {_slist _wlist _delta}
	method purgeHistory        {}

	# ArcherCore Override Section
	method Load                {_target}
	method updateTheme {}
    }

    protected {
	variable mArcherVersion "0.9.2"
	variable mActiveEditDialogs {}
	variable wizardXmlCallbacks ""

	# plugin list
	variable mWizardClass ""
	variable mWizardTop ""
	variable mWizardState ""
	variable mNoWizardActive 0

	# ArcherCore Override Section
	method dblClick {_tags}
	method initDefaultBindings {{_comp ""}}
	method initMged {}
	method selectNode {_tags {_rflag 1}}
	method toggleTreeView {_state}

	# Miscellaneous Section
	method archerWrapper {_cmd _eflag _hflag _sflag _tflag args}
	method buildAboutDialog {}
	method buildBackgroundColor {_parent}
	method buildDisplayPreferences {}
	method buildEmbeddedMenubar {}
	method buildGeneralPreferences {}
	method buildGroundPlanePreferences {}
	method buildInfoDialogs {}
	method buildModelAxesPosition {_parent}
	method buildModelAxesPreferences {}
	method buildMouseOverridesDialog {}
	method buildPreferencesDialog {}
	method buildToplevelMenubar {}
	method buildViewAxesPreferences {}
	method doAboutArcher {}
	method doArcherHelp {}
	method launchDisplayMenuBegin {_dm _m _x _y}
	method launchDisplayMenuEnd {}
	method menuStatusCB {_w}
	method updateCreationButtons {_on}
	method updateModesMenu {}

	# Modes Section
	method setAdvancedMode {}
	method setBasicMode {}
	method setIntermediateMode {}
	method setMode {{_updateFractions 0}}
	method updateCopyMode {}
	method updateCutMode {}
	method updatePasteMode {}
	method updateViewToolbarForEdit {}

	# Object Edit Section
	method applyEdit {}
	method copyObj {}
	method cutObj {}
	method initEdit {}
	method finalizeObjEdit {_obj _path}
	method gotoNextObj {}
	method gotoPrevObj {}
	method pasteObj {}
	method purgeObjHistory {obj}
	method resetEdit {}
	method updateNextObjButton {_obj}
	method updateObjHistory {_obj}
	method updatePrevObjButton {_obj}

	# Object Edit VIA Mouse Section
	method beginObjRotate {}
	method beginObjScale {}
	method beginObjTranslate {}
	method beginObjCenter {}
	method endObjCenter {_dm _obj}
	method endObjRotate {_dm _obj}
	method endObjScale {_dm _obj}
	method endObjTranslate {_dm _obj}
	method handleObjCenter {_obj _x _y}
	method handleObjRotate {_obj _rx _ry _rz _kx _ky _kz}
	method handleObjScale {_obj _sf _kx _ky _kz}
	method handleObjTranslate {_obj _dx _dy _dz}


	# Object Views Section
	method buildArb4EditView {}
	method buildArb5EditView {}
	method buildArb6EditView {}
	method buildArb7EditView {}
	method buildArb8EditView {}
	method buildBotEditView {}
	method buildCombEditView {}
	method buildDbAttrView {}
	method buildEhyEditView {}
	method buildEllEditView {}
	method buildEpaEditView {}
	method buildEtoEditView {}
	method buildExtrudeEditView {}
	method buildGripEditView {}
	method buildHalfEditView {}
	method buildObjAttrView {}
	method buildObjEditView {}
	method buildObjViewToolbar {}
	method buildPartEditView {}
	method buildPipeEditView {}
	method buildRhcEditView {}
	method buildRpcEditView {}
	method buildSketchEditView {}
	method buildSphereEditView {}
	method buildTgcEditView {}
	method buildTorusEditView {}

	method initArb4EditView {_odata}
	method initArb5EditView {_odata}
	method initArb6EditView {_odata}
	method initArb7EditView {_odata}
	method initArb8EditView {_odata}
	method initBotEditView {_odata}
	method initCombEditView {_odata}
	method initDbAttrView {_name}
	method initEhyEditView {_odata}
	method initEllEditView {_odata}
	method initEpaEditView {_odata}
	method initEtoEditView {_odata}
	method initExtrudeEditView {_odata}
	method initGripEditView {_odata}
	method initHalfEditView {_odata}
	method initNoWizard {_parent _msg}
	method initObjAttrView {}
	method initObjEdit {_obj}
	method initObjEditView {}
	method initObjHistory {_obj}
	method initObjWizard {_obj _wizardLoaded}
	method initPartEditView {_odata}
	method initPipeEditView {_odata}
	method initRhcEditView {_odata}
	method initRpcEditView {_odata}
	method initSketchEditView {_odata}
	method initSphereEditView {_odata}
	method initTgcEditView {_odata}
	method initTorusEditView {_odata}

	method updateObjEdit {_updateObj _needInit _needSave}
	method updateObjEditView {}

	# Plugins Section
	method buildUtilityMenu {}
	method buildWizardMenu {}
	method buildWizardObj {_dialog _wizard _action _oname}
	method invokeUtilityDialog {_class _wname _w}
	method invokeWizardDialog {_class _action _wname}
	method invokeWizardUpdate {_wizard _action _oname _name}
	method updateUtilityMenu {}
	method updateWizardMenu {}

	# Preferences Section
	method applyDisplayPreferences {}
	method applyDisplayPreferencesIfDiff {}
	method applyGeneralPreferences {}
	method applyGeneralPreferencesIfDiff {}
	method applyGroundPlanePreferencesIfDiff {}
	method applyModelAxesPreferences {}
	method applyModelAxesPreferencesIfDiff {}
	method applyPreferences {}
	method applyPreferencesIfDiff {}
	method applyViewAxesPreferences {}
	method applyViewAxesPreferencesIfDiff {}
	method doPreferences {}
	method readPreferences {}
	method writePreferences {}

	# Primitive Creation Section
	method createObj {_type}
	method createArb4 {_name}
	method createArb5 {_name}
	method createArb6 {_name}
	method createArb7 {_name}
	method createArb8 {_name}
	method createBot {_name}
	method createComb {_name}
	method createEhy {_name}
	method createEll {_name}
	method createEpa {_name}
	method createEto {_name}
	method createExtrude {_name}
	method createGrip {_name}
	method createHalf {_name}
	method createPart {_name}
	method createPipe {_name}
	method createRhc {_name}
	method createRpc {_name}
	method createSketch {_name}
	method createSphere {_name}
	method createTgc {_name}
	method createTorus {_name}
    }

    private {
	variable mInstanceInit 1
    }
}


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------
::itcl::body Archer::constructor {{_viewOnly 0} {_noCopy 0} args} {
    # Append a few more commands
    lappend mgedCommands importFg4Sections purgeHistory

    if {!$mViewOnly} {
	if {$ArcherCore::inheritFromToplevel} {
	    buildToplevelMenubar
	    $this configure -menu $itk_component(menubar)
	} else {
	    buildEmbeddedMenubar
	    pack $itk_component(menubar) -side top -fill x -padx 1
	}

    # Populate the primary toolbar
    $itk_component(primaryToolbar) insert 0 button new \
	-balloonstr "Create a new geometry file" \
	-helpstr "Create a new geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this newDb]

    #    $itk_component(primaryToolbar) add button open \
	-balloonstr "Open an existing geometry file" \
	-helpstr "Open an existing geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this openDb]

    #    $itk_component(primaryToolbar) add button save \
	-balloonstr "Save the current geometry file" \
	-helpstr "Save the current geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this saveDb]

    #    $itk_component(primaryToolbar) add button close \
	-image [image create photo \
		    -file [file join $mImgDir Themes $mTheme closeall.png]] \
	-balloonstr "Close the current geometry file" \
	-helpstr "Close the current geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this closeDb]

    # half-size spacer
    $itk_component(primaryToolbar) add frame sep1 \
	-relief sunken \
	-width 2

    $itk_component(primaryToolbar) add button cut \
	-balloonstr "Cut current object" \
	-helpstr "Cut current object" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this cutObj]

    $itk_component(primaryToolbar) add button copy \
	-balloonstr "Copy current object" \
	-helpstr "Copy current object" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this copyObj]

    $itk_component(primaryToolbar) add button paste \
	-balloonstr "Paste object" \
	-helpstr "Paste object" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this pasteObj]

    # half-size spacer
    $itk_component(primaryToolbar) add frame sep2 \
	-relief sunken \
	-width 2

    $itk_component(primaryToolbar) add button preferences \
	-balloonstr "Set application preferences" \
	-helpstr "Set application preferences" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this doPreferences]

    # half-size spacer
    $itk_component(primaryToolbar) add frame sep3 \
	-relief sunken \
	-width 2

    $itk_component(primaryToolbar) add button arb6 \
	-balloonstr "Create an arb6" \
	-helpstr "Create an arb6" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj arb6]

    $itk_component(primaryToolbar) add button arb8 \
	-balloonstr "Create an arb8" \
	-helpstr "Create an arb8" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj arb8]

    $itk_component(primaryToolbar) add button cone \
	-balloonstr "Create a tgc" \
	-helpstr "Create a tgc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj tgc]

    $itk_component(primaryToolbar) add button sphere \
	-balloonstr "Create a sphere" \
	-helpstr "Create a sphere" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj sph]

    $itk_component(primaryToolbar) add button torus \
	-balloonstr "Create a torus" \
	-helpstr "Create a torus" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj tor]

    #    $itk_component(primaryToolbar) add button pipe \
	-balloonstr "Create a pipe" \
	-helpstr "Create a pipe" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj pipe]

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
	-command [::itcl::code $this createObj comb]

    if {$::Archer::plugins != ""} {
	# half-size spacer
	$itk_component(primaryToolbar) add frame sep5 \
	    -relief sunken \
	    -width 2
    }

    buildWizardMenu

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
	-command [::itcl::code $this createObj arb4]
    $itk_component(arbMenu) add command \
	-label arb5 \
	-command [::itcl::code $this createObj arb5]
    $itk_component(arbMenu) add command \
	-label arb6 \
	-command [::itcl::code $this createObj arb6]
    $itk_component(arbMenu) add command \
	-label arb7 \
	-command [::itcl::code $this createObj arb7]
    $itk_component(arbMenu) add command \
	-label arb8 \
	-command [::itcl::code $this createObj arb8]

    $itk_component(primitiveMenu) add cascade \
	-label Arbs \
	-menu $itk_component(arbMenu)
    #    $itk_component(primitiveMenu) add command \
	-label bot \
	-command [::itcl::code $this createObj bot]
    #    $itk_component(primitiveMenu) add command \
	-label comb \
	-command [::itcl::code $this createObj comb]
    #    $itk_component(primitiveMenu) add command \
	-label ehy \
	-command [::itcl::code $this createObj ehy]
    #    $itk_component(primitiveMenu) add command \
	-label ell \
	-command [::itcl::code $this createObj ell]
    #    $itk_component(primitiveMenu) add command \
	-label epa \
	-command [::itcl::code $this createObj epa]
    #    $itk_component(primitiveMenu) add command \
	-label eto \
	-command [::itcl::code $this createObj eto]
    #    $itk_component(primitiveMenu) add command \
	-label extrude \
	-command [::itcl::code $this createObj extrude]
    #    $itk_component(primitiveMenu) add command \
	-label grip \
	-command [::itcl::code $this createObj grip]
    #    $itk_component(primitiveMenu) add command \
	-label half \
	-command [::itcl::code $this createObj half]
    #    $itk_component(primitiveMenu) add command \
	-label part \
	-command [::itcl::code $this createObj part]
    #    $itk_component(primitiveMenu) add command \
	-label pipe \
	-command [::itcl::code $this createObj pipe]
    #    $itk_component(primitiveMenu) add command \
	-label rhc \
	-command [::itcl::code $this createObj rhc]
    #    $itk_component(primitiveMenu) add command \
	-label rpc \
	-command [::itcl::code $this createObj rpc]
    #    $itk_component(primitiveMenu) add command \
	-label sketch \
	-command [::itcl::code $this createObj sketch]
    $itk_component(primitiveMenu) add command \
	-label sph \
	-command [::itcl::code $this createObj sph]
    $itk_component(primitiveMenu) add command \
	-label tgc \
	-command [::itcl::code $this createObj tgc]
    $itk_component(primitiveMenu) add command \
	-label tor \
	-command [::itcl::code $this createObj tor]

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
	-command [::itcl::code $this createObj ehy]

    #    $itk_component(primaryToolbar) add button epa \
	-balloonstr "Create an epa" \
	-helpstr "Create an epa" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj epa]

    #    $itk_component(primaryToolbar) add button rpc \
	-balloonstr "Create an rpc" \
	-helpstr "Create an rpc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj rpc]

    #    $itk_component(primaryToolbar) add button rhc \
	-balloonstr "Create an rhc" \
	-helpstr "Create an rhc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj rhc]

    #    $itk_component(primaryToolbar) add button ell \
	-balloonstr "Create an ellipsoid" \
	-helpstr "Create an ellipsoid" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj ell]

    #    $itk_component(primaryToolbar) add button eto \
	-balloonstr "Create an eto" \
	-helpstr "Create an eto" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj eto]

    #    $itk_component(primaryToolbar) add button half \
	-balloonstr "Create a half space" \
	-helpstr "Create a half space" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj half]

    #    $itk_component(primaryToolbar) add button part \
	-balloonstr "Create a particle" \
	-helpstr "Create a particle" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj part]

    #    $itk_component(primaryToolbar) add button grip \
	-balloonstr "Create a grip" \
	-helpstr "Create a grip" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj grip]

    #    $itk_component(primaryToolbar) add button extrude \
	-balloonstr "Create an extrusion" \
	-helpstr "Create an extrusion" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj extrude]

    #    $itk_component(primaryToolbar) add button sketch \
	-balloonstr "Create a sketch" \
	-helpstr "Create a sketch" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj sketch]

    #    $itk_component(primaryToolbar) add button bot \
	-balloonstr "Create a bot" \
	-helpstr "Create a bot" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj bot]
    }

    eval itk_initialize $args

    if {!$mViewOnly} {
	buildInfoDialogs
	buildSaveDialog
	buildViewCenterDialog
	buildPreferencesDialog
	buildDbAttrView
	buildObjViewToolbar
	buildObjAttrView
	buildObjEditView

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

	readPreferences
	updateCreationButtons 0
	updateSaveMode
	updateCutMode
	updateCopyMode
	updatePasteMode
    } else {
	backgroundColor [lindex $mBackground 0] \
	    [lindex $mBackground 1] \
	    [lindex $mBackground 2]
    }

    set mInstanceInit 0
    updateTheme

    if {!$mViewOnly} {
	# Change the command window's prompt
	$itk_component(cmd) configure -prompt "Archer> "
	$itk_component(cmd) reinitialize
    }
}


# ------------------------------------------------------------
#                       DESTRUCTOR
# ------------------------------------------------------------
::itcl::body Archer::destructor {} {
    writePreferences
}



################################### Public Section ###################################

################################### Plugins Section ###################################
::itcl::body Archer::initArcher {} {
    # Load plugins
    if {[catch {pluginLoader} msg]} {
	tk_messageBox -message "Failed to load plugins\n$msg"
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

::itcl::body Archer::pluginLoader {} {
    global env

    set pwd [::pwd]

    # developer & user plugins
    set pluginPath [file join [bu_brlcad_data "plugins"] archer]
    if { ![file exists $pluginPath] } {
	# try a source dir invocation
	set pluginPath [file join [bu_brlcad_data "src"] archer plugins]
    }
    if { ![file exists $pluginPath] } {
	# give up on loading any plugins
	return
    }

    foreach plugindir [list $pluginPath] {
	::cd $plugindir
	pluginLoadCWDFiles
    }

    ::cd $pwd
}

::itcl::body Archer::pluginMged {_archer} {
    if {[catch {$_archer component mged} mged]} {
	return ""
    } else {
	return $mged
    }
}

::itcl::body Archer::pluginQuery {name} {
    foreach plugin $::Archer::plugins {
	if {[$plugin get -class] == $name} {
	    return $plugin
	}
    }

    return ""
}

::itcl::body Archer::pluginRegister {majorType minorType name class file {description ""} {version ""} {developer ""} {icon ""} {tooltip ""} {action ""} {xmlAction ""}} {
    lappend ::Archer::plugins [Plugin \#auto $majorType $minorType $name $class $file $description $version $developer $icon $tooltip $action $xmlAction]
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

	    if {![catch {$itk_component(mged) get_type $solidName} ret]} {
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
		if {[catch {$itk_component(mged) get_type $gname} ret]} {
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

	    render $pname\.r $vmode $tr 0 0
	}

	# Add wizard attributes
	foreach {key val} $wlist {
	    $itk_component(mged) attr set $wizTop $key $val
	}

	refreshTree
	#	if {$firstName != ""} {
	#	    if {$firstName == $lastName} {
	#		after idle [::itcl::code $this _select_node [$itk_component(tree) find $regionName] 0]
	#	    } else {
	#		after idle [::itcl::code $this _select_node [$itk_component(tree) find $firstName] 0]
	#	    }
	#	}

	set mNeedSave 1
	updateSaveMode
	$itk_component(mged) units $savedUnits
	$itk_component(mged) attachObservers
	$itk_component(mged) refreshAll
	$itk_component(mged) configure -autoViewEnable 1
	SetNormalCursor
    }
}


::itcl::body Archer::purgeHistory {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    foreach obj [$itk_component(mged) ls] {
	set obj [regsub {(/)|(/R)} $obj ""]
	purgeObjHistory $obj
    }

    updatePrevObjButton ""
    updateNextObjButton ""

    set selNode [$itk_component(tree) query -selected]
    if {$selNode != ""} {
	set selNodePath [$itk_component(tree) query -path $selNode]
	toggleTreePath $selNodePath
    }

    set mNeedSave 1
    updateSaveMode
}

::itcl::body Archer::Load {_target} {
    if {$mNeedSave} {
	$itk_component(saveDialog) center [namespace tail $this]
	if {[$itk_component(saveDialog) activate]} {
	    saveDb
	} else {
	    set mNeedSave 0
	    updateSaveMode
	}
    }

    # Get rid of any existing edit dialogs
    foreach ed $mActiveEditDialogs {
	::itcl::delete object $ed
    }
    set mActiveEditDialogs {}

    set mTarget $_target
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

    setColorOption dbCmd -primitiveLabelColor $mPrimitiveLabelColor
    setColorOption dbCmd -scaleColor $mScaleColor
    setColorOption dbCmd -viewingParamsColor $mViewingParamsColor

    set mDbTitle [$itk_component(mged) title]
    set mDbUnits [$itk_component(mged) units]

    if {!$mViewOnly} {
	initDbAttrView $mTarget
	applyPreferences
	doLighting
	updateModesMenu
	updateWizardMenu
	updateUtilityMenu
	deleteTargetOldCopy

	# refresh tree contents
	SetWaitCursor
	refreshTree 0
	SetNormalCursor
    } else {
	applyPreferences
	doLighting
    }

    if {$mBindingMode == 0} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
    }
}

::itcl::body Archer::updateTheme {} {
    if {$mInstanceInit} {
	return
    }

    ArcherCore::updateTheme

    set dir [file join $mImgDir Themes $mTheme]

    if {!$mViewOnly} {
	# Primary 
	$itk_component(primaryToolbar) itemconfigure new \
	    -image [image create photo \
			-file [file join $dir file_new.png]]
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
	#$itk_component(primaryToolbar) itemconfigure pipe \
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

	#$itk_component(primaryToolbar) itemconfigure ehy \
	    -image [image create photo \
			-file [file join $dir primitive_ehy.png]]
	#$itk_component(primaryToolbar) itemconfigure epa \
	    -image [image create photo \
			-file [file join $dir primitive_epa.png]]
	#$itk_component(primaryToolbar) itemconfigure rpc \
	    -image [image create photo \
			-file [file join $dir primitive_rpc.png]]
	#$itk_component(primaryToolbar) itemconfigure rhc \
	    -image [image create photo \
			-file [file join $dir primitive_rhc.png]]
	#$itk_component(primaryToolbar) itemconfigure ell \
	    -image [image create photo \
			-file [file join $dir primitive_ell.png]]
	#$itk_component(primaryToolbar) itemconfigure eto \
	    -image [image create photo \
			-file [file join $dir primitive_eto.png]]
	#$itk_component(primaryToolbar) itemconfigure half \
	    -image [image create photo \
			-file [file join $dir primitive_half.png]]
	#$itk_component(primaryToolbar) itemconfigure part \
	    -image [image create photo \
			-file [file join $dir primitive_part.png]]
	#$itk_component(primaryToolbar) itemconfigure grip \
	    -image [image create photo \
			-file [file join $dir primitive_grip.png]]
	#$itk_component(primaryToolbar) itemconfigure extrude \
	    -image [image create photo \
			-file [file join $dir primitive_extrude.png]]
	#$itk_component(primaryToolbar) itemconfigure sketch \
	    -image [image create photo \
			-file [file join $dir primitive_sketch.png]]
	#$itk_component(primaryToolbar) itemconfigure bot \
	    -image [image create photo \
			-file [file join $dir primitive_bot.png]]

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

################################### End Public Section ###################################



################################### Protected Section ###################################

################################### ArcherCore Override Section ###################################

::itcl::body Archer::dblClick {tags} {
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
		initObjAttrView
	    } else {
		initObjEditView
	    }
	}

	set mPrevSelectedObjPath $mSelectedObjPath
	set mPrevSelectedObj $mSelectedObj
    }

    $itk_component(tree) selection clear
    $itk_component(tree) selection set $tags

    if {!$mViewOnly} {
	updateCutMode
	updateCopyMode
	updatePasteMode
    }

    switch -- $type {
	"leaf"   -
	"branch" {renderComp $node}
    }
}


::itcl::body Archer::initDefaultBindings {{_comp ""}} {
    if {$_comp == ""} {
	if {[info exists itk_component(mged)]} {
	    set _comp $itk_component(mged)
	} else {
	    return
	}
    }

    ArcherCore::initDefaultBindings $_comp

    if {!$mViewOnly} {
	foreach dname {ul ur ll lr} {
	    set dm [$_comp component $dname]
	    set win [$dm component dm]

	    if {$ArcherCore::inheritFromToplevel} {
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

    if {$mMode != 0} {
	$itk_component(viewToolbar) itemconfigure edit_rotate -state normal
	$itk_component(viewToolbar) itemconfigure edit_translate -state normal
	$itk_component(viewToolbar) itemconfigure edit_scale -state normal
	$itk_component(viewToolbar) itemconfigure edit_center -state normal
    }
}

::itcl::body Archer::initMged {} {
    ArcherCore::initMged

    if {!$mViewOnly} {
	if {$ArcherCore::inheritFromToplevel} {
	    $itk_component(filemenu) entryconfigure "Raytrace Control Panel..." -state normal
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
	    $itk_component(menubar) menuconfigure .file.rt  -state normal
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
    }
}

::itcl::body Archer::selectNode {tags {rflag 1}} {
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

    if {!$mViewOnly} {
	if {$mObjViewMode == $OBJ_ATTR_VIEW_MODE} {
	    initObjAttrView
	} else {
	    if {!$mRestoringTree} {
		initObjEditView
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
	updateCutMode
	updateCopyMode
	updatePasteMode
    }
}


::itcl::body Archer::toggleTreeView {state} {
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


################################### Miscellaneous Section ###################################
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
	updateSaveMode
    }

    dbCmd configure -primitiveLabels {}
    if {$tflag} {
	catch {refreshTree}
    }
    SetNormalCursor

    return $ret
}

::itcl::body Archer::buildAboutDialog {} {
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
    $itk_component(licenseDialogTabs) insert end -text "BRL-CAD" -stipple gray25

    incr aboutTabIndex
    $itk_component(aboutDialogTabs) tab configure $aboutTabIndex \
	-window $itk_component(licenseDialogTabs) \
	-fill both

    set licenseTabIndex -1

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

::itcl::body Archer::buildBackgroundColor {parent} {
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
	    -vcmd [::itcl::code $this validateColorComp %P]
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
	    -vcmd [::itcl::code $this validateColorComp %P]
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
	    -vcmd [::itcl::code $this validateColorComp %P]
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


::itcl::body Archer::buildDisplayPreferences {} {
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


::itcl::body Archer::buildEmbeddedMenubar {} {
    # menubar
    itk_component add menubar {
	::iwidgets::menubar $itk_interior.menubar \
	    -helpvariable [::itcl::scope mStatusStr] \
	    -font $mFontText \
	    -activebackground $SystemHighlight \
	    -activeforeground $SystemHighlightText
    } {
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
	    command rt -label "Raytrace Control Panel..." \
		-helpstr "Launch Ray Trace Panel"
	    #	command png -label "Export Geometry to PNG..." \
		-helpstr "Launch PNG Export Panel"
	    #	separator sep1
	    command pref \
		-label "Preferences..." \
		-command [::itcl::code $this doPreferences] \
		-helpstr "Set application preferences"
	    separator sep2
	    command exit \
		-label "Quit" \
		-helpstr "Exit Archer"
	}
    $itk_component(menubar) menuconfigure .file.new \
	-command [::itcl::code $this newDb]
    $itk_component(menubar) menuconfigure .file.open \
	-command [::itcl::code $this openDb]
    $itk_component(menubar) menuconfigure .file.save \
	-command [::itcl::code $this saveDb]
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
    $itk_component(menubar) menuconfigure .file.rt \
	-command [::itcl::code $this raytracePanel] \
	-state disabled
    #    $itk_component(menubar) menuconfigure .file.png \
	-command [::itcl::code $this doPng] \
	-state disabled
    $itk_component(menubar) menuconfigure .file.pref \
	-command [::itcl::code $this doPreferences]
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
	-command [::itcl::code $this doViewReset] \
	-state disabled
    $itk_component(menubar) menuconfigure .display.autoview \
	-command [::itcl::code $this doAutoview] \
	-state disabled
    $itk_component(menubar) menuconfigure .display.center \
	-command [::itcl::code $this doViewCenter] \
	-state disabled
    $itk_component(menubar) menuconfigure .display.standard.front \
	-command [::itcl::code $this doAe 0 0]
    $itk_component(menubar) menuconfigure .display.standard.rear \
	-command [::itcl::code $this doAe 180 0]
    $itk_component(menubar) menuconfigure .display.standard.port \
	-command [::itcl::code $this doAe 90 0]
    $itk_component(menubar) menuconfigure .display.standard.starboard \
	-command [::itcl::code $this doAe -90 0]
    $itk_component(menubar) menuconfigure .display.standard.top \
	-command [::itcl::code $this doAe 0 90]
    $itk_component(menubar) menuconfigure .display.standard.bottom \
	-command [::itcl::code $this doAe 0 -90]
    $itk_component(menubar) menuconfigure .display.standard.35,25 \
	-command [::itcl::code $this doAe 35 25]
    $itk_component(menubar) menuconfigure .display.standard.45,45 \
	-command [::itcl::code $this doAe 45 45]
    $itk_component(menubar) menuconfigure .display.clear \
	-command [::itcl::code $this clear] \
	-state disabled
    $itk_component(menubar) menuconfigure .display.refresh \
	-command [::itcl::code $this refreshDisplay] \
	-state disabled
    #    $itk_component(menubar) menuconfigure .display.toolbars.primary -offvalue 0 -onvalue 1 \
	-variable [::itcl::scope itk_option(-primaryToolbar)] \
	-command [::itcl::code $this doPrimaryToolbar]
    $itk_component(menubar) menuconfigure .display.toolbars.views -offvalue 0 -onvalue 1 \
	-variable [::itcl::scope itk_option(-viewToolbar)] \
	-command [::itcl::code $this doViewToolbar]
    $itk_component(menubar) menuconfigure .display.statusbar -offvalue 0 -onvalue 1 \
	-variable [::itcl::scope itk_option(-statusbar)] \
	-command [::itcl::code $this doStatusbar]

    # Modes Menu
    $itk_component(menubar) add menubutton modes \
	-text "Modes" \

    updateModesMenu
    updateUtilityMenu

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
	-command [::itcl::code $this doAboutArcher]
    #    $itk_component(menubar) menuconfigure .help.overrides \
	-command [::itcl::code $this doMouseOverrides]
}

::itcl::body Archer::buildGeneralPreferences {} {
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
    buildBackgroundColor $itk_component(backgroundColorF)

    buildComboBox $itk_component(generalF) \
	primitiveLabelColor \
	color \
	mPrimitiveLabelColorPref \
	"Primitive Label Color:" \
	$mColorListNoTriple

    buildComboBox $itk_component(generalF) \
	scaleColor \
	scolor \
	mScaleColorPref \
	"Scale Color:" \
	$mColorListNoTriple

    buildComboBox $itk_component(generalF) \
	measuringStickColor \
	mcolor \
	mMeasuringStickColorPref \
	"Measuring Stick Color:" \
	$mColorListNoTriple

    buildComboBox $itk_component(generalF) \
	viewingParamsColor \
	vcolor \
	mViewingParamsColorPref \
	"Viewing Parameters Color:" \
	$mColorListNoTriple

    set tmp_themes [glob [file join $mImgDir Themes *]]
    set themes {}
    foreach theme $tmp_themes {
	set theme [file tail $theme]

	# This is not needed for the released code.
	# However, it won't hurt anything to leave it.
	if {$theme != "CVS" && $theme != ".svn"} {
	    lappend themes $theme
	}
    }
    buildComboBox $itk_component(generalF) \
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


::itcl::body Archer::buildGroundPlanePreferences {} {
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
	    -vcmd [::itcl::code $this validateDouble %P]
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
	    -vcmd [::itcl::code $this validateDouble %P]
    } {}
    itk_component add groundPlaneIntervalUnitsL {
	::label $itk_component(groundPlaneF2).intervalUnitsL \
	    -anchor e \
	    -text "mm"
    } {}

    buildComboBox $itk_component(groundPlaneF2) \
	groundPlaneMajorColor \
	majorColor \
	mGroundPlaneMajorColorPref \
	"Major Color:" \
	$mColorListNoTriple

    buildComboBox $itk_component(groundPlaneF2) \
	groundPlaneMinorColor \
	minorColor \
	mGroundPlaneMinorColorPref \
	"Minor Color:" \
	$mColorListNoTriple

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


::itcl::body Archer::buildInfoDialogs {} {
    global tcl_platform

    buildAboutDialog
    buildMouseOverridesDialog
    #    buildInfoDialog mouseOverridesDialog \
	"Mouse Overrides" $mMouseOverrideInfo \
	370x190 word application

    # Lame tcl/tk won't center the window properly
    # the first time unless we call update.
    update

    after idle "$itk_component(aboutDialog) center; $itk_component(mouseOverridesDialog) center"

    #    if {$tcl_platform(platform) == "windows"} {
    #	wm attributes $itk_component(aboutDialog) -topmost 1
    #	wm attributes $itk_component(mouseOverridesDialog) -topmost 1
    #    }

    #    wm group $itk_component(aboutDialog) [namespace tail $this]
    #    wm group $itk_component(mouseOverridesDialog) [namespace tail $this]
}


::itcl::body Archer::buildModelAxesPosition {parent} {
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
	    -vcmd [::itcl::code $this validateDouble %P]
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
	    -vcmd [::itcl::code $this validateDouble %P]
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
	    -vcmd [::itcl::code $this validateDouble %P]
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


::itcl::body Archer::buildModelAxesPreferences {} {
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

    buildComboBox $itk_component(modelAxesF2) \
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
    buildModelAxesPosition $itk_component(modelAxesF2)

    buildComboBox $itk_component(modelAxesF2) \
	modelAxesLineWidth \
	lineWidth \
	mModelAxesLineWidthPref \
	"Line Width:" \
	{1 2 3}

    buildComboBox $itk_component(modelAxesF2) \
	modelAxesColor \
	color \
	mModelAxesColorPref \
	"Axes Color:" \
	$mColorList

    buildComboBox $itk_component(modelAxesF2) \
	modelAxesLabelColor \
	labelColor \
	mModelAxesLabelColorPref \
	"Label Color:" \
	$mColorListNoTriple

    itk_component add modelAxesTickIntervalL {
	::label $itk_component(modelAxesF2).tickIntervalL \
	    -text "Tick Interval:"
    } {}
    set hbc [$itk_component(modelAxesTickIntervalL) cget -background]
    itk_component add modelAxesTickIntervalE {
	::entry $itk_component(modelAxesF2).tickIntervalE \
	    -textvariable [::itcl::scope mModelAxesTickIntervalPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateTickInterval %P] \
	    -background $SystemWindow \
	    -highlightbackground $hbc
    } {}

    buildComboBox $itk_component(modelAxesF2) \
	modelAxesTicksPerMajor \
	ticksPerMajor \
	mModelAxesTicksPerMajorPref \
	"Ticks Per Major:" \
	{2 3 4 5 6 8 10 12}

    buildComboBox $itk_component(modelAxesF2) \
	modelAxesTickThreshold \
	tickThreshold \
	mModelAxesTickThresholdPref \
	"Tick Threshold:" \
	{4 8 16 32 64}

    buildComboBox $itk_component(modelAxesF2) \
	modelAxesTickLength \
	tickLength \
	mModelAxesTickLengthPref \
	"Tick Length:" \
	{2 4 8 16}

    buildComboBox $itk_component(modelAxesF2) \
	modelAxesTickMajorLength \
	tickMajorLength \
	mModelAxesTickMajorLengthPref \
	"Major Tick Length:" \
	{2 4 8 16}

    buildComboBox $itk_component(modelAxesF2) \
	modelAxesTickColor \
	tickColor \
	mModelAxesTickColorPref \
	"Tick Color:" \
	$mColorListNoTriple

    buildComboBox $itk_component(modelAxesF2) \
	modelAxesTickMajorColor \
	tickMajorColor \
	mModelAxesTickMajorColorPref \
	"Major Tick Color:" \
	$mColorListNoTriple

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


::itcl::body Archer::buildMouseOverridesDialog {} {
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

::itcl::body Archer::buildPreferencesDialog {} {
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
	-command [::itcl::code $this applyPreferencesIfDiff]
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
    buildGeneralPreferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(generalLF) -fill both

    incr i
    buildModelAxesPreferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(modelAxesF) -fill both

    incr i
    buildViewAxesPreferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(viewAxesF) -fill both

    incr i
    buildGroundPlanePreferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(groundPlaneF) -fill both

    incr i
    buildDisplayPreferences
    $itk_component(preferenceTabs) tab configure $i \
	-window $itk_component(displayLF) -fill both

    pack $itk_component(preferenceTabs) -expand yes -fill both

    wm geometry $itk_component(preferencesDialog) 450x500
}

::itcl::body Archer::buildToplevelMenubar {} {
    itk_component add menubar {
	menu $itk_interior.menubar \
	    -tearoff 0
    } {
	keep -background
    }

    # File
    itk_component add filemenu {
	::menu $itk_component(menubar).filemenu \
	    -tearoff 1
    } {
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
	-command [::itcl::code $this newDb]
    $itk_component(filemenu) add command \
	-label "Open..." \
	-command [::itcl::code $this openDb]
    $itk_component(filemenu) add command \
	-label "Save" \
	-command [::itcl::code $this saveDb]
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
    $itk_component(filemenu) add command \
	-label "Raytrace Control Panel..." \
	-command [::itcl::code $this raytracePanel] \
	-state disabled
    #    $itk_component(filemenu) add command \
	-label "Export Geometry to PNG..." \
	-command [::itcl::code $this doPng] \
	-state disabled
    #    $itk_component(filemenu) add separator
    $itk_component(filemenu) add command \
	-label "Preferences..." \
	-command [::itcl::code $this doPreferences]
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
	-command [::itcl::code $this doViewReset] \
	-state disabled
    $itk_component(displaymenu) add command \
	-label "Autoview" \
	-command [::itcl::code $this doAutoview] \
	-state disabled
    $itk_component(displaymenu) add command \
	-label "Center..." \
	-command [::itcl::code $this doViewCenter] \
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
	-command [::itcl::code $this doAe 0 0]
    $itk_component(stdviewsmenu) add command \
	-label "Rear" \
	-command [::itcl::code $this doAe 180 0]
    $itk_component(stdviewsmenu) add command \
	-label "Port" \
	-command [::itcl::code $this doAe 90 0]
    $itk_component(stdviewsmenu) add command \
	-label "Starboard" \
	-command [::itcl::code $this doAe -90 0]
    $itk_component(stdviewsmenu) add command \
	-label "Top" \
	-command [::itcl::code $this doAe 0 90]
    $itk_component(stdviewsmenu) add command \
	-label "Bottom" \
	-command [::itcl::code $this doAe 0 -90]
    $itk_component(stdviewsmenu) add separator
    $itk_component(stdviewsmenu) add command \
	-label "35,25" \
	-command [::itcl::code $this doAe 35 25]
    $itk_component(stdviewsmenu) add command \
	-label "45,45" \
	-command [::itcl::code $this doAe 45 45]
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
	-command [::itcl::code $this doPrimaryToolbar]
    $itk_component(toolbarsmenu) add checkbutton \
	-label "View Controls" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope itk_option(-viewToolbar)] \
	-command [::itcl::code $this doViewToolbar]
    $itk_component(displaymenu) add cascade \
	-label "Toolbars" \
	-menu $itk_component(toolbarsmenu)
    $itk_component(displaymenu) add checkbutton \
	-label "Status Bar" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope itk_option(-statusbar)] \
	-command [::itcl::code $this doStatusbar]

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
	-command [::itcl::code $this setActivePane ul]
    incr i
    $itk_component(activepanemenu) add radiobutton \
	-label "Upper Right" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane ur]
    set mActivePane $i
    incr i
    $itk_component(activepanemenu) add radiobutton \
	-label "Lower Left" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane ll]
    incr i
    $itk_component(activepanemenu) add radiobutton \
	-label "Lower Right" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane lr]

    updateModesMenu
    updateUtilityMenu

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
	-command [::itcl::code $this doAboutArcher]
    #    $itk_component(helpmenu) add command \
	-label "Mouse Mode Overrides..." \
	-command [::itcl::code $this doMouseOverrides]

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
    bind $itk_component(filemenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(importMenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(exportMenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(displaymenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(stdviewsmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(toolbarsmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(modesmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(activepanemenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(helpmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
}


::itcl::body Archer::buildViewAxesPreferences {} {
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

    buildComboBox $itk_component(viewAxesF2) \
	viewAxesSize \
	size \
	mViewAxesSizePref \
	"Size:" \
	{Small Medium Large X-Large}

    buildComboBox $itk_component(viewAxesF2) \
	viewAxesPosition \
	position \
	mViewAxesPositionPref \
	"Position:" \
	{Center "Upper Left" "Upper Right" "Lower Left" "Lower Right"}

    buildComboBox $itk_component(viewAxesF2) \
	viewAxesLineWidth \
	lineWidth \
	mViewAxesLineWidthPref \
	"Line Width:" \
	{1 2 3}

    buildComboBox $itk_component(viewAxesF2) \
	viewAxesColor \
	color \
	mViewAxesColorPref \
	"Axes Color:" \
	$mColorList

    buildComboBox $itk_component(viewAxesF2) \
	viewAxesLabelColor \
	labelColor \
	mViewAxesLabelColorPref \
	"Label Color:" \
	$mColorListNoTriple

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


::itcl::body Archer::doAboutArcher {} {
    bind $itk_component(aboutDialog) <Enter> "raise $itk_component(aboutDialog)"
    bind $itk_component(aboutDialog) <Configure> "raise $itk_component(aboutDialog)"
    bind $itk_component(aboutDialog) <FocusOut> "raise $itk_component(aboutDialog)"

    $itk_component(aboutDialog) center [namespace tail $this]
    $itk_component(aboutDialog) activate
}


::itcl::body Archer::doArcherHelp {} {
    global tcl_platform

    if {$tcl_platform(platform) == "windows"} {
	exec hh [file join $brlcadDataPath html manuals archer Archer_Documentation.chm] &
    } else {
	tk_messageBox -title "" -message "Not yet implemented!"
    }
}


::itcl::body Archer::launchDisplayMenuBegin {_dm _m _x _y} {
    set currentDisplay $_dm
    tk_popup $_m $_x $_y
    after idle [::itcl::code $this launchDisplayMenuEnd]
}

::itcl::body Archer::launchDisplayMenuEnd {} {
    set currentDisplay ""
}

::itcl::body Archer::menuStatusCB {w} {
    if {$mDoStatus} {
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
	    "About..." {
		set mStatusStr "Info about ArcherCore"
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

::itcl::body Archer::updateCreationButtons {on} {
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

::itcl::body Archer::updateModesMenu {} {
    if {$ArcherCore::inheritFromToplevel} {
	catch {
	    $itk_component(modesmenu) delete 0 end
	}

	set i 0
	$itk_component(modesmenu) add radiobutton \
	    -label "Basic" \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this setMode 1]
	incr i
	$itk_component(modesmenu) add radiobutton \
	    -label "Intermediate" \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this setMode 1]
	incr i
	$itk_component(modesmenu) add radiobutton \
	    -label "Advanced" \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this setMode 1]

	if {[info exists itk_component(mged)] && $mMode != 0} {
	    $itk_component(modesmenu) add separator
	    $itk_component(modesmenu) add cascade \
		-label "Active Pane" \
		-menu $itk_component(activepanemenu)
	    $itk_component(modesmenu) add checkbutton \
		-label "Quad View" \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mMultiPane] \
		-command [::itcl::code $this doMultiPane]
	}

	$itk_component(modesmenu) add separator

	$itk_component(modesmenu) add checkbutton \
	    -label "View Axes" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowViewAxes] \
	    -command [::itcl::code $this showViewAxes]
	$itk_component(modesmenu) add checkbutton \
	    -label "Model Axes" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowModelAxes] \
	    -command [::itcl::code $this showModelAxes]
	$itk_component(modesmenu) add checkbutton \
	    -label "Ground Plane" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowGroundPlane] \
	    -command [::itcl::code $this showGroundPlane]
	$itk_component(modesmenu) add checkbutton \
	    -label "Primitive Labels" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowPrimitiveLabels] \
	    -command [::itcl::code $this showPrimitiveLabels]
	$itk_component(modesmenu) add checkbutton \
	    -label "Viewing Parameters" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowViewingParams] \
	    -command [::itcl::code $this showViewParams]
	$itk_component(modesmenu) add checkbutton \
	    -label "Scale" \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowScale] \
	    -command [::itcl::code $this showScale]
	$itk_component(modesmenu) add checkbutton \
	    -label "Two Sided Lighting" \
	    -offvalue 1 \
	    -onvalue 2 \
	    -variable [::itcl::scope mLighting] \
	    -command [::itcl::code $this doLighting]

	if {![info exists itk_component(mged)]} {
	    # Disable a few entries until we read a database
	    #$itk_component(modesmenu) entryconfigure "Active Pane" -state disabled
	    $itk_component(modesmenu) entryconfigure "View Axes" -state disabled
	    $itk_component(modesmenu) entryconfigure "Model Axes" -state disabled
	}
    } else {
	if {[info exists itk_component(mged)] && $mMode != 0} {
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
	    -command [::itcl::code $this setMode 1]
	incr i
	$itk_component(menubar) menuconfigure .modes.intermediate \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this setMode 1]
	incr i
	$itk_component(menubar) menuconfigure .modes.advanced \
	    -value $i \
	    -variable [::itcl::scope mMode] \
	    -command [::itcl::code $this setMode 1]

	if {[info exists itk_component(mged)] && $mMode != 0} {
	    set i 0
	    $itk_component(menubar) menuconfigure .modes.activePane.ul \
		-value $i \
		-variable [::itcl::scope mActivePane] \
		-command [::itcl::code $this setActivePane ul]
	    incr i
	    $itk_component(menubar) menuconfigure .modes.activePane.ur \
		-value $i \
		-variable [::itcl::scope mActivePane] \
		-command [::itcl::code $this setActivePane ur]
	    set mActivePane $i
	    incr i
	    $itk_component(menubar) menuconfigure .modes.activePane.ll \
		-value $i \
		-variable [::itcl::scope mActivePane] \
		-command [::itcl::code $this setActivePane ll]
	    incr i
	    $itk_component(menubar) menuconfigure .modes.activePane.lr \
		-value $i \
		-variable [::itcl::scope mActivePane] \
		-command [::itcl::code $this setActivePane lr]
	    $itk_component(menubar) menuconfigure .modes.quadview \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mMultiPane] \
		-command [::itcl::code $this doMultiPane]
	}

	if {![info exists itk_component(mged)]} {
	    $itk_component(menubar) menuconfigure .modes.viewaxes \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mShowViewAxes] \
		-command [::itcl::code $this showViewAxes] \
		-state disabled
	    $itk_component(menubar) menuconfigure .modes.modelaxes \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mShowModelAxes] \
		-command [::itcl::code $this showModelAxes] \
		-state disabled
	} else {
	    $itk_component(menubar) menuconfigure .modes.viewaxes \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mShowViewAxes] \
		-command [::itcl::code $this showViewAxes]
	    $itk_component(menubar) menuconfigure .modes.modelaxes \
		-offvalue 0 \
		-onvalue 1 \
		-variable [::itcl::scope mShowModelAxes] \
		-command [::itcl::code $this showModelAxes]
	}
	$itk_component(menubar) menuconfigure .modes.groundplane \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowGroundPlane] \
	    -command [::itcl::code $this showGroundPlane]
	$itk_component(menubar) menuconfigure .modes.primitiveLabels \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowPrimitiveLabels] \
	    -command [::itcl::code $this showPrimitiveLabels]
	$itk_component(menubar) menuconfigure .modes.viewingParams \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowViewingParams] \
	    -command [::itcl::code $this showViewParams]
	$itk_component(menubar) menuconfigure .modes.scale \
	    -offvalue 0 \
	    -onvalue 1 \
	    -variable [::itcl::scope mShowScale] \
	    -command [::itcl::code $this showScale]
	$itk_component(menubar) menuconfigure .modes.lighting \
	    -offvalue 1 \
	    -onvalue 2 \
	    -variable [::itcl::scope mLighting] \
	    -command [::itcl::code $this doLighting]
    }
}


################################### Modes Section ###################################

::itcl::body Archer::setAdvancedMode {} {
    grid $itk_component(attr_expand) -row 0 -column 2 -sticky e

    set itk_option(-primaryToolbar) 1
    doPrimaryToolbar

    if {$ArcherCore::inheritFromToplevel == 0} {
	pack forget $itk_component(menubar)
	::itcl::delete object $itk_component(menubar)
	buildEmbeddedMenubar
	pack $itk_component(menubar) -side top -fill x -padx 1 -before $itk_component(south)
    } else {
	updateModesMenu
	updateUtilityMenu
    }

    updateWizardMenu
    updateViewToolbarForEdit

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
	toggleTreeView "close"
    } else {
	toggleTreeView "open"
    }

    if {$mVPaneFraction5 == 0} {
	toggleAttrView "close"
    } else {
	toggleAttrView "open"
    }
    set mVPaneToggle3 $toggle3
    set mVPaneToggle5 $toggle5

    # How screwed up is this?
    $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
    update
    after idle $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
}


::itcl::body Archer::setBasicMode {} {
    grid forget $itk_component(attr_expand)

    #catch {$itk_component(displaymenu) delete "Command Window"}
    set itk_option(-primaryToolbar) 0
    doPrimaryToolbar

    catch {
	if {$mMultiPane} {
	    set mMultiPane 0
	    doMultiPane
	}
    }

    if {$ArcherCore::inheritFromToplevel == 0} {
	pack forget $itk_component(menubar)
	::itcl::delete object $itk_component(menubar)
	buildEmbeddedMenubar
	pack $itk_component(menubar) -side top -fill x -padx 1 -before $itk_component(south)
    } else {
	updateModesMenu
	updateUtilityMenu
    }

    updateWizardMenu

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
	toggleTreeView "close"
    } else {
	toggleTreeView "open"
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


::itcl::body Archer::setIntermediateMode {} {
    grid $itk_component(attr_expand) -row 0 -column 2 -sticky e

    #catch {$itk_component(displaymenu) delete "Command Window"}
    set itk_option(-primaryToolbar) 1
    doPrimaryToolbar

    # DRH: hack to make sure it doesn't crash in Crossbow
    if {$ArcherCore::inheritFromToplevel == 0} {
	pack forget $itk_component(menubar)
	::itcl::delete object $itk_component(menubar)
	buildEmbeddedMenubar
	pack $itk_component(menubar) -side top -fill x -padx 1 -before $itk_component(south)
    } else {
	updateModesMenu
	updateUtilityMenu
    }

    updateWizardMenu
    updateViewToolbarForEdit

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
	toggleTreeView "close"
    } else {
	toggleTreeView "open"
    }

    if {$mVPaneFraction5 == 0} {
	toggleAttrView "close"
    } else {
	toggleAttrView "open"
    }
    set mVPaneToggle3 $toggle3
    set mVPaneToggle5 $toggle5

    # How screwed up is this?
    $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
    update
    after idle $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
}


::itcl::body Archer::updateCopyMode {} {
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


::itcl::body Archer::updateCutMode {} {
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


::itcl::body Archer::updatePasteMode {} {
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


::itcl::body Archer::updateViewToolbarForEdit {} {
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
			-file [file join $mImgDir Themes $mTheme edit_rotate.png]]
	$itk_component(viewToolbar) add radiobutton edit_translate \
	    -balloonstr "Translate selected object" \
	    -helpstr "Translate selected object" \
	    -variable [::itcl::scope mDefaultBindingMode] \
	    -value $OBJECT_TRANSLATE_MODE \
	    -command [::itcl::code $this beginObjTranslate] \
	    -image [image create photo \
			-file [file join $mImgDir Themes $mTheme edit_translate.png]]
	$itk_component(viewToolbar) add radiobutton edit_scale \
	    -balloonstr "Scale selected object" \
	    -helpstr "Scale selected object" \
	    -variable [::itcl::scope mDefaultBindingMode] \
	    -value $OBJECT_SCALE_MODE \
	    -command [::itcl::code $this beginObjScale] \
	    -image [image create photo \
			-file [file join $mImgDir Themes $mTheme edit_scale.png]]
	$itk_component(viewToolbar) add radiobutton edit_center \
	    -balloonstr "Center selected object" \
	    -helpstr "Center selected object" \
	    -variable [::itcl::scope mDefaultBindingMode] \
	    -value $OBJECT_CENTER_MODE \
	    -command [::itcl::code $this beginObjCenter] \
	    -image [image create photo \
			-file [file join $mImgDir Themes $mTheme edit_select.png]]

	$itk_component(viewToolbar) itemconfigure edit_rotate -state disabled
	$itk_component(viewToolbar) itemconfigure edit_translate -state disabled
	$itk_component(viewToolbar) itemconfigure edit_scale -state disabled
	$itk_component(viewToolbar) itemconfigure edit_center -state disabled
    }
}


::itcl::body Archer::setMode {{updateFractions 0}} {
    if {$updateFractions} {
	updateHPaneFractions
	updateVPaneFractions
    }

    switch -- $mMode {
	0 {
	    setBasicMode
	}
	1 {
	    setIntermediateMode
	}
	2 {
	    setAdvancedMode
	}
    }
}


################################### Object Edit Section ###################################

::itcl::body Archer::applyEdit {} {
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

    updateObjHistory $mSelectedObj
    if {$doRefreshTree} {
	refreshTree
    }

    set mNeedSave 1
    updateSaveMode

    # Disable the "apply" and "reset" buttons
    $itk_component(objEditToolbar) itemconfigure apply \
	-state disabled
    $itk_component(objEditToolbar) itemconfigure reset \
	-state disabled
}


::itcl::body Archer::copyObj {} {
    if {$mDbNoCopy || $mDbReadOnly} {
	return
    }

    set mPasteActive 1
    updatePasteMode
}

::itcl::body Archer::cutObj {} {
    if {$mDbNoCopy || $mDbReadOnly} {
	return
    }

    set mPasteActive 1
    updatePasteMode
}

::itcl::body Archer::initEdit {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    set mSelectedObjType [dbCmd get_type $mSelectedObj]

    if {$mSelectedObjType != "bot"} {
	set odata [lrange [dbCmd get $mSelectedObj] 1 end]
    } else {
	set odata ""
    }

    switch -- $mSelectedObjType {
	"arb4" {
	    if {![info exists itk_component(arb4View)]} {
		buildArb4_edit_view
	    }
	    initArb4EditView $odata
	}
	"arb5" {
	    if {![info exists itk_component(arb5View)]} {
		buildArb5EditView
	    }
	    initArb5EditView $odata
	}
	"arb6" {
	    if {![info exists itk_component(arb6View)]} {
		buildArb6EditView
	    }
	    initArb6EditView $odata
	}
	"arb7" {
	    if {![info exists itk_component(arb7View)]} {
		buildArb7EditView
	    }
	    initArb7EditView $odata
	}
	"arb8" {
	    if {![info exists itk_component(arb8View)]} {
		buildArb8EditView
	    }
	    initArb8EditView $odata
	}
	"bot" {
	    if {![info exists itk_component(botView)]} {
		buildBotEditView
	    }
	    initBotEditView $odata
	}
	"comb" {
	    if {![info exists itk_component(combView)]} {
		buildCombEditView
	    }
	    initCombEditView $odata
	}
	"ell" {
	    if {![info exists itk_component(ellView)]} {
		buildEllEditView
	    }
	    initEllEditView $odata
	}
	"ehy" {
	    if {![info exists itk_component(ehyView)]} {
		buildEhyEditView
	    }
	    initEhyEditView $odata
	}
	"epa" {
	    if {![info exists itk_component(epaView)]} {
		buildEpaEditView
	    }
	    initEpaEditView $odata
	}
	"eto" {
	    if {![info exists itk_component(etoView)]} {
		buildEtoEditView
	    }
	    initEtoEditView $odata
	}
	"extrude" {
	    if {![info exists itk_component(extrudeView)]} {
		buildExtrudeEditView
	    }
	    initExtrudeEditView $odata
	}
	"grip" {
	    if {![info exists itk_component(gripView)]} {
		buildGripEditView
	    }
	    initGripEditView $odata
	}
	"half" {
	    if {![info exists itk_component(halfView)]} {
		buildHalfEditView
	    }
	    initHalfEditView $odata
	}
	"part" {
	    if {![info exists itk_component(partView)]} {
		buildPartEditView
	    }
	    initPartEditView $odata
	}
	"pipe" {
	    if {![info exists itk_component(pipeView)]} {
		buildPipeEditView
	    }
	    initPipeEditView $odata
	}
	"rpc" {
	    if {![info exists itk_component(rpcView)]} {
		buildRpcEditView
	    }
	    initRpcEditView $odata
	}
	"rhc" {
	    if {![info exists itk_component(rhcView)]} {
		buildRhcEditView
	    }
	    initRhcEditView $odata
	}
	"sketch" {
	    if {![info exists itk_component(sketchView)]} {
		buildSketchEditView
	    }
	    initSketchEditView $odata
	}
	"sph" {
	    if {![info exists itk_component(sphView)]} {
		buildSphereEditView
	    }
	    initSphereEditView $odata
	}
	"tgc" {
	    if {![info exists itk_component(tgcView)]} {
		buildTgcEditView
	    }
	    initTgcEditView $odata
	}
	"tor" {
	    if {![info exists itk_component(torView)]} {
		buildTorusEditView
	    }
	    initTorusEditView $odata
	}
    }
}


::itcl::body Archer::finalizeObjEdit {obj path} {
    if {$obj == "" ||
	[catch {dbCmd get_type $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    # No history
    if {$hname == "" ||
	[catch {dbCmd get_type $hname} stuff]} {
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
    render $path $renderMode $renderTrans 0
    dbCmd configure -autoViewEnable 1
}

::itcl::body Archer::gotoNextObj {} {
    set obj $mSelectedObj

    if {$obj == "" ||
	[catch {dbCmd get_type $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get_type $hname} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $hname next} next]} {
	set next ""
    }

    if {$next == "" ||
	[catch {dbCmd get_type $next} stuff]} {
	return
    }

    updateObjEdit $next 1 1
    updatePrevObjButton $obj
    updateNextObjButton $obj

    if {$mSelectedObjType == "comb"} {
	refreshTree
    }
}

::itcl::body Archer::gotoPrevObj {} {
    set obj $mSelectedObj

    if {$obj == "" ||
	[catch {dbCmd get_type $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get_type $hname} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $hname previous} previous]} {
	set previous ""
    }

    if {$previous == "" ||
	[catch {dbCmd get_type $previous} stuff]} {
	return
    }

    updateObjEdit $previous 1 1
    updatePrevObjButton $obj
    updateNextObjButton $obj

    if {$mSelectedObjType == "comb"} {
	refreshTree
    }
}

::itcl::body Archer::pasteObj {} {
    if {$mDbNoCopy || $mDbReadOnly} {
	return
    }

    #    set mPasteActive 0
    #    updatePasteMode
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


::itcl::body Archer::resetEdit {} {
    set obj $mSelectedObj

    if {$obj == "" ||
	[catch {dbCmd get_type $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get_type $hname} stuff]} {
	return
    }

    updateObjEdit $hname 1 0

    # Disable the apply and reset buttons
    $itk_component(objEditToolbar) itemconfigure apply \
	-state disabled
    $itk_component(objEditToolbar) itemconfigure reset \
	-state disabled
}


::itcl::body Archer::updateNextObjButton {obj} {
    if {$obj == "" ||
	[catch {dbCmd get_type $obj} stuff]} {
	$itk_component(objEditToolbar) itemconfigure next \
	    -state disabled

	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get_type $hname} stuff]} {
	$itk_component(objEditToolbar) itemconfigure next \
	    -state disabled

	return
    }

    if {[catch {dbCmd attr get $hname next} next]} {
	set next ""
    }

    if {$next == "" ||
	[catch {dbCmd get_type $next} stuff]} {
	$itk_component(objEditToolbar) itemconfigure next \
	    -state disabled
    } else {
	$itk_component(objEditToolbar) itemconfigure next \
	    -state normal
    }
}

::itcl::body Archer::updateObjHistory {obj} {
    if {$obj == "" ||
	[catch {dbCmd get_type $obj} stuff]} {
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
	![catch {dbCmd get_type $old_hname} stuff]} {
	# Insert into the history list

	if {[catch {dbCmd attr get $old_hname next} next]} {
	    set next ""
	}

	# Delete the future
	if {$next != "" &&
	    ![catch {dbCmd get_type $next} stuff]} {

	    while {$next != ""} {
		set deadObj $next

		if {[catch {dbCmd attr get $next next} next]} {
		    set next ""
		} elseif {[catch {dbCmd get_type $next} stuff]} {
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
    updatePrevObjButton $obj
    updateNextObjButton $obj
}

::itcl::body Archer::updatePrevObjButton {obj} {
    if {$obj == "" ||
	[catch {dbCmd get_type $obj} stuff]} {
	$itk_component(objEditToolbar) itemconfigure prev \
	    -state disabled

	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    if {$hname == "" ||
	[catch {dbCmd get_type $hname} stuff]} {
	$itk_component(objEditToolbar) itemconfigure prev \
	    -state disabled

	return
    }

    if {[catch {dbCmd attr get $hname previous} previous]} {
	set previous ""
    }

    if {$previous == "" ||
	[catch {dbCmd get_type $previous} stuff]} {
	$itk_component(objEditToolbar) itemconfigure prev \
	    -state disabled
    } else {
	$itk_component(objEditToolbar) itemconfigure prev \
	    -state normal
    }
}



################################### Object Edit via Mouse Section ###################################

::itcl::body Archer::beginObjRotate {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
	return
    }

    # These values are insignificant (i.e. they will be ignored by the callback)
    set x 0
    set y 0
    set z 0

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm orotate_mode %x %y [list [::itcl::code $this handleObjRotate]] $obj $x $y $z; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjRotate $dm $obj]; break"
    }
}

::itcl::body Archer::beginObjScale {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
	return
    }

    # These values are insignificant (i.e. they will be ignored by the callback)
    set x 0
    set y 0
    set z 0

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm oscale_mode %x %y [list [::itcl::code $this handleObjScale]] $obj $x $y $z; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjScale $dm $obj]; break"
    }
}

::itcl::body Archer::beginObjTranslate {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "$dm otranslate_mode %x %y [list [::itcl::code $this handleObjTranslate]] $obj; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjTranslate $dm $obj]; break"
    }
}

::itcl::body Archer::beginObjCenter {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
	return
    }

    foreach dname {ul ur ll lr} {
	set dm [$itk_component(mged) component $dname]
	set win [$dm component dm]
	bind $win <1> "[::itcl::code $this handleObjCenter $obj %x %y]; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjCenter $dm $obj]; break"
    }
}

::itcl::body Archer::endObjCenter {dsp obj} {
    $dsp idle_mode

    if {![info exists itk_component(mged)]} {
	return
    }

    set center [$itk_component(mged) ocenter $obj]
    addHistory "ocenter $center"
}

::itcl::body Archer::endObjRotate {dsp obj} {
    $dsp idle_mode

    #XXX Need code to track overall transformation
    if {[info exists itk_component(mged)]} {
	#addHistory "orotate obj rx ry rz"
    }
}

::itcl::body Archer::endObjScale {dsp obj} {
    $dsp idle_mode

    #XXX Need code to track overall transformation
    if {[info exists itk_component(mged)]} {
	#addHistory "oscale obj sf"
    }
}

::itcl::body Archer::endObjTranslate {dsp obj} {
    $dsp idle_mode

    #XXX Need code to track overall transformation
    if {[info exists itk_component(mged)]} {
	#addHistory "otranslate obj dx dy dz"
    }
}

::itcl::body Archer::handleObjCenter {obj x y} {
    if {[info exists itk_component(mged)]} {
	set ocenter [dbCmd ocenter $obj]
    } else {
	set savePwd [pwd]
	cd /
	set ocenter [dbCmd ocenter $obj]
    }

    set ocenter [vscale $ocenter [dbCmd local2base]]
    set ovcenter [eval dbCmd m2vPoint $ocenter]

    # This is the updated view center (i.e. we keep the original view Z)
    set vcenter [dbCmd screen2view $x $y]
    set vcenter [list [lindex $vcenter 0] [lindex $vcenter 1] [lindex $ovcenter 2]]

    set ocenter [vscale [eval dbCmd v2mPoint $vcenter] [dbCmd base2local]]

    if {[info exists itk_component(mged)]} {
	eval archerWrapper ocenter 0 0 1 0 $obj $ocenter
    } else {
	eval archerWrapper ocenter 0 0 1 0 $obj $ocenter
	cd $savePwd
    }

    redrawObj $obj 0
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

    redrawObj $obj 0
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

    redrawObj $obj 0
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

    redrawObj $obj 0
}


################################### Object Views Section ###################################


::itcl::body Archer::buildArb4EditView {} {
    set parent $itk_component(objEditView)
    itk_component add arb4View {
	Arb4EditFrame $parent.arb4view \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildArb5EditView {} {
    set parent $itk_component(objEditView)
    itk_component add arb5View {
	Arb5EditFrame $parent.arb5view \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildArb6EditView {} {
    set parent $itk_component(objEditView)
    itk_component add arb6View {
	Arb6EditFrame $parent.arb6view \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildArb7EditView {} {
    set parent $itk_component(objEditView)
    itk_component add arb7View {
	Arb7EditFrame $parent.arb7view \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildArb8EditView {} {
    set parent $itk_component(objEditView)
    itk_component add arb8View {
	Arb8EditFrame $parent.arb8view \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildBotEditView {} {
    #XXX Not ready yet
    return

    set parent $itk_component(objEditView)
    itk_component add botView {
	BotEditFrame $parent.botview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildCombEditView {} {
    set parent $itk_component(objEditView)
    itk_component add combView {
	CombEditFrame $parent.combview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildDbAttrView {} {
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

::itcl::body Archer::buildEhyEditView {} {
    set parent $itk_component(objEditView)
    itk_component add ehyView {
	EhyEditFrame $parent.ehyview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildEllEditView {} {
    set parent $itk_component(objEditView)
    itk_component add ellView {
	EllEditFrame $parent.ellview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildEpaEditView {} {
    set parent $itk_component(objEditView)
    itk_component add epaView {
	EpaEditFrame $parent.epaview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildEtoEditView {} {
    set parent $itk_component(objEditView)
    itk_component add etoView {
	EtoEditFrame $parent.etoview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildExtrudeEditView {} {
    set parent $itk_component(objEditView)
    itk_component add extrudeView {
	ExtrudeEditFrame $parent.extrudeview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildGripEditView {} {
    set parent $itk_component(objEditView)
    itk_component add gripView {
	GripEditFrame $parent.gripview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildHalfEditView {} {
    set parent $itk_component(objEditView)
    itk_component add halfView {
	HalfEditFrame $parent.halfview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildObjAttrView {} {
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

::itcl::body Archer::buildObjEditView {} {
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
	-command [::itcl::code $this gotoPrevObj]
    $itk_component(objEditToolbar) add button apply \
	-text Apply \
	-helpstr "Apply the current edits" \
	-balloonstr "Apply the current edits" \
	-pady 0 \
	-command [::itcl::code $this applyEdit]
    $itk_component(objEditToolbar) add button reset \
	-text Reset \
	-helpstr "Delete the current edits" \
	-balloonstr "Delete the current edits" \
	-pady 0 \
	-command [::itcl::code $this resetEdit]
    $itk_component(objEditToolbar) add button next \
	-text $mRightArrow \
	-helpstr "Go to the next object state" \
	-balloonstr "Go to the next object state" \
	-pady 0 \
	-command [::itcl::code $this gotoNextObj]

    # Configure the font here so that the font actually gets
    # applied to the button. Sheesh!!!
    $itk_component(objEditToolbar) itemconfigure prev \
	-font $mFontArrowsName
    $itk_component(objEditToolbar) itemconfigure next \
	-font $mFontArrowsName

    pack $itk_component(objEditToolbar) -expand yes
}

::itcl::body Archer::buildObjViewToolbar {} {
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
	-command [::itcl::code $this initObjEditView]

    $itk_component(objViewToolbar) add radiobutton objAttrView \
	-helpstr "Object text mode" \
	-balloonstr "Object text mode" \
	-variable [::itcl::scope mObjViewMode] \
	-value $OBJ_ATTR_VIEW_MODE \
	-command [::itcl::code $this initObjAttrView]
}

::itcl::body Archer::buildPartEditView {} {
    set parent $itk_component(objEditView)
    itk_component add partView {
	PartEditFrame $parent.partview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildPipeEditView {} {
    #XXX Not ready yet
    return

    set parent $itk_component(objEditView)
    itk_component add pipeView {
	PipeEditFrame $parent.pipeview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildRhcEditView {} {
    set parent $itk_component(objEditView)
    itk_component add rhcView {
	RhcEditFrame $parent.rhcview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildRpcEditView {} {
    set parent $itk_component(objEditView)
    itk_component add rpcView {
	RpcEditFrame $parent.rpcview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildSketchEditView {} {
    #XXX Not ready yet
    return

    set parent $itk_component(objEditView)
    itk_component add sketchView {
	SketchEditFrame $parent.sketchview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildSphereEditView {} {
    set parent $itk_component(objEditView)
    itk_component add sphView {
	SphereEditFrame $parent.sphview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildTgcEditView {} {
    set parent $itk_component(objEditView)
    itk_component add tgcView {
	TgcEditFrame $parent.tgcview \
	    -units "mm"
    } {}
}

::itcl::body Archer::buildTorusEditView {} {
    set parent $itk_component(objEditView)
    itk_component add torView {
	TorusEditFrame $parent.torview \
	    -units "mm"
    } {}
}

::itcl::body Archer::initArb4EditView {odata} {
    $itk_component(arb4View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb4View) initGeometry $odata

    pack $itk_component(arb4View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initArb5EditView {odata} {
    $itk_component(arb5View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb5View) initGeometry $odata

    pack $itk_component(arb5View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initArb6EditView {odata} {
    $itk_component(arb6View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb6View) initGeometry $odata

    pack $itk_component(arb6View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initArb7EditView {odata} {
    $itk_component(arb7View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb7View) initGeometry $odata

    pack $itk_component(arb7View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initArb8EditView {odata} {
    $itk_component(arb8View) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(arb8View) initGeometry $odata

    pack $itk_component(arb8View) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initBotEditView {odata} {
    #XXX Not ready yet
    return

    $itk_component(botView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(botView) initGeometry $odata

    pack $itk_component(botView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initCombEditView {odata} {
    $itk_component(combView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(combView) initGeometry $odata

    pack $itk_component(combView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initDbAttrView {name} {
    if {![info exists itk_component(mged)]} {
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

    set mDbName $name
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

::itcl::body Archer::initEhyEditView {odata} {
    $itk_component(ehyView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(ehyView) initGeometry $odata

    pack $itk_component(ehyView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initEllEditView {odata} {
    $itk_component(ellView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(ellView) initGeometry $odata

    pack $itk_component(ellView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initEpaEditView {odata} {
    $itk_component(epaView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(epaView) initGeometry $odata

    pack $itk_component(epaView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initEtoEditView {odata} {
    $itk_component(etoView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(etoView) initGeometry $odata

    pack $itk_component(etoView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initExtrudeEditView {odata} {
    $itk_component(extrudeView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(extrudeView) initGeometry $odata

    pack $itk_component(extrudeView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initGripEditView {odata} {
    $itk_component(gripView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(gripView) initGeometry $odata

    pack $itk_component(gripView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initHalfEditView {odata} {
    $itk_component(halfView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(halfView) initGeometry $odata

    pack $itk_component(halfView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initNoWizard {parent msg} {
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

::itcl::body Archer::initObjAttrView {} {
    if {![info exists itk_component(mged)]} {
	return
    }

    if {$mSelectedObj == ""} {
	return
    }

    if {$mWizardClass == "" &&
	!$mNoWizardActive &&
	$mPrevObjViewMode == $OBJ_EDIT_VIEW_MODE} {
	finalizeObjEdit $mPrevSelectedObj $mPrevSelectedObjPath
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

::itcl::body Archer::initObjEdit {obj} {
    if {![info exists itk_component(mged)]} {
	return
    }

    set mPendingEdits 0

    initObjHistory $obj

    updatePrevObjButton $obj
    updateNextObjButton $obj

    # Disable the apply and reset buttons
    $itk_component(objEditToolbar) itemconfigure apply \
	-state disabled
    $itk_component(objEditToolbar) itemconfigure reset \
	-state disabled
}

::itcl::body Archer::initObjEditView {} {
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
	finalizeObjEdit $mPrevSelectedObj $mPrevSelectedObjPath
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
	initObjEdit $mSelectedObj

	# free the current primitive view if any
	set _slaves [pack slaves $itk_component(objEditView)]
	catch {eval pack forget $_slaves}

	initEdit

	if {[info exists itk_component(mged)]} {
	    pack $itk_component(objViewToolbar) -expand no -fill x -anchor n
	    pack $itk_component(objEditView) -expand yes -fill both -anchor n
	    pack $itk_component(objEditToolbarF) -expand no -fill x -anchor s
	}
    } else {
	if {[pluginQuery $mWizardClass] == -1} {
	    # the wizard plugin has not been loaded
	    initObjQizard $mSelectedObj 0
	} else {
	    initObjWizard $mSelectedObj 1
	}
    }
}

::itcl::body Archer::initObjHistory {obj} {
    if {$obj == "" ||
	[catch {dbCmd get_type $obj} stuff]} {
	return
    }

    if {[catch {dbCmd attr get $obj history} hname]} {
	set hname ""
    }

    # The history list is non-existent or
    # the link to it is broken, so create
    # a new one.
    if {$hname == "" ||
	[catch {dbCmd get_type $hname} stuff]} {
	dbCmd make_name -s 1
	set hname [dbCmd make_name $obj.version]
	#dbCmd attr set $obj history $hname

	dbCmd cp $obj $hname
	dbCmd hide $hname
	dbCmd attr set $hname previous ""
	dbCmd attr set $hname next ""
	dbCmd attr set $hname history $hname
	dbCmd attr set $obj history $hname
    }
}

## - initObjWizard
#
# Note - Before we get here, any previous wizard instances are destroyed
#        and mWizardClass has been initialized to the name of the wizard class.
#        Also, mWizardState is initialized to "".
#
::itcl::body Archer::initObjWizard {obj wizardLoaded} {
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

	initNoWizard $parent $msg
    } else {
	if {[catch {$itk_component(mged) attr get $mWizardTop WizardOrigin} wizOrigin]} {
	    set wizOrigin [dbCmd center]
	    set wizUnits [dbCmd units]
	} elseif {[catch {$itk_component(mged) attr get $mWizardTop WizardUnits} wizUnits]} {
	    set wizUnits mm
	}

	set fail [catch {$mWizardClass $parent.\#auto $this $mWizardTop $mWizardState $wizOrigin $wizUnits} wiz]

	if {$fail} {
	    initNoWizard $parent "Failed to initialize the $mWizardClass wizard."
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
		    -command [::itcl::code $this invokeWizardUpdate \
				  $itk_component($mWizardClass) $action \
				  $mWizardTop ""]
	    } {}

	    pack $itk_component(objViewToolbar) -expand no -fill x -anchor n
	    pack $itk_component($mWizardClass) -expand yes -fill both -anchor n
	    pack $itk_component(wizardUpdate) -expand no -fill none -anchor s
	}
    }
}

::itcl::body Archer::initPartEditView {odata} {
    $itk_component(partView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(partView) initGeometry $odata

    pack $itk_component(partView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initPipeEditView {odata} {
    #XXX Not ready yet
    return

    $itk_component(pipeView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(pipeView) initGeometry $odata

    pack $itk_component(pipeView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initRhcEditView {odata} {
    $itk_component(rhcView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(rhcView) initGeometry $odata

    pack $itk_component(rhcView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initRpcEditView {odata} {
    $itk_component(rpcView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(rpcView) initGeometry $odata

    pack $itk_component(rpcView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initSketchEditView {odata} {
    #XXX Not ready yet
    return

    $itk_component(sketchView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(sketchView) initGeometry $odata

    pack $itk_component(sketchView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initSphereEditView {odata} {
    $itk_component(sphView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(sphView) initGeometry $odata

    pack $itk_component(sphView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initTgcEditView {odata} {
    $itk_component(tgcView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(tgcView) initGeometry $odata

    pack $itk_component(tgcView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::initTorusEditView {odata} {
    $itk_component(torView) configure \
	-geometryObject $mSelectedObj \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(mged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(torView) initGeometry $odata

    pack $itk_component(torView) \
	-expand yes \
	-fill both
}

::itcl::body Archer::updateObjEdit {updateObj needInit needSave} {
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
	initEdit
    }

    if {$needSave} {
	set mNeedSave 1
	updateSaveMode
    }

    render $mSelectedObjPath $renderMode $renderTrans 0
    dbCmd configure -autoViewEnable 1
}

::itcl::body Archer::updateObjEditView {} {
    set mPendingEdits 1
    redrawObj $mSelectedObjPath

    # Enable the apply and reset buttons
    $itk_component(objEditToolbar) itemconfigure apply \
	-state normal
    $itk_component(objEditToolbar) itemconfigure reset \
	-state normal
}


################################### Plugins Section ###################################

::itcl::body Archer::buildUtilityMenu {} {
    if {$ArcherCore::inheritFromToplevel} {
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

::itcl::body Archer::buildWizardMenu {} {
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

::itcl::body Archer::buildWizardObj {dialog wizard action oname} {
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
	invokeWizardUpdate $wizard $action $oname $name
    }

    ::itcl::delete object $wizard
    ::itcl::delete object $dialog
}

::itcl::body Archer::invokeUtilityDialog {class wname w} {
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
    #set utility [$class $parent.\#auto $this]
    if {[catch {$class $parent.\#auto $this} utility]} {
	rename $dialog ""
	return
    }

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

::itcl::body Archer::invokeWizardDialog {class action wname} {
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
	-command [::itcl::code $this buildWizardObj $dialog $wizard $action $oname]
    $dialog buttonconfigure Cancel \
	-command "$dialog deactivate; ::itcl::delete object $dialog"

    wm protocol $dialog WM_DELETE_WINDOW "$dialog deactivate; ::itcl::delete object $dialog"
    wm geometry $dialog "400x400"
    $dialog center [namespace tail $this]
    $dialog activate
}

::itcl::body Archer::invokeWizardUpdate {wizard action oname name} {
    if {$name == ""} {
	set name [$wizard getWizardTop]
    }

    #XXX Temporary special case for TankWizardI
    if {[namespace tail [$wizard info class]] != "TankWizardI"} {
	# Here we have the case where the name is being
	# changed to an object that already exists.
	if {$oname != $name && ![catch {dbCmd get_type $name} stuff]} {
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
    updateSaveMode

    set xmlAction [$wizard getWizardXmlAction]
    if {$xmlAction != ""} {
	set xml [$wizard $xmlAction]
	foreach callback $wizardXmlCallbacks {
	    $callback $xml
	}
    }

    refreshTree
    selectNode [$itk_component(tree) find $obj]
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

::itcl::body Archer::pluginUpdateSaveMode {mode} {
    if {1 <= $mode} {
	set mode 1
    } else {
	set mode 0
    }

    set mNeedSave $mode
    updateSaveMode
}

::itcl::body Archer::pluginUpdateStatusBar {msg} {
    set mStatusStr $msg
    ::update
}

::itcl::body Archer::updateUtilityMenu {} {
    foreach dialog [::itcl::find object -class ::iwidgets::Dialog] {
	if {[regexp {utility_dialog} $dialog]} {
	    catch {rename $dialog ""}
	}
    }

    if {$mMode == 0} {
	if {$ArcherCore::inheritFromToplevel} {
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
	if {[info exists itk_component(mged)] &&
	    ($minorType == $pluginMinorTypeAll ||
	     $minorType == $pluginMinorTypeMged)} {
	    lappend uplugins $plugin
	}
    }

    if {$uplugins == {}} {
	if {$ArcherCore::inheritFromToplevel} {
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
	buildUtilityMenu
    }

    $itk_component(utilityMenu) delete 0 end

    foreach plugin $uplugins {
	set class [$plugin get -class]
	set wname [$plugin get -name]

	$itk_component(utilityMenu) add command \
	    -label $wname \
	    -command [::itcl::code $this invokeUtilityDialog $class $wname [namespace tail $this]]
    }
}

::itcl::body Archer::updateWizardMenu {} {
    # Look for appropriate wizard plugins
    set wplugins {}
    foreach plugin $::Archer::plugins {
	set majorType [$plugin get -majorType]
	if {$majorType != $pluginMajorTypeWizard} {
	    continue
	}


	set minorType [$plugin get -minorType]
	if {[info exists itk_component(mged)] &&
	    ($minorType == $pluginMinorTypeAll ||
	     $minorType == $pluginMinorTypeMged)} {
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
	    -command [::itcl::code $this invokeWizardDialog $class $action $wname]
    }
}


################################### Preferences Section ###################################

::itcl::body Archer::applyDisplayPreferences {} {
    updateDisplaySettings
}


::itcl::body Archer::applyDisplayPreferencesIfDiff {} {
    if {$mZClipModePref != $mZClipMode} {
	set mZClipMode $mZClipModePref
	updateDisplaySettings
    }
}


::itcl::body Archer::applyGeneralPreferences {} {
    switch -- $mBindingMode {
	0 {
	    initDefaultBindings
	}
	1 {
	    initBrlcadBindings
	}
    }

    backgroundColor [lindex $mBackground 0] \
	[lindex $mBackground 1] \
	[lindex $mBackground 2]
    setColorOption dbCmd -primitiveLabelColor $mPrimitiveLabelColor
    setColorOption dbCmd -scaleColor $mScaleColor
    setColorOption dbCmd -viewingParamsColor $mViewingParamsColor
}


::itcl::body Archer::applyGeneralPreferencesIfDiff {} {
    if {$mBindingModePref != $mBindingMode} {
	set mBindingMode $mBindingModePref
	switch -- $mBindingMode {
	    0 {
		initDefaultBindings
	    }
	    1 {
		initBrlcadBindings
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

	backgroundColor $mBackgroundRedPref $mBackgroundGreenPref $mBackgroundBluePref
    }

    if {$mPrimitiveLabelColor != $mPrimitiveLabelColorPref} {
	set mPrimitiveLabelColor $mPrimitiveLabelColorPref
	setColorOption dbCmd -primitiveLabelColor $mPrimitiveLabelColor
    }

    if {$mViewingParamsColor != $mViewingParamsColorPref} {
	set mViewingParamsColor $mViewingParamsColorPref
	setColorOption dbCmd -viewingParamsColor $mViewingParamsColor
    }

    if {$mScaleColor != $mScaleColorPref} {
	set mScaleColor $mScaleColorPref
	setColorOption dbCmd -scaleColor $mScaleColor
    }

    if {$mMeasuringStickColor != $mMeasuringStickColorPref} {
	set mMeasuringStickColor $mMeasuringStickColorPref
    }

    if {$mTheme != $mThemePref} {
	set mTheme $mThemePref
	updateTheme
    }

    if {$mEnableBigEPref != $mEnableBigE} {
	set mEnableBigE $mEnableBigEPref
    }
}


::itcl::body Archer::applyGroundPlanePreferencesIfDiff {} {
    if {$mGroundPlaneSize != $mGroundPlaneSizePref ||
	$mGroundPlaneInterval != $mGroundPlaneIntervalPref ||
	$mGroundPlaneMajorColor != $mGroundPlaneMajorColorPref ||
	$mGroundPlaneMinorColor != $mGroundPlaneMinorColorPref} {

	set mGroundPlaneSize $mGroundPlaneSizePref
	set mGroundPlaneInterval $mGroundPlaneIntervalPref
	set mGroundPlaneMajorColor $mGroundPlaneMajorColorPref
	set mGroundPlaneMinorColor $mGroundPlaneMinorColorPref

	buildGroundPlane
	showGroundPlane
    }
}


::itcl::body Archer::applyModelAxesPreferences {} {
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

    setColorOption dbCmd -modelAxesColor $mModelAxesColor -modelAxesTripleColor
    setColorOption dbCmd -modelAxesLabelColor $mModelAxesLabelColor

    dbCmd configure -modelAxesTickInterval $mModelAxesTickInterval
    dbCmd configure -modelAxesTicksPerMajor $mModelAxesTicksPerMajor
    dbCmd configure -modelAxesTickThreshold $mModelAxesTickThreshold
    dbCmd configure -modelAxesTickLength $mModelAxesTickLength
    dbCmd configure -modelAxesTickMajorLength $mModelAxesTickMajorLength

    setColorOption dbCmd -modelAxesTickColor $mModelAxesTickColor
    setColorOption dbCmd -modelAxesTickMajorColor $mModelAxesTickMajorColor
}


::itcl::body Archer::applyModelAxesPreferencesIfDiff {} {
    if {$mModelAxesSizePref != $mModelAxesSize} {
	set mModelAxesSize $mModelAxesSizePref

	if {[info exists itk_component(mged)]} {
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

	if {[info exists itk_component(mged)]} {
	    dbCmd configure -modelAxesPosition $mModelAxesPosition
	}
    }

    if {$mModelAxesLineWidthPref != $mModelAxesLineWidth} {
	set mModelAxesLineWidth $mModelAxesLineWidthPref

	if {[info exists itk_component(mged)]} {
	    dbCmd configure -modelAxesLineWidth $mModelAxesLineWidth
	}
    }

    if {$mModelAxesColorPref != $mModelAxesColor} {
	set mModelAxesColor $mModelAxesColorPref

	if {[info exists itk_component(mged)]} {
	    setColorOption dbCmd -modelAxesColor $mModelAxesColor -modelAxesTripleColor
	}
    }

    if {$mModelAxesLabelColorPref != $mModelAxesLabelColor} {
	set mModelAxesLabelColor $mModelAxesLabelColorPref

	if {[info exists itk_component(mged)]} {
	    setColorOption dbCmd -modelAxesLabelColor $mModelAxesLabelColor
	}
    }

    if {$mModelAxesTickIntervalPref != $mModelAxesTickInterval} {
	set mModelAxesTickInterval $mModelAxesTickIntervalPref

	if {[info exists itk_component(mged)]} {
	    dbCmd configure -modelAxesTickInterval $mModelAxesTickInterval
	}
    }

    if {$mModelAxesTicksPerMajorPref != $mModelAxesTicksPerMajor} {
	set mModelAxesTicksPerMajor $mModelAxesTicksPerMajorPref

	if {[info exists itk_component(mged)]} {
	    dbCmd configure -modelAxesTicksPerMajor $mModelAxesTicksPerMajor
	}
    }

    if {$mModelAxesTickThresholdPref != $mModelAxesTickThreshold} {
	set mModelAxesTickThreshold $mModelAxesTickThresholdPref

	if {[info exists itk_component(mged)]} {
	    dbCmd configure -modelAxesTickThreshold $mModelAxesTickThreshold
	}
    }

    if {$mModelAxesTickLengthPref != $mModelAxesTickLength} {
	set mModelAxesTickLength $mModelAxesTickLengthPref

	if {[info exists itk_component(mged)]} {
	    dbCmd configure -modelAxesTickLength $mModelAxesTickLength
	}
    }

    if {$mModelAxesTickMajorLengthPref != $mModelAxesTickMajorLength} {
	set mModelAxesTickMajorLength $mModelAxesTickMajorLengthPref

	if {[info exists itk_component(mged)]} {
	    dbCmd configure -modelAxesTickMajorLength $mModelAxesTickMajorLength
	}
    }

    if {$mModelAxesTickColorPref != $mModelAxesTickColor} {
	set mModelAxesTickColor $mModelAxesTickColorPref

	if {[info exists itk_component(mged)]} {
	    setColorOption dbCmd -modelAxesTickColor $mModelAxesTickColor
	}
    }

    if {$mModelAxesTickMajorColorPref != $mModelAxesTickMajorColor} {
	set mModelAxesTickMajorColor $mModelAxesTickMajorColorPref

	if {[info exists itk_component(mged)]} {
	    setColorOption dbCmd -modelAxesTickMajorColor $mModelAxesTickMajorColor
	}
    }
}


::itcl::body Archer::applyPreferences {} {
    # Apply preferences to the cad widget.
    applyGeneralPreferences
    applyViewAxesPreferences
    applyModelAxesPreferences
    applyDisplayPreferences
}


::itcl::body Archer::applyPreferencesIfDiff {} {
    applyGeneralPreferencesIfDiff
    applyViewAxesPreferencesIfDiff
    applyModelAxesPreferencesIfDiff
    applyGroundPlanePreferencesIfDiff
    applyDisplayPreferencesIfDiff
}


::itcl::body Archer::applyViewAxesPreferences {} {
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

    setColorOption dbCmd -viewAxesColor $mViewAxesColor -viewAxesTripleColor
    setColorOption dbCmd -viewAxesLabelColor $mViewAxesLabelColor
}


::itcl::body Archer::applyViewAxesPreferencesIfDiff {} {

    set positionNotSet 1
    if {$mViewAxesSizePref != $mViewAxesSize} {
	set mViewAxesSize $mViewAxesSizePref

	if {[info exists itk_component(mged)]} {
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

	if {[info exists itk_component(mged)]} {
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

	if {[info exists itk_component(mged)]} {
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

	if {[info exists itk_component(mged)]} {
	    dbCmd configure -viewAxesLineWidth $mViewAxesLineWidth
	}
    }

    if {$mViewAxesColorPref != $mViewAxesColor} {
	set mViewAxesColor $mViewAxesColorPref

	if {[info exists itk_component(mged)]} {
	    setColorOption dbCmd -viewAxesColor $mViewAxesColor -viewAxesTripleColor
	}
    }

    if {$mViewAxesLabelColorPref != $mViewAxesLabelColor} {
	set mViewAxesLabelColor $mViewAxesLabelColorPref

	if {[info exists itk_component(mged)]} {
	    setColorOption dbCmd -viewAxesLabelColor $mViewAxesLabelColor
	}
    }
}

::itcl::body Archer::doPreferences {} {
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
	applyPreferencesIfDiff
    }
}


::itcl::body Archer::readPreferences {} {
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

    backgroundColor [lindex $mBackground 0] \
	[lindex $mBackground 1] \
	[lindex $mBackground 2]
    setColorOption dbCmd -primitiveLabelColor $mPrimitiveLabelColor
    setColorOption dbCmd -viewingParamsColor $mViewingParamsColor
    setColorOption dbCmd -scaleColor $mScaleColor

    update
    setMode
    updateTheme
    updateToggleMode
}


::itcl::body Archer::writePreferences {} {
    global env

    if {$mViewOnly} {
	return
    }

    if {[info exists env(HOME)]} {
	set home $env(HOME)
    } else {
	set home .
    }

    updateHPaneFractions
    updateVPaneFractions

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


################################### Primitive Creation Section ###################################

::itcl::body Archer::createObj {type} {
    dbCmd make_name -s 1

    switch -- $type {
	"arb4" {
	    set name [dbCmd make_name "arb4."]
	    createArb4 $name
	}
	"arb5" {
	    set name [dbCmd make_name "arb5."]
	    createArb5 $name
	}
	"arb6" {
	    set name [dbCmd make_name "arb6."]
	    createArb6 $name
	}
	"arb7" {
	    set name [dbCmd make_name "arb7."]
	    createArb7 $name
	}
	"arb8" {
	    set name [dbCmd make_name "arb8."]
	    createArb8 $name
	}
	"bot" {
	    #XXX Not ready yet
	    return

	    set name [dbCmd make_name "bot."]
	    createBot $name
	}
	"comb" {
	    set name [dbCmd make_name "comb."]
	    createComb $name
	}
	"ehy" {
	    set name [dbCmd make_name "ehy."]
	    createEhy $name
	}
	"ell" {
	    set name [dbCmd make_name "ell."]
	    createEll $name
	}
	"epa" {
	    set name [dbCmd make_name "epa."]
	    createEpa $name
	}
	"eto" {
	    set name [dbCmd make_name "eto."]
	    createEto $name
	}
	"extrude" {
	    #XXX Not ready yet
	    return

	    set name [dbCmd make_name "extrude."]
	    createExtrude $name
	}
	"grip" {
	    set name [dbCmd make_name "grip."]
	    createGrip $name
	}
	"half" {
	    set name [dbCmd make_name "half."]
	    createHalf $name
	}
	"part" {
	    set name [dbCmd make_name "part."]
	    createPart $name
	}
	"pipe" {
	    #XXX Not ready yet
	    return

	    set name [dbCmd make_name "pipe."]
	    createPipe $name
	}
	"rhc" {
	    set name [dbCmd make_name "rhc."]
	    createRhc $name
	}
	"rpc" {
	    set name [dbCmd make_name "rpc."]
	    createRpc $name
	}
	"sketch" {
	    set name [dbCmd make_name "sketch."]
	    createSketch $name
	}
	"sph" {
	    set name [dbCmd make_name "sph."]
	    createSphere $name
	}
	"tgc" {
	    set name [dbCmd make_name "tgc."]
	    createTgc $name
	}
	"tor" {
	    set name [dbCmd make_name "tor."]
	    createTorus $name
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
    dblClick [$itk_component(tree) selection get]
    dbCmd configure -autoViewEnable 1

    set mNeedSave 1
    updateSaveMode
}

::itcl::body Archer::createArb4 {name} {
    if {![info exists itk_component(arb4View)]} {
	buildArb4EditView
	$itk_component(arb4View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb4View) createGeometry $name
}

::itcl::body Archer::createArb5 {name} {
    if {![info exists itk_component(arb5View)]} {
	buildArb5EditView
	$itk_component(arb5View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb5View) createGeometry $name
}

::itcl::body Archer::createArb6 {name} {
    if {![info exists itk_component(arb6View)]} {
	buildArb6EditView
	$itk_component(arb6View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb6View) createGeometry $name
}

::itcl::body Archer::createArb7 {name} {
    if {![info exists itk_component(arb7View)]} {
	buildArb7EditView
	$itk_component(arb7View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb7View) createGeometry $name
}

::itcl::body Archer::createArb8 {name} {
    if {![info exists itk_component(arb8View)]} {
	buildArb8EditView
	$itk_component(arb8View) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(arb8View) createGeometry $name
}

::itcl::body Archer::createBot {name} {
    #XXX Not ready yet
    return

    if {![info exists itk_component(botView)]} {
	buildBotEditView
	$itk_component(botView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(botView) createGeometry $name
}

::itcl::body Archer::createComb {name} {
    if {![info exists itk_component(combView)]} {
	buildCombEditView
	$itk_component(combView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(combView) createGeometry $name
}

::itcl::body Archer::createEhy {name} {
    if {![info exists itk_component(ehyView)]} {
	buildEhyEditView
	$itk_component(ehyView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(ehyView) createGeometry $name
}

::itcl::body Archer::createEll {name} {
    if {![info exists itk_component(ellView)]} {
	buildEllEditView
	$itk_component(ellView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(ellView) createGeometry $name
}

::itcl::body Archer::createEpa {name} {
    if {![info exists itk_component(epaView)]} {
	buildEpaEditView
	$itk_component(epaView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(epaView) createGeometry $name
}

::itcl::body Archer::createEto {name} {
    if {![info exists itk_component(etoView)]} {
	buildEtoEditView
	$itk_component(etoView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(etoView) createGeometry $name
}

::itcl::body Archer::createExtrude {name} {
    #XXX Not ready yet
    return

    if {![info exists itk_component(extrudeView)]} {
	buildExtrudeEditView
	$itk_component(extrudeView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(extrudeView) createGeometry $name
}

::itcl::body Archer::createGrip {name} {
    if {![info exists itk_component(gripView)]} {
	buildGripEditView
	$itk_component(gripView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(gripView) createGeometry $name
}

::itcl::body Archer::createHalf {name} {
    if {![info exists itk_component(halfView)]} {
	buildHalfEditView
	$itk_component(halfView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(halfView) createGeometry $name
}

::itcl::body Archer::createPart {name} {
    if {![info exists itk_component(partView)]} {
	buildPartEditView
	$itk_component(partView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(partView) createGeometry $name
}

::itcl::body Archer::createPipe {name} {
    #XXX Not ready yet
    return

    if {![info exists itk_component(pipeView)]} {
	buildPipeEditView
	$itk_component(pipeView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(pipeView) createGeometry $name
}

::itcl::body Archer::createRhc {name} {
    if {![info exists itk_component(rhcView)]} {
	buildRhcEditView
	$itk_component(rhcView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(rhcView) createGeometry $name
}

::itcl::body Archer::createRpc {name} {
    if {![info exists itk_component(rpcView)]} {
	buildRpcEditView
	$itk_component(rpcView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(rpcView) createGeometry $name
}

::itcl::body Archer::createSketch {name} {
    if {![info exists itk_component(sketchView)]} {
	buildSketchEditView
	$itk_component(sketchView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(sketchView) createGeometry $name
}

::itcl::body Archer::createSphere {name} {
    if {![info exists itk_component(sphView)]} {
	buildSphereEditView
	$itk_component(sphView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(sphView) createGeometry $name
}

::itcl::body Archer::createTgc {name} {
    if {![info exists itk_component(tgcView)]} {
	buildTgcEditView
	$itk_component(tgcView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(tgcView) createGeometry $name
}

::itcl::body Archer::createTorus {name} {
    if {![info exists itk_component(torView)]} {
	buildTorusEditView
	$itk_component(torView) configure \
	    -mged $itk_component(mged)
    }
    $itk_component(torView) createGeometry $name
}


################################### End Protected Section ###################################


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
