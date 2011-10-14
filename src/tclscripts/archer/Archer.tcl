#                     A R C H E R . T C L
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
#    Archer mega-widget.
#


# Set the Tcl encoding to UTF-8
encoding system utf-8

namespace eval Archer {
    if {![info exists debug]} {
	set debug 0
    }

    set methodDecls ""
    set methodImpls ""
    set extraMgedCommands ""
    set corePluginInit ""

    set pluginsdir [file join [bu_brlcad_data "plugins"] archer]
    if {![file exists $pluginsdir]} {
	# searching 'src' is only necessary for items installed to a
	# different hierarchy.
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
catch {package require Tktable 2.10} tktable
package require ManBrowser 1.0
package provide Archer 1.0

::itcl::class Archer {
    inherit ArcherCore

    constructor {{_viewOnly 0} {_noCopy 0} args} {
	ArcherCore::constructor $_viewOnly $_noCopy
    } {}
    destructor {}


    # Dynamically load methods
    if {$Archer::methodDecls != ""} {
	foreach meth $::Archer::methodDecls {
	    eval $meth
	}
    }


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
	proc pluginLoader {}
	proc pluginGed {_archer}
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
	method initAppendPipePoint {_obj _button _callback}
	method initFindPipePoint {_obj _button _callback}
	method initPrependPipePoint {_obj _button _callback}

	# General
	method askToRevert {}
	method bot_fix_all {}
	method bot_fix_all_wrapper {}
	method bot_flip_check      {args}
	method bot_flip_check_all  {}
	method bot_flip_check_all_wrapper  {}
	method bot_split_all {}
	method bot_split_all_wrapper {}
	method bot_sync_all {}
	method bot_sync_all_wrapper {}
	method fbclear {}
	method raytracePlus {}

	# ArcherCore Override Section
	method cmd                 {args}
	method p                   {args}
	method p_protate           {args}
	method p_pscale            {args}
	method p_ptranslate        {args}
	method p_move_arb_edge     {args}
	method p_move_arb_face     {args}
	method p_rotate_arb_face   {args}
	method Load                {_target}
	method saveDb              {}
	method units               {args}
	method initImages          {}
	method initFbImages        {}
	method setDefaultBindingMode {_mode}

	# Object Edit Management
	method revert {}
    }

    protected {
	variable mPrefFile ""

	variable mArcherVersion "0.9.2"
	variable mActiveEditDialogs {}
	variable wizardXmlCallbacks ""
	variable mBotFixAllFlag 0

	# plugin list
	variable mWizardClass ""
	variable mWizardTop ""
	variable mWizardState ""
	variable mNoWizardActive 0

	# ArcherCore Override Section
	method buildCommandView {}
	method dblClick {_tags}
	method handleTreeSelect {}
	method initDefaultBindings {{_comp ""}}
	method initGed {}
	method setActivePane {_pane}
	method updateSaveMode {}

	# Miscellaneous Section
	method buildAboutDialog {}
	method buildarcherHelp {}
	method buildCommandViewNew {_mflag}
	method buildDisplayPreferences {}
	method buildGeneralPreferences {}
	method buildGridPreferences {}
	method buildGroundPlanePreferences {}
	method buildInfoDialogs {}
	method buildModelAxesPosition {_parent}
	method buildModelAxesPreferences {}
	method buildMouseOverridesDialog {}
	method buildOtherGeneralPreferences {_i}
	method buildPreferencesDialog {}
	method buildRevertDialog {}
	method buildToplevelMenubar {_parent {_prefix ""}}
	method buildViewAxesPreferences {}
	method doAboutArcher {}
	method doarcherHelp {}
	method handleConfigure {}
	method launchDisplayMenuBegin {_dm _m _x _y}
	method launchDisplayMenuEnd {}
	method fbActivePaneCallback {_pane}
	method fbEnabledCallback {_on}
	method fbModeCallback {_mode}
	method fbModeToggle {}
	method fbToggle {}
	method rtEndCallback {_aborted}
	method getRayBotNormalCos {_start _target _bot}

	#XXX Need to split up menuStatusCB into one method per menu
	method menuStatusCB {_w}
	method modesMenuStatusCB {_w}
	method rtCheckMenuStatusCB {_w}
	method rtEdgeMenuStatusCB {_w}
	method rtMenuStatusCB {_w}

	method updateCreationButtons {_on}
	method updatePrimaryToolbar {}
	method updateRaytraceButtons {_on}

	method buildEmbeddedMenubar {}
	method buildEmbeddedFileMenu {}
	method buildEmbeddedDisplayMenu {}
	method buildEmbeddedModesMenu {}
	method buildEmbeddedRaytraceMenu {}
	method buildEmbeddedHelpMenu {}

	method buildModesMenu {{_prefix ""}}

	method activateMenusEtc {}

	# Modes Section
	method initMode {{_updateFractions 0}}

	# Object Edit Section
	method initEdit {{_initEditMode 1}}

	# Object Edit VIA Mouse Section
	method beginObjRotate {}
	method beginObjScale {}
	method beginObjTranslate {}
	method beginObjCenter {}
	method endObjCenter {_obj}
	method endObjRotate {_dm _obj}
	method endObjScale {_dm _obj}
	method endObjTranslate {_dm _obj _mx _my}
	method handleObjCenter {_dm _obj _mx _my}


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
	method buildHypEditView {}
	method buildObjAttrView {}
	method buildObjEditView {}
	method buildObjToolView {}
	method buildObjViewToolbar {}
	method buildPartEditView {}
	method buildPipeEditView {}
	method buildRhcEditView {}
	method buildRpcEditView {}
	method buildSketchEditView {}
	method buildSphereEditView {}
	method buildSuperellEditView {}
	method buildTgcEditView {}
	method buildTorusEditView {}
	method buildInvalidObjEditView {}

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
	method initHypEditView {_odata}
	method initNoWizard {_parent _msg}
	method initObjAttrView {}
	method initObjEditView {}
	method initObjToolView {}
	method initObjWizard {_obj _wizardLoaded}
	method initPartEditView {_odata}
	method initPipeEditView {_odata}
	method initRhcEditView {_odata}
	method initRpcEditView {_odata}
	method initSketchEditView {_odata}
	method initSphereEditView {_odata}
	method initSuperellEditView {_odata}
	method initTgcEditView {_odata}
	method initTorusEditView {_odata}
	method initInvalidObjEditView {_oname}

	method updateCombEditView {}
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
	method applyGridPreferences {}
	method applyGridPreferencesIfDiff {}
	method applyGroundPlanePreferencesIfDiff {}
	method applyModelAxesPreferences {}
	method applyModelAxesPreferencesIfDiff {}
	method applyPreferences {}
	method applyPreferencesIfDiff {}
	method applyViewAxesPreferences {}
	method applyViewAxesPreferencesIfDiff {}
	method cancelPreferences {}
	method doPreferences {}
	method readPreferences {}
	method readPreferencesInit {}
	method writePreferences {}
	method writePreferencesHeader {_pfile}
	method writePreferencesBody {_pfile}
	method affectedNodeHighlightCallback {}
	method listViewAllAffectedCallback {}

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
	method createHyp {_name}
	method createPart {_name}
	method createPipe {_name}
	method createRhc {_name}
	method createRpc {_name}
	method createSketch {_name}
	method createSphere {_name}
	method createSuperell {_name}
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
    if {$Archer::extraMgedCommands != ""} {
	eval lappend mArcherCoreCommands $Archer::extraMgedCommands
    }
    lappend mArcherCoreCommands importFg4Sections bot_flip_check \
	bot_flip_check_all bot_split_all bot_sync_all bot_fix_all

    if {!$mViewOnly} {
	updatePrimaryToolbar
    }

    eval itk_initialize $args

    $this configure -background $LABEL_BACKGROUND_COLOR

    if {!$mViewOnly} {
	buildInfoDialogs
	buildSaveDialog
	buildRevertDialog
	buildViewCenterDialog
	buildPreferencesDialog
	buildDbAttrView
	buildObjViewToolbar
	buildObjAttrView
	buildObjEditView
	buildObjToolView

	# set initial toggle variables
	set mVPaneToggle3 $mVPaneFraction3
	set mVPaneToggle5 $mVPaneFraction5

	readPreferences
	set save_hpanefraction1 $mHPaneFraction1
	set save_hpanefraction2 $mHPaneFraction2
	buildCommandViewNew 1

	if {!$mSeparateCommandWindow} {
	    set mHPaneFraction1 $save_hpanefraction1
	    set mHPaneFraction2 $save_hpanefraction2
	    $itk_component(hpane) fraction $mHPaneFraction1 $mHPaneFraction2
	}


	set mDelayCommandViewBuild 0
	pack $itk_component(advancedTabs) -fill both -expand yes
	::update
	initMode

	setTreeView
	updateCreationButtons 0
	updateRaytraceButtons 0
	updateSaveMode
    } else {
	backgroundColor $mBackgroundColor
    }

    set mInstanceInit 0
    initImages

    if {!$mViewOnly} {
	# Change the command window's prompt
	$itk_component(cmd) configure -prompt "Archer> "
	$itk_component(cmd) reinitialize
    }

    $itk_component(primaryToolbar) itemconfigure new -state normal
    $itk_component(primaryToolbar) itemconfigure preferences -state normal


    ::update
    Load ""

    if {!$mViewOnly} {
	pushZClipSettings
    }


    bind [namespace tail $this] <Configure> [::itcl::code $this handleConfigure]
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

    $dialog configure -background $LABEL_BACKGROUND_COLOR

    pack $sf -padx 2 -pady 2 -expand yes -fill both

    $dialog buttonconfigure OK -command "$dialog deactivate; destroy $dialog"
    wm protocol $dialog WM_DELETE_WINDOW "$dialog deactivate; destroy $dialog"
    wm geometry $dialog 400x400

    # Event bindings
    #    bind $dialog <Enter> "raise $dialog"
    #    bind $dialog <Configure> "raise $dialog"
    #    bind $dialog <FocusOut> "raise $dialog"

    $dialog center $w
    ::update
    $dialog activate
}

::itcl::body Archer::pluginLoader {} {
    global env

    set pwd [::pwd]

    # developer & user plugins
    set pluginPath [file join [bu_brlcad_data "plugins"] archer]
    if { ![file exists $pluginPath] } {
	# try a source dir invocation

	# searching 'src' is only necessary for items installed to a
	# different hierarchy.
	set pluginPath [file join [bu_brlcad_data "src"] archer plugins]
    }
    if { ![file exists $pluginPath] } {
	# give up on loading any plugins
	return
    }

    foreach plugindir [list $pluginPath] {
	set utilities_list [concat [lsort [glob -nocomplain $plugindir/Utility/*]]] 
	set wizards_list [concat [lsort [glob -nocomplain $plugindir/Wizards/*]]]
	set plugins_list [concat $utilities_list $wizards_list]
	foreach filename $plugins_list {
	    if [file isfile $filename] {
		set ext [file extension $filename]
		switch -exact -- $ext {
		    ".tcl" -
		    ".itk" -
		    ".itcl" {
			uplevel \#0 source \"$filename\"
	    	    }
	            default {
		        # silently ignore
	            }
		}
	    }
	}
    }

    ::cd $pwd
}


::itcl::body Archer::pluginGed {_archer} {
    if {[catch {$_archer component ged} ged]} {
	return ""
    } else {
	return $ged
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


::itcl::body Archer::initAppendPipePoint {_obj _button _callback} {
    if {![info exists itk_component(ged)]} {
	return
    }

    # This deselects the selected mouse mode in the primary toolbar
    set mDefaultBindingMode FIRST_FREE_BINDING_MODE

    # For the moment, the callback being used here is from PipeEditFrame. At some point,
    # Archer may want to provide the callback in order to do something before passing
    # things through to PipeEditFrame.

    $itk_component(ged) init_append_pipept $_obj $_button $_callback
}


::itcl::body Archer::initFindPipePoint {_obj _button _callback} {
    if {![info exists itk_component(ged)]} {
	return
    }

    # This deselects the selected mouse mode in the primary toolbar
    set mDefaultBindingMode FIRST_FREE_BINDING_MODE

    # For the moment, the callback being used here is from PipeEditFrame. At some point,
    # Archer may want to provide the callback in order to do something before passing
    # things through to PipeEditFrame.

    $itk_component(ged) init_find_pipept $_obj $_button $_callback
}


::itcl::body Archer::initPrependPipePoint {_obj _button _callback} {
    if {![info exists itk_component(ged)]} {
	return
    }

    # This deselects the selected mouse mode in the primary toolbar
    set mDefaultBindingMode FIRST_FREE_BINDING_MODE

    # For the moment, the callback being used here is from PipeEditFrame. At some point,
    # Archer may want to provide the callback in order to do something before passing
    # things through to PipeEditFrame.

    $itk_component(ged) init_prepend_pipept $_obj $_button $_callback
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

    SetWaitCursor $this
    set savedUnits [$itk_component(ged) units -s]
    $itk_component(ged) units in
    $itk_component(ged) configure -autoViewEnable 0
    $itk_component(ged) detachObservers

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

	if { [ $itk_component(ged) exists $solidName ] } {
	    #$itk_component(ged) attachObservers
	    $itk_component(ged) units $savedUnits
	    error "importFg4Sections: $solidName already exists!"
	}

	if {[catch {$itk_component(ged) importFg4Section $solidName $sdata} ret]} {
	    #$itk_component(ged) attachObservers
	    $itk_component(ged) units $savedUnits
	    error "importFg4Sections: $ret"
	}

	eval $itk_component(ged) otranslate $solidName $delta

	# Add to the region
	$itk_component(ged) r $regionName u $solidName

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
	    if { ! [ $itk_component(ged) exists $gname ] } {
		$itk_component(ged) g $gname $gmember
	    } else {
		if {[catch {gedCmd get $gname tree} tree]} {
		    #$itk_component(ged) attachObservers
		    $itk_component(ged) units $savedUnits
		    error "importFg4Sections: $gname is not a group!"
		}

		# Add gmember only if its not already there
		regsub -all {(\{[ul])|([{}]+)} $tree " " tmembers
		if {[lsearch $tmembers $gmember] == -1} {
		    $itk_component(ged) g $gname $gmember
		}
	    }

	    # Add WizardTop attribute
	    $itk_component(ged) attr set $gname WizardTop $wizTop

	    set gmember $gname
	}

	# Add WizardTop attribute to the region and its solid
	$itk_component(ged) attr set $regionName WizardTop $wizTop
	$itk_component(ged) attr set $solidName WizardTop $wizTop

	# Add wizard attributes
	#foreach {key val} $wlist {
	#$itk_component(ged) attr set $wizTop $key $val
	#}

	# Add other attributes that are specific to this region
	foreach {key val} $attrList {
	    $itk_component(ged) attr set $regionName $key $val
	}

	if {[catch {$itk_component(ged) attr get $regionName transparency} tr]} {
	    set tr 1.0
	} else {
	    if {![string is double $tr] || $tr < 0.0 || 1.0 < $tr} {
		set tr 1.0
	    }
	}

	if {[catch {$itk_component(ged) attr get $regionName vmode} vmode]} {
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

	# Add wizard attributes
	foreach {key val} $wlist {
	    $itk_component(ged) attr set $wizTop $key $val
	}

	syncTree
	set mNeedSave 1
	updateSaveMode
	$itk_component(ged) units $savedUnits
	$itk_component(ged) attachObservers
	$itk_component(ged) refreshAll
	$itk_component(ged) configure -autoViewEnable 1
	SetNormalCursor $this
    }
}


::itcl::body Archer::fbclear {} {
    $itk_component(rtcntrl) clear
}


::itcl::body Archer::raytracePlus {} {
    $itk_component(primaryToolbar) itemconfigure raytrace \
	-image $mImage_rtAbort \
	-command "$itk_component(rtcntrl) abort"
    $itk_component(rtcntrl) raytracePlus
}


::itcl::body Archer::askToRevert {} {
    if {!$mNeedSave} {
	return 0
    }

    $itk_component(revertDialog) center [namespace tail $this]
    ::update
    if {[$itk_component(revertDialog) activate]} {
	revert
	return 1
    }

    return 0
}


::itcl::body Archer::bot_fix_all {} {
    set mBotFixAllFlag 1

    set ret_string ""
    append ret_string "[bot_split_all]\n\n"
    append ret_string "[bot_sync_all]\n\n"
    append ret_string "[bot_flip_check_all]"

    set mBotFixAllFlag 0
    redrawWho

    return $ret_string
}


::itcl::body Archer::bot_fix_all_wrapper {} {
    putString [bot_fix_all]
}

::itcl::body Archer::bot_flip_check {args} {
    SetWaitCursor $this

    set skip_count 0
    set no_flip_count 0
    set flip_count 0
    set miss_count 0

    set skip_string ""
    set not_bot_string ""
    set no_flip_string ""
    set flip_string ""
    set miss_string ""

    set nitems [llength $args]
    set icount 0
    set last_percent 0

    putString "Flipping bots ..."

    foreach item $args {
	incr icount

	if {$item == ""} {
	    continue
	}

	if {[catch {$itk_component(ged) get_type $item} gtype]} {
	    incr skip_count
	    append skip_string "$gtype\n"
	    continue
	}

	if {$gtype != "bot"} {
	    incr skip_count
	    append not_bot_string "$item "
	    continue
	}

	if {[$itk_component(ged) get $item mode] != "volume"} {
	    incr skip_count
	    append not_bot_string "$item "
	    continue
	}

	set rpp [$itk_component(ged) bb -q -e $item]
	set min [lindex $rpp 1]
	set max [lindex $rpp 3]
	set target [expr {[vscale [vadd2 $min $max] 0.5]}]

	set diag [vsub2 $max $min]
	set start [vsub2 $target $diag]

	# Turn off orientation
	set save_orient [$itk_component(ged) get $item orient]
	if {$save_orient != "no"} {
	    $itk_component(ged) adjust $item orient no
	}

	set cosa [getRayBotNormalCos $start $target $item]
	if {$cosa == ""} {
	    # Try firing another ray. This time use points from the first triangle.
	    
	    if {[catch {$itk_component(ged) get $item V} bot_vertices]} {
		incr skip_count
		append skip_string "$item: unable to get vertices\n"

		# Set orientation back to original setting
		if {$save_orient != "no"} {
		    $itk_component(ged) adjust $item orient $save_orient
		}

		continue
	    }

	    if {[catch {$itk_component(ged) get $item F} bot_faces] ||
		[llength $bot_faces] < 1} {
		incr skip_count
		append skip_string "$item: unable to get faces\n"

		# Set orientation back to original setting
		if {$save_orient != "no"} {
		    $itk_component(ged) adjust $item orient $save_orient
		}

		continue
	    }

	    set face [lindex $bot_faces 0]
	    set v0 [lindex $bot_vertices [lindex $face 0]]
	    set v1 [lindex $bot_vertices [lindex $face 1]]
	    set v2 [lindex $bot_vertices [lindex $face 2]]

	    if {$v0 == "" || $v1 == "" || $v2 == ""} {
		incr skip_count
		append skip_string "$item: unable to get vertices\n"

		# Set orientation back to original setting
		if {$save_orient != "no"} {
		    $itk_component(ged) adjust $item orient $save_orient
		}

		continue
	    }

	    set oneThird [expr {1 / 3.0}]
	    set target [vscale [vadd3 $v0 $v1 $v2] $oneThird]

	    set v1m0 [vsub2 $v1 $v0]
	    set v2m0 [vsub2 $v2 $v0]
	    set n [vunitize [vcross $v1m0 $v2m0]]
	    set diag_len [vmagnitude $diag]
	    set start [vsub2 $target [vscale $n $diag_len]]

	    set cosa [getRayBotNormalCos $start $target $item]
	    if {$cosa == ""} {
		# This should not happen

		incr skip_count
		append skip_string "$item: unable to get faces\n"

		# Set orientation back to original setting
		if {$save_orient != "no"} {
		    $itk_component(ged) adjust $item orient $save_orient
		}

		continue
	    }
	}

	if {$cosa > 0} {
	    # Set orientation to rh
	    if {$save_orient != "no"} {
		$itk_component(ged) adjust $item orient rh
	    }

	    incr flip_count
	    append flip_string "$item "
	    $itk_component(ged) bot_flip $item
	} else {
	    # Set orientation back to original setting
	    if {$save_orient != "no"} {
		$itk_component(ged) adjust $item orient $save_orient
	    }

	    incr no_flip_count
	    append no_flip_string "$item "
	}

	set percent [format "%.2f" [expr {$icount / double($nitems)}]]
	if {$percent > $last_percent} {
	    set last_percent $percent
	    pluginUpdateProgressBar $percent
	}
    }

    set ret_string "************************ ITEMS SKIPPED ************************\n"
    if {$skip_count} {
	append ret_string "$skip_string\n"
	if {$not_bot_string != ""} {
	    append ret_string "The following are not volume mode bots:\n$not_bot_string\n"
	}
    } else {
	append ret_string "None\n"
    }
    append ret_string "\n"
    append ret_string "\n"
    append ret_string "************************ ITEMS MISSED ************************\n"
    if {$miss_count} {
	append ret_string "$miss_string\n"
    } else {
	append ret_string "None\n"
    }
    append ret_string "\n"
    append ret_string "\n"
    append ret_string "************************ ITEMS NOT FLIPPED ************************\n"
    if {$no_flip_count} {
	append ret_string "$no_flip_string\n"
    } else {
	append ret_string "None\n"
    }
    append ret_string "\n"
    append ret_string "\n"
    append ret_string "************************ ITEMS FLIPPED ************************\n"
    if {$flip_count} {
	append ret_string "$flip_string\n"
    } else {
	append ret_string "None"
    }
    append ret_string "\n"
    append ret_string "\n"
    append ret_string "\n"
    append ret_string "************************ SUMMARY ************************\n"

    if {$skip_count == 1} {
	append ret_string "$skip_count item skipped\n"
    } else {
	append ret_string "$skip_count items skipped\n"
    }

    if {$miss_count == 1} {
        append ret_string "$miss_count item missed\n"
    } else {
	append ret_string "$miss_count items missed\n"
    }

    if {$no_flip_count == 1} {
	append ret_string "$no_flip_count item NOT flipped\n"
    } else {
	append ret_string "$no_flip_count items NOT flipped\n"
    }

    if {$flip_count == 1} {
	append ret_string "$flip_count item flipped\n"
    } else {
	append ret_string "$flip_count items flipped\n"
    }

    SetNormalCursor $this
    pluginUpdateProgressBar 0

    if {$flip_count} {
	setSave
    }

    return $ret_string
}


::itcl::body Archer::bot_flip_check_all {} {
    set blist [split [search . -type bot] "\n "]
    set ret [eval bot_flip_check $blist]

    if {!$mBotFixAllFlag} {
	redrawWho
    }

    return $ret
}


::itcl::body Archer::bot_flip_check_all_wrapper {} {
    putString [bot_flip_check_all]
}


::itcl::body Archer::bot_split_all {} {
    SetWaitCursor $this

    set ret_string "The following bots have been split:\n"
    set blist [split [search . -type bot] "\n "]
    set nitems [llength $blist]
    set icount 0
    set last_percent 0
    set split_count 0
    set split_bots {}

    putString "Splitting bots ..."

    foreach bot $blist {
	incr icount

	if {$bot == ""} {
	    continue
	}

	if {![catch {bot_split2 $bot} bnames] && $bnames != ""} {
	    incr split_count
	    append ret_string "[lindex $bnames 0] "
	    lappend split_bots [lindex $bnames 1]
	}

	set percent [format "%.2f" [expr {$icount / double($nitems)}]]
	if {$percent > $last_percent} {
	    set last_percent $percent
	    pluginUpdateProgressBar $percent
	}
    }

    pluginUpdateProgressBar 0
    SetNormalCursor $this

    if {$split_count} {
	gedCmd make_name -s 0
	set gname [gedCmd make_name bot_split_backup]
	eval gedCmd g $gname $split_bots

	syncTree
	setSave
    }

    if {!$mBotFixAllFlag} {
	redrawWho
    }

    return $ret_string
}


::itcl::body Archer::bot_split_all_wrapper {} {
    putString [bot_split_all]
}


::itcl::body Archer::bot_sync_all {} {
    SetWaitCursor $this

    set blist [split [search . -type bot] "\n "]
    set nitems [llength $blist]
    set icount 0
    set last_percent 0

    putString "Syncing bots ..."
    set ret_string "The following bots have been synced:\n"

    foreach bot $blist {
	incr icount

	if {$bot == ""} {
	    continue
	}
	append ret_string "$bot "
	catch {gedCmd bot_sync $bot}

	set percent [format "%.2f" [expr {$icount / double($nitems)}]]
	if {$percent > $last_percent} {
	    set last_percent $percent
	    pluginUpdateProgressBar $percent
	}
    }

    pluginUpdateProgressBar 0
    SetNormalCursor $this

    setSave
    if {!$mBotFixAllFlag} {
	redrawWho
    }


    return $ret_string
}


::itcl::body Archer::bot_sync_all_wrapper {} {
    putString [bot_sync_all]
}


::itcl::body Archer::cmd {args} {
    set mCoreCmdLevel 1
    set ret [eval ArcherCore::cmd $args]
    set mCoreCmdLevel 0

    return $ret
}


::itcl::body Archer::p {args} {
    if {$mSelectedObj == ""} {
	return
    }

    set ret ""

    if {$GeometryEditFrame::mEditPCommand != ""} {
	set err [catch {eval $GeometryEditFrame::mEditPCommand $mSelectedObj $args} ret]
    } else {
	switch -- $mDefaultBindingMode \
	    $OBJECT_ROTATE_MODE {
		if {[llength $args] != 3 ||
		    ![string is double [lindex $args 0]] ||
		    ![string is double [lindex $args 1]] ||
		    ![string is double [lindex $args 2]]} {
		    return "Usage: p rx ry rz"
		}

		set err [catch {eval gedCmd orotate $mSelectedObj $args} ret]
	    } \
	    $OBJECT_TRANSLATE_MODE {
		if {[llength $args] != 3 ||
		    ![string is double [lindex $args 0]] ||
		    ![string is double [lindex $args 1]] ||
		    ![string is double [lindex $args 2]]} {
		    return "Usage: p tx ty tz"
		}

		set err [catch {eval gedCmd otranslate $mSelectedObj $args} ret]
	    } \
	    $OBJECT_SCALE_MODE {
		if {[llength $args] != 1 || ![string is double $args]} {
		    return "Usage: p sf"
		}

		set err [catch {eval gedCmd oscale $mSelectedObj $args} ret]
	    } \
	    $OBJECT_CENTER_MODE {
		if {[llength $args] != 3 ||
		    ![string is double [lindex $args 0]] ||
		    ![string is double [lindex $args 1]] ||
		    ![string is double [lindex $args 2]]} {
		    return "Usage: p cx cy cz"
		}

		set err [catch {eval gedCmd ocenter $mSelectedObj $args} ret]
	    } \
	    default {
		return "Nothing appropriate."
	    }
    }

    if {!$err} {
	set mNeedSave 1
	updateSaveMode
	redrawObj $mSelectedObjPath
	initEdit 0
    }

    return $ret
}


::itcl::body Archer::p_protate {args} {
    catch {eval gedCmd protate $args} ret
    return $ret
}


::itcl::body Archer::p_pscale {args} {
    catch {eval gedCmd pscale $args} ret
    return $ret
}


::itcl::body Archer::p_ptranslate {args} {
    catch {eval gedCmd ptranslate $args} ret
    return $ret
}


::itcl::body Archer::p_move_arb_edge {args} {
    catch {eval gedCmd move_arb_edge $args} ret
    return $ret
}


::itcl::body Archer::p_move_arb_face {args} {
    catch {eval gedCmd move_arb_face $args} ret
    return $ret
}


::itcl::body Archer::p_rotate_arb_face {args} {
    catch {eval gedCmd rotate_arb_face $args} ret
    return $ret
}


::itcl::body Archer::Load {_target} {
    SetWaitCursor $this
    if {$mNeedSave} {
	askToSave
    }

    set mNeedSave 0
    updateSaveMode

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
	    grid $itk_component(ged) -row 0 -column 0 -columnspan 3 -sticky news
	    after idle "$this component cmd configure -cmd_prefix \"[namespace tail $this] cmd\""
	} else {
	    grid $itk_component(ged) -row 1 -column 0 -sticky news
	}
    }

    $itk_component(ged) refresh_off

    set mDbTitle [$itk_component(ged) title]
    set mDbUnits [$itk_component(ged) units -s]

    if {!$mViewOnly} {
	initDbAttrView $mTarget
	applyPreferences
	doLighting
	updateWizardMenu
	updateUtilityMenu
	deleteTargetOldCopy

	updateCreationButtons 1
	#	updateRaytraceButtons 1

	buildGroundPlane
	showGroundPlane
    } else {
	applyPreferences
	doLighting
    }

    # update the units combobox in the General tab of the preferences panel
    set utypes {}
    foreach utype [split [$itk_component(ged) units -t] , ] {
	lappend utypes [string trim $utype]
    }
    $itk_component(unitsCB) configure \
	-values $utypes \
	-state readonly

    # refresh tree contents
    rebuildTree

    if {$mBindingMode == "Default"} {
	set mDefaultBindingMode $VIEW_ROTATE_MODE
	beginViewRotate
    }
    $itk_component(ged) refresh_on
    $itk_component(ged) refresh_all
    SetNormalCursor $this
}


::itcl::body Archer::saveDb {} {
    ArcherCore::saveDb
}


::itcl::body Archer::units {args} {
    set b2l_1 [gedCmd base2local]
    set ret [eval ArcherCore::units $args]
    set mDbUnits [gedCmd units -s]
    set b2l_2 [gedCmd base2local]

    if {$b2l_1 != $b2l_2} {
	# Update grid parameters
	set sf [expr {$b2l_2 / $b2l_1}]

	set X [lindex $mGridAnchor 0]
	set Y [lindex $mGridAnchor 1]
	set Z [lindex $mGridAnchor 2]
	set X [expr {$sf * $X}]
	set Y [expr {$sf * $Y}]
	set Z [expr {$sf * $Z}]
	set mGridAnchor "$X $Y $Z"
	set mGridRh [expr {$sf * $mGridRh}]
	set mGridRv [expr {$sf * $mGridRv}]
    }

    return $ret
}


::itcl::body Archer::initImages {} {
    if {$mInstanceInit} {
	return
    }

    ArcherCore::initImages

    if {!$mViewOnly} {
	# Primary 
	$itk_component(primaryToolbar) itemconfigure checkpoint \
	    -image [image create photo \
			-file [file join $mImgDir checkpoint.png]]
	$itk_component(primaryToolbar) itemconfigure object_undo \
	    -image [image create photo \
			-file [file join $mImgDir object_undo.png]]
	$itk_component(primaryToolbar) itemconfigure new \
	    -image [image create photo \
			-file [file join $mImgDir file_new.png]]
	$itk_component(primaryToolbar) itemconfigure global_undo \
	    -image [image create photo \
			-file [file join $mImgDir global_undo.png]]
	$itk_component(primaryToolbar) itemconfigure revert \
	    -image [image create photo \
			-file [file join $mImgDir revert.png]]
	$itk_component(primaryToolbar) itemconfigure preferences \
	    -image [image create photo \
			-file [file join $mImgDir configure.png]]
	$itk_component(primaryToolbar) itemconfigure comb \
	    -image [image create photo \
			-file [file join $mImgDir combination.png]]
	#	$itk_component(primaryToolbar) itemconfigure arb6 \
	    -image [image create photo \
			-file [file join $mImgDir primitive_arb6.png]]
	#	$itk_component(primaryToolbar) itemconfigure arb8 \
	    -image [image create photo \
			-file [file join $mImgDir primitive_arb8.png]]
	#	$itk_component(primaryToolbar) itemconfigure cone \
	    -image [image create photo \
			-file [file join $mImgDir primitive_cone.png]]
	#	$itk_component(primaryToolbar) itemconfigure pipe \
	    -image [image create photo \
			-file [file join $mImgDir primitive_pipe.png]]
	#	$itk_component(primaryToolbar) itemconfigure sphere \
	    -image [image create photo \
			-file [file join $mImgDir primitive_sph.png]]
	#	$itk_component(primaryToolbar) itemconfigure torus \
	    -image [image create photo \
			-file [file join $mImgDir primitive_tor.png]]
	$itk_component(primaryToolbar) itemconfigure other \
	    -image [image create photo \
			-file [file join $mImgDir primitive_list.png]]

	# View Toolbar
	#	$itk_component(primaryToolbar) itemconfigure rotate \
	    #	    -image [image create photo \
	    #			-file [file join $mImgDir view_rotate.png]]
	#	$itk_component(primaryToolbar) itemconfigure translate \
	    #	    -image [image create photo \
	    #			-file [file join $mImgDir view_translate.png]]
	#	$itk_component(primaryToolbar) itemconfigure scale \
	    #	    -image [image create photo \
	    #			-file [file join $mImgDir view_scale.png]]
	#	$itk_component(primaryToolbar) itemconfigure center \
	    #	    -image [image create photo \
	    #			-file [file join $mImgDir view_select.png]]
	#	$itk_component(primaryToolbar) itemconfigure cpick \
	    #	    -image [image create photo \
	    #			-file [file join $mImgDir compSelect.png]]
	#	$itk_component(primaryToolbar) itemconfigure measure \
	    #	    -image [image create photo \
	    #			-file [file join $mImgDir measure.png]]

	# We catch this because the item may not exist
	catch {$itk_component(primaryToolbar) itemconfigure wizards \
		   -image [image create photo \
			       -file [file join $mImgDir wizard.png]]}

	#$itk_component(primaryToolbar) itemconfigure ehy \
	    -image [image create photo \
			-file [file join $mImgDir primitive_ehy.png]]
	#$itk_component(primaryToolbar) itemconfigure epa \
	    -image [image create photo \
			-file [file join $mImgDir primitive_epa.png]]
	#$itk_component(primaryToolbar) itemconfigure rpc \
	    -image [image create photo \
			-file [file join $mImgDir primitive_rpc.png]]
	#$itk_component(primaryToolbar) itemconfigure rhc \
	    -image [image create photo \
			-file [file join $mImgDir primitive_rhc.png]]
	#$itk_component(primaryToolbar) itemconfigure ell \
	    -image [image create photo \
			-file [file join $mImgDir primitive_ell.png]]
	#$itk_component(primaryToolbar) itemconfigure eto \
	    -image [image create photo \
			-file [file join $mImgDir primitive_eto.png]]
	#$itk_component(primaryToolbar) itemconfigure half \
	    -image [image create photo \
			-file [file join $mImgDir primitive_half.png]]
	#$itk_component(primaryToolbar) itemconfigure part \
	    -image [image create photo \
			-file [file join $mImgDir primitive_part.png]]
	#$itk_component(primaryToolbar) itemconfigure grip \
	    -image [image create photo \
			-file [file join $mImgDir primitive_grip.png]]
	#$itk_component(primaryToolbar) itemconfigure extrude \
	    -image [image create photo \
			-file [file join $mImgDir primitive_extrude.png]]
	#$itk_component(primaryToolbar) itemconfigure sketch \
	    -image [image create photo \
			-file [file join $mImgDir primitive_sketch.png]]
	#$itk_component(primaryToolbar) itemconfigure bot \
	    -image [image create photo \
			-file [file join $mImgDir primitive_bot.png]]
	#$itk_component(primaryToolbar) itemconfigure tgc \
	    -image [image create photo \
			-file [file join $mImgDir primitive_tgc.png]]
	#$itk_component(primaryToolbar) itemconfigure superell \
	    -image [image create photo \
			-file [file join $mImgDir primitive_superell.png]]
	#$itk_component(primaryToolbar) itemconfigure hyp \
	    -image [image create photo \
			-file [file join $mImgDir primitive_hyp.png]]

	catch {
	    $itk_component(primaryToolbar) itemconfigure edit_rotate \
		-image [image create photo \
			    -file [file join $mImgDir edit_rotate.png]]
	    $itk_component(primaryToolbar) itemconfigure edit_translate \
		-image [image create photo \
			    -file [file join $mImgDir edit_translate.png]]
	    $itk_component(primaryToolbar) itemconfigure edit_scale \
		-image [image create photo \
			    -file [file join $mImgDir edit_scale.png]]
	    $itk_component(primaryToolbar) itemconfigure edit_center \
		-image [image create photo \
			    -file [file join $mImgDir edit_select.png]]
	}

	# Attribute View Toolbar
	$itk_component(objViewToolbar) itemconfigure objAttrView \
	    -image [image create photo \
			-file [file join $mImgDir option_text.png]]
	$itk_component(objViewToolbar) itemconfigure objEditView \
	    -image [image create photo \
			-file [file join $mImgDir option_edit.png]]
	$itk_component(objViewToolbar) itemconfigure objToolView \
	    -image [image create photo \
			-file [file join $mImgDir tools.png]]
    }

    initFbImages
}


::itcl::body Archer::initFbImages {} {
    set mImage_fbOff [image create photo -file [file join $mImgDir framebuffer_off.png]]
    set mImage_fbOn [image create photo -file [file join $mImgDir framebuffer.png]]
    set mImage_fbInterlay [image create photo -file [file join $mImgDir framebuffer_interlay.png]]
    set mImage_fbOverlay [image create photo -file [file join $mImgDir framebuffer_overlay.png]]
    set mImage_fbUnderlay [image create photo -file [file join $mImgDir framebuffer_underlay.png]]
    set mImage_rt [image create photo -file [file join $mImgDir raytrace.png]]
    set mImage_rtAbort [image create photo -file [file join $mImgDir raytrace_abort.png]]

    $itk_component(primaryToolbar) itemconfigure toggle_fb \
	-image $mImage_fbOn
    $itk_component(primaryToolbar) itemconfigure toggle_fb_mode \
	-image $mImage_fbUnderlay
    $itk_component(primaryToolbar) itemconfigure raytrace \
	-image $mImage_rt
    $itk_component(primaryToolbar) itemconfigure clear_fb \
	-image [image create photo \
		    -file [file join $mImgDir framebuffer_clear.png]]
}


::itcl::body Archer::setDefaultBindingMode {_mode} {
    set saved_mode $mDefaultBindingMode

    if {[::ArcherCore::setDefaultBindingMode $_mode]} {
	return
    }

    switch -- $mDefaultBindingMode \
	$OBJECT_ROTATE_MODE { \
				  beginObjRotate
	} \
	$OBJECT_TRANSLATE_MODE { \
				     beginObjTranslate
	} \
	$OBJECT_SCALE_MODE { \
				 beginObjScale
	} \
	$OBJECT_CENTER_MODE { \
				  if {$saved_mode == $OBJECT_TRANSLATE_MODE} { \
										   set mDefaultBindingMode $saved_mode
				      beginObjTranslate
				  } else { \
					       beginObjCenter
				  } \
			      }
}


################################### End Public Section ###################################


################################### Protected Section ###################################

################################### ArcherCore Override Section ###################################

::itcl::body Archer::buildCommandView {} {
    set mDelayCommandViewBuild 1
}


::itcl::body Archer::buildCommandViewNew {_mflag} {
    ::ArcherCore::buildCommandView

    if {$ArcherCore::inheritFromToplevel} {
	if {$_mflag} {
	    buildToplevelMenubar $itk_interior
	    $this configure -menu $itk_component(menubar)
	}

	if {$mSeparateCommandWindow} {
	    buildToplevelMenubar $itk_component(sepcmdT) $mSepCmdPrefix
	    $itk_component(sepcmdT) configure -menu $itk_component(${mSepCmdPrefix}menubar)
	    set mHPaneFraction1 100
	    set mHPaneFraction2 0
	}

	if {!$mSeparateCommandWindow} {
	    set mHPaneFraction1 80
	    set mHPaneFraction2 20
	} else {
	    set xy [winfo pointerxy [namespace tail $this]]
	    wm geometry $itk_component(sepcmdT) "+[lindex $xy 0]+[lindex $xy 1]"
	}

	after idle "$itk_component(cmd) configure -cmd_prefix \"[namespace tail $this] cmd\""
    } else {
	if {$_mflag} {
	    buildEmbeddedMenubar
	    pack $itk_component(menubar) -side top -fill x -padx 1
	}
    }

    if {!$mViewOnly} {
	$itk_component(cmd) configure -prompt "Archer> "
	$itk_component(cmd) reinitialize
    }
}


::itcl::body Archer::dblClick {tags} {
    return

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

    switch -- $type {
	"leaf"   -
	"branch" {renderComp $node}
    }
}


::itcl::body Archer::handleTreeSelect {} {
    ::ArcherCore::handleTreeSelect

    if {!$mViewOnly} {
	if {$mObjViewMode == $OBJ_ATTR_VIEW_MODE} {
	    initObjAttrView
	} elseif {$mObjViewMode == $OBJ_EDIT_VIEW_MODE} {
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
}


::itcl::body Archer::initDefaultBindings {{_comp ""}} {
    if {$_comp == ""} {
	set _comp $itk_component(ged)
    }

    ArcherCore::initDefaultBindings $_comp

    if {$mSeparateCommandWindow} {
	set prefix $mSepCmdPrefix
    } else {
	set prefix ""
    }

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win $dm

	if {$mViewOnly} {
	    bind $win <3> \
		"[::itcl::code $this launchDisplayMenuBegin $dname [$itk_component(canvas_menu) component view-menu] %X %Y]; break"
	} else {
	    if {$ArcherCore::inheritFromToplevel} {
		bind $win <3> \
		    "[::itcl::code $this launchDisplayMenuBegin $dname $itk_component(${prefix}displaymenu) %X %Y]; break"
	    } else {
		bind $win <3> \
		    "[::itcl::code $this launchDisplayMenuBegin $dname [$itk_component(menubar) component display-menu] %X %Y]; break"
	    }
	}
    }

    $itk_component(primaryToolbar) itemconfigure edit_rotate -state normal
    $itk_component(primaryToolbar) itemconfigure edit_translate -state normal
    $itk_component(primaryToolbar) itemconfigure edit_scale -state normal
    $itk_component(primaryToolbar) itemconfigure edit_center -state normal
}


::itcl::body Archer::initGed {} {
    ArcherCore::initGed
    activateMenusEtc
    setActivePane ur
}


::itcl::body Archer::setActivePane {_pane} {
    $itk_component(rtcntrl) setActivePane $_pane
}


::itcl::body Archer::updateSaveMode {} {
    ArcherCore::updateSaveMode

    if {$mViewOnly} {
	return
    }

    if {$mSeparateCommandWindow} {
	set prefix $mSepCmdPrefix
    } else {
	set prefix ""
    }

    catch {
	if {$mSeparateCommandWindow} {
	    set plist [list {} $mSepCmdPrefix]
	} else {
	    set plist {{}}
	}

	if {!$mDbNoCopy && !$mDbReadOnly && $mNeedSave} {
	    $itk_component(primaryToolbar) itemconfigure revert \
		-state normal

	    if {$ArcherCore::inheritFromToplevel} {
		foreach prefix $plist {
		    $itk_component(${prefix}filemenu) entryconfigure "Save" -state normal
		    $itk_component(${prefix}filemenu) entryconfigure "Revert" -state normal
		}
	    } else {
		$itk_component(menubar) menuconfigure .file.save \
		    -state normal
		$itk_component(menubar) menuconfigure .file.revert \
		    -state normal
	    }
	} else {
	    $itk_component(primaryToolbar) itemconfigure revert \
		-state disabled

	    if {$ArcherCore::inheritFromToplevel} {
		foreach prefix $plist {
		    $itk_component(${prefix}filemenu) entryconfigure "Save" -state disabled
		    $itk_component(${prefix}filemenu) entryconfigure "Revert" -state disabled
		}
	    } else {
		$itk_component(menubar) menuconfigure .file.save \
		    -state disabled
		$itk_component(menubar) menuconfigure .file.revert \
		    -state disabled
	    }
	}
    }
}



################################### Miscellaneous Section ###################################

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
	::ttk::notebook $parent.tabs
    } {}

    # About Info
    set imgfile [file join [bu_brlcad_data "tclscripts"] archer images aboutArcher.png]
    set aboutImg [image create photo -file $imgfile]
    itk_component add aboutInfo {
	::ttk::label $itk_component(aboutDialogTabs).aboutInfo \
	    -image $aboutImg
    } {}

    # BRL-CAD License Info
    itk_component add brlcadLicenseInfo {
	::iwidgets::scrolledtext $itk_component(aboutDialogTabs).brlcadLicenseInfo \
	    -wrap word \
	    -hscrollmode dynamic \
	    -vscrollmode dynamic \
	    -textfont $mFontText \
	    -background $SystemButtonFace \
	    -textbackground $SystemButtonFace
    } {}

    set brlcadLicenseFile [file join [bu_brlcad_data "."] COPYING]
    if {![catch {open $brlcadLicenseFile "r"} fd]} {
	set brlcadLicenseInfo [read $fd]
	close $fd
	$itk_component(brlcadLicenseInfo) insert 0.0 $brlcadLicenseInfo
    }
    $itk_component(brlcadLicenseInfo) configure -state disabled

    # Acknowledgement Info
    itk_component add ackInfo {
	::iwidgets::scrolledtext $itk_component(aboutDialogTabs).info \
	    -wrap word \
	    -hscrollmode dynamic \
	    -vscrollmode dynamic \
	    -textfont $mFontText \
	    -background $SystemButtonFace \
	    -textbackground $SystemButtonFace
    } {}

    set ackFile [file join [bu_brlcad_data "doc"] archer_ack.txt]
    if {![catch {open $ackFile "r"} fd]} {
	set ackInfo [read $fd]
	close $fd
	$itk_component(ackInfo) insert 0.0 $ackInfo
    }
    $itk_component(ackInfo) configure -state disabled

    itk_component add mikeF {
	::frame $itk_component(aboutDialogTabs).mikeInfo
    } {}

    # try installed, uninstalled
    set imgfile [file join [bu_brlcad_data "tclscripts"] mged mike-tux.png]
    set mikeImg [image create photo -file $imgfile]
    itk_component add mikePic {
	::label $itk_component(mikeF).pic \
	    -image $mikeImg
    } {}

    set row 0
    grid $itk_component(mikePic) -row $row -sticky ew

    itk_component add mikeDates {
	label $itk_component(mikeF).dates \
	    -text "Michael John Muuss\nOctober 16, 1958 - November 20, 2000"
    } {}

    incr row
    grid $itk_component(mikeDates) -row $row -sticky ew

    itk_component add mikeInfo {
	::iwidgets::scrolledtext $itk_component(mikeF).info \
	    -wrap word \
	    -hscrollmode dynamic \
	    -vscrollmode dynamic \
	    -textfont $mFontText \
	    -background $SystemButtonFace \
	    -textbackground $SystemButtonFace
    } {}

    set mikeInfoFile [file join [bu_brlcad_data "tclscripts"] mged mike-dedication.txt]
    if {![catch {open $mikeInfoFile "r"} fd]} {
	set mikeInfo [read -nonewline $fd]
	close $fd
	$itk_component(mikeInfo) insert 0.0 $mikeInfo
    }

    incr row
    grid $itk_component(mikeInfo) -row $row -sticky nsew

    grid columnconfigure $itk_component(mikeF) 0 -weight 1
    grid rowconfigure $itk_component(mikeF) $row -weight 1

    $itk_component(aboutDialogTabs) add $itk_component(aboutInfo) -text "About" -stick ns
    $itk_component(aboutDialogTabs) add $itk_component(brlcadLicenseInfo) -text "License"
    $itk_component(aboutDialogTabs) add $itk_component(ackInfo) -text "Acknowledgements"
    $itk_component(aboutDialogTabs) add $itk_component(mikeF) -text "Dedication"

    $itk_component(aboutDialog) configure -background $LABEL_BACKGROUND_COLOR

    pack $itk_component(aboutDialogTabs) -expand yes -fill both

    wm geometry $itk_component(aboutDialog) "600x600"
}


proc Archer::get_html_data {helpfile} {
    global archer_help_data

    set help_fd [open $helpfile]
    set archer_help_data [read $help_fd]
    close $help_fd
}


proc Archer::get_html_man_data {cmdname} {
    global archer_help_data
    set help_fd [open [file join [bu_brlcad_data "html"] mann en $cmdname.html]]
    set archer_help_data [read $help_fd]
    close $help_fd
}


proc Archer::html_man_display {w} {
    global archer_help_data
    $w reset;
    $w configure -parsemode html
    $w parse $archer_help_data
}


proc Archer::html_help_display {me} {
    global htmlviewer
    global archer_help_data

    upvar $me O
    set origurl $O(-uri)
    if {[catch {regexp {(home://blank)(.+)} $origurl match prefix tempurl} msg]} {
	tk_messageBox -message "html_help_display: regexp failed, msg - $msg"
    }
    set url [bu_brlcad_data "html"]
    append url $tempurl
    get_html_data $url
    $htmlviewer reset
    $htmlviewer parse $archer_help_data
}


proc Archer::mkHelpTkImage {file} {
    set fullpath [file join [bu_brlcad_data "html"] manuals mged $file]
    set name [image create photo -file $fullpath]
    return [list $name [list image delete $name]]
}


proc title_node_handler {node} {
    set titletext ""
    foreach child [$node children] {
	append titletext [$child text]
    }
}


::itcl::body Archer::buildarcherHelp {} {
    global env
    global archer_help_data
    global htmlviewer

    itk_component add archerHelp {
	::iwidgets::dialog $itk_interior.archerHelp \
	    -modality none \
	    -title "Archer Help Browser" \
	    -background $SystemButtonFace
    } {}
    $itk_component(archerHelp) hide 1
    $itk_component(archerHelp) hide 2
    $itk_component(archerHelp) hide 3
    $itk_component(archerHelp) configure \
	-thickness 2 \
	-buttonboxpady 0
    $itk_component(archerHelp) buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0

    # ITCL can be nasty
    set win [$itk_component(archerHelp) component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set tlparent [$itk_component(archerHelp) childsite]


    if {[file exists [file join [bu_brlcad_data "html"] books en BRL-CAD_Tutorial_Series-VolumeI.html]] &&
        [file exists [file join [bu_brlcad_data "html"] toc.html]] } {

	# Table of Contents
	itk_component add archerHelpToC {
	    ::tk::frame $tlparent.archerManToc
	} {}
	set docstoc $itk_component(archerHelpToC)
	pack $docstoc -side left -expand yes -fill y
	
	# HTML widget
	set docstoclist [::hv3::hv3 $docstoc.htmlview -width 250 -requestcmd Archer::html_help_display]
	set docstochtml [$docstoclist html]
	$docstochtml configure -parsemode html 
	set help_fd [lindex [list [file join [bu_brlcad_data "html"] toc.html]] 0]
	get_html_data $help_fd
	$docstochtml parse $archer_help_data

	grid $docstoclist -sticky nsew -in $docstoc

	grid columnconfigure $docstoc 0 -weight 1
	grid rowconfigure $docstoc 0 -weight 1

	pack $docstoc -side left -expand yes -fill both

	pack $itk_component(archerHelpToC) -side left -expand no -fill y


	# Main HTML window

	itk_component add archerHelpF {
	    ::tk::frame $tlparent.archerHelpF
	} {}

	set sfcs $itk_component(archerHelpF)
	pack $sfcs -expand yes -fill both 

	# HTML widget
	set hv3htmlviewer [::hv3::hv3 $sfcs.htmlview]
	set htmlviewer [$hv3htmlviewer html]
	$htmlviewer configure -parsemode html
	$htmlviewer configure -imagecmd Archer::mkHelpTkImage
	set help_fd [lindex [list [file join [bu_brlcad_data "html"] books en BRL-CAD_Tutorial_Series-VolumeI.html]] 0]
	get_html_data $help_fd
	$htmlviewer parse $archer_help_data

	grid $hv3htmlviewer -sticky nsew -in $sfcs

	grid columnconfigure $sfcs 0 -weight 1
	grid rowconfigure $sfcs 0 -weight 1

	pack $itk_component(archerHelpF) -side left -expand yes -fill both

    } 

    wm geometry $itk_component(archerHelp) "1100x800"
}

::itcl::body Archer::buildDisplayPreferences {} {
    set oglParent $itk_component(preferenceTabs)
    itk_component add displayF {
	::ttk::frame $oglParent.displayF
    } {}

    set parent $itk_component(displayF)

    itk_component add zclipFrontL {
	::ttk::label $parent.zclipFrontL \
	    -anchor se \
	    -text "ZClip Percent (Front):"
    } {}
    itk_component add zclipFrontS {
	::scale $parent.zclipFrontS \
	    -showvalue 1 \
	    -orient horizontal \
	    -from 1.0 \
	    -to 100.0 \
	    -resolution 0.01 \
	    -variable [::itcl::scope mZClipFrontPref] \
	    -command [::itcl::code $this updateZClipPlanes]
    }

    itk_component add zclipBackL {
	::ttk::label $parent.zclipBackL \
	    -anchor se \
	    -text "ZClip Percent (Back):"
    } {}
    itk_component add zclipBackS {
	::scale $parent.zclipBackS \
	    -showvalue 1 \
	    -orient horizontal \
	    -from 1.0 \
	    -to 100.0 \
	    -resolution 0.01 \
	    -variable [::itcl::scope mZClipBackPref] \
	    -command [::itcl::code $this updateZClipPlanes]
    }

    itk_component add zclipBackMaxL {
	::ttk::label $parent.zclipBackMaxL \
	    -anchor e \
	    -text "ZClip Max (Back):"
    } {}
    itk_component add zclipBackMaxF {
	::ttk::frame $parent.zclipBackMaxF
    } {}
    itk_component add zclipBackMaxE {
	::ttk::entry $itk_component(zclipBackMaxF).zclipBackMaxE \
	    -width 12 \
	    -textvariable [::itcl::scope mZClipBackMaxPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add zclipBackMaxB {
	::ttk::button $itk_component(zclipBackMaxF).zclipBackMaxB \
	    -width -1 \
	    -text "Compute" \
	    -command [::itcl::code $this calculateZClipBackMax]
    } {}
    grid $itk_component(zclipBackMaxE) -column 0 -row 0 -sticky nsew
    grid $itk_component(zclipBackMaxB) -column 1 -row 0 -sticky nse
    grid columnconfigure $itk_component(zclipBackMaxF) 0 -weight 1

    itk_component add zclipFrontMaxL {
	::ttk::label $parent.zclipFrontMaxL \
	    -anchor e \
	    -text "ZClip Max (Front):"
    } {}
    itk_component add zclipFrontMaxF {
	::ttk::frame $parent.zclipFrontMaxF
    } {}
    itk_component add zclipFrontMaxE {
	::ttk::entry $itk_component(zclipFrontMaxF).zclipFrontMaxE \
	    -width 12 \
	    -textvariable [::itcl::scope mZClipFrontMaxPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add zclipFrontMaxB {
	::ttk::button $itk_component(zclipFrontMaxF).zclipFrontMaxB \
	    -width -1 \
	    -text "Compute" \
	    -command [::itcl::code $this calculateZClipFrontMax]
    } {}
    grid $itk_component(zclipFrontMaxE) -column 0 -row 0 -sticky nsew
    grid $itk_component(zclipFrontMaxB) -column 1 -row 0 -sticky nse
    grid columnconfigure $itk_component(zclipFrontMaxF) 0 -weight 1

    set i 0
    grid $itk_component(zclipBackL) -column 0 -row $i -sticky se
    grid $itk_component(zclipBackS) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(zclipBackMaxL) -column 0 -row $i -sticky e
    grid $itk_component(zclipBackMaxF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(zclipFrontL) -column 0 -row $i -sticky se
    grid $itk_component(zclipFrontS) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(zclipFrontMaxL) -column 0 -row $i -sticky e
    grid $itk_component(zclipFrontMaxF) -column 1 -row $i -sticky ew

    incr i
    grid rowconfigure $parent $i -weight 1
    grid columnconfigure $parent 1 -weight 1

    set i 0
    grid $parent -column 0 -row $i -sticky nsew

    $itk_component(preferenceTabs) add $itk_component(displayF) -text "Display"
}


::itcl::body Archer::buildGeneralPreferences {} {
    set parent $itk_component(preferenceTabs)
    itk_component add generalF {
	::ttk::frame $parent.generalF
    } {}

    buildComboBox $itk_component(generalF) \
	backgroundColor \
	bcolor \
	mBackgroundColorPref \
	"Background Color:" \
	$mColorListNoTriple

    buildComboBox $itk_component(generalF) \
	fbbackgroundColor \
	fbbcolor \
	mFBBackgroundColorPref \
	"FB Background Color:" \
	$mColorListNoTriple

    buildComboBox $itk_component(generalF) \
	fontsize \
	fontsize \
	mDisplayFontSizePref \
	"Font Size:" \
	$mDisplayFontSizes

    buildComboBox $itk_component(generalF) \
	binding \
	binding \
	mBindingModePref \
	"Display Window Bindings:" \
	{Default BRL-CAD}

    buildComboBox $itk_component(generalF) \
	primitiveLabelColor \
	plcolor \
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

    buildComboBox $itk_component(generalF) \
	units \
	units \
	mDbUnits \
	"Units:" \
	{}
    $itk_component(unitsCB) configure -state disabled

    itk_component add treeAttrsL {
	::ttk::label $itk_component(generalF).treeAttrsL \
	    -anchor e \
	    -text "Tree Attributes"
    } {}
    itk_component add treeAttrsE {
	::ttk::entry $itk_component(generalF).treeAttrsE \
	    -width 12 \
	    -textvariable [::itcl::scope mTreeAttrColumnsPref]
    } {}

    itk_component add affectedTreeNodesModeCB {
	::ttk::checkbutton $itk_component(generalF).affectedTreeNodesModeCB \
	    -text "Highlight Affected Tree/List Nodes" \
	    -variable [::itcl::scope mEnableAffectedNodeHighlightPref] \
	    -command [::itcl::code $this affectedNodeHighlightCallback]
    } {}

    itk_component add listViewCB {
	::ttk::checkbutton $itk_component(generalF).listViewCB \
	    -text "List View" \
	    -variable [::itcl::scope mEnableListViewPref]
    } {}

    itk_component add listViewAllAffectedCB {
	::ttk::checkbutton $itk_component(generalF).listViewAllAffectedCB \
	    -text "All Affected Nodes Highlighted (List View Only)" \
	    -variable [::itcl::scope mEnableListViewAllAffectedPref] \
	    -command [::itcl::code $this listViewAllAffectedCallback]
    } {}

    if {$ArcherCore::inheritFromToplevel} {
	itk_component add sepCmdWinCB {
	    ::ttk::checkbutton $itk_component(generalF).sepCmdWinCB \
		-text "Separate Command Window" \
		-variable [::itcl::scope mSeparateCommandWindowPref]
	} {}
    }

    itk_component add generalF2 {
	::ttk::frame $itk_component(generalF).generalF2 \
	    -height 10
    } {}

    itk_component add bigEMenuItemCB {
	::ttk::checkbutton $itk_component(generalF).bigECB \
	    -text "Enable Evaluate Menu Item (Experimental)" \
	    -variable [::itcl::scope mEnableBigEPref]
    } {}

    set i 0
    grid $itk_component(bindingL) -column 0 -row $i -sticky e
    grid $itk_component(bindingF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(unitsL) -column 0 -row $i -sticky e
    grid $itk_component(unitsF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(generalF2) -column 0 -row $i -columnspan 2 -sticky nsew
    incr i
    grid $itk_component(backgroundColorL) -column 0 -row $i -sticky ne
    grid $itk_component(backgroundColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(fbbackgroundColorL) -column 0 -row $i -sticky ne
    grid $itk_component(fbbackgroundColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(fontsizeL) -column 0 -row $i -sticky e
    grid $itk_component(fontsizeF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(measuringStickColorL) -column 0 -row $i -sticky e
    grid $itk_component(measuringStickColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(primitiveLabelColorL) -column 0 -row $i -sticky e
    grid $itk_component(primitiveLabelColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(scaleColorL) -column 0 -row $i -sticky e
    grid $itk_component(scaleColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(viewingParamsColorL) -column 0 -row $i -sticky e
    grid $itk_component(viewingParamsColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(treeAttrsL) -column 0 -row $i -sticky e
    grid $itk_component(treeAttrsE) -column 1 -row $i -sticky ew
    incr i
    set i [buildOtherGeneralPreferences $i]
    grid $itk_component(affectedTreeNodesModeCB) \
	-columnspan 2 \
	-column 0 \
	-row $i \
	-sticky sw
    grid rowconfigure $itk_component(generalF) $i -weight 1
    incr i
    grid $itk_component(listViewCB) \
	-columnspan 2 \
	-column 0 \
	-row $i \
	-sticky sw
    incr i
    grid $itk_component(listViewAllAffectedCB) \
	-columnspan 2 \
	-column 0 \
	-row $i \
	-sticky sw
    if {$ArcherCore::inheritFromToplevel} {
	incr i
	grid $itk_component(sepCmdWinCB) \
	    -columnspan 2 \
	    -column 0 \
	    -row $i \
	    -sticky sw
    }
    incr i
    grid $itk_component(bigEMenuItemCB) \
	-columnspan 2 \
	-column 0 \
	-row $i \
	-sticky sw
    grid columnconfigure $itk_component(generalF) 1 -weight 1

    set i 0
    grid $itk_component(generalF) -column 0 -row $i -sticky nsew

    $itk_component(preferenceTabs) add $itk_component(generalF) -text "General"
}


::itcl::body Archer::buildGridPreferences {} {
    set parent $itk_component(preferenceTabs)
    itk_component add gridF {
	::ttk::frame $parent.gridF
    } {}

    itk_component add gridAnchorXL {
	::ttk::label $itk_component(gridF).anchorXL \
	    -anchor e \
	    -text "Anchor X:"
    } {}
    itk_component add gridAnchorXE {
	::ttk::entry $itk_component(gridF).anchorXE \
	    -width 12 \
	    -textvariable [::itcl::scope mGridAnchorXPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add gridAnchorXUnitsL {
	::ttk::label $itk_component(gridF).anchorXUnitsL \
	    -anchor e \
	    -textvariable [::itcl::scope mDbUnits]
    } {}

    itk_component add gridAnchorYL {
	::ttk::label $itk_component(gridF).anchorYL \
	    -anchor e \
	    -text "Anchor Y:"
    } {}
    itk_component add gridAnchorYE {
	::ttk::entry $itk_component(gridF).anchorYE \
	    -width 12 \
	    -textvariable [::itcl::scope mGridAnchorYPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add gridAnchorYUnitsL {
	::ttk::label $itk_component(gridF).anchorYUnitsL \
	    -anchor e \
	    -textvariable [::itcl::scope mDbUnits]
    } {}

    itk_component add gridAnchorZL {
	::ttk::label $itk_component(gridF).anchorZL \
	    -anchor e \
	    -text "Anchor Z:"
    } {}
    itk_component add gridAnchorZE {
	::ttk::entry $itk_component(gridF).anchorZE \
	    -width 12 \
	    -textvariable [::itcl::scope mGridAnchorZPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add gridAnchorZUnitsL {
	::ttk::label $itk_component(gridF).anchorZUnitsL \
	    -anchor e \
	    -textvariable [::itcl::scope mDbUnits]
    } {}

    buildComboBox $itk_component(gridF) \
	gridColor \
	color \
	mGridColorPref \
	"Color:" \
	$mColorListNoTriple

    itk_component add gridMrhL {
	::ttk::label $itk_component(gridF).mrhL \
	    -anchor e \
	    -text "Major Resolution (Horizontal):"
    } {}
    itk_component add gridMrhE {
	::ttk::entry $itk_component(gridF).mrhE \
	    -width 12 \
	    -textvariable [::itcl::scope mGridMrhPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}

    itk_component add gridMrvL {
	::ttk::label $itk_component(gridF).mrvL \
	    -anchor e \
	    -text "Major Resolution (Vertical):"
    } {}
    itk_component add gridMrvE {
	::ttk::entry $itk_component(gridF).mrvE \
	    -width 12 \
	    -textvariable [::itcl::scope mGridMrvPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}

    itk_component add gridRhL {
	::ttk::label $itk_component(gridF).rhL \
	    -anchor e \
	    -text "Minor Resolution (Horizontal):"
    } {}
    itk_component add gridRhE {
	::ttk::entry $itk_component(gridF).rhE \
	    -width 12 \
	    -textvariable [::itcl::scope mGridRhPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add gridRhUnitsL {
	::ttk::label $itk_component(gridF).rhUnitsL \
	    -anchor e \
	    -textvariable [::itcl::scope mDbUnits]
    } {}

    itk_component add gridRvL {
	::ttk::label $itk_component(gridF).rvL \
	    -anchor e \
	    -text "Minor Resolution (Vertical):"
    } {}
    itk_component add gridRvE {
	::ttk::entry $itk_component(gridF).rvE \
	    -width 12 \
	    -textvariable [::itcl::scope mGridRvPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add gridRvUnitsL {
	::ttk::label $itk_component(gridF).rvUnitsL \
	    -anchor e \
	    -textvariable [::itcl::scope mDbUnits]
    } {}

    set i 0
    grid $itk_component(gridAnchorXL) -column 0 -row $i -sticky e
    grid $itk_component(gridAnchorXE) -column 1 -row $i -sticky ew
    grid $itk_component(gridAnchorXUnitsL) -column 2 -row $i -sticky ew
    incr i
    grid $itk_component(gridAnchorYL) -column 0 -row $i -sticky e
    grid $itk_component(gridAnchorYE) -column 1 -row $i -sticky ew
    grid $itk_component(gridAnchorYUnitsL) -column 2 -row $i -sticky ew
    incr i
    grid $itk_component(gridAnchorZL) -column 0 -row $i -sticky e
    grid $itk_component(gridAnchorZE) -column 1 -row $i -sticky ew
    grid $itk_component(gridAnchorZUnitsL) -column 2 -row $i -sticky ew
    incr i
    grid $itk_component(gridColorL) -column 0 -row $i -sticky e
    grid $itk_component(gridColorF) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(gridMrhL) -column 0 -row $i -sticky e
    grid $itk_component(gridMrhE) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(gridMrvL) -column 0 -row $i -sticky e
    grid $itk_component(gridMrvE) -column 1 -row $i -sticky ew
    incr i
    grid $itk_component(gridRhL) -column 0 -row $i -sticky e
    grid $itk_component(gridRhE) -column 1 -row $i -sticky ew
    grid $itk_component(gridRhUnitsL) -column 2 -row $i -sticky ew
    incr i
    grid $itk_component(gridRvL) -column 0 -row $i -sticky e
    grid $itk_component(gridRvE) -column 1 -row $i -sticky ew
    grid $itk_component(gridRvUnitsL) -column 2 -row $i -sticky ew

    grid columnconfigure $itk_component(gridF) 1 -weight 1

    set i 0
    grid $itk_component(gridF) -column 0 -row $i -sticky nw

    $itk_component(preferenceTabs) add $itk_component(gridF) -text "Grid"
}


::itcl::body Archer::buildGroundPlanePreferences {} {
    set parent $itk_component(preferenceTabs)
    itk_component add groundPlaneF {
	::ttk::frame $parent.groundPlaneF
    } {}

    itk_component add groundPlaneSizeL {
	::ttk::label $itk_component(groundPlaneF).sizeL \
	    -anchor e \
	    -text "Square Size:"
    } {}
    itk_component add groundPlaneSizeE {
	::ttk::entry $itk_component(groundPlaneF).sizeE \
	    -width 12 \
	    -textvariable [::itcl::scope mGroundPlaneSizePref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add groundPlaneSizeUnitsL {
	::ttk::label $itk_component(groundPlaneF).sizeUnitsL \
	    -anchor e \
	    -text "mm"
    } {}

    itk_component add groundPlaneIntervalL {
	::ttk::label $itk_component(groundPlaneF).intervalL \
	    -anchor e \
	    -text "Line Interval:"
    } {}
    itk_component add groundPlaneIntervalE {
	::ttk::entry $itk_component(groundPlaneF).intervalE \
	    -width 12 \
	    -textvariable [::itcl::scope mGroundPlaneIntervalPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add groundPlaneIntervalUnitsL {
	::ttk::label $itk_component(groundPlaneF).intervalUnitsL \
	    -anchor e \
	    -text "mm"
    } {}

    buildComboBox $itk_component(groundPlaneF) \
	groundPlaneMajorColor \
	majorColor \
	mGroundPlaneMajorColorPref \
	"Major Color:" \
	$mColorListNoTriple

    buildComboBox $itk_component(groundPlaneF) \
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

    grid columnconfigure $itk_component(groundPlaneF) 1 -weight 1

    set i 0
    grid $itk_component(groundPlaneF) -column 0 -row $i -sticky nw

    $itk_component(preferenceTabs) add $itk_component(groundPlaneF) -text "Ground Plane"
}


::itcl::body Archer::buildInfoDialogs {} {
    global tcl_platform

    buildAboutDialog
    buildarcherHelp

    # Build manual browser
    ManBrowser $itk_interior.archerMan -parentName Archer

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
	::ttk::label $parent.xl \
	    -anchor e \
	    -text "Position X:"
    } {}

    itk_component add modelAxesPositionXE {
	::ttk::entry $parent.xe \
	    -width 12 \
	    -textvariable [::itcl::scope mModelAxesPositionXPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add modelAxesPositionYL {
	::ttk::label $parent.yl \
	    -anchor e \
	    -text "Position Y:"
    } {}
    itk_component add modelAxesPositionYE {
	::ttk::entry $parent.ye \
	    -width 12 \
	    -textvariable [::itcl::scope mModelAxesPositionYPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
    } {}
    itk_component add modelAxesPositionZL {
	::ttk::label $parent.zl \
	    -anchor e \
	    -text "Position Z:"
    } {}
    itk_component add modelAxesPositionZE {
	::ttk::entry $parent.ze \
	    -width 12 \
	    -textvariable [::itcl::scope mModelAxesPositionZPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateDouble %P]
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
    set parent $itk_component(preferenceTabs)
    itk_component add modelAxesF {
	::ttk::frame $parent.modelAxesF
    } {}

    buildComboBox $itk_component(modelAxesF) \
	modelAxesSize \
	size \
	mModelAxesSizePref \
	"Size:" \
	{Small Medium Large X-Large \
	     "View (1x)" "View (2x)" "View (4x)" "View (8x)"}

    #    itk_component add modelAxesPositionL {
    #	::label $itk_component(modelAxesF).positionL \
	#	    -text "Position:"
    #    }
    #    itk_component add modelAxesPositionF {
    #	::frame $itk_component(modelAxesF).positionF
    #    } {}
    #    _build_model_axes_position $itk_component(modelAxesPositionF)
    buildModelAxesPosition $itk_component(modelAxesF)

    buildComboBox $itk_component(modelAxesF) \
	modelAxesLineWidth \
	lineWidth \
	mModelAxesLineWidthPref \
	"Line Width:" \
	{1 2 3}

    buildComboBox $itk_component(modelAxesF) \
	modelAxesColor \
	color \
	mModelAxesColorPref \
	"Axes Color:" \
	$mColorList

    buildComboBox $itk_component(modelAxesF) \
	modelAxesLabelColor \
	labelColor \
	mModelAxesLabelColorPref \
	"Label Color:" \
	$mColorListNoTriple

    itk_component add modelAxesTickIntervalL {
	::ttk::label $itk_component(modelAxesF).tickIntervalL \
	    -text "Tick Interval:"
    } {}

    itk_component add modelAxesTickIntervalE {
	::ttk::entry $itk_component(modelAxesF).tickIntervalE \
	    -textvariable [::itcl::scope mModelAxesTickIntervalPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateTickInterval %P]
    } {}

    buildComboBox $itk_component(modelAxesF) \
	modelAxesTicksPerMajor \
	ticksPerMajor \
	mModelAxesTicksPerMajorPref \
	"Ticks Per Major:" \
	{2 3 4 5 6 8 10 12}

    buildComboBox $itk_component(modelAxesF) \
	modelAxesTickThreshold \
	tickThreshold \
	mModelAxesTickThresholdPref \
	"Tick Threshold:" \
	{4 8 16 32 64}

    buildComboBox $itk_component(modelAxesF) \
	modelAxesTickLength \
	tickLength \
	mModelAxesTickLengthPref \
	"Tick Length:" \
	{2 4 8 16}

    buildComboBox $itk_component(modelAxesF) \
	modelAxesTickMajorLength \
	tickMajorLength \
	mModelAxesTickMajorLengthPref \
	"Major Tick Length:" \
	{2 4 8 16}

    buildComboBox $itk_component(modelAxesF) \
	modelAxesTickColor \
	tickColor \
	mModelAxesTickColorPref \
	"Tick Color:" \
	$mColorListNoTriple

    buildComboBox $itk_component(modelAxesF) \
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

    grid columnconfigure $itk_component(modelAxesF) 1 -weight 1

    set i 0
    grid $itk_component(modelAxesF) -column 0 -row $i -sticky nw

    $itk_component(preferenceTabs) add $itk_component(modelAxesF) -text "Model Axes"
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
	::ttk::label $parent.tranl1 \
	    -text "Translate: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesTranL2 {
	::ttk::label $parent.tranl2 \
	    -text "Shift-Right" \
	    -font $mFontText \
	    -anchor w
    } {}
    itk_component add mouseOverridesRotateL1 {
	::ttk::label $parent.rotateL1 \
	    -text "Rotate: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesRotateL2 {
	::ttk::label $parent.rotateL2 \
	    -text "Shift-Left" \
	    -font $mFontText \
	    -anchor w
    } {}
    itk_component add mouseOverridesScaleL1 {
	::ttk::label $parent.scaleL1 \
	    -text "Scale: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesScaleL2 {
	::ttk::label $parent.scaleL2 \
	    -text "Shift-Middle" \
	    -font $mFontText \
	    -anchor w
    } {}
    itk_component add mouseOverridesCenterL1 {
	::ttk::label $parent.centerL1 \
	    -text "Center: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesCenterL2 {
	::ttk::label $parent.centerL2 \
	    -text "Shift-Ctrl-Right" \
	    -font $mFontText \
	    -anchor w
    } {}
    itk_component add mouseOverridesPopupL1 {
	::ttk::label $parent.popupL1 \
	    -text "Popup Menu: " \
	    -font $mFontText \
	    -anchor e
    } {}
    itk_component add mouseOverridesPopupL2 {
	::ttk::label $parent.popupL2 \
	    -text "Right or Ctrl-Left" \
	    -font $mFontText \
	    -anchor w
    } {}

    $itk_component(mouseOverridesDialog) configure -background $LABEL_BACKGROUND_COLOR

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


::itcl::body Archer::buildOtherGeneralPreferences {_i} {
    return $_i
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
    set b2_cmd [$itk_component(preferencesDialog) buttoncget 2 -command]
    $itk_component(preferencesDialog) buttonconfigure 2 \
	-borderwidth 1 \
	-pady 0 \
	-command "[::itcl::code $this cancelPreferences]; $b2_cmd"

    grid [$itk_component(preferencesDialog) component bbox] -sticky e

    # ITCL can be nasty
    set win [$itk_component(preferencesDialog) component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$itk_component(preferencesDialog) childsite]
    itk_component add preferenceTabs {
	::ttk::notebook $parent.tabs
    } {}

    grid rowconfigure $itk_component(preferenceTabs) 0 -weight 1
    grid columnconfigure $itk_component(preferenceTabs) 0 -weight 1

    buildGeneralPreferences
    buildModelAxesPreferences
    buildViewAxesPreferences
    buildGroundPlanePreferences
    buildDisplayPreferences
    buildGridPreferences

    $itk_component(preferencesDialog) configure -background $LABEL_BACKGROUND_COLOR

    pack $itk_component(preferenceTabs) -expand yes -fill both

    wm geometry $itk_component(preferencesDialog) 450x500
}


::itcl::body Archer::buildRevertDialog {} {
    buildInfoDialog revertDialog \
	"Revert Database?" \
	"Do you wish to revert the database?" \
	450x85 none application

    $itk_component(revertDialog) show 2
    $itk_component(revertDialog) buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0 \
	-text Yes
    $itk_component(revertDialog) buttonconfigure 2 \
	-borderwidth 1 \
	-pady 0 \
	-text No
    $itk_component(revertDialogInfo) configure \
	-vscrollmode none \
	-hscrollmode none
}


::itcl::body Archer::buildToplevelMenubar {_parent {_prefix ""}} {
    itk_component add ${_prefix}menubar {
	::menu $_parent.${_prefix}menubar \
	    -tearoff 0
    } {
	keep -background
    }

    # File
    itk_component add ${_prefix}filemenu {
	::menu $itk_component(${_prefix}menubar).${_prefix}filemenu \
	    -tearoff 0
    } {
	keep -background
    }

    # Populate File menu
    $itk_component(${_prefix}filemenu) add command \
	-label "New..." \
	-command [::itcl::code $this newDb]
    $itk_component(${_prefix}filemenu) add command \
	-label "Open..." \
	-command [::itcl::code $this openDb]
    $itk_component(${_prefix}filemenu) add command \
	-label "Save" \
	-command [::itcl::code $this askToSave] \
	-state disabled
    $itk_component(${_prefix}filemenu) add command \
	-label "Revert" \
	-command [::itcl::code $this askToRevert] \
	-state disabled
    $itk_component(${_prefix}filemenu) add separator
    $itk_component(${_prefix}filemenu) add command \
	-label "Raytrace Control Panel..." \
	-command [::itcl::code $this raytracePanel] \
	-state disabled
    $itk_component(${_prefix}filemenu) add command \
	-label "Preferences..." \
	-command [::itcl::code $this doPreferences]
    $itk_component(${_prefix}filemenu) add separator
    $itk_component(${_prefix}filemenu) add command \
	-label "Quit" \
	-command [::itcl::code $this Close]

    itk_component add ${_prefix}displaymenu {
	::menu $itk_component(${_prefix}menubar).${_prefix}displaymenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(${_prefix}displaymenu) add command \
	-label "Reset" \
	-command [::itcl::code $this doViewReset] \
	-state disabled
    $itk_component(${_prefix}displaymenu) add command \
	-label "Autoview" \
	-command [::itcl::code $this doAutoview] \
	-state disabled
    $itk_component(${_prefix}displaymenu) add command \
	-label "Center..." \
	-command [::itcl::code $this doViewCenter] \
	-state disabled

    itk_component add ${_prefix}backgroundmenu {
	::menu $itk_component(${_prefix}displaymenu).${_prefix}backgroundmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(${_prefix}backgroundmenu) add command \
	-label "Black" \
	-command [::itcl::code $this backgroundColor Black]
    $itk_component(${_prefix}backgroundmenu) add command \
	-label "Grey" \
	-command [::itcl::code $this backgroundColor Grey]
    $itk_component(${_prefix}backgroundmenu) add command \
	-label "White" \
	-command [::itcl::code $this backgroundColor White]
    $itk_component(${_prefix}backgroundmenu) add command \
	-label "Cyan" \
	-command [::itcl::code $this backgroundColor Cyan]
    $itk_component(${_prefix}backgroundmenu) add command \
	-label "Blue" \
	-command [::itcl::code $this backgroundColor Blue]
    $itk_component(${_prefix}backgroundmenu) add command \
	-label "Navy" \
	-command [::itcl::code $this backgroundColor Navy]
    $itk_component(${_prefix}displaymenu) add cascade \
	-label "Background Color" \
	-menu $itk_component(${_prefix}backgroundmenu) \
	-state normal

    itk_component add ${_prefix}stdviewsmenu {
	::menu $itk_component(${_prefix}displaymenu).${_prefix}stdviewsmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(${_prefix}stdviewsmenu) add command \
	-label "Front" \
	-command [::itcl::code $this doAe 0 0]
    $itk_component(${_prefix}stdviewsmenu) add command \
	-label "Rear" \
	-command [::itcl::code $this doAe 180 0]
    $itk_component(${_prefix}stdviewsmenu) add command \
	-label "Port" \
	-command [::itcl::code $this doAe 90 0]
    $itk_component(${_prefix}stdviewsmenu) add command \
	-label "Starboard" \
	-command [::itcl::code $this doAe -90 0]
    $itk_component(${_prefix}stdviewsmenu) add command \
	-label "Top" \
	-command [::itcl::code $this doAe 0 90]
    $itk_component(${_prefix}stdviewsmenu) add command \
	-label "Bottom" \
	-command [::itcl::code $this doAe 0 -90]
    $itk_component(${_prefix}stdviewsmenu) add separator
    $itk_component(${_prefix}stdviewsmenu) add command \
	-label "35, 25" \
	-command [::itcl::code $this doAe 35 25]
    $itk_component(${_prefix}stdviewsmenu) add command \
	-label "45, 45" \
	-command [::itcl::code $this doAe 45 45]
    $itk_component(${_prefix}displaymenu) add cascade \
	-label "Standard Views" \
	-menu $itk_component(${_prefix}stdviewsmenu) \
	-state disabled
    $itk_component(${_prefix}displaymenu) add command \
	-label "Clear" \
	-command [::itcl::code $this zap] \
	-state disabled
    $itk_component(${_prefix}displaymenu) add command \
	-label "Refresh" \
	-command [::itcl::code $this refreshDisplay] \
	-state disabled

    buildModesMenu $_prefix
    updateUtilityMenu

    itk_component add ${_prefix}raytracemenu {
	::menu $itk_component(${_prefix}menubar).${_prefix}raytracemenu \
	    -tearoff 0
    } {
	keep -background
    }

    itk_component add ${_prefix}rtmenu {
	::menu $itk_component(${_prefix}raytracemenu).${_prefix}rtmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(${_prefix}rtmenu) add command \
	-label "512x512" \
	-command [::itcl::code $this launchRtApp rt 512]
    $itk_component(${_prefix}rtmenu) add command \
	-label "1024x1024" \
	-command [::itcl::code $this launchRtApp rt 1024]
    $itk_component(${_prefix}rtmenu) add command \
	-label "Window Size" \
	-command [::itcl::code $this launchRtApp rt window]

    itk_component add ${_prefix}rtcheckmenu {
	::menu $itk_component(${_prefix}raytracemenu).${_prefix}rtcheckmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(${_prefix}rtcheckmenu) add command \
	-label "50x50" \
	-command [::itcl::code $this launchRtApp rtcheck 50]
    $itk_component(${_prefix}rtcheckmenu) add command \
	-label "100x100" \
	-command [::itcl::code $this launchRtApp rtcheck 100]
    $itk_component(${_prefix}rtcheckmenu) add command \
	-label "256x256" \
	-command [::itcl::code $this launchRtApp rtcheck 256]
    $itk_component(${_prefix}rtcheckmenu) add command \
	-label "512x512" \
	-command [::itcl::code $this launchRtApp rtcheck 512]

    itk_component add ${_prefix}rtedgemenu {
	::menu $itk_component(${_prefix}raytracemenu).${_prefix}rtedgemenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(${_prefix}rtedgemenu) add command \
	-label "512x512" \
	-command [::itcl::code $this launchRtApp rtedge 512]
    $itk_component(${_prefix}rtedgemenu) add command \
	-label "1024x1024" \
	-command [::itcl::code $this launchRtApp rtedge 1024]
    $itk_component(${_prefix}rtedgemenu) add command \
	-label "Window Size" \
	-command [::itcl::code $this launchRtApp rtedge window]

    $itk_component(${_prefix}raytracemenu) add cascade \
	-label "rt" \
	-menu $itk_component(${_prefix}rtmenu) \
	-state disabled
    $itk_component(${_prefix}raytracemenu) add cascade \
	-label "rtcheck" \
	-menu $itk_component(${_prefix}rtcheckmenu) \
	-state disabled
    $itk_component(${_prefix}raytracemenu) add cascade \
	-label "rtedge" \
	-menu $itk_component(${_prefix}rtedgemenu) \
	-state disabled
    $itk_component(${_prefix}raytracemenu) add separator
    $itk_component(${_prefix}raytracemenu) add command \
	-label "nirt" \
	-command [::itcl::code $this launchNirt] \
	-state disabled

    itk_component add ${_prefix}helpmenu {
	::menu $itk_component(${_prefix}menubar).${_prefix}helpmenu \
	    -tearoff 0
    } {
	#	rename -font -menuFont menuFont MenuFont
	#	keep -font
	keep -background
    }
    $itk_component(${_prefix}helpmenu) add command \
	-label "Archer Man Pages..." \
	-command [::itcl::code $this man]
    $itk_component(${_prefix}helpmenu) add command \
	-label "Archer Help..." \
	-command [::itcl::code $this doarcherHelp]
    $itk_component(${_prefix}helpmenu) add separator
    $itk_component(${_prefix}helpmenu) add command \
	-label "About Plug-ins..." \
	-command "::Archer::pluginDialog [namespace tail $this]"
    $itk_component(${_prefix}helpmenu) add command \
	-label "About Archer..." \
	-command [::itcl::code $this doAboutArcher]
    #    $itk_component(${_prefix}helpmenu) add command \
	-label "Mouse Mode Overrides..." \
	-command [::itcl::code $this doMouseOverrides]

    # Hook in the first tier of menus
    $itk_component(${_prefix}menubar) add cascade \
	-label "File" \
	-menu $itk_component(${_prefix}filemenu)

    $itk_component(${_prefix}menubar) add cascade \
	-label "Display" \
	-menu $itk_component(${_prefix}displaymenu)

    $itk_component(${_prefix}menubar) add cascade \
	-label "Modes" \
	-menu $itk_component(${_prefix}modesmenu) \

    $itk_component(${_prefix}menubar) add cascade \
	-label "Raytrace" \
	-menu $itk_component(${_prefix}raytracemenu) \

    $itk_component(${_prefix}menubar) add cascade \
	-label "Help" \
	-menu $itk_component(${_prefix}helpmenu)

    # set up bindings for status
    bind $itk_component(${_prefix}filemenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(${_prefix}displaymenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(${_prefix}backgroundmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(${_prefix}stdviewsmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(${_prefix}modesmenu) <<MenuSelect>> [::itcl::code $this modesMenuStatusCB %W]
    bind $itk_component(${_prefix}activepanemenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(${_prefix}comppickmenu) <<MenuSelect>> [::itcl::code $this modesMenuStatusCB %W]
    bind $itk_component(${_prefix}helpmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]

    bind $itk_component(${_prefix}raytracemenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(${_prefix}rtmenu) <<MenuSelect>> [::itcl::code $this rtMenuStatusCB %W]
    bind $itk_component(${_prefix}rtcheckmenu) <<MenuSelect>> [::itcl::code $this rtCheckMenuStatusCB %W]
    bind $itk_component(${_prefix}rtedgemenu) <<MenuSelect>> [::itcl::code $this rtEdgeMenuStatusCB %W]
}


::itcl::body Archer::buildViewAxesPreferences {} {
    set parent $itk_component(preferenceTabs)
    itk_component add viewAxesF {
	::ttk::frame $parent.viewAxesF
    } {}

    buildComboBox $itk_component(viewAxesF) \
	viewAxesSize \
	size \
	mViewAxesSizePref \
	"Size:" \
	{Small Medium Large X-Large}

    buildComboBox $itk_component(viewAxesF) \
	viewAxesPosition \
	position \
	mViewAxesPositionPref \
	"Position:" \
	{Center "Upper Left" "Upper Right" "Lower Left" "Lower Right"}

    buildComboBox $itk_component(viewAxesF) \
	viewAxesLineWidth \
	lineWidth \
	mViewAxesLineWidthPref \
	"Line Width:" \
	{1 2 3}

    buildComboBox $itk_component(viewAxesF) \
	viewAxesColor \
	color \
	mViewAxesColorPref \
	"Axes Color:" \
	$mColorList

    buildComboBox $itk_component(viewAxesF) \
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

    grid columnconfigure $itk_component(viewAxesF) 1 -weight 1

    set i 0
    grid $itk_component(viewAxesF) -column 0 -row $i -sticky nw

    $itk_component(preferenceTabs) add $itk_component(viewAxesF) -text "View Axes"
}


::itcl::body Archer::doAboutArcher {} {
    #    bind $itk_component(aboutDialog) <Enter> "raise $itk_component(aboutDialog)"
    #    bind $itk_component(aboutDialog) <Configure> "raise $itk_component(aboutDialog)"
    #    bind $itk_component(aboutDialog) <FocusOut> "raise $itk_component(aboutDialog)"

    $itk_component(aboutDialog) center [namespace tail $this]
    ::update
    $itk_component(aboutDialog) activate
}

::itcl::body Archer::doarcherHelp {} {
    global tcl_platform

    $itk_component(archerHelp) center [namespace tail $this]
    ::update
    $itk_component(archerHelp) activate

}

::itcl::body Archer::handleConfigure {} {
    if {$mWindowGeometry != ""} {
	wm geometry [namespace tail $this] $mWindowGeometry
    }

    bind [namespace tail $this] <Configure> {}
}

::itcl::body Archer::launchDisplayMenuBegin {_dm _m _x _y} {
    set mCurrentPaneName $_dm
    tk_popup $_m $_x $_y
    after idle [::itcl::code $this launchDisplayMenuEnd]
}


::itcl::body Archer::launchDisplayMenuEnd {} {
    #    set mCurrentPaneName ""
}


::itcl::body Archer::fbActivePaneCallback {_pane} {
    ArcherCore::setActivePane $_pane
}


::itcl::body Archer::fbEnabledCallback {_on} {
    if {$_on} {
	$itk_component(primaryToolbar) itemconfigure toggle_fb \
	    -image $mImage_fbOff
    } else {
	$itk_component(primaryToolbar) itemconfigure toggle_fb \
	    -image $mImage_fbOn
    }
}


::itcl::body Archer::fbModeCallback {_mode} {
    switch -- $_mode {
	1 {
	    set img $mImage_fbUnderlay
	}
	2 {
	    set img $mImage_fbInterlay
	}
	3 {
	    set img $mImage_fbOverlay
	}
	default {
	    return
	}
    }

    $itk_component(primaryToolbar) itemconfigure toggle_fb_mode \
	-image $img
}


::itcl::body Archer::fbModeToggle {} {
    $itk_component(rtcntrl) toggleFbMode
}


::itcl::body Archer::fbToggle {} {
    $itk_component(rtcntrl) toggleFB
}


::itcl::body Archer::rtEndCallback {_aborted} {
    $itk_component(primaryToolbar) itemconfigure raytrace \
	-image $mImage_rt \
	-command [::itcl::code $this raytracePlus]
}


##
# At this point _bot is expected to be the name of an existing, unoriented
# volume mode bot. This method fires a ray at the bot and determines if it
# needs flipping and returns either an empty string that indicates no determination
# could be made or the cosine of the angle between the ray and the bot's normal.
#
::itcl::body Archer::getRayBotNormalCos {_start _target _bot} {
    set miss_flag 0
    set partitions [shootRay_doit $_start "at" $_target 1 1 1 1 $_bot]

    if {$partitions == ""} {
	return ""
    }

    set partition [lindex $partitions 0]
    if {[catch {bu_get_value_by_keyword in $partition} in] ||
	[catch {bu_get_value_by_keyword normal $in} hit_normal]} {
	return ""
    }

    set diff [vsub2 $_target $_start]
    set raydir [vunitize $diff]

    set hit_normal [vunitize $hit_normal]
    set cosa [vdot $raydir $hit_normal]

    if {[vnear_zero $cosa 0.00001]} {
	return ""
    }

    return $cosa
}


::itcl::body Archer::menuStatusCB {_w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$_w entrycget active -label} op]} {
	    set op ""
	}

	set validOp 1
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
	    "Revert" {
		set mStatusStr "Discard all edits waiting to be saved"
	    }
	    "Raytrace Control Panel..." {
		set mStatusStr "Launch the raytrace control panel"
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
	    "35, 25" {
		set mStatusStr "Set view to az=35, el=25"
	    }
	    "45, 45" {
		set mStatusStr "Set view to az=45, el=45"
	    }
	    "Primary" {
		set mStatusStr "Toggle on/off primary toolbar"
	    }
	    "View Controls" {
		set mStatusStr "Toggle on/off view toolbar"
	    }
	    "About Archer..." {
		set mStatusStr "Info about Archer"
	    }
	    "Archer Help..." {
		set mStatusStr "Help for Archer"
	    }
	    "About Plug-ins..." {
		set mStatusStr "Info about Archer's plugins"
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
	    "Black" {
		set mStatusStr "Set the display background color to black"
	    }
	    "Grey" {
		set mStatusStr "Set the display background color to grey"
	    }
	    "White" {
		set mStatusStr "Set the display background color to white"
	    }
	    "Cyan" {
		set mStatusStr "Set the display background color to cyan"
	    }
	    "Blue" {
		set mStatusStr "Set the display background color to blue"
	    }
	    "Clear" {
		set mStatusStr "Clear the display"
	    }
	    "Refresh" {
		set mStatusStr "Refresh the display"
	    }
	    "nirt" {
		set mStatusStr "Run nirt on the displayed geometry"
	    }
	    default {
		set validOp 0
		set mStatusStr ""
	    }
	}

	if {!$validOp} {
	    ArcherCore::menuStatusCB $_w
	}
    }
}


::itcl::body Archer::modesMenuStatusCB {_w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$_w entrycget active -label} op]} {
	    set op ""
	}

	switch -- $op {
	    "Active Pane" {
		set mStatusStr ""
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
	    "Ground Plane" {
		set mStatusStr "Hide/Show ground plane"
	    }
	    "Primitive Labels" {
		set mStatusStr "Hide/Show primitive labels"
	    }
	    "Viewing Parameters" {
		set mStatusStr "Hide/Show viewing parameters"
	    }
	    "Scale" {
		set mStatusStr "Hide/Show view scale"
	    }
	    "Lighting" {
		set mStatusStr "Toggle lighting on/off "
	    }
	    "Tree Select" {
		set mStatusStr "Select the picked object in the hierarchy tree."
	    }
	    "Get Object Name" {
		set mStatusStr "Get the name of the picked object."
	    }
	    "Erase Object" {
		set mStatusStr "Erase the picked object."
	    }
	    "Bot Flip" {
		set mStatusStr "Flip the picked object if it's a bot."
	    }
	    "Bot Split" {
		set mStatusStr "Split the picked object if it's a bot."
	    }
	    "Bot Sync" {
		set mStatusStr "Sync the picked object if it's a bot."
	    }
	    default {
		set mStatusStr ""
	    }
	}
    }
}


::itcl::body Archer::rtCheckMenuStatusCB {_w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$_w entrycget active -label} op]} {
	    set op ""
	}

	switch -- $op {
	    "50x50" {
		set mStatusStr "Run rtcheck with a size of 50 on the displayed geometry"
	    }
	    "100x100" {
		set mStatusStr "Run rtcheck with a size of 100 on the displayed geometry"
	    }
	    "256x256" {
		set mStatusStr "Run rtcheck with a size of 256 on the displayed geometry"
	    }
	    "512x512" {
		set mStatusStr "Run rtcheck with a size of 512 on the displayed geometry"
	    }
	    default {
		set mStatusStr ""
	    }
	}
    }
}


::itcl::body Archer::rtEdgeMenuStatusCB {_w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$_w entrycget active -label} op]} {
	    set op ""
	}

	switch -- $op {
	    "512x512" {
		set mStatusStr "Run rtedge with a size of 512 on the displayed geometry"
	    }
	    "1024x1024" {
		set mStatusStr "Run rtedge with a size of 1024 on the displayed geometry"
	    }
	    "Window Size" {
		set mStatusStr "Run rtedge with a size of \"window size\" on the displayed geometry"
	    }
	    default {
		set mStatusStr ""
	    }
	}
    }
}


::itcl::body Archer::rtMenuStatusCB {_w} {
    if {$mDoStatus} {
	# entry might not support -label (i.e. tearoffs)
	if {[catch {$_w entrycget active -label} op]} {
	    set op ""
	}

	switch -- $op {
	    "512x512" {
		set mStatusStr "Run rt with a size of 512 on the displayed geometry"
	    }
	    "1024x1024" {
		set mStatusStr "Run rt with a size of 1024 on the displayed geometry"
	    }
	    "Window Size" {
		set mStatusStr "Run rt with a size of \"window size\" on the displayed geometry"
	    }
	    default {
		set mStatusStr ""
	    }
	}
    }
}


::itcl::body Archer::updateCreationButtons {_on} {
    if {$_on} {
	$itk_component(primaryToolbar) itemconfigure other -state normal
	$itk_component(primaryToolbar) itemconfigure comb -state normal
    } else {
	$itk_component(primaryToolbar) itemconfigure other -state disabled
	$itk_component(primaryToolbar) itemconfigure comb -state disabled
    }
}


::itcl::body Archer::updateRaytraceButtons {_on} {
    if {$_on} {
	$itk_component(primaryToolbar) itemconfigure toggle_fb \
	    -state normal \
	    -command [::itcl::code $this fbToggle]
	$itk_component(primaryToolbar) itemconfigure toggle_fb_mode \
	    -state normal \
	    -command [::itcl::code $this fbModeToggle]
	$itk_component(primaryToolbar) itemconfigure raytrace \
	    -state normal \
	    -command [::itcl::code $this raytracePlus]
	$itk_component(primaryToolbar) itemconfigure clear_fb \
	    -state normal \
	    -command "$itk_component(rtcntrl) clear"

	$itk_component(rtcntrl) configure \
	    -fb_active_pane_callback [::itcl::code $this fbActivePaneCallback] \
	    -fb_enabled_callback [::itcl::code $this fbEnabledCallback] \
	    -fb_mode_callback [::itcl::code $this fbModeCallback]

	gedCmd rt_end_callback [::itcl::code $this rtEndCallback]
    } else {
	$itk_component(primaryToolbar) itemconfigure toggle_fb -state disabled
	$itk_component(primaryToolbar) itemconfigure raytrace -state disabled
	$itk_component(primaryToolbar) itemconfigure clear_fb -state disabled
    }
}


::itcl::body Archer::updatePrimaryToolbar {} {
    # Populate the primary toolbar
    $itk_component(primaryToolbar) insert 0 button new \
	-balloonstr "Create a new geometry file" \
	-helpstr "Create a new geometry file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this newDb]

    $itk_component(primaryToolbar) insert rotate button preferences \
	-balloonstr "Set application preferences" \
	-helpstr "Set application preferences" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this doPreferences]

    # half-size spacer
    $itk_component(primaryToolbar) insert rotate frame fill0 \
	-relief flat \
	-width 3
    $itk_component(primaryToolbar) insert rotate frame sep0 \
	-relief sunken \
	-width 2
    $itk_component(primaryToolbar) insert rotate frame fill1 \
	-relief flat \
	-width 3

    $itk_component(primaryToolbar) insert rotate button checkpoint \
	-balloonstr "Create checkpoint" \
	-helpstr "Create checkpoint" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this object_checkpoint]

    $itk_component(primaryToolbar) insert rotate button object_undo \
	-balloonstr "Object undo" \
	-helpstr "Object undo" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this object_undo]

    $itk_component(primaryToolbar) insert rotate button global_undo \
	-balloonstr "Global undo" \
	-helpstr "Global undo" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this global_undo]

    $itk_component(primaryToolbar) insert rotate button revert \
	-balloonstr "Revert database" \
	-helpstr "Revert database" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this askToRevert]


    if {$::Archer::plugins != ""} {
	$itk_component(primaryToolbar) insert rotate frame fill2 \
	    -relief flat \
	    -width 3
	$itk_component(primaryToolbar) insert rotate frame sep1 \
	    -relief sunken \
	    -width 2
	$itk_component(primaryToolbar) insert rotate frame fill3 \
	    -relief flat \
	    -width 3
    }

    buildWizardMenu
    
    $itk_component(primaryToolbar) insert rotate frame fill4 \
	-relief flat \
	-width 3
    $itk_component(primaryToolbar) insert rotate frame sep2 \
	-relief sunken \
	-width 2
    $itk_component(primaryToolbar) insert rotate frame fill5 \
	-relief flat \
	-width 3

    #    $itk_component(primaryToolbar) insert rotate button arb6 \
	-balloonstr "Create an arb6" \
	-helpstr "Create an arb6" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj arb6]

    #    $itk_component(primaryToolbar) insert rotate button arb8 \
	-balloonstr "Create an arb8" \
	-helpstr "Create an arb8" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj arb8]

    #    $itk_component(primaryToolbar) insert rotate button cone \
	-balloonstr "Create a tgc" \
	-helpstr "Create a tgc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj tgc]

    #    $itk_component(primaryToolbar) insert rotate button sphere \
	-balloonstr "Create a sphere" \
	-helpstr "Create a sphere" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj sph]

    #    $itk_component(primaryToolbar) insert rotate button torus \
	-balloonstr "Create a torus" \
	-helpstr "Create a torus" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj tor]

    #    $itk_component(primaryToolbar) insert rotate button pipe \
	-balloonstr "Create a pipe" \
	-helpstr "Create a pipe" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj pipe]

    $itk_component(primaryToolbar) insert rotate menubutton other \
	-balloonstr "Create other primitives" \
	-helpstr "Create other primitives" \
	-relief flat

    # half-size spacer
    $itk_component(primaryToolbar) insert rotate frame fill6 \
	-relief flat \
	-width 3
    $itk_component(primaryToolbar) insert rotate frame sep3 \
	-relief sunken \
	-width 2
    $itk_component(primaryToolbar) insert rotate frame fill7 \
	-relief flat \
	-width 3

    $itk_component(primaryToolbar) insert rotate button comb \
	-state disabled \
	-balloonstr "Create a combination" \
	-helpstr "Create a combination" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj comb]


    set parent [$itk_component(primaryToolbar) component other]
    itk_component add primitiveMenu {
	::menu $parent.menu \
	    -tearoff 0
    } {
	keep -background
    }
    itk_component add arbsMenu {
	::menu $itk_component(primitiveMenu).arbmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(arbsMenu) add command \
	-image $mImage_arb8Labeled \
	-command [::itcl::code $this createObj arb8]
    $itk_component(arbsMenu) add command \
	-image $mImage_arb7Labeled \
	-command [::itcl::code $this createObj arb7]
    $itk_component(arbsMenu) add command \
	-image $mImage_arb6Labeled \
	-label arb6 \
	-command [::itcl::code $this createObj arb6]
    $itk_component(arbsMenu) add command \
	-image $mImage_arb5Labeled \
	-label arb5 \
	-command [::itcl::code $this createObj arb5]
    $itk_component(arbsMenu) add command \
	-image $mImage_arb4Labeled \
	-command [::itcl::code $this createObj arb4]
    $itk_component(arbsMenu) add command \
	-label rpp \
	-command [::itcl::code $this createObj arb8]
    $itk_component(arbsMenu) add separator
    $itk_component(arbsMenu) add command \
	-image $mImage_arb5Labeled \
	-command [::itcl::code $this createObj arbn]

    itk_component add conesCylsMenu {
	::menu $itk_component(primitiveMenu).conescylsmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(conesCylsMenu) add command \
	-label rcc \
	-command [::itcl::code $this createObj rcc]
    $itk_component(conesCylsMenu) add command \
	-label rec \
	-command [::itcl::code $this createObj rec]
    $itk_component(conesCylsMenu) add command \
	-image $mImage_rhcLabeled \
	-command [::itcl::code $this createObj rhc]
    $itk_component(conesCylsMenu) add command \
	-image $mImage_rpcLabeled \
	-command [::itcl::code $this createObj rpc]
    $itk_component(conesCylsMenu) add command \
	-label tec \
	-command [::itcl::code $this createObj tec]
    $itk_component(conesCylsMenu) add command \
	-image $mImage_tgcLabeled \
	-command [::itcl::code $this createObj tgc]
    $itk_component(conesCylsMenu) add command \
	-label trc \
	-command [::itcl::code $this createObj trc]

    itk_component add ellsMenu {
	::menu $itk_component(primitiveMenu).ellsmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(ellsMenu) add command \
	-image $mImage_ellLabeled \
	-command [::itcl::code $this createObj ell]
    $itk_component(ellsMenu) add command \
	-label ell1 \
	-command [::itcl::code $this createObj ell1]
    $itk_component(ellsMenu) add command \
	-image $mImage_epaLabeled \
	-command [::itcl::code $this createObj epa]
    $itk_component(ellsMenu) add command \
	-image $mImage_sphLabeled \
	-command [::itcl::code $this createObj sph]

    itk_component add toriiMenu {
	::menu $itk_component(primitiveMenu).toriimenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(toriiMenu) add command \
	-image $mImage_etoLabeled \
	-command [::itcl::code $this createObj eto]
    $itk_component(toriiMenu) add command \
	-image $mImage_torLabeled \
	-command [::itcl::code $this createObj tor]

    $itk_component(primitiveMenu) add cascade \
	-label Arbs \
	-menu $itk_component(arbsMenu)
    $itk_component(primitiveMenu) add cascade \
	-label "Cones & Cylinders" \
	-menu $itk_component(conesCylsMenu)
    $itk_component(primitiveMenu) add cascade \
	-label Ellipsoids \
	-menu $itk_component(ellsMenu)
    $itk_component(primitiveMenu) add cascade \
	-label Torii \
	-menu $itk_component(toriiMenu)
    $itk_component(primitiveMenu) add separator

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
	-image $mImage_arsLabeled \
	-command [::itcl::code $this createObj ars]
    $itk_component(primitiveMenu) add command \
	-image $mImage_ehyLabeled \
	-command [::itcl::code $this createObj ehy]
    #    $itk_component(primitiveMenu) add command \
	-image $mImage_etoLabeled \
	-command [::itcl::code $this createObj eto]
    $itk_component(primitiveMenu) add command \
	-image $mImage_extrudeLabeled \
	-command [::itcl::code $this createObj extrude]
    $itk_component(primitiveMenu) add command \
	-image $mImage_halfLabeled \
	-command [::itcl::code $this createObj hyp]
    $itk_component(primitiveMenu) add command \
	-image $mImage_hypLabeled \
	-command [::itcl::code $this createObj hyp]
    $itk_component(primitiveMenu) add command \
	-image $mImage_metaballLabeled \
	-command [::itcl::code $this createObj metaball]
    $itk_component(primitiveMenu) add command \
	-label part \
	-command [::itcl::code $this createObj part]
    $itk_component(primitiveMenu) add command \
	-image $mImage_pipeLabeled \
	-command [::itcl::code $this createObj pipe]
    $itk_component(primitiveMenu) add command \
	-image $mImage_sketchLabeled \
	-command [::itcl::code $this createObj sketch]
    #    $itk_component(primitiveMenu) add command \
	-image $mImage_torLabeled \
	-command [::itcl::code $this createObj tor]

    set parent [$itk_component(primaryToolbar) component other]
    $parent configure \
	-menu $itk_component(primitiveMenu) \
	-activebackground [$parent cget -background]

    #    $itk_component(primaryToolbar) insert rotate button ehy \
	-balloonstr "Create an ehy" \
	-helpstr "Create an ehy" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj ehy]

    #    $itk_component(primaryToolbar) insert rotate button epa \
	-balloonstr "Create an epa" \
	-helpstr "Create an epa" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj epa]

    #    $itk_component(primaryToolbar) insert rotate button rpc \
	-balloonstr "Create an rpc" \
	-helpstr "Create an rpc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj rpc]

    #    $itk_component(primaryToolbar) insert rotate button rhc \
	-balloonstr "Create an rhc" \
	-helpstr "Create an rhc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj rhc]

    #    $itk_component(primaryToolbar) insert rotate button ell \
	-balloonstr "Create an ellipsoid" \
	-helpstr "Create an ellipsoid" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj ell]

    #    $itk_component(primaryToolbar) insert rotate button eto \
	-balloonstr "Create an eto" \
	-helpstr "Create an eto" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj eto]

    #    $itk_component(primaryToolbar) insert rotate button half \
	-balloonstr "Create a half space" \
	-helpstr "Create a half space" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj half]

    #    $itk_component(primaryToolbar) insert rotate button part \
	-balloonstr "Create a particle" \
	-helpstr "Create a particle" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj part]

    #    $itk_component(primaryToolbar) insert rotate button grip \
	-balloonstr "Create a grip" \
	-helpstr "Create a grip" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj grip]

    #    $itk_component(primaryToolbar) insert rotate button extrude \
	-balloonstr "Create an extrusion" \
	-helpstr "Create an extrusion" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj extrude]

    #    $itk_component(primaryToolbar) insert rotate button sketch \
	-balloonstr "Create a sketch" \
	-helpstr "Create a sketch" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj sketch]

    #    $itk_component(primaryToolbar) insert rotate button bot \
	-balloonstr "Create a bot" \
	-helpstr "Create a bot" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj bot]

    # half-size spacer
    $itk_component(primaryToolbar) insert rotate frame fill8 \
	-relief flat \
	-width 3
    $itk_component(primaryToolbar) insert rotate frame sep4 \
	-relief sunken \
	-width 2
    $itk_component(primaryToolbar) insert rotate frame fill9 \
	-relief flat \
	-width 3

    # add spacer
    $itk_component(primaryToolbar) add frame fill10 \
	-relief flat \
	-width 3
    $itk_component(primaryToolbar) add frame sep5 \
	-relief sunken \
	-width 2
    $itk_component(primaryToolbar) add frame fill11 \
	-relief flat \
	-width 3
    $itk_component(primaryToolbar) add radiobutton edit_rotate \
	-balloonstr "Rotate selected object" \
	-helpstr "Rotate selected object" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $OBJECT_ROTATE_MODE \
	-command [::itcl::code $this beginObjRotate] \
	-image [image create photo \
		    -file [file join $mImgDir edit_rotate.png]]
    $itk_component(primaryToolbar) add radiobutton edit_translate \
	-balloonstr "Translate selected object" \
	-helpstr "Translate selected object" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $OBJECT_TRANSLATE_MODE \
	-command [::itcl::code $this beginObjTranslate] \
	-image [image create photo \
		    -file [file join $mImgDir edit_translate.png]]
    $itk_component(primaryToolbar) add radiobutton edit_scale \
	-balloonstr "Scale selected object" \
	-helpstr "Scale selected object" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $OBJECT_SCALE_MODE \
	-command [::itcl::code $this beginObjScale] \
	-image [image create photo \
		    -file [file join $mImgDir edit_scale.png]]
    $itk_component(primaryToolbar) add radiobutton edit_center \
	-balloonstr "Center selected object" \
	-helpstr "Center selected object" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $OBJECT_CENTER_MODE \
	-command [::itcl::code $this beginObjCenter] \
	-image [image create photo \
		    -file [file join $mImgDir edit_select.png]]

    $itk_component(primaryToolbar) itemconfigure edit_rotate -state disabled
    $itk_component(primaryToolbar) itemconfigure edit_translate -state disabled
    $itk_component(primaryToolbar) itemconfigure edit_scale -state disabled
    $itk_component(primaryToolbar) itemconfigure edit_center -state disabled

    # add spacer
    $itk_component(primaryToolbar) add frame fill12 \
	-relief flat \
	-width 3
    $itk_component(primaryToolbar) add frame sep6 \
	-relief sunken \
	-width 2
    $itk_component(primaryToolbar) add frame fill13 \
	-relief flat \
	-width 3

    $itk_component(primaryToolbar) add button raytrace \
	-state disabled \
	-balloonstr "Raytrace current view" \
	-helpstr "Raytrace current view" \
	-relief flat \
	-overrelief raised
    $itk_component(primaryToolbar) add button toggle_fb_mode \
	-state disabled \
	-balloonstr "Change framebuffer mode" \
	-helpstr "Change framebuffer mode" \
	-relief flat \
	-overrelief raised
    $itk_component(primaryToolbar) add button clear_fb \
	-state disabled \
	-balloonstr "Clear framebuffer" \
	-helpstr "Clear framebuffer" \
	-relief flat \
	-overrelief raised
    $itk_component(primaryToolbar) add button toggle_fb \
	-state disabled \
	-balloonstr "Toggle framebuffer" \
	-helpstr "Toggle framebuffer" \
	-relief flat \
	-overrelief raised
}


::itcl::body Archer::buildEmbeddedMenubar {} {
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

    buildEmbeddedFileMenu
    buildEmbeddedDisplayMenu
    buildEmbeddedModesMenu
    buildEmbeddedRaytraceMenu
    updateUtilityMenu
    buildEmbeddedHelpMenu
}


::itcl::body Archer::buildEmbeddedFileMenu {} {
    $itk_component(menubar) add menubutton file \
	-text "File" -menu {
	    options -tearoff 0

	    command new \
		-label "New..." \
		-helpstr "Open target description"
	    command open \
		-label "Open..." \
		-helpstr "Open target description"
	    command save \
		-label "Save" \
		-helpstr "Save target description"
	    separator sep0
	    command rt -label "Raytrace Control Panel..." \
		-helpstr "Launch Ray Trace Panel"
	    command pref \
		-label "Preferences..." \
		-helpstr "Set application preferences"
	    separator sep1
	    command exit \
		-label "Quit" \
		-helpstr "Exit Archer"
	}
    $itk_component(menubar) menuconfigure .file.new \
	-command [::itcl::code $this newDb]
    $itk_component(menubar) menuconfigure .file.open \
	-command [::itcl::code $this openDb]
    $itk_component(menubar) menuconfigure .file.save \
	-command [::itcl::code $this askToSave] \
	-state disabled
    $itk_component(menubar) menuconfigure .file.revert \
	-command [::itcl::code $this askToRevert] \
	-state disabled
    $itk_component(menubar) menuconfigure .file.rt \
	-command [::itcl::code $this raytracePanel] \
	-state disabled
    $itk_component(menubar) menuconfigure .file.pref \
	-command [::itcl::code $this doPreferences]
    $itk_component(menubar) menuconfigure .file.exit \
	-command [::itcl::code $this Close]
}


::itcl::body Archer::buildEmbeddedDisplayMenu {} {
    $itk_component(menubar) add menubutton display \
	-text "Display" -menu {
	    options -tearoff 0

	    command reset -label "Reset" \
		-helpstr "Set view to default"
	    command autoview -label "Autoview" \
		-helpstr "Set view size and center according to what's being displayed"
	    command center -label "Center..." \
		-helpstr "Set the view center"

	    cascade background -label "Background Color" -menu {
		command black -label "Black" \
		    -helpstr "Set display background to Black"
		command grey -label "Grey" \
		    -helpstr "Set display background to Grey"
		command white -label "White" \
		    -helpstr "Set display background to White"
		command cyan -label "Cyan" \
		    -helpstr "Set display background to Cyan"
		command blue -label "Blue" \
		    -helpstr "Set display background to Blue"
		command navy -label "Navy" \
		    -helpstr "Set display background to Navy"
	    }

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
		command 35, 25 -label "35, 25" \
		    -helpstr "Set view to az=35, el=25"
		command 45, 45 -label "45, 45" \
		    -helpstr "Set view to az=45, el=45"
	    }

	    command clear -label "Clear" \
		-helpstr "Clear the display"
	    command refresh -label "Refresh" \
		-helpstr "Refresh the display"
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
    $itk_component(menubar) menuconfigure .display.background.black \
	-command [::itcl::code $this backgroundColor black]
    $itk_component(menubar) menuconfigure .display.background.grey \
	-command [::itcl::code $this backgroundColor grey]
    $itk_component(menubar) menuconfigure .display.background.white \
	-command [::itcl::code $this backgroundColor white]
    $itk_component(menubar) menuconfigure .display.background.cyan \
	-command [::itcl::code $this backgroundColor cyan]
    $itk_component(menubar) menuconfigure .display.background.blue \
	-command [::itcl::code $this backgroundColor blue]
    $itk_component(menubar) menuconfigure .display.background.navy \
	-command [::itcl::code $this backgroundColor navy]
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
    $itk_component(menubar) menuconfigure .display.standard.35, 25 \
	-command [::itcl::code $this doAe 35 25]
    $itk_component(menubar) menuconfigure .display.standard.45, 45 \
	-command [::itcl::code $this doAe 45 45]
    $itk_component(menubar) menuconfigure .display.clear \
	-command [::itcl::code $this zap] \
	-state disabled
    $itk_component(menubar) menuconfigure .display.refresh \
	-command [::itcl::code $this refreshDisplay] \
	-state disabled
}


::itcl::body Archer::buildEmbeddedHelpMenu {} {
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
	-command [::itcl::code $this doarcherHelp]
    $itk_component(menubar) menuconfigure .help.aboutPlugins \
	-command "::Archer::pluginDialog [namespace tail $this]"
    $itk_component(menubar) menuconfigure .help.aboutArcher \
	-command [::itcl::code $this doAboutArcher]
    #    $itk_component(menubar) menuconfigure .help.overrides \
	-command [::itcl::code $this doMouseOverrides]
}


::itcl::body Archer::buildEmbeddedModesMenu {} {
    $itk_component(menubar) add menubutton modes \
	-text "Modes" -menu {
	    options -tearoff 0

	    cascade activepane -label "Active Pane" -menu {
		radiobutton ul -label "Upper Left" \
		    -helpstr "Set active pane to upper left"
		radiobutton ur -label "Upper Right" \
		    -helpstr "Set active pane to upper right"
		radiobutton ll -label "Lower Left" \
		    -helpstr "Set active pane to lower left"
		radiobutton lr -label "Lower Right" \
		    -helpstr "Set active pane to lower right"
	    }
	    cascade comppick -label "Comp Pick Mode" -menu {
		radiobutton tselect -label "Tree Select" \
		    -helpstr "Select the picked object in the hierarchy tree."
		radiobutton gname -label "Get Object Name" \
		    -helpstr "Get the name of the picked object."
		radiobutton cerase -label "Erase Object" \
		    -helpstr "Erase the picked object."
		radiobutton bsplit -label "Bot Split" \
		    -helpstr "Split the picked object if it's a bot."
		radiobutton bsync -label "Bot Sync" \
		    -helpstr "Sync the picked object if it's a bot."
		radiobutton bflip -label "Bot Flip" \
		    -helpstr "Flip the picked object if it's a bot."
	    }
	    checkbutton quad -label "Quad View" \
		-helpstr "Toggle between single and quad display."
	    separator sep1
	    checkbutton vaxes -label "View Axes" \
		-helpstr "Toggle display of the view axes."
	    checkbutton maxes -label "Model Axes" \
		-helpstr "Toggle display of the model axes."
	    checkbutton gplane -label "Ground Plane" \
		-helpstr "Toggle display of the ground plane."
	    checkbutton plabels -label "Primitive Labels" \
		-helpstr "Toggle display of the primitive labels."
	    checkbutton vparams -label "Viewing Parameters" \
		-helpstr "Toggle display of the viewing parameters."
	    checkbutton scale -label "Scale" \
		-helpstr "Toggle display of the view scale."
	    checkbutton light -label "Lighting" \
		-helpstr "Toggle lighting on/off."
	    checkbutton grid -label "Grid" \
		-helpstr "Toggle display of the grid."
	    checkbutton sgrid -label "Snap Grid" \
		-helpstr "Toggle grid snapping."
	    checkbutton adc -label "Angle/Distance Cursor" \
		-helpstr "Toggle display of the angle distance cursor."
	}
    $itk_component(menubar) menuconfigure .modes.activepane \
	-state disabled
    set i 0
    $itk_component(menubar) menuconfigure .modes.activepane.ul \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane ul]
    incr i
    $itk_component(menubar) menuconfigure .modes.activepane.ur \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane ur]
    incr i
    $itk_component(menubar) menuconfigure .modes.activepane.ll \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane ll]
    incr i
    $itk_component(menubar) menuconfigure .modes.activepane.lr \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane lr]

    $itk_component(menubar) menuconfigure .modes.comppick.tselect \
	-command [::itcl::code $this initCompPick] \
	-value $COMP_PICK_TREE_SELECT_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(menubar) menuconfigure .modes.comppick.gname \
	-command [::itcl::code $this initCompPick] \
	-value $COMP_PICK_NAME_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(menubar) menuconfigure .modes.comppick.cerase \
	-command [::itcl::code $this initCompPick] \
	-value $COMP_PICK_ERASE_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(menubar) menuconfigure .modes.comppick.bsplit \
	-command [::itcl::code $this initCompPick] \
	-value $COMP_PICK_BOT_SPLIT_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(menubar) menuconfigure .modes.comppick.bsync \
	-command [::itcl::code $this initCompPick] \
	-value $COMP_PICK_BOT_SYNC_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(menubar) menuconfigure .modes.comppick.bflip \
	-command [::itcl::code $this initCompPick] \
	-value $COMP_PICK_BOT_FLIP_MODE \
	-variable [::itcl::scope mCompPickMode]

    $itk_component(menubar) menuconfigure .modes.quad \
	-command [::itcl::code $this doMultiPane] \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mMultiPane] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.vaxes \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowViewAxes] \
	-command [::itcl::code $this showViewAxes] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.maxes \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowModelAxes] \
	-command [::itcl::code $this showModelAxes] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.gplane \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowGroundPlane] \
	-command [::itcl::code $this showGroundPlane] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.plabels \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowPrimitiveLabels] \
	-command [::itcl::code $this showPrimitiveLabels] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.vparams \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowViewingParams] \
	-command [::itcl::code $this showViewParams] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.scale \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowScale] \
	-command [::itcl::code $this showScale] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.light \
	-offvalue 0 \
	-onvalue 2 \
	-variable [::itcl::scope mLighting] \
	-command [::itcl::code $this doLighting] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.grid \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowGrid] \
	-command [::itcl::code $this showGrid] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.sgrid \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mSnapGrid] \
	-command [::itcl::code $this snapGrid] \
	-state disabled
    $itk_component(menubar) menuconfigure .modes.adc \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowADC] \
	-command [::itcl::code $this showADC] \
	-state disabled
}


::itcl::body Archer::buildEmbeddedRaytraceMenu {} {
    $itk_component(menubar) add menubutton raytrace \
	-text "Raytrace" -menu {
	    options -tearoff 0

	    cascade rt -label "rt" -menu {
		command rt512 -label "512x512" \
		    -helpstr "Render the current view with rt -s 512."
		command rt1024 -label "1024x1024" \
		    -helpstr "Render the current view with rt -s 1024."
		command rtwinsize -label "Window Size" \
		    -helpstr "Render the current view with rt using the window size."
	    }

	    cascade rtcheck -label "rtcheck" -menu {
		command rtcheck50 -label "50x50" \
		    -helpstr "Run rtcheck -s 50."
		command rtcheck100 -label "100x100" \
		    -helpstr "Run rtcheck -s 100."
		command rtcheck256 -label "256x256" \
		    -helpstr "Run rtcheck -s 256."
		command rtcheck512 -label "512x512" \
		    -helpstr "Run rtcheck -s 512."
	    }

	    cascade rtedge -label "rtedge" -menu {
		command rtedge512 -label "512x512" \
		    -helpstr "Render the current view with rtedge -s 512."
		command rtedge1024 -label "1024x1024" \
		    -helpstr "Render the current view with rtedge -s 1024."
		command rtedgewinsize -label "Window Size" \
		    -helpstr "Render the current view with rtedge using the window size."
	    }

	    command nirt -label "nirt" \
		-helpstr "Fire nirt at current view."
	}

    $itk_component(menubar) menuconfigure .raytrace.rt \
	-state disabled
    $itk_component(menubar) menuconfigure .raytrace.rt.rt512 \
	-command [::itcl::code $this launchRtApp rt 512]
    $itk_component(menubar) menuconfigure .raytrace.rt.rt1024 \
	-command [::itcl::code $this launchRtApp rt 1024]
    $itk_component(menubar) menuconfigure .raytrace.rt.rtwinsize \
	-command [::itcl::code $this launchRtApp rt window]

    $itk_component(menubar) menuconfigure .raytrace.rtcheck \
	-state disabled
    $itk_component(menubar) menuconfigure .raytrace.rtcheck.rtcheck50 \
	-command [::itcl::code $this launchRtApp rtcheck 50]
    $itk_component(menubar) menuconfigure .raytrace.rtcheck.rtcheck100 \
	-command [::itcl::code $this launchRtApp rtcheck 100]
    $itk_component(menubar) menuconfigure .raytrace.rtcheck.rtcheck256 \
	-command [::itcl::code $this launchRtApp rtcheck 256]
    $itk_component(menubar) menuconfigure .raytrace.rtcheck.rtcheck512 \
	-command [::itcl::code $this launchRtApp rtcheck 512]

    $itk_component(menubar) menuconfigure .raytrace.rtedge \
	-state disabled
    $itk_component(menubar) menuconfigure .raytrace.rtedge.rtedge512 \
	-command [::itcl::code $this launchRtApp rtedge 512]
    $itk_component(menubar) menuconfigure .raytrace.rtedge.rtedge1024 \
	-command [::itcl::code $this launchRtApp rtedge 1024]
    $itk_component(menubar) menuconfigure .raytrace.rtedge.rtedgewinsize \
	-command [::itcl::code $this launchRtApp rtedge window]

    $itk_component(menubar) menuconfigure .raytrace.nirt \
	-command [::itcl::code $this launchNirt] \
	-state disabled
}


::itcl::body Archer::buildModesMenu {{_prefix ""}} {
    itk_component add ${_prefix}modesmenu {
	menu $itk_component(menubar).${_prefix}modesmenu \
	    -tearoff 0
    } {
	keep -background
    }

    itk_component add ${_prefix}activepanemenu {
	menu $itk_component(${_prefix}modesmenu).${_prefix}activepanemenu \
	    -tearoff 0
    } {
	keep -background
    }
    set i 0
    $itk_component(${_prefix}activepanemenu) add radiobutton \
	-label "Upper Left" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane ul]
    incr i
    $itk_component(${_prefix}activepanemenu) add radiobutton \
	-label "Upper Right" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane ur]
    set mActivePane $i
    incr i
    $itk_component(${_prefix}activepanemenu) add radiobutton \
	-label "Lower Left" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane ll]
    incr i
    $itk_component(${_prefix}activepanemenu) add radiobutton \
	-label "Lower Right" \
	-value $i \
	-variable [::itcl::scope mActivePane] \
	-command [::itcl::code $this setActivePane lr]

    itk_component add ${_prefix}comppickmenu {
	menu $itk_component(${_prefix}modesmenu).${_prefix}comppickmenu \
	    -tearoff 0
    } {
	keep -background
    }

    $itk_component(${_prefix}comppickmenu) add radiobutton \
	-command [::itcl::code $this initCompPick] \
	-label "Tree Select" \
	-value $COMP_PICK_TREE_SELECT_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(${_prefix}comppickmenu) add radiobutton \
	-command [::itcl::code $this initCompPick] \
	-label "Get Object Name" \
	-value $COMP_PICK_NAME_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(${_prefix}comppickmenu) add radiobutton \
	-command [::itcl::code $this initCompPick] \
	-label "Erase Object" \
	-value $COMP_PICK_ERASE_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(${_prefix}comppickmenu) add radiobutton \
	-command [::itcl::code $this initCompPick] \
	-label "Bot Split" \
	-value $COMP_PICK_BOT_SPLIT_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(${_prefix}comppickmenu) add radiobutton \
	-command [::itcl::code $this initCompPick] \
	-label "Bot Sync" \
	-value $COMP_PICK_BOT_SYNC_MODE \
	-variable [::itcl::scope mCompPickMode]
    $itk_component(${_prefix}comppickmenu) add radiobutton \
	-command [::itcl::code $this initCompPick] \
	-label "Bot Flip" \
	-value $COMP_PICK_BOT_FLIP_MODE \
	-variable [::itcl::scope mCompPickMode]

    $itk_component(${_prefix}modesmenu) add cascade \
	-label "Active Pane" \
	-menu $itk_component(${_prefix}activepanemenu) \
	-state disabled
    $itk_component(${_prefix}modesmenu) add cascade \
	-label "Comp Pick Mode" \
	-menu $itk_component(${_prefix}comppickmenu)
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Quad View" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mMultiPane] \
	-command [::itcl::code $this doMultiPane] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add separator
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "View Axes" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowViewAxes] \
	-command [::itcl::code $this showViewAxes] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Model Axes" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowModelAxes] \
	-command [::itcl::code $this showModelAxes] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Ground Plane" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowGroundPlane] \
	-command [::itcl::code $this showGroundPlane] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Primitive Labels" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowPrimitiveLabels] \
	-command [::itcl::code $this showPrimitiveLabels] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Viewing Parameters" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowViewingParams] \
	-command [::itcl::code $this showViewParams] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Scale" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowScale] \
	-command [::itcl::code $this showScale] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Lighting" \
	-offvalue 0 \
	-onvalue 2 \
	-variable [::itcl::scope mLighting] \
	-command [::itcl::code $this doLighting] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Grid" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowGrid] \
	-command [::itcl::code $this showGrid] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Snap Grid" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mSnapGrid] \
	-command [::itcl::code $this snapGrid] \
	-state disabled
    $itk_component(${_prefix}modesmenu) add checkbutton \
	-label "Angle/Distance Cursor" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowADC] \
	-command [::itcl::code $this showADC] \
	-state disabled
}


::itcl::body Archer::activateMenusEtc {} {
    if {!$mViewOnly} {
	updateRaytraceButtons 1

	if {$ArcherCore::inheritFromToplevel} {
	    if {$mSeparateCommandWindow} {
		set plist [list {} $mSepCmdPrefix]
	    } else {
		set plist {{}}
	    }

	    foreach prefix $plist {
		$itk_component(${prefix}filemenu) entryconfigure "Raytrace Control Panel..." -state normal

		$itk_component(${prefix}displaymenu) entryconfigure "Standard Views" -state normal
		$itk_component(${prefix}displaymenu) entryconfigure "Reset" -state normal
		$itk_component(${prefix}displaymenu) entryconfigure "Autoview" -state normal
		$itk_component(${prefix}displaymenu) entryconfigure "Center..." -state normal
		$itk_component(${prefix}displaymenu) entryconfigure "Clear" -state normal
		$itk_component(${prefix}displaymenu) entryconfigure "Refresh" -state normal

		$itk_component(${prefix}modesmenu) entryconfigure "Active Pane" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Quad View" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "View Axes" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Model Axes" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Ground Plane" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Primitive Labels" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Viewing Parameters" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Scale" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Lighting" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Grid" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Snap Grid" -state normal
		$itk_component(${prefix}modesmenu) entryconfigure "Angle/Distance Cursor" -state normal

		$itk_component(${prefix}raytracemenu) entryconfigure "rt" -state normal
		$itk_component(${prefix}raytracemenu) entryconfigure "rtcheck" -state normal
		$itk_component(${prefix}raytracemenu) entryconfigure "rtedge" -state normal
		$itk_component(${prefix}raytracemenu) entryconfigure "nirt" -state normal
	    }
	} else {
	    $itk_component(menubar) menuconfigure .file.rt -state normal

	    $itk_component(menubar) menuconfigure .display.standard -state normal
	    $itk_component(menubar) menuconfigure .display.reset -state normal
	    $itk_component(menubar) menuconfigure .display.autoview -state normal
	    $itk_component(menubar) menuconfigure .display.center -state normal
	    $itk_component(menubar) menuconfigure .display.clear -state normal
	    $itk_component(menubar) menuconfigure .display.refresh -state normal

	    $itk_component(menubar) menuconfigure .modes.activepane -state normal
	    $itk_component(menubar) menuconfigure .modes.quad -state normal
	    $itk_component(menubar) menuconfigure .modes.vaxes -state normal
	    $itk_component(menubar) menuconfigure .modes.maxes -state normal
	    $itk_component(menubar) menuconfigure .modes.gplane -state normal
	    $itk_component(menubar) menuconfigure .modes.plabels -state normal
	    $itk_component(menubar) menuconfigure .modes.vparams -state normal
	    $itk_component(menubar) menuconfigure .modes.cdot -state normal
	    $itk_component(menubar) menuconfigure .modes.scale -state normal
	    $itk_component(menubar) menuconfigure .modes.light -state normal
	    $itk_component(menubar) menuconfigure .modes.grid -state normal
	    $itk_component(menubar) menuconfigure .modes.sgrid -state normal
	    $itk_component(menubar) menuconfigure .modes.adc -state normal

	    $itk_component(menubar) menuconfigure .raytrace.rt -state normal
	    $itk_component(menubar) menuconfigure .raytrace.rtcheck -state normal
	    $itk_component(menubar) menuconfigure .raytrace.rtedge -state normal
	    $itk_component(menubar) menuconfigure .raytrace.nirt -state normal
	}
    }
}


################################### Modes Section ###################################

::itcl::body Archer::initMode {{_updateFractions 0}} {
    if {$_updateFractions} {
	updateHPaneFractions
	updateVPaneFractions
    }

    if {$ArcherCore::inheritFromToplevel == 0} {
	pack forget $itk_component(menubar)
	::itcl::delete object $itk_component(menubar)
	buildEmbeddedMenubar
	pack $itk_component(menubar) -side top -fill x -padx 1 -before $itk_component(south)
    } else {
	updateUtilityMenu
    }

    updateWizardMenu

    if {$mTarget != "" &&
	$mBindingMode == "Default"} {
	$itk_component(primaryToolbar) itemconfigure edit_rotate -state normal
	$itk_component(primaryToolbar) itemconfigure edit_translate -state normal
	$itk_component(primaryToolbar) itemconfigure edit_scale -state normal
	$itk_component(primaryToolbar) itemconfigure edit_center -state normal
    }

    $itk_component(hpane) show bottomView
    $itk_component(hpane) fraction $mHPaneFraction1 $mHPaneFraction2
    $itk_component(vpane) show attrView

    # How screwed up is this?
    $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
    update
    after idle $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
}


################################### Object Edit Section ###################################


::itcl::body Archer::initEdit {{_initEditMode 1}} {
    if {[catch {gedCmd get_type $mSelectedObj} mSelectedObjType]} {
	if {![info exists itk_component(invalidView)]} {
	    buildInvalidObjEditView
	}

	initInvalidObjEditView $mSelectedObj
	return
    }

    if {$mSelectedObjType != "bot"} {
	set odata [lrange [gedCmd get $mSelectedObj] 1 end]
    } else {
	set odata ""
    }

    if {$_initEditMode && [info exists GeometryEditFrame::mEditCommand]} {
	set GeometryEditFrame::mEditMode 0
	set GeometryEditFrame::mEditClass $GeometryEditFrame::EDIT_CLASS_NONE
	set GeometryEditFrame::mEditCommand ""
	set GeometryEditFrame::mEditParam1 0
	set GeometryEditFrame::mEditParam2 0
	set GeometryEditFrame::mEditPCommand ""
    }

    switch -- $mSelectedObjType {
	"arb4" {
	    if {![info exists itk_component(arb4View)]} {
		buildArb4EditView
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
	"hyp" {
	    if {![info exists itk_component(hypView)]} {
		buildHypEditView
	    }
	    initHypEditView $odata
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
	"superell" {
	    if {![info exists itk_component(superellView)]} {
		buildSuperellEditView
	    }
	    initSuperellEditView $odata
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


################################### Object Edit via Mouse Section ###################################

::itcl::body Archer::beginObjRotate {} {
    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $VIEW_ROTATE_MODE
	beginViewRotate
	return
    }

    if {$GeometryEditFrame::mEditClass != $GeometryEditFrame::EDIT_CLASS_ROT} {
	initEdit
    }

    $itk_component(ged) init_button_no_op 2

    foreach dname {ul ur ll lr} {
	set win [$itk_component(ged) component $dname]

	if {$GeometryEditFrame::mEditCommand != ""} {
	    if {$GeometryEditFrame::mEditParam2 != 0} {
		bind $win <1> "$itk_component(ged) pane_$GeometryEditFrame::mEditCommand\_mode $dname $obj $GeometryEditFrame::mEditParam1 $GeometryEditFrame::mEditParam2 %x %y; break"
	    } else {
		bind $win <1> "$itk_component(ged) pane_$GeometryEditFrame::mEditCommand\_mode $dname $obj $GeometryEditFrame::mEditParam1 %x %y; break"
	    }
	} else {
	    bind $win <1> "$itk_component(ged) pane_orotate_mode $dname $obj %x %y; break"
	}

	bind $win <ButtonRelease-1> "[::itcl::code $this endObjRotate $dname $obj]; break"
    }

    $itk_component(ged) rect lwidth 0
}


::itcl::body Archer::beginObjScale {} {
    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $VIEW_ROTATE_MODE
	beginViewRotate
	return
    }

    if {$GeometryEditFrame::mEditClass != $GeometryEditFrame::EDIT_CLASS_SCALE} {
	initEdit
    }

    $itk_component(ged) init_button_no_op 2

    foreach dname {ul ur ll lr} {
	set win [$itk_component(ged) component $dname]

	if {$GeometryEditFrame::mEditCommand != ""} {
	    bind $win <1> "$itk_component(ged) pane_$GeometryEditFrame::mEditCommand\_mode $dname $obj $GeometryEditFrame::mEditParam1 %x %y; break"
	} else {
	    bind $win <1> "$itk_component(ged) pane_oscale_mode $dname $obj %x %y; break"
	}

	bind $win <ButtonRelease-1> "[::itcl::code $this endObjScale $dname $obj]; break"
    }

    $itk_component(ged) rect lwidth 0
}


::itcl::body Archer::beginObjTranslate {} {
    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $VIEW_ROTATE_MODE
	beginViewRotate
	return
    }

    if {$GeometryEditFrame::mEditClass != $GeometryEditFrame::EDIT_CLASS_TRANS} {
	initEdit
    }

    $itk_component(ged) init_button_no_op 2
    set ::GeometryEditFrame::mEditLastTransMode $OBJECT_TRANSLATE_MODE

    foreach dname {ul ur ll lr} {
	set win [$itk_component(ged) component $dname]

	if {$GeometryEditFrame::mEditCommand != ""} {
	    bind $win <1> "$itk_component(ged) pane_$GeometryEditFrame::mEditCommand\_mode $dname $obj $GeometryEditFrame::mEditParam1 %x %y; break"
	} else {
	    bind $win <1> "$itk_component(ged) pane_otranslate_mode $dname $obj %x %y; break"
	}

	bind $win <ButtonRelease-1> "[::itcl::code $this endObjTranslate $dname $obj %x %y]; break"
    }

    $itk_component(ged) rect lwidth 0
}


::itcl::body Archer::beginObjCenter {} {
    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $VIEW_ROTATE_MODE
	beginViewRotate
	return
    }

    if {$GeometryEditFrame::mEditClass != $GeometryEditFrame::EDIT_CLASS_TRANS} {
	initEdit
    }

    $itk_component(ged) init_button_no_op 2
    set ::GeometryEditFrame::mEditLastTransMode $OBJECT_CENTER_MODE

    foreach dname {ul ur ll lr} {
	set win [$itk_component(ged) component $dname]
	bind $win <1> "[::itcl::code $this handleObjCenter $dname $obj %x %y]; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjCenter $obj]; break"
    }

    $itk_component(ged) rect lwidth 0
}


::itcl::body Archer::endObjCenter {_obj} {
    set mNeedSave 1
    updateSaveMode
    initEdit 0

    set center [$itk_component(ged) ocenter $_obj]
    addHistory "ocenter $_obj $center"
}


::itcl::body Archer::endObjRotate {dname obj} {
    $itk_component(ged) pane_idle_mode $dname
    set mNeedSave 1
    updateSaveMode
    initEdit 0

    #XXX Need code to track overall transformation
    if {[info exists itk_component(ged)]} {
	#addHistory "orotate obj rx ry rz"
    }
}


::itcl::body Archer::endObjScale {dname obj} {
    $itk_component(ged) pane_idle_mode $dname
    set mNeedSave 1
    updateSaveMode
    initEdit 0

    #XXX Need code to track overall transformation
    if {[info exists itk_component(ged)]} {
	#addHistory "oscale obj sf"
    }
}


::itcl::body Archer::endObjTranslate {_dm _obj _mx _my} {
    $itk_component(ged) pane_idle_mode $_dm
    handleObjCenter $_dm $_obj $_mx $_my 
    endObjCenter $_obj
}


::itcl::body Archer::handleObjCenter {_dm _obj _mx _my} {
    set ocenter [gedCmd ocenter $_obj]
    set ocenter [vscale $ocenter [gedCmd local2base]]
    set ovcenter [eval gedCmd pane_m2v_point $_dm $ocenter]

    # This is the updated view center (i.e. we keep the original view Z)
    set vcenter [gedCmd pane_screen2view $_dm $_mx $_my]

    set vx [lindex $vcenter 0]
    set vy [lindex $vcenter 1]

    set vl [gedCmd pane_snap_view $_dm $vx $vy]
    set vx [lindex $vl 0]
    set vy [lindex $vl 1]
    set vcenter [list $vx $vy [lindex $ovcenter 2]]

    set ocenter [vscale [eval gedCmd pane_v2m_point $_dm $vcenter] [gedCmd base2local]]

    set ret [catch {
	if {$GeometryEditFrame::mEditCommand != ""} {
	    gedCmd $GeometryEditFrame::mEditCommand $_obj $GeometryEditFrame::mEditParam1 $ocenter
	} else {
	    eval gedCmd ocenter $_obj $ocenter
	}
    } msg]

    redrawObj $_obj 0
    initEdit 0

    if {$ret} {
	putString $msg
    }
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
    $parent configure \
	-borderwidth 1 \
	-relief sunken

    itk_component add dbAttrView {
	::iwidgets::scrolledframe $parent.dbattrview \
	    -vscrollmode dynamic \
	    -hscrollmode dynamic \
	    -background $LABEL_BACKGROUND_COLOR
    } {}

    set parent [$itk_component(dbAttrView) childsite]

    itk_component add dbnamekey {
	::ttk::label $parent.namekey \
	    -font $mFontTextBold \
	    -text "Database:" -anchor e
    } {}

    itk_component add dbnameval {
	::ttk::label $parent.nameval \
	    -font $mFontText \
	    -textvariable [::itcl::scope mDbName] \
	    -anchor w \
	    -relief flat
	#	    -fg #7e7e7e
    } {}

    itk_component add dbtitlekey {
	::ttk::label $parent.titlekey \
	    -font $mFontTextBold \
	    -text "Title:" -anchor e
    } {}

    itk_component add dbtitleval {
	::ttk::label $parent.titleval \
	    -font $mFontText \
	    -textvariable [::itcl::scope mDbTitle] \
	    -anchor w \
	    -relief flat
	#	    -fg #7e7e7e
    } {}

    itk_component add dbunitskey {
	::ttk::label $parent.unitskey \
	    -font $mFontTextBold \
	    -text "Units:" -anchor e
    } {}

    itk_component add dbunitsval {
	::ttk::label $parent.unitsval \
	    -font $mFontText \
	    -textvariable [::itcl::scope mDbUnits] \
	    -anchor w \
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


::itcl::body Archer::buildHypEditView {} {
    set parent $itk_component(objEditView)
    itk_component add hypView {
	HypEditFrame $parent.hypview \
	    -units "mm"
    } {}
}


::itcl::body Archer::buildObjAttrView {} {
    set parent [$itk_component(vpane) childsite attrView]
    itk_component add objAttrView {
	::ttk::frame $parent.objattrview
    } {}

    itk_component add objAttrText {
	::iwidgets::scrolledtext $itk_component(objAttrView).objattrtext \
	    -vscrollmode dynamic \
	    -hscrollmode dynamic \
	    -wrap none \
	    -state disabled \
	    -borderwidth 1 \
	    -relief sunken \
	    -background $LABEL_BACKGROUND_COLOR
    } {}

    pack $itk_component(objAttrText) -expand yes -fill both
}


::itcl::body Archer::buildObjEditView {} {
    set parent [$itk_component(vpane) childsite attrView]
    itk_component add objEditView {
	::ttk::frame $parent.objeditview \
	    -borderwidth 1 \
	    -relief sunken
    } {}
}


::itcl::body Archer::buildObjToolView {} {
    set parent [$itk_component(vpane) childsite attrView]
    itk_component add objToolView {
	::ttk::frame $parent.objtoolview \
	    -borderwidth 1 \
	    -relief sunken
    } {}

    set parent $itk_component(objToolView)
    itk_component add compPickF {
	::ttk::labelframe $parent.compPickF \
	    -text "Comp Pick Modes" \
	    -labelanchor n
    } {}

    set parent $itk_component(compPickF)
    itk_component add compPickTreeSelectRB {
	::ttk::radiobutton $parent.compPickTreeSelectRB \
	    -command [::itcl::code $this initCompPick] \
	    -text "Tree Select" \
	    -value $COMP_PICK_TREE_SELECT_MODE \
	    -variable [::itcl::scope mCompPickMode]
    } {}

    itk_component add compPickNameRB {
	::ttk::radiobutton $parent.compPickNameRB \
	    -command [::itcl::code $this initCompPick] \
	    -text "Get Object Name" \
	    -value $COMP_PICK_NAME_MODE \
	    -variable [::itcl::scope mCompPickMode]
    } {}

    itk_component add compPickEraseRB {
	::ttk::radiobutton $parent.compPickEraseRB \
	    -command [::itcl::code $this initCompPick] \
	    -text "Erase Object" \
	    -value $COMP_PICK_ERASE_MODE \
	    -variable [::itcl::scope mCompPickMode]
    } {}

    itk_component add compPickBotSplitRB {
	::ttk::radiobutton $parent.compPickBotSplitRB \
	    -command [::itcl::code $this initCompPick] \
	    -text "Bot Split" \
	    -value $COMP_PICK_BOT_SPLIT_MODE \
	    -variable [::itcl::scope mCompPickMode]
    } {}

    itk_component add compPickBotSyncRB {
	::ttk::radiobutton $parent.compPickBotSyncRB \
	    -command [::itcl::code $this initCompPick] \
	    -text "Bot Sync" \
	    -value $COMP_PICK_BOT_SYNC_MODE \
	    -variable [::itcl::scope mCompPickMode]
    } {}

    itk_component add compPickBotFlipRB {
	::ttk::radiobutton $parent.compPickBotFlipRB \
	    -command [::itcl::code $this initCompPick] \
	    -text "Bot Flip" \
	    -value $COMP_PICK_BOT_FLIP_MODE \
	    -variable [::itcl::scope mCompPickMode]
    } {}

    grid $itk_component(compPickTreeSelectRB) -sticky w
    grid $itk_component(compPickNameRB) -sticky w
    grid $itk_component(compPickEraseRB) -sticky w
    grid $itk_component(compPickBotSplitRB) -sticky w
    grid $itk_component(compPickBotSyncRB) -sticky w
    grid $itk_component(compPickBotFlipRB) -sticky w


    set parent $itk_component(objToolView)
    itk_component add botToolsF {
	::ttk::labelframe $parent.botToolsF \
	    -text "Bot Tools" \
	    -labelanchor n
    } {}

    set parent $itk_component(botToolsF)
    itk_component add botSplitAllB {
	::ttk::button $parent.botSplitAllB \
	    -text "Split All Bots" \
	    -command [::itcl::code $this bot_split_all_wrapper]
    } {}

    itk_component add botSyncAllB {
	::ttk::button $parent.botSyncAllB \
	    -text "Sync All Bots" \
	    -command [::itcl::code $this bot_sync_all_wrapper]
    } {}

    itk_component add botFlipCheckAllB {
	::ttk::button $parent.botFlipCheckAllB \
	    -text "Flip Check All Bots" \
	    -command [::itcl::code $this bot_flip_check_all_wrapper]
    } {}

    itk_component add botFixAllB {
	::ttk::button $parent.botFixAllB \
	    -text "Fix All Bots" \
	    -command [::itcl::code $this bot_fix_all_wrapper]
    } {}

    grid $itk_component(botSplitAllB) -sticky ew
    grid $itk_component(botSyncAllB) -sticky ew
    grid $itk_component(botFlipCheckAllB) -sticky ew
    grid $itk_component(botFixAllB) -sticky ew
    grid columnconfigure $itk_component(botToolsF) 0 -weight 1


    set i 0
    grid $itk_component(compPickF) -row $i -pady 4 -sticky ew
    incr i
    grid $itk_component(botToolsF) -row $i -pady 4 -sticky ew
    incr i
    grid rowconfigure $itk_component(objToolView) $i -weight 1
    grid columnconfigure $itk_component(objToolView) 0 -weight 1
}


::itcl::body Archer::buildObjViewToolbar {} {
    set parent [$itk_component(vpane) childsite attrView]
    itk_component add objViewToolbar {
	::iwidgets::toolbar $parent.objViewToolbar \
	    -helpvariable [::itcl::scope mStatusStr] \
	    -balloonfont "{CG Times} 8" \
	    -balloonbackground \#ffffdd \
	    -borderwidth 1 \
	    -orient horizontal \
	    -background $LABEL_BACKGROUND_COLOR
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

    $itk_component(objViewToolbar) add radiobutton objToolView \
	-helpstr "Tool view mode" \
	-balloonstr "Tool view mode" \
	-variable [::itcl::scope mObjViewMode] \
	-value $OBJ_TOOL_VIEW_MODE \
	-command [::itcl::code $this initObjToolView]
}


::itcl::body Archer::buildPartEditView {} {
    set parent $itk_component(objEditView)
    itk_component add partView {
	PartEditFrame $parent.partview \
	    -units "mm"
    } {}
}


::itcl::body Archer::buildPipeEditView {} {
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

    #     set parent $itk_component(objEditView)
    #     itk_component add sketchView {
    # 	SketchEditFrame $parent.sketchview \
	# 	    -units "mm"
    #     } {}
}


::itcl::body Archer::buildSphereEditView {} {
    set parent $itk_component(objEditView)
    itk_component add sphView {
	SphereEditFrame $parent.sphview \
	    -units "mm"
    } {}
}


::itcl::body Archer::buildSuperellEditView {} {
    set parent $itk_component(objEditView)
    itk_component add superellView {
	SuperellEditFrame $parent.superellview \
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


::itcl::body Archer::buildInvalidObjEditView {} {
    set parent $itk_component(objEditView)
    itk_component add invalidView {
	::ttk::label $parent.invalidview \
	    -anchor center
    } {}
}


::itcl::body Archer::initArb4EditView {odata} {
    $itk_component(arb4View) configure \
	-geometryObject $mSelectedObj \
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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

    #     $itk_component(botView) configure \
	# 	-geometryObject $mSelectedObj \
	#       -geometryObjectPath $mSelectedObjPath \
	# 	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	# 	-mged $itk_component(ged) \
	# 	-labelFont $mFontText \
	# 	-boldLabelFont $mFontTextBold \
	# 	-entryFont $mFontText
    #     $itk_component(botView) initGeometry $odata

    #     pack $itk_component(botView) \
	# 	-expand yes \
	# 	-fill both
}


::itcl::body Archer::initCombEditView {odata} {
    $itk_component(combView) configure \
	-geometryObject $mSelectedObj \
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateCombEditView] \
	-mged $itk_component(ged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(combView) initGeometry $odata

    pack $itk_component(combView) \
	-expand yes \
	-fill both
}


::itcl::body Archer::initDbAttrView {name} {
    catch {pack forget $itk_component(dbAttrView)}
    catch {pack forget $itk_component(objViewToolbar)}
    catch {pack forget $itk_component(objAttrView)}
    catch {pack forget $itk_component(objEditView)}
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(halfView) initGeometry $odata

    pack $itk_component(halfView) \
	-expand yes \
	-fill both
}


::itcl::body Archer::initHypEditView {odata} {
    $itk_component(hypView) configure \
	-geometryObject $mSelectedObj \
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(hypView) initGeometry $odata

    pack $itk_component(hypView) \
	-expand yes \
	-fill both
}


::itcl::body Archer::initNoWizard {parent msg} {
    if {![info exists itk_component(noWizard)]} {
	itk_component add noWizard {
	    ::ttk::label $parent.noWizard
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
    if {$mSelectedObj == ""} {
	return
    }

    set mPrevObjViewMode $mObjViewMode

    catch {pack forget $itk_component(dbAttrView)}
    catch {pack forget $itk_component(objViewToolbar)}
    catch {pack forget $itk_component(objAttrView)}
    catch {pack forget $itk_component(objEditView)}
    catch {pack forget $itk_component(objToolView)}
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
    set odata [$itk_component(ged) l $mSelectedObj]
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


::itcl::body Archer::initObjEditView {} {
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

    set mPrevObjViewMode $mObjViewMode

    catch {pack forget $itk_component(dbAttrView)}
    catch {pack forget $itk_component(objViewToolbar)}
    catch {pack forget $itk_component(objAttrView)}
    catch {pack forget $itk_component(objEditView)}
    catch {pack forget $itk_component(objToolView)}
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

    if {[catch {$itk_component(ged) attr get $mSelectedObj WizardTop} mWizardTop]} {
	set mWizardTop ""
    } else {
	if {[catch {$itk_component(ged) attr get $mWizardTop WizardClass} mWizardClass]} {
	    set mWizardClass ""
	}
    }

    if {$mWizardClass == ""} {
	# free the current primitive view if any
	set _slaves [pack slaves $itk_component(objEditView)]
	catch {eval pack forget $_slaves}

	initEdit

	pack $itk_component(objViewToolbar) -expand no -fill x -anchor n
	pack $itk_component(objEditView) -expand yes -fill both -anchor n
    } else {
	if {[pluginQuery $mWizardClass] == -1} {
	    # the wizard plugin has not been loaded
	    initObjWizard $mSelectedObj 0
	} else {
	    initObjWizard $mSelectedObj 1
	}
    }
}


::itcl::body Archer::initObjToolView {} {
    if {$mSelectedObj == ""} {
	return
    }

    set mPrevObjViewMode $mObjViewMode

    catch {pack forget $itk_component(dbAttrView)}
    catch {pack forget $itk_component(objViewToolbar)}
    catch {pack forget $itk_component(objAttrView)}
    catch {pack forget $itk_component(objEditView)}
    catch {pack forget $itk_component(objToolView)}
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

    pack $itk_component(objViewToolbar) -expand no -fill both -anchor n
    pack $itk_component(objToolView) -expand yes -fill both -anchor n
}


## - initObjWizard
#
# Note - Before we get here, any previous wizard instances are destroyed
#        and mWizardClass has been initialized to the name of the wizard class.
#        Also, mWizardState is initialized to "".
#
::itcl::body Archer::initObjWizard {obj wizardLoaded} {
    set parent [$itk_component(vpane) childsite attrView]

    if {[catch {$itk_component(ged) attr get $mWizardTop WizardState} mWizardState]} {
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
	if {[catch {$itk_component(ged) attr get $mWizardTop WizardOrigin} wizOrigin]} {
	    set wizOrigin [gedCmd center]
	    set wizUnits [gedCmd units -s]
	} elseif {[catch {$itk_component(ged) attr get $mWizardTop WizardUnits} wizUnits]} {
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
		::ttk::button $parent.wizardUpdate \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(partView) initGeometry $odata

    pack $itk_component(partView) \
	-expand yes \
	-fill both
}


::itcl::body Archer::initPipeEditView {odata} {
    $itk_component(pipeView) configure \
	-geometryObject $mSelectedObj \
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(sphView) initGeometry $odata

    pack $itk_component(sphView) \
	-expand yes \
	-fill both
}


::itcl::body Archer::initSuperellEditView {odata} {
    $itk_component(superellView) configure \
	-geometryObject $mSelectedObj \
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(superellView) initGeometry $odata

    pack $itk_component(superellView) \
	-expand yes \
	-fill both
}


::itcl::body Archer::initTgcEditView {odata} {
    $itk_component(tgcView) configure \
	-geometryObject $mSelectedObj \
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
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
	-geometryObjectPath $mSelectedObjPath \
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
	-mged $itk_component(ged) \
	-labelFont $mFontText \
	-boldLabelFont $mFontTextBold \
	-entryFont $mFontText
    $itk_component(torView) initGeometry $odata

    pack $itk_component(torView) \
	-expand yes \
	-fill both
}


::itcl::body Archer::initInvalidObjEditView {_oname} {
    $itk_component(invalidView) configure \
	-text "$_oname does not exist"
    pack $itk_component(invalidView) \
	-expand yes \
	-fill both
}


::itcl::body Archer::updateObjEdit {updateObj needInit needSave} {
    set renderData [gedCmd how -b $mSelectedObjPath]
    set renderMode [lindex $renderData 0]
    set renderTrans [lindex $renderData 1]
    gedCmd configure -autoViewEnable 0
    gedCmd kill $mSelectedObj
    gedCmd cp $updateObj $mSelectedObj
    gedCmd unhide $mSelectedObj
    gedCmd attr rm $mSelectedObj previous
    gedCmd attr rm $mSelectedObj next

    if {$needInit} {
	initEdit
    }

    if {$needSave} {
	set mNeedSave 1
	updateSaveMode
    }

    render $mSelectedObjPath $renderMode $renderTrans 0
    gedCmd configure -autoViewEnable 1
}


::itcl::body Archer::updateObjEditView {} {
    set mNeedSave 1
    updateSaveMode
    redrawObj $mSelectedObjPath
}


::itcl::body Archer::updateCombEditView {} {
    updateObjEditView
    syncTree
    initEdit
}


################################### Plugins Section ###################################

::itcl::body Archer::buildUtilityMenu {} {
    if {$ArcherCore::inheritFromToplevel} {
	# Utility
	itk_component add utilityMenu {
	    ::menu $itk_component(menubar).utilityMenu \
		-tearoff 0
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
    $itk_component(primaryToolbar) insert rotate menubutton wizards \
	-balloonstr "Wizard Plugins" \
	-helpstr "Wizard Plugins" \
	-relief flat

    set parent [$itk_component(primaryToolbar) component wizards]
    itk_component add wizardMenu {
	::menu $parent.menu \
	    -tearoff 0
    } {
	keep -background
    }

    $parent configure \
	-menu $itk_component(wizardMenu) \
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

    $dialog configure -background $LABEL_BACKGROUND_COLOR

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
    #    bind $dialog <Enter> "raise $dialog"
    #    bind $dialog <Configure> "raise $dialog"
    #    bind $dialog <FocusOut> "raise $dialog"

    $dialog center $w
    ::update
    $dialog activate
}


::itcl::body Archer::invokeWizardDialog {class action wname} {
    gedCmd make_name -s 1
    set name [string tolower $class]
    regsub wizard $name "" name
    #XXX Temporary special case for TankWizardI
    #if {$class == "TankWizardI"} {
    #set name "simpleTank"
    #} else {
    #set name [gedCmd make_name $name]
    #}
    set name [gedCmd make_name $name]
    set oname $name
    set origin [gedCmd center]
    set units [gedCmd units -s]

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

    $dialog configure -background $LABEL_BACKGROUND_COLOR

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
    ::update
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
	if {$oname != $name && ![catch {gedCmd get_type $name} stuff]} {
	    ::sdialogs::Stddlgs::errordlg "User Error" \
		"$name already exists!"
	    return
	}
    }

    gedCmd erase $oname
    gedCmd killtree -a $oname
    gedCmd configure -autoViewEnable 0
    set obj [$wizard $action]
    gedCmd configure -autoViewEnable 1

    set mNeedSave 1
    updateSaveMode

    set xmlAction [$wizard getWizardXmlAction]
    if {$xmlAction != ""} {
	set xml [$wizard $xmlAction]
	foreach callback $wizardXmlCallbacks {
	    $callback $xml
	}
    }

    syncTree
}


::itcl::body Archer::pluginGetMinAllowableRid {} {
    set maxRid 0
    foreach {rid rname} [$itk_component(ged) rmap] {
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

    ::update idletasks
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
    ::update idletasks
}


::itcl::body Archer::updateUtilityMenu {} {
    foreach dialog [::itcl::find object -class ::iwidgets::Dialog] {
	if {[regexp {utility_dialog} $dialog]} {
	    catch {rename $dialog ""}
	}
    }

    # Look for appropriate utility plugins
    set uplugins {}
    foreach plugin $::Archer::plugins {
	set majorType [$plugin get -majorType]
	if {$majorType != $pluginMajorTypeUtility} {
	    continue
	}

	set minorType [$plugin get -minorType]
	if {[info exists itk_component(ged)] &&
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
	if {[info exists itk_component(ged)] &&
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
    if {$mZClipBackMaxPref != $mZClipBackMax ||
	$mZClipFrontMaxPref != $mZClipFrontMax ||
	$mZClipBackPref != $mZClipBack ||
	$mZClipFrontPref != $mZClipFront} {
	set mZClipBackMax $mZClipBackMaxPref
	set mZClipFrontMax $mZClipFrontMaxPref
	set mZClipBack $mZClipBackPref
	set mZClipFront $mZClipFrontPref
	updateDisplaySettings
    }
}


::itcl::body Archer::applyGeneralPreferences {} {
    switch -- $mBindingMode {
	"Default" {
	    initDefaultBindings
	}
	"BRL-CAD" {
	    initBrlcadBindings
	}
    }

    backgroundColor $mBackgroundColor
    $itk_component(rtcntrl) configure -color [cadwidgets::Ged::get_rgb_color $mFBBackgroundColor]
    gedCmd configure -measuringStickColor $mMeasuringStickColor
    gedCmd configure -measuringStickMode $mMeasuringStickMode
    gedCmd configure -primitiveLabelColor $mPrimitiveLabelColor
    gedCmd configure -scaleColor $mScaleColor
    gedCmd configure -viewingParamsColor $mViewingParamsColor

    $itk_component(ged) fontsize $mDisplayFontSize
}


::itcl::body Archer::applyGeneralPreferencesIfDiff {} {
    if {$mBindingModePref != $mBindingMode} {
	set mBindingMode $mBindingModePref
	switch -- $mBindingMode {
	    "Default" {
		initDefaultBindings
	    }
	    "BRL-CAD" {
		initBrlcadBindings
	    }
	}
    }

    if {$mMeasuringStickModePref != $mMeasuringStickMode} {
	set mMeasuringStickMode $mMeasuringStickModePref
    }

    if {$mBackgroundColor != $mBackgroundColorPref} {
	set mBackgroundColor $mBackgroundColorPref
	backgroundColor $mBackgroundColor

    }

    if {$mFBBackgroundColor != $mFBBackgroundColorPref} {
	set mFBBackgroundColor $mFBBackgroundColorPref
    }

    if {$mDisplayFontSize != $mDisplayFontSizePref} {
	set mDisplayFontSize $mDisplayFontSizePref
    }

    if {$mPrimitiveLabelColor != $mPrimitiveLabelColorPref} {
	set mPrimitiveLabelColor $mPrimitiveLabelColorPref
    }

    if {$mViewingParamsColor != $mViewingParamsColorPref} {
	set mViewingParamsColor $mViewingParamsColorPref
    }

    if {$mScaleColor != $mScaleColorPref} {
	set mScaleColor $mScaleColorPref
    }

    if {$mMeasuringStickColor != $mMeasuringStickColorPref} {
	set mMeasuringStickColor $mMeasuringStickColorPref
    }

    set lflag 0
    set cflag 0
    set tflag 0
    if {$mTreeAttrColumns != $mTreeAttrColumnsPref} {
	set mTreeAttrColumns $mTreeAttrColumnsPref
	set cflag 1
    }

    if {$mEnableListView != $mEnableListViewPref} {
	set mEnableListView $mEnableListViewPref
	set lflag 1
    }

    if {$mEnableListViewAllAffected != $mEnableListViewAllAffectedPref} {
	set mEnableListViewAllAffected $mEnableListViewAllAffectedPref
	set tflag 1
    }

    if {$mEnableAffectedNodeHighlight != $mEnableAffectedNodeHighlightPref} {
	set mEnableAffectedNodeHighlight $mEnableAffectedNodeHighlightPref
	set tflag 1
    }

    if {$lflag} {
	setTreeView 1

	if {$cflag && $mTreeAttrColumns == {}} {
	    set twidth [expr {[winfo width $itk_component(newtree)] - 4}]
	    set c0width [$itk_component(newtree) column \#0 -width]

	    if {$c0width < $twidth} {
		$itk_component(newtree) column \#0 -width $twidth
	    }
	}
    } elseif {$cflag} {
	rebuildTree

	if {$mTreeAttrColumns == {}} {
	    set twidth [expr {[winfo width $itk_component(newtree)] - 4}]
	    set c0width [$itk_component(newtree) column \#0 -width]

	    if {$c0width < $twidth} {
		$itk_component(newtree) column \#0 -width $twidth
	    }
	}
    } elseif {$tflag} {
	handleTreeSelect
    }

    if {$mSeparateCommandWindow != $mSeparateCommandWindowPref} {
	set mSeparateCommandWindow $mSeparateCommandWindowPref

	if {$mSeparateCommandWindow} {
	    rename $itk_component(advancedTabs) ""
	} else {
	    rename $itk_component(sepcmdT) ""

	    #This should have been killed by the previous statement
	    rename $itk_component(${mSepCmdPrefix}modesmenu) ""
	}

	$itk_component(hpane) delete bottomView
	buildCommandViewNew 0
	pack $itk_component(advancedTabs) -fill both -expand yes
	$itk_component(hpane) fraction $mHPaneFraction1 $mHPaneFraction2

	activateMenusEtc
	updateSaveMode

	$itk_component(ged) set_outputHandler "$itk_component(cmd) putstring"
    }

    if {$mEnableBigEPref != $mEnableBigE} {
	set mEnableBigE $mEnableBigEPref
    }

    set units [gedCmd units -s]
    if {$units != $mDbUnits} {
	units $mDbUnits
    }
}


::itcl::body Archer::applyGridPreferences {} {
    eval gedCmd grid anchor $mGridAnchor
    eval gedCmd grid color [getRgbColor $mGridColor]
    gedCmd grid mrh $mGridMrh
    gedCmd grid mrv $mGridMrv
    gedCmd grid rh $mGridRh
    gedCmd grid rv $mGridRv
}


::itcl::body Archer::applyGridPreferencesIfDiff {} {
    set X [lindex $mGridAnchor 0]
    set Y [lindex $mGridAnchor 1]
    set Z [lindex $mGridAnchor 2]
    if {$mGridAnchorXPref != $X ||
	$mGridAnchorYPref != $Y ||
	$mGridAnchorZPref != $Z} {
	set mGridAnchor "$mGridAnchorXPref $mGridAnchorYPref $mGridAnchorZPref"
	eval gedCmd grid anchor $mGridAnchor
    }

    if {$mGridColor != $mGridColorPref} {
	set mGridColor $mGridColorPref
	eval gedCmd grid color [getRgbColor $mGridColor]
    }

    if {$mGridMrh != $mGridMrhPref} {
	set mGridMrh $mGridMrhPref
	gedCmd grid mrh $mGridMrh
    }

    if {$mGridMrv != $mGridMrvPref} {
	set mGridMrv $mGridMrvPref
	gedCmd grid mrv $mGridMrv
    }

    if {$mGridRh != $mGridRhPref} {
	set mGridRh $mGridRhPref
	gedCmd grid rh $mGridRh
    }

    if {$mGridRv != $mGridRvPref} {
	set mGridRv $mGridRvPref
	gedCmd grid rv $mGridRv
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
	    gedCmd configure -modelAxesSize 0.2
	}
	"Medium" {
	    gedCmd configure -modelAxesSize 0.4
	}
	"Large" {
	    gedCmd configure -modelAxesSize 0.8
	}
	"X-Large" {
	    gedCmd configure -modelAxesSize 1.6
	}
	"View (1x)" {
	    gedCmd configure -modelAxesSize 2.0
	}
	"View (2x)" {
	    gedCmd configure -modelAxesSize 4.0
	}
	"View (4x)" {
	    gedCmd configure -modelAxesSize 8.0
	}
	"View (8x)" {
	    gedCmd configure -modelAxesSize 16.0
	}
    }

    gedCmd configure -modelAxesPosition $mModelAxesPosition
    gedCmd configure -modelAxesLineWidth $mModelAxesLineWidth
    gedCmd configure -modelAxesColor $mModelAxesColor
    gedCmd configure -modelAxesLabelColor $mModelAxesLabelColor

    gedCmd configure -modelAxesTickInterval $mModelAxesTickInterval
    gedCmd configure -modelAxesTicksPerMajor $mModelAxesTicksPerMajor
    gedCmd configure -modelAxesTickThreshold $mModelAxesTickThreshold
    gedCmd configure -modelAxesTickLength $mModelAxesTickLength
    gedCmd configure -modelAxesTickMajorLength $mModelAxesTickMajorLength
    gedCmd configure -modelAxesTickColor $mModelAxesTickColor
    gedCmd configure -modelAxesTickMajorColor $mModelAxesTickMajorColor
}


::itcl::body Archer::applyModelAxesPreferencesIfDiff {} {
    if {$mModelAxesSizePref != $mModelAxesSize} {
	set mModelAxesSize $mModelAxesSizePref

	switch -- $mModelAxesSize {
	    "Small" {
		gedCmd configure -modelAxesSize 0.2
	    }
	    "Medium" {
		gedCmd configure -modelAxesSize 0.4
	    }
	    "Large" {
		gedCmd configure -modelAxesSize 0.8
	    }
	    "X-Large" {
		gedCmd configure -modelAxesSize 1.6
	    }
	    "View (1x)" {
		gedCmd configure -modelAxesSize 2.0
	    }
	    "View (2x)" {
		gedCmd configure -modelAxesSize 4.0
	    }
	    "View (4x)" {
		gedCmd configure -modelAxesSize 8.0
	    }
	    "View (8x)" {
		gedCmd configure -modelAxesSize 16.0
	    }
	}
    }

    set X [lindex $mModelAxesPosition 0]
    set Y [lindex $mModelAxesPosition 1]
    set Z [lindex $mModelAxesPosition 2]
    if {$mModelAxesPositionXPref != $X ||
	$mModelAxesPositionYPref != $Y ||
	$mModelAxesPositionZPref != $Z} {
	set mModelAxesPosition \
	    "$mModelAxesPositionXPref $mModelAxesPositionYPref $mModelAxesPositionZPref"
	gedCmd configure -modelAxesPosition $mModelAxesPosition
    }

    if {$mModelAxesLineWidthPref != $mModelAxesLineWidth} {
	set mModelAxesLineWidth $mModelAxesLineWidthPref
	gedCmd configure -modelAxesLineWidth $mModelAxesLineWidth
    }

    if {$mModelAxesColorPref != $mModelAxesColor} {
	set mModelAxesColor $mModelAxesColorPref
    }

    if {$mModelAxesLabelColorPref != $mModelAxesLabelColor} {
	set mModelAxesLabelColor $mModelAxesLabelColorPref
    }

    if {$mModelAxesTickIntervalPref != $mModelAxesTickInterval} {
	set mModelAxesTickInterval $mModelAxesTickIntervalPref
	gedCmd configure -modelAxesTickInterval $mModelAxesTickInterval
    }

    if {$mModelAxesTicksPerMajorPref != $mModelAxesTicksPerMajor} {
	set mModelAxesTicksPerMajor $mModelAxesTicksPerMajorPref
	gedCmd configure -modelAxesTicksPerMajor $mModelAxesTicksPerMajor
    }

    if {$mModelAxesTickThresholdPref != $mModelAxesTickThreshold} {
	set mModelAxesTickThreshold $mModelAxesTickThresholdPref
	gedCmd configure -modelAxesTickThreshold $mModelAxesTickThreshold
    }

    if {$mModelAxesTickLengthPref != $mModelAxesTickLength} {
	set mModelAxesTickLength $mModelAxesTickLengthPref
	gedCmd configure -modelAxesTickLength $mModelAxesTickLength
    }

    if {$mModelAxesTickMajorLengthPref != $mModelAxesTickMajorLength} {
	set mModelAxesTickMajorLength $mModelAxesTickMajorLengthPref
	gedCmd configure -modelAxesTickMajorLength $mModelAxesTickMajorLength
    }

    if {$mModelAxesTickColorPref != $mModelAxesTickColor} {
	set mModelAxesTickColor $mModelAxesTickColorPref
    }

    if {$mModelAxesTickMajorColorPref != $mModelAxesTickMajorColor} {
	set mModelAxesTickMajorColor $mModelAxesTickMajorColorPref
    }
}


::itcl::body Archer::applyPreferences {} {
    $itk_component(ged) refresh_off

    # Apply preferences to the cad widget.
    applyDisplayPreferences
    applyGeneralPreferences
    applyGridPreferences
    applyModelAxesPreferences
    applyViewAxesPreferences

    $itk_component(ged) refresh_on
    $itk_component(ged) refresh
}


::itcl::body Archer::applyPreferencesIfDiff {} {
    gedCmd refresh_off

    applyDisplayPreferencesIfDiff
    applyGeneralPreferencesIfDiff
    applyGridPreferencesIfDiff
    applyGroundPlanePreferencesIfDiff
    applyModelAxesPreferencesIfDiff
    applyViewAxesPreferencesIfDiff

    ::update

    gedCmd refresh_on
    gedCmd refresh
}


::itcl::body Archer::applyViewAxesPreferences {} {
    # sanity
    set offset 0.0
    switch -- $mViewAxesSize {
	"Small" {
	    set offset 0.85
	    gedCmd configure -viewAxesSize 0.2
	}
	"Medium" {
	    set offset 0.75
	    gedCmd configure -viewAxesSize 0.4
	}
	"Large" {
	    set offset 0.55
	    gedCmd configure -viewAxesSize 0.8
	}
	"X-Large" {
	    set offset 0.0
	    gedCmd configure -viewAxesSize 1.6
	}
    }

    switch -- $mViewAxesPosition {
	default -
	"Center" {
	    gedCmd configure -viewAxesPosition {0 0 0}
	}
	"Upper Left" {
	    gedCmd configure -viewAxesPosition "-$offset $offset 0"
	}
	"Upper Right" {
	    gedCmd configure -viewAxesPosition "$offset $offset 0"
	}
	"Lower Left" {
	    gedCmd configure -viewAxesPosition "-$offset -$offset 0"
	}
	"Lower Right" {
	    gedCmd configure -viewAxesPosition "$offset -$offset 0"
	}
    }

    if {$mViewAxesColor == "Triple"} {
	gedCmd configure -viewAxesTripleColor 1
    } else {
	gedCmd configure -viewAxesTripleColor 0
	gedCmd configure -viewAxesColor $mViewAxesColor
    }

    gedCmd configure -viewAxesLineWidth $mViewAxesLineWidth
    gedCmd configure -viewAxesLabelColor $mViewAxesLabelColor
}


::itcl::body Archer::applyViewAxesPreferencesIfDiff {} {

    set positionNotSet 1
    if {$mViewAxesSizePref != $mViewAxesSize} {
	set mViewAxesSize $mViewAxesSizePref

	# sanity
	set offset 0.0
	switch -- $mViewAxesSize {
	    "Small" {
		set offset 0.85
		gedCmd configure -viewAxesSize 0.2
	    }
	    "Medium" {
		set offset 0.75
		gedCmd configure -viewAxesSize 0.4
	    }
	    "Large" {
		set offset 0.55
		gedCmd configure -viewAxesSize 0.8
	    }
	    "X-Large" {
		set offset 0.0
		gedCmd configure -viewAxesSize 1.6
	    }
	}

	set positionNotSet 0
	set mViewAxesPosition $mViewAxesPositionPref

	switch -- $mViewAxesPosition {
	    default -
	    "Center" {
		gedCmd configure -viewAxesPosition {0 0 0}
	    }
	    "Upper Left" {
		gedCmd configure -viewAxesPosition "-$offset $offset 0"
	    }
	    "Upper Right" {
		gedCmd configure -viewAxesPosition "$offset $offset 0"
	    }
	    "Lower Left" {
		gedCmd configure -viewAxesPosition "-$offset -$offset 0"
	    }
	    "Lower Right" {
		gedCmd configure -viewAxesPosition "$offset -$offset 0"
	    }
	}
    }

    if {$positionNotSet &&
	$mViewAxesPositionPref != $mViewAxesPosition} {
	set mViewAxesPosition $mViewAxesPositionPref

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
		gedCmd configure -viewAxesPosition {0 0 0}
	    }
	    "Upper Left" {
		gedCmd configure -viewAxesPosition "-$offset $offset 0"
	    }
	    "Upper Right" {
		gedCmd configure -viewAxesPosition "$offset $offset 0"
	    }
	    "Lower Left" {
		gedCmd configure -viewAxesPosition "-$offset -$offset 0"
	    }
	    "Lower Right" {
		gedCmd configure -viewAxesPosition "$offset -$offset 0"
	    }
	}
    }

    if {$mViewAxesLineWidthPref != $mViewAxesLineWidth} {
	set mViewAxesLineWidth $mViewAxesLineWidthPref
	gedCmd configure -viewAxesLineWidth $mViewAxesLineWidth
    }

    if {$mViewAxesColorPref != $mViewAxesColor} {
	set mViewAxesColor $mViewAxesColorPref
    }

    if {$mViewAxesLabelColorPref != $mViewAxesLabelColor} {
	set mViewAxesLabelColor $mViewAxesLabelColorPref
    }
}


::itcl::body Archer::cancelPreferences {} {

    # Handling special case for zclip prefences (i.e. put zclip planes back where they were)
    if {$mZClipBackMaxPref != $mZClipBackMax ||
	$mZClipFrontMaxPref != $mZClipFrontMax ||
	$mZClipBackPref != $mZClipBack ||
	$mZClipFrontPref != $mZClipFront} {
	set mZClipBackMaxPref $mZClipBackMax
	set mZClipFrontMaxPref $mZClipFrontMax
	set mZClipBackPref $mZClipBack
	set mZClipFrontPref $mZClipFront
	updateDisplaySettings
    }
}

::itcl::body Archer::doPreferences {} {
    # update preference variables
    set mBackgroundColorPref $mBackgroundColor
    set mBindingModePref $mBindingMode
    set mEnableBigEPref $mEnableBigE
    set mFBBackgroundColorPref $mFBBackgroundColor
    set mDisplayFontSizePref $mDisplayFontSize
    set mMeasuringStickColorPref $mMeasuringStickColor
    set mMeasuringStickModePref $mMeasuringStickMode
    set mPrimitiveLabelColorPref $mPrimitiveLabelColor
    set mScaleColorPref $mScaleColor
    set mViewingParamsColorPref $mViewingParamsColor
    set mTreeAttrColumnsPref $mTreeAttrColumns
    set mEnableListViewPref $mEnableListView
    set mEnableListViewAllAffectedPref $mEnableListViewAllAffected
    set mEnableAffectedNodeHighlightPref $mEnableAffectedNodeHighlight
    set mSeparateCommandWindowPref $mSeparateCommandWindow
    set mDbUnits [gedCmd units -s]

    set mGridAnchorXPref [lindex $mGridAnchor 0]
    set mGridAnchorYPref [lindex $mGridAnchor 1]
    set mGridAnchorZPref [lindex $mGridAnchor 2]
    set mGridColorPref $mGridColor
    set mGridMrhPref $mGridMrh
    set mGridMrvPref $mGridMrv
    set mGridRhPref $mGridRh
    set mGridRvPref $mGridRv

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

    set mZClipBackMaxPref $mZClipBackMax
    set mZClipFrontMaxPref $mZClipFrontMax
    set mZClipBackPref $mZClipBack
    set mZClipFrontPref $mZClipFront

    $itk_component(preferencesDialog) center [namespace tail $this]
    ::update
    if {[$itk_component(preferencesDialog) activate]} {
	applyPreferencesIfDiff
    }
}


::itcl::body Archer::readPreferences {} {
    global env
    global no_tree_decorate

    if {$mViewOnly} {
	return
    }

    if {[info exists env(HOME)]} {
	set home $env(HOME)
    } else {
	set home .
    }

    readPreferencesInit

    # Read in the preferences file.
    if {![catch {open [file join $home $mPrefFile] r} pfile]} {
	set lines [split [read $pfile] "\n"]
	close $pfile

	foreach line $lines {
	    catch {eval $line}
	}
    }

    backgroundColor $mBackgroundColor

    if {!$mDelayCommandViewBuild} {
	::update
	initMode
    }
}


::itcl::body Archer::readPreferencesInit {} {
    set mPrefFile ".archerrc"
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
    if {![catch {open [file join $home $mPrefFile] w} pfile]} {
	writePreferencesHeader $pfile
	writePreferencesBody $pfile
	close $pfile
    } else {
	putString "Failed to write the preferences file:\n$pfile"
    }
}


::itcl::body Archer::writePreferencesHeader {_pfile} {
    puts $_pfile "# Archer's Preferences File"
    puts $_pfile "# Version 1.0.0"
    puts $_pfile "#"
    puts $_pfile "# DO NOT EDIT THIS FILE"
    puts $_pfile "#"
    puts $_pfile "# This file is created and updated by Archer."
    puts $_pfile "#"
}


::itcl::body Archer::writePreferencesBody {_pfile} {
    global no_tree_decorate

    if {[info exists no_tree_decorate]} {
	puts $_pfile "set no_tree_decorate $no_tree_decorate"
    }

    puts $_pfile "set mBackgroundColor \"$mBackgroundColor\""
    puts $_pfile "set mBindingMode $mBindingMode"
    puts $_pfile "set mEnableBigE $mEnableBigE"
    puts $_pfile "set mFBBackgroundColor \"$mFBBackgroundColor\""
    puts $_pfile "set mDisplayFontSize \"$mDisplayFontSize\""
    puts $_pfile "set mMeasuringStickColor \"$mMeasuringStickColor\""
    puts $_pfile "set mMeasuringStickMode $mMeasuringStickMode"
    puts $_pfile "set mPrimitiveLabelColor \"$mPrimitiveLabelColor\""
    puts $_pfile "set mScaleColor \"$mScaleColor\""
    puts $_pfile "set mViewingParamsColor \"$mViewingParamsColor\""
    puts $_pfile "set mTreeAttrColumns \"$mTreeAttrColumns\""
    puts $_pfile "set mEnableListView $mEnableListView"
    puts $_pfile "set mEnableListViewAllAffected $mEnableListViewAllAffected"
    puts $_pfile "set mEnableAffectedNodeHighlight $mEnableAffectedNodeHighlight"
    puts $_pfile "set mSeparateCommandWindow $mSeparateCommandWindow"

    puts $_pfile "set mGridAnchor \"$mGridAnchor\""
    puts $_pfile "set mGridColor \"$mGridColor\""
    puts $_pfile "set mGridMrh \"$mGridMrh\""
    puts $_pfile "set mGridMrv \"$mGridMrv\""
    puts $_pfile "set mGridRh \"$mGridRh\""
    puts $_pfile "set mGridRv \"$mGridRv\""

    puts $_pfile "set mGroundPlaneMajorColor \"$mGroundPlaneMajorColor\""
    puts $_pfile "set mGroundPlaneMinorColor \"$mGroundPlaneMinorColor\""
    puts $_pfile "set mGroundPlaneInterval \"$mGroundPlaneInterval\""
    puts $_pfile "set mGroundPlaneSize \"$mGroundPlaneSize\""

    puts $_pfile "set mViewAxesSize \"$mViewAxesSize\""
    puts $_pfile "set mViewAxesPosition \"$mViewAxesPosition\""
    puts $_pfile "set mViewAxesLineWidth $mViewAxesLineWidth"
    puts $_pfile "set mViewAxesColor \"$mViewAxesColor\""
    puts $_pfile "set mViewAxesLabelColor \"$mViewAxesLabelColor\""

    puts $_pfile "set mModelAxesSize \"$mModelAxesSize\""
    puts $_pfile "set mModelAxesPosition \"$mModelAxesPosition\""
    puts $_pfile "set mModelAxesLineWidth $mModelAxesLineWidth"
    puts $_pfile "set mModelAxesColor \"$mModelAxesColor\""
    puts $_pfile "set mModelAxesLabelColor \"$mModelAxesLabelColor\""

    puts $_pfile "set mModelAxesTickInterval $mModelAxesTickInterval"
    puts $_pfile "set mModelAxesTicksPerMajor $mModelAxesTicksPerMajor"
    puts $_pfile "set mModelAxesTickThreshold $mModelAxesTickThreshold"
    puts $_pfile "set mModelAxesTickLength $mModelAxesTickLength"
    puts $_pfile "set mModelAxesTickMajorLength $mModelAxesTickMajorLength"
    puts $_pfile "set mModelAxesTickColor \"$mModelAxesTickColor\""
    puts $_pfile "set mModelAxesTickMajorColor \"$mModelAxesTickMajorColor\""

    puts $_pfile "set mLastSelectedDir \"$mLastSelectedDir\""
    puts $_pfile "set mZClipBackMax $mZClipBackMax"
    puts $_pfile "set mZClipFrontMax $mZClipFrontMax"
    puts $_pfile "set mZClipBack $mZClipBack"
    puts $_pfile "set mZClipFront $mZClipFront"

    puts $_pfile "set mHPaneFraction1 $mHPaneFraction1"
    puts $_pfile "set mHPaneFraction2 $mHPaneFraction2"
    puts $_pfile "set mVPaneFraction1 $mVPaneFraction1"
    puts $_pfile "set mVPaneFraction2 $mVPaneFraction2"
    puts $_pfile "set mVPaneFraction3 $mVPaneFraction3"
    puts $_pfile "set mVPaneFraction4 $mVPaneFraction4"
    puts $_pfile "set mVPaneFraction5 $mVPaneFraction5"
    puts $_pfile "set mVPaneToggle1 $mVPaneToggle1"
    puts $_pfile "set mVPaneToggle3 $mVPaneToggle3"
    puts $_pfile "set mVPaneToggle5 $mVPaneToggle5"

    puts $_pfile "set mWindowGeometry [winfo geometry [namespace tail $this]]"

    puts $_pfile "set mShowViewAxes $mShowViewAxes"
    puts $_pfile "set mShowModelAxes $mShowModelAxes"
    puts $_pfile "set mShowGroundPlane $mShowGroundPlane"
    puts $_pfile "set mShowPrimitiveLabels $mShowPrimitiveLabels"
    puts $_pfile "set mShowViewingParams $mShowViewingParams"
    puts $_pfile "set mShowScale $mShowScale"
    puts $_pfile "set mLighting $mLighting"
    puts $_pfile "set mShowGrid $mShowGrid"
    puts $_pfile "set mSnapGrid $mSnapGrid"
    puts $_pfile "set mShowADC $mShowADC"
}


::itcl::body Archer::affectedNodeHighlightCallback {} {
    if {!$mEnableAffectedNodeHighlightPref} {
	set mEnableListViewAllAffectedPref 0
    }
}


::itcl::body Archer::listViewAllAffectedCallback {} {
    if {$mEnableListViewAllAffectedPref} {
	set mEnableAffectedNodeHighlightPref 1
    }
}


################################### Primitive Creation Section ###################################

::itcl::body Archer::createObj {type} {
    gedCmd make_name -s 1

    switch -- $type {
	"arb4" {
	    set name [gedCmd make_name "arb4."]
	    createArb4 $name
	}
	"arb5" {
	    set name [gedCmd make_name "arb5."]
	    createArb5 $name
	}
	"arb6" {
	    set name [gedCmd make_name "arb6."]
	    createArb6 $name
	}
	"arb7" {
	    set name [gedCmd make_name "arb7."]
	    createArb7 $name
	}
	"arb8" {
	    set name [gedCmd make_name "arb8."]
	    createArb8 $name
	}
	"arbn" {
	    set name [gedCmd make_name "arbn."]
	    vmake $name arbn
	}
	"ars" {
	    set name [gedCmd make_name "ars."]
	    vmake $name ars
	}
	"binunif" {
	    #XXX Not ready yet
	    return
	}
	"bot" {
	    set name [gedCmd make_name "bot."]
	    vmake $name bot

	    #XXX Not ready yet
	    #	    return

	    #	    set name [gedCmd make_name "bot."]
	    #	    createBot $name
	}
	"comb" {
	    set name [gedCmd make_name "comb."]
	    createComb $name
	}
	"ehy" {
	    set name [gedCmd make_name "ehy."]
	    createEhy $name
	}
	"ell" {
	    set name [gedCmd make_name "ell."]
	    createEll $name
	}
	"ell1" {
	    set name [gedCmd make_name "ell1."]
	    vmake $name ell1
	}
	"epa" {
	    set name [gedCmd make_name "epa."]
	    createEpa $name
	}
	"eto" {
	    set name [gedCmd make_name "eto."]
	    createEto $name
	}
	"extrude" {
	    set name [gedCmd make_name "extrude."]
	    vmake $name extrude

	    #XXX Not ready yet
	    #	    return

	    #	    set name [gedCmd make_name "extrude."]
	    #	    createExtrude $name
	}
	"grip" {
	    set name [gedCmd make_name "grip."]
	    createGrip $name
	}
	"half" {
	    set name [gedCmd make_name "half."]
	    createHalf $name
	}
	"hyp" {
	    set name [gedCmd make_name "hyp."]
	    createHyp $name
	}
	"metaball" {
	    set name [gedCmd make_name "metaball."]
	    vmake $name metaball
	}
	"nmg" {
	    set name [gedCmd make_name "nmg."]
	    vmake $name nmg
	}
	"part" {
	    set name [gedCmd make_name "part."]
	    createPart $name
	}
	"pipe" {
	    set name [gedCmd make_name "pipe."]
	    createPipe $name
	    #vmake $name pipe
	}
	"rcc" {
	    set name [gedCmd make_name "rcc."]
	    vmake $name rcc
	}
	"rec" {
	    set name [gedCmd make_name "rec."]
	    vmake $name rec
	}
	"rhc" {
	    set name [gedCmd make_name "rhc."]
	    createRhc $name
	}
	"rpc" {
	    set name [gedCmd make_name "rpc."]
	    createRpc $name
	}
	"rpp" {
	    set name [gedCmd make_name "rpp."]
	    createArb8 $name
	}
	"sketch" {
	    set name [gedCmd make_name "sketch."]
	    vmake $name sketch
	    #	    set name [gedCmd make_name "sketch."]
	    #	    createSketch $name
	}
	"sph" {
	    set name [gedCmd make_name "sph."]
	    createSphere $name
	}
	"superell" {
	    set name [gedCmd make_name "ell."]
	    createSuperell $name
	}
	"tec" {
	    set name [gedCmd make_name "tec."]
	    vmake $name tec
	}
	"tgc" {
	    set name [gedCmd make_name "tgc."]
	    createTgc $name
	}
	"tor" {
	    set name [gedCmd make_name "tor."]
	    createTorus $name
	}
	"trc" {
	    set name [gedCmd make_name "trc."]
	    gedCmd vmake $name trc
	}
	default {
	    return
	}
    }

    fillTree {} $name $mEnableListView
    $itk_component(ged) draw $name
    selectTreePath $name
    #    updateTreeDrawLists

    set mNeedSave 1
    updateSaveMode
}


::itcl::body Archer::createArb4 {name} {
    if {![info exists itk_component(arb4View)]} {
	buildArb4EditView
	$itk_component(arb4View) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(arb4View) createGeometry $name
}


::itcl::body Archer::createArb5 {name} {
    if {![info exists itk_component(arb5View)]} {
	buildArb5EditView
	$itk_component(arb5View) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(arb5View) createGeometry $name
}


::itcl::body Archer::createArb6 {name} {
    if {![info exists itk_component(arb6View)]} {
	buildArb6EditView
	$itk_component(arb6View) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(arb6View) createGeometry $name
}


::itcl::body Archer::createArb7 {name} {
    if {![info exists itk_component(arb7View)]} {
	buildArb7EditView
	$itk_component(arb7View) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(arb7View) createGeometry $name
}


::itcl::body Archer::createArb8 {name} {
    if {![info exists itk_component(arb8View)]} {
	buildArb8EditView
	$itk_component(arb8View) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(arb8View) createGeometry $name
}


::itcl::body Archer::createBot {name} {
    #XXX Not ready yet
    return

    if {![info exists itk_component(botView)]} {
	buildBotEditView
	$itk_component(botView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(botView) createGeometry $name
}


::itcl::body Archer::createComb {name} {
    if {![info exists itk_component(combView)]} {
	buildCombEditView
	$itk_component(combView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(combView) createGeometry $name
}


::itcl::body Archer::createEhy {name} {
    if {![info exists itk_component(ehyView)]} {
	buildEhyEditView
	$itk_component(ehyView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(ehyView) createGeometry $name
}


::itcl::body Archer::createEll {name} {
    if {![info exists itk_component(ellView)]} {
	buildEllEditView
	$itk_component(ellView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(ellView) createGeometry $name
}


::itcl::body Archer::createEpa {name} {
    if {![info exists itk_component(epaView)]} {
	buildEpaEditView
	$itk_component(epaView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(epaView) createGeometry $name
}


::itcl::body Archer::createEto {name} {
    if {![info exists itk_component(etoView)]} {
	buildEtoEditView
	$itk_component(etoView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(etoView) createGeometry $name
}


::itcl::body Archer::createExtrude {name} {
    #XXX Not ready yet
    return

    if {![info exists itk_component(extrudeView)]} {
	buildExtrudeEditView
	$itk_component(extrudeView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(extrudeView) createGeometry $name
}


::itcl::body Archer::createGrip {name} {
    if {![info exists itk_component(gripView)]} {
	buildGripEditView
	$itk_component(gripView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(gripView) createGeometry $name
}


::itcl::body Archer::createHalf {name} {
    if {![info exists itk_component(halfView)]} {
	buildHalfEditView
	$itk_component(halfView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(halfView) createGeometry $name
}


::itcl::body Archer::createHyp {name} {
    if {![info exists itk_component(hypView)]} {
	buildHypEditView
	$itk_component(hypView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(hypView) createGeometry $name
}


::itcl::body Archer::createPart {name} {
    if {![info exists itk_component(partView)]} {
	buildPartEditView
	$itk_component(partView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(partView) createGeometry $name
}


::itcl::body Archer::createPipe {name} {
    if {![info exists itk_component(pipeView)]} {
	buildPipeEditView
	$itk_component(pipeView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(pipeView) createGeometry $name
}


::itcl::body Archer::createRhc {name} {
    if {![info exists itk_component(rhcView)]} {
	buildRhcEditView
	$itk_component(rhcView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(rhcView) createGeometry $name
}


::itcl::body Archer::createRpc {name} {
    if {![info exists itk_component(rpcView)]} {
	buildRpcEditView
	$itk_component(rpcView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(rpcView) createGeometry $name
}


::itcl::body Archer::createSketch {name} {
    if {![info exists itk_component(sketchView)]} {
	buildSketchEditView
	$itk_component(sketchView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(sketchView) createGeometry $name
}


::itcl::body Archer::createSphere {name} {
    if {![info exists itk_component(sphView)]} {
	buildSphereEditView
	$itk_component(sphView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(sphView) createGeometry $name
}


::itcl::body Archer::createSuperell {name} {
    if {![info exists itk_component(superellView)]} {
	buildSuperellEditView
	$itk_component(superellView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(superellView) createGeometry $name
}


::itcl::body Archer::createTgc {name} {
    if {![info exists itk_component(tgcView)]} {
	buildTgcEditView
	$itk_component(tgcView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(tgcView) createGeometry $name
}


::itcl::body Archer::createTorus {name} {
    if {![info exists itk_component(torView)]} {
	buildTorusEditView
	$itk_component(torView) configure \
	    -mged $itk_component(ged)
    }
    $itk_component(torView) createGeometry $name
}


################################### Begin Object Edit Management ###################################

::itcl::body Archer::revert {} {
    set mNeedSave 0
    Load $mTarget

    set mNeedSave 0
    updateSaveMode
}



################################### End Object Edit Management ###################################


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
