#                      A R C H E R . T C L
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
#    Archer mega-widget.
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
	common LEDGER_ENTRY_OUT_OF_SYNC_ATTR "Ledger_Entry_Out_Of_Sync"
	common LEDGER_ENTRY_TYPE_ATTR "Ledger_Entry_Type"
	common LEDGER_ENTRY_MOVE_COMMAND "Ledger_Entry_Move_Command"
	common LEDGER_CREATE "Create"
	common LEDGER_DESTROY "Destroy"
	common LEDGER_MODIFY "Modify"
	common LEDGER_RENAME "Rename"

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
	method setDefaultBindingMode {_mode}

	# General
	method askToRevert {}

	# ArcherCore Override Section
	method 3ptarb              {args}
	method attr                {args}
	method bo                  {args}
	method bot_condense        {args}
	method bot_decimate        {args}
	method bot_face_fuse       {args}
	method bot_merge           {args}
	method bot_smooth          {args}
	method bot_split           {args}
	method bot_vertex_fuse     {args}
	method c                   {args}
	method clone               {args}
	method color               {args}
	method comb                {args}
	method cp                  {args}
	method cpi                 {args}
	method copyeval            {args}
	method copymat             {args}
	method dbconcat            {args}
	method decompose           {args}
	method edcodes             {args}
	method edcolor             {args}
	method edmater             {args}
	method facetize            {args}
	method fracture            {args}
	method g                   {args}
	method human               {args}
	method i                   {args}
	method in                  {args}
	method inside              {args}
	method kill                {args}
	method killall             {args}
	method killrefs            {args}
	method killtree            {args}
	method make                {args}
	method make_bb             {args}
	method make_pnts           {args}
	method mirror              {args}
	method mv                  {args}
	method mvall               {args}
	method nmg_collapse        {args}
	method nmg_simplify        {args}
	method p                   {args}
	method p_protate           {args}
	method p_pscale            {args}
	method p_ptranslate        {args}
	method p_move_arb_edge     {args}
	method p_move_arb_face     {args}
	method p_rotate_arb_face   {args}
	method prefix              {args}
	method push                {args}
	method put                 {args}
	method putmat              {args}
	method Load                {_target}
	method r                   {args}
	method rcodes              {args}
	method rfarb               {args}
	method rmater              {args}
	method saveDb              {}
	method shells              {args}
	method tire                {args}
	method title               {args}
	method track               {args}
	method units               {args}
	method vmake               {args}
	method updateTheme         {}

	# Object Edit Management
	method checkpoint {_obj _type}
	method checkpoint_olist {_olist _type}
	method clearTargetLedger {}
	method createTargetLedger {}
	method global_undo {}
	method global_undo_callback {_gname}
	method ledger_cleanup {}
	method object_checkpoint {}
	method object_undo {}
	method revert {}
	method selection_checkpoint {_obj}
	method updateObjSave {}
    }

    protected {
	variable mPrefFile ""
	variable mNeedObjSave 0
	variable mNeedGlobalUndo 0
	variable mNeedObjUndo 0
	variable mNeedCheckpoint 0
	variable mLedger ""
	variable mLedgerGID 0

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
	method combWrapper {_cmd _minArgs args}
	method createWrapper {_cmd args}
	method gedWrapper {_cmd _eflag _hflag _sflag _tflag args}
	method gedWrapper2 {_cmd _oindex _pindex _eflag _hflag _sflag _tflag args}
	method globalWrapper {_cmd args}
	method killWrapper {_cmd args}
	method moveWrapper {_cmd args}
	method initDefaultBindings {{_comp ""}}
	method initGed {}
	method selectNode {_tags {_rflag 1}}
	method updateCheckpointMode {}
	method updateSaveMode {}
	method updateUndoMode {{_oflag 1}}
	method updateUndoState {}

	# Miscellaneous Section
	method buildAboutDialog {}
	method buildBackgroundColor {_parent}
	method buildDisplayPreferences {}
	method buildGeneralPreferences {}
	method buildGroundPlanePreferences {}
	method buildInfoDialogs {}
	method buildModelAxesPosition {_parent}
	method buildModelAxesPreferences {}
	method buildMouseOverridesDialog {}
	method buildPreferencesDialog {}
	method buildRevertDialog {}
	method buildToplevelMenubar {}
	method buildViewAxesPreferences {}
	method doAboutArcher {}
	method doArcherHelp {}
	method launchDisplayMenuBegin {_dm _m _x _y}
	method launchDisplayMenuEnd {}

	#XXX Need to split up menuStatusCB into one method per menu
	method menuStatusCB {_w}
	method modesMenuStatusCB {_w}
	method rtCheckMenuStatusCB {_w}
	method rtEdgeMenuStatusCB {_w}
	method rtMenuStatusCB {_w}

	method updateCreationButtons {_on}
	method updatePrimaryToolbar {}

	method buildEmbeddedMenubar {}
	method buildEmbeddedFileMenu {}
	method buildEmbeddedDisplayMenu {}
	method buildEmbeddedModesMenu {}
	method buildEmbeddedRaytraceMenu {}
	method buildEmbeddedHelpMenu {}

	method buildModesMenu {}

	# Modes Section
	method initMode {{_updateFractions 0}}

	# Object Edit Section
	method initEdit {{_initEditMode 1}}

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
	method readPreferencesInit {}
	method writePreferences {}
	method writePreferencesHeader {_pfile}
	method writePreferencesBody {_pfile}

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
    lappend mArcherCoreCommands importFg4Sections

    if {!$mViewOnly} {
	if {$ArcherCore::inheritFromToplevel} {
	    buildToplevelMenubar
	    $this configure -menu $itk_component(menubar)
	} else {
	    buildEmbeddedMenubar
	    pack $itk_component(menubar) -side top -fill x -padx 1
	}

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

	# set initial toggle variables
	set mVPaneToggle3 $mVPaneFraction3
	set mVPaneToggle5 $mVPaneFraction5

	readPreferences
	updateCreationButtons 0
	updateCheckpointMode
	updateSaveMode
	updateUndoMode
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

    $itk_component(primaryToolbar) itemconfigure new -state normal
    $itk_component(primaryToolbar) itemconfigure preferences -state normal
}


# ------------------------------------------------------------
#                       DESTRUCTOR
# ------------------------------------------------------------
::itcl::body Archer::destructor {} {
    writePreferences

    if {$mLedger != ""} {
	catch {rename $mLedger ""}
    }
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

    if {[info exists itk_component(ged)]} {
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

	    if {![catch {$itk_component(ged) get_type $solidName} ret]} {
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
		if {[catch {$itk_component(ged) get_type $gname} ret]} {
		    $itk_component(ged) g $gname $gmember
		} else {
		    if {[catch {gedCmd get $gname tree} tree]} {
			#$itk_component(ged) attachObservers
			$itk_component(ged) units $savedUnits
			error "importFg4Sections: $gname is not a group!"
		    }

		    # Add gmember only if its not already there
		    regsub -all {(\{[ul] )|([{}]+)} $tree " " tmembers
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
	}

	# Add wizard attributes
	foreach {key val} $wlist {
	    $itk_component(ged) attr set $wizTop $key $val
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
	$itk_component(ged) units $savedUnits
	$itk_component(ged) attachObservers
	$itk_component(ged) refreshAll
	$itk_component(ged) configure -autoViewEnable 1
	SetNormalCursor $this
    }
}

::itcl::body Archer::3ptarb {args} {
    eval ArcherCore::gedWrapper 3ptarb 0 0 1 1 $args
}

::itcl::body Archer::attr {args} {
    set len [llength $args]

    set cmd [lindex $args 0]
    switch -- $cmd {
	"append" -
	"set" {
	    if {$len < 4} {
		return [eval gedWrapper2 attr 1 0 0 0 0 0 $args]
	    }

	    return [eval gedWrapper2 attr 1 0 0 0 1 0 $args]
	}
	"rm" {
	    if {$len < 3} {
		return [eval gedWrapper2 attr 1 0 0 0 0 0 $args]
	    }

	    return [eval gedWrapper2 attr 1 0 0 0 1 0 $args]
	}
	"get" -
	"show" {
	    return [eval gedWrapper2 attr 1 0 0 0 0 0 $args]
	}
    }
}

::itcl::body Archer::bo {args} {
    eval ArcherCore::gedWrapper bo 0 0 1 1 $args
}

::itcl::body Archer::bot_condense {args} {
    eval ArcherCore::gedWrapper bot_condense 0 0 1 1 $args
}

::itcl::body Archer::bot_decimate {args} {
    eval ArcherCore::gedWrapper bot_decimate 0 0 1 1 $args
}

::itcl::body Archer::bot_face_fuse {args} {
    eval ArcherCore::gedWrapper bot_face_fuse 0 0 1 1 $args
}

::itcl::body Archer::bot_merge {args} {
    eval ArcherCore::gedWrapper bot_merge 1 0 1 1 $args
}

::itcl::body Archer::bot_smooth {args} {
    eval ArcherCore::gedWrapper bot_smooth 0 0 1 1 $args
}

::itcl::body Archer::bot_split {args} {
    eval ArcherCore::gedWrapper bot_split 0 0 1 1 $args
}

::itcl::body Archer::bot_vertex_fuse {args} {
    eval ArcherCore::gedWrapper bot_vertex_fuse 0 0 1 1 $args
}

#
# Create a combination.
#
::itcl::body Archer::c {args} {
    eval combWrapper c 2 $args
}

::itcl::body Archer::clone {args} {
    eval createWrapper clone $args
}

::itcl::body Archer::color {args} {
    eval ArcherCore::gedWrapper color 0 0 1 1 $args
}

#
# Create a combination or modify an existing one.
#
::itcl::body Archer::comb {args} {
    eval combWrapper comb 3 $args
}

::itcl::body Archer::cp {args} {
    eval createWrapper cp $args
}

::itcl::body Archer::cpi {args} {
    eval createWrapper cpi $args
}

::itcl::body Archer::copyeval {args} {
    eval createWrapper copyeval $args
}

::itcl::body Archer::copymat {args} {
    eval gedWrapper2 copymat 1 0 0 0 1 1 $args
}

::itcl::body Archer::dbconcat {args} {
    eval ArcherCore::gedWrapper dbconcat 0 0 1 1 $args
}

::itcl::body Archer::decompose {args} {
    eval ArcherCore::gedWrapper decompose 0 0 1 1 $args
}

::itcl::body Archer::edcodes {args} {
    # Returns a help message
    if {[llength $args] == 0} {
	return [gedCmd edcodes]
    }

    set optionsAndArgs [eval dbExpand $args]
    set options [lindex $optionsAndArgs 0]
    set expandedArgs [lindex $optionsAndArgs 1]

    # Returns a help message
    if {[llength $expandedArgs] == 0} {
	return [gedCmd edcodes]
    }

    switch [lindex $options 0] {
	"-n" {
	    return [eval gedCmd edcodes $options $expandedArgs]
	}
	default {
	    set olist [gedCmd edcodes -n $expandedArgs]
	}
    }

    if {[llength $olist] == 0} {
	return
    }

    SetWaitCursor $this
    set lnames [checkpoint_olist $olist $LEDGER_MODIFY]

    if {[catch {eval gedCmd edcodes $options $expandedArgs} ret]} {
	ledger_cleanup
	SetNormalCursor $this
	return $ret
    }

    set fflag 0
    set gflag 0
    set oflag 0
    set new_olist {}

    foreach oname $olist lname $lnames {
	if {$oname == $mSelectedObj} {
	    # Found selected object
	    set fflag 1
	}

	# Compare region ids
	set o_val [gedCmd get $oname id]
	set l_val [$mLedger get $lname id]
	if {$o_val != $l_val} {
	    set gflag 1

	    if {$oname == $mSelectedObj} {
		set oflag 1
	    }

	    $mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
	    lappend new_olist $lname
	    continue
	}

	# Compare air
	set o_val [gedCmd get $oname air]
	set l_val [$mLedger get $lname air]
	if {$o_val != $l_val} {
	    set gflag 1

	    if {$oname == $mSelectedObj} {
		set oflag 1
	    }

	    $mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
	    lappend new_olist $lname
	    continue
	}

	# Compare gift material
	set o_val [gedCmd get $oname GIFTmater]
	set l_val [$mLedger get $lname GIFTmater]
	if {$o_val != $l_val} {
	    set gflag 1

	    if {$oname == $mSelectedObj} {
		set oflag 1
	    }

	    $mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
	    lappend new_olist $lname
	    continue
	}

	# Compare los
	set o_val [gedCmd get $oname los]
	set l_val [$mLedger get $lname los]
	if {$o_val != $l_val} {
	    set gflag 1

	    if {$oname == $mSelectedObj} {
		set oflag 1
	    }

	    $mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
	    lappend new_olist $lname
	    continue
	}

	# No mods found
	$mLedger kill $lname
    }

    if {$oflag} {
	# Checkpoint again in case the user starts interacting via the mouse
	checkpoint $mSelectedObj $LEDGER_MODIFY
    }

    updateUndoState

    SetNormalCursor $this
#    eval ArcherCore::gedWrapper edcodes 0 0 1 1 $args
}

::itcl::body Archer::edcolor {args} {
    return [eval globalWrapper edcolor $args]
}

# Needs an edWrapper to create checkpoints for each object
::itcl::body Archer::edmater {args} {
    eval ArcherCore::gedWrapper edmater 0 0 1 1 $args
}

::itcl::body Archer::facetize {args} {
    eval createWrapper facetize $args
}

::itcl::body Archer::fracture {args} {
    eval ArcherCore::gedWrapper fracture 0 0 1 1 $args
}

#
# Create a group or modify an existing one.
#
::itcl::body Archer::g {args} {
    eval combWrapper g 2 $args
}

::itcl::body Archer::human {args} {
    eval ArcherCore::gedWrapper human 0 0 1 1 $args
}

::itcl::body Archer::i {args} {
    eval gedWrapper2 i 1 0 0 0 1 1 $args
}

::itcl::body Archer::in {args} {
    return [eval createWrapper in $args]

    SetWaitCursor $this

    if {[llength $args] == 0} {
	set new_args [handleMoreArgs "Enter name of solid: "]
	while {[llength $new_args] == 0} {
	    set new_args [handleMoreArgs "Enter name of solid: "]
	}

	set args $new_args
    }

    set new_name [lindex $args 0]

    if {[catch {eval gedCmd in $args} ret]} {
	SetNormalCursor $this
	return $ret
    }

    # Checkpoint the created object
    set lnew_name [checkpoint $new_name $LEDGER_CREATE]

    refreshTree 1

    checkpoint $new_name $LEDGER_MODIFY
    updateUndoState
    SetNormalCursor $this
}

::itcl::body Archer::inside {args} {
    eval createWrapper inside $args
}

::itcl::body Archer::kill {args} {
    eval killWrapper kill $args
}

::itcl::body Archer::killall {args} {
    eval killWrapper killall $args
}

::itcl::body Archer::killrefs {args} {
#    eval ArcherCore::gedWrapper killrefs 0 0 1 1 $args

    # Returns a help message
    if {[llength $args] == 0} {
	return [gedCmd killrefs]
    }

    set optionsAndArgs [eval dbExpand $args]
    set options [lindex $optionsAndArgs 0]
    set expandedArgs [lindex $optionsAndArgs 1]

    # Returns a help message
    if {[llength $expandedArgs] == 0} {
	return [gedCmd killrefs]
    }

    switch [lindex $options 0] {
	"-n" {
	    return [eval gedCmd killrefs $options $expandedArgs]
	}
	default {
	    set olist [gedCmd killrefs -n $expandedArgs]
	}
    }

    if {[llength $olist] == 0} {
	return
    }

    SetWaitCursor $this
    set lnames [checkpoint_olist $olist $LEDGER_MODIFY]

    if {[catch {eval gedCmd killrefs $options $expandedArgs} ret]} {
	ledger_cleanup
	SetNormalCursor $this
	return $ret
    }

    foreach lname $lnames {
	$mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
    }

    refreshTree 1

    checkpoint_olist $olist $LEDGER_MODIFY
    updateUndoState
    SetNormalCursor $this
}

::itcl::body Archer::killtree {args} {
    eval killWrapper killtree $args
}

::itcl::body Archer::make {args} {
    eval createWrapper make $args
}

::itcl::body Archer::make_bb {args} {
    eval ArcherCore::gedWrapper make_bb 0 0 1 1 $args
}

::itcl::body Archer::make_pnts {args} {
    eval ArcherCore::gedWrapper make_pnts 0 0 1 1 $args
}

::itcl::body Archer::mirror {args} {
    eval createWrapper mirror $args
}

::itcl::body Archer::mv {args} {
    eval moveWrapper mv $args
}

::itcl::body Archer::mvall {args} {
    eval moveWrapper mvall $args
}

::itcl::body Archer::nmg_collapse {args} {
    eval ArcherCore::gedWrapper nmg_collapse 0 0 1 1 $args
}

::itcl::body Archer::nmg_simplify {args} {
    eval ArcherCore::gedWrapper nmg_simplify 0 0 1 1 $args
}

::itcl::body Archer::p {args} {
    if {$mSelectedObj == ""} {
	return
    }

    checkpoint $mSelectedObj $LEDGER_MODIFY

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
	updateObjSave
	redrawObj $mSelectedObjPath
	initEdit 0

	# Checkpoint again in case the user starts interacting via the mouse
	checkpoint $mSelectedObj $LEDGER_MODIFY
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

::itcl::body Archer::prefix {args} {
    eval ArcherCore::gedWrapper prefix 0 0 1 1 $args
}

::itcl::body Archer::push {args} {
    eval ArcherCore::gedWrapper push 0 0 1 1 $args
}

::itcl::body Archer::put {args} {
    eval ArcherCore::gedWrapper put 0 0 1 1 $args
}

::itcl::body Archer::putmat {args} {
    eval gedWrapper2 putmat 0 0 0 0 1 1 $args
}

::itcl::body Archer::Load {_target} {
    SetWaitCursor $this
    if {$mNeedSave} {
	if {![askToSave]} {
	    set mNeedSave 0
	    updateSaveMode
	    updateUndoMode
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

    if {!$mViewOnly} {
	createTargetLedger
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

    $itk_component(ged) refresh_off
    set mDbTitle [$itk_component(ged) title]
    set mDbUnits [$itk_component(ged) units]

    if {!$mViewOnly} {
	initDbAttrView $mTarget
	applyPreferences
	doLighting
	updateWizardMenu
	updateUtilityMenu
	deleteTargetOldCopy
#	createTargetLedger

	updateCreationButtons 1

	buildGroundPlane
	showGroundPlane
    } else {
	applyPreferences
	doLighting
    }

    # refresh tree contents
    refreshTree 0

    if {$mBindingMode == 0} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
    }
    $itk_component(ged) refresh_on
    $itk_component(ged) refresh
    SetNormalCursor $this
}

#
# Create a region or modify an existing one.
#
::itcl::body Archer::r {args} {
    eval combWrapper r 3 $args
}

# XXX libged's rcodes needs to return the objects affected
# The affected objects would be added to the ledger.
::itcl::body Archer::rcodes {args} {
    eval ArcherCore::gedWrapper rcodes 0 0 1 1 $args
}

::itcl::body Archer::rfarb {args} {
    eval ArcherCore::gedWrapper rfarb 0 0 1 1 $args
}

# XXX libged's rmater needs to return the objects affected
# The affected objects would be added to the ledger.
::itcl::body Archer::rmater {args} {
    eval ArcherCore::gedWrapper rmater 0 0 1 1 $args
}

::itcl::body Archer::saveDb {} {
    ArcherCore::saveDb
    clearTargetLedger
#    createTargetLedger

    set mNeedCheckpoint 0
    updateCheckpointMode

    set mNeedGlobalUndo 0
    set mNeedObjSave 0
    set mNeedObjUndo 0
    updateUndoMode

    checkpoint $mSelectedObj $LEDGER_MODIFY
}

::itcl::body Archer::shells {args} {
    eval ArcherCore::gedWrapper shells 0 0 1 1 $args
}

::itcl::body Archer::tire {args} {
    eval ArcherCore::gedWrapper tire 0 0 1 1 $args
}

::itcl::body Archer::title {args} {
    return [eval globalWrapper title $args]
}

# XXX libged's track needs to return the objects affected
# The affected objects would be added to the ledger.
::itcl::body Archer::track {args} {
    eval ArcherCore::gedWrapper track 0 0 1 0 $args
}

::itcl::body Archer::units {args} {
    return [eval globalWrapper units $args]
}

::itcl::body Archer::vmake {args} {
    eval ArcherCore::gedWrapper vmake 0 0 1 1 $args
}

::itcl::body Archer::updateTheme {} {
    if {$mInstanceInit} {
	return
    }

    ArcherCore::updateTheme

    set dir [file join $mImgDir Themes $mTheme]

    if {!$mViewOnly} {
	# Primary 
	$itk_component(primaryToolbar) itemconfigure checkpoint \
	    -image [image create photo \
			-file [file join $dir checkpoint.png]]
	$itk_component(primaryToolbar) itemconfigure object_undo \
	    -image [image create photo \
			-file [file join $dir object_undo.png]]
	$itk_component(primaryToolbar) itemconfigure new \
	    -image [image create photo \
			-file [file join $dir file_new.png]]
	$itk_component(primaryToolbar) itemconfigure global_undo \
	    -image [image create photo \
			-file [file join $dir global_undo.png]]
	$itk_component(primaryToolbar) itemconfigure revert \
	    -image [image create photo \
			-file [file join $dir revert.png]]
	$itk_component(primaryToolbar) itemconfigure preferences \
	    -image [image create photo \
			-file [file join $dir configure.png]]
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

	# View Toolbar
#	$itk_component(primaryToolbar) itemconfigure rotate \
#	    -image [image create photo \
#			-file [file join $dir view_rotate.png]]
#	$itk_component(primaryToolbar) itemconfigure translate \
#	    -image [image create photo \
#			-file [file join $dir view_translate.png]]
#	$itk_component(primaryToolbar) itemconfigure scale \
#	    -image [image create photo \
#			-file [file join $dir view_scale.png]]
#	$itk_component(primaryToolbar) itemconfigure center \
#	    -image [image create photo \
#			-file [file join $dir view_select.png]]
#	$itk_component(primaryToolbar) itemconfigure cpick \
#	    -image [image create photo \
#			-file [file join $dir compSelect.png]]
#	$itk_component(primaryToolbar) itemconfigure measure \
#	    -image [image create photo \
#			-file [file join $dir measure.png]]

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
	#$itk_component(primaryToolbar) itemconfigure tgc \
	    -image [image create photo \
			-file [file join $dir primitive_tgc.png]]
	#$itk_component(primaryToolbar) itemconfigure superell \
	    -image [image create photo \
			-file [file join $dir primitive_superell.png]]
	#$itk_component(primaryToolbar) itemconfigure hyp \
	    -image [image create photo \
			-file [file join $dir primitive_hyp.png]]

	catch {
	    $itk_component(primaryToolbar) itemconfigure edit_rotate \
		-image [image create photo \
			    -file [file join $dir edit_rotate.png]]
	    $itk_component(primaryToolbar) itemconfigure edit_translate \
		-image [image create photo \
			    -file [file join $dir edit_translate.png]]
	    $itk_component(primaryToolbar) itemconfigure edit_scale \
		-image [image create photo \
			    -file [file join $dir edit_scale.png]]
	    $itk_component(primaryToolbar) itemconfigure edit_center \
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
    }
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

    switch -- $type {
	"leaf"   -
	"branch" {renderComp $node}
    }
}

#
# Create a combination or modify an existing one.
#
::itcl::body Archer::combWrapper {_cmd _minArgs args} {
    set alen [llength $args]
    if {$alen < $_minArgs} {
	return [gedCmd $_cmd]
    }

    set obj [lindex $args 0]

    # Check for the existence of obj
    if {[catch {gedCmd attr show $obj} adata]} {
	# Create a new combination
	eval createWrapper $_cmd $args
    } else {
	# Modifying an existing combination
	eval gedWrapper $_cmd 0 0 1 1 $args
    }
}

::itcl::body Archer::createWrapper {_cmd args} {
    # Set the list of created objects (i.e. clist)
    switch -- $_cmd {
	"c" -
	"facetize" {
	    set optionsAndArgs [eval dbExpand $args]
	    set options [lindex $optionsAndArgs 0]
	    set expandedArgs [lindex $optionsAndArgs 1]

	    # Returns a help message.
	    if {[llength $expandedArgs] < 2} {
		return [gedCmd $_cmd]
	    }

	    set clist [lindex $expandedArgs 0]
	}
	"clone" {
	    # Returns a help message.
	    if {[llength $args] == 0} {
		return [gedCmd $_cmd]
	    }

	    set options [lrange $args 0 end-1]
	    set expandedArgs [lrange $args end end]

	    # Clone will return the clist info. Consequently,
	    # clist is set after invoking clone below.
	}
	"copyeval" -
	"cp" -
	"cpi" -
	"mirror" {
	    # Returns a help message.
	    if {[llength $args] < 2} {
		return [gedCmd $_cmd]
	    }

	    set options [lrange $args 0 end-2]
	    set expandedArgs [lrange $args end-1 end]

	    if {[llength $expandedArgs] != 2} {
		return [gedCmd $_cmd]
	    }

	    set old [lindex $expandedArgs 0]

	    # Check for the existence of old
#	    if {[catch {gedCmd attr show $old} adata]} {
#		return [eval gedCmd $_cmd $expandedArgs]
#	    }

	    set clist [lindex $expandedArgs 1]
	}
	"in" -
	"inside" {
	    if {[llength $args] == 0} {
		if {$_cmd == "in"} {
		    set prompt "Enter name of solid: "
		} else {
		    set prompt "Enter name of outside solid: "
		}

		set new_args [handleMoreArgs $prompt]
		while {[llength $new_args] == 0} {
		    set new_args [handleMoreArgs $prompt]
		}

		set args $new_args
	    }
	}
	"make" {
	    # Returns a help message.
	    if {[llength $args] < 2} {
		return [gedCmd $_cmd]
	    }

	    set options [lrange $args 0 end-2]
	    set expandedArgs [lrange $args end-1 end]

	    if {[llength $expandedArgs] != 2} {
		return [gedCmd $_cmd]
	    }

	    set clist [lindex $expandedArgs 0]
	}
	"comb" -
	"g" -
	"r" {
	    # Returns a help message.
	    if {[llength $args] < 2} {
		return [gedCmd $_cmd]
	    }

	    set options {}
	    set expandedArgs $args
	    set clist [lindex $expandedArgs 0]
	}
	default {
	    return "createWrapper: $_cmd not recognized."
	}
    }

    SetWaitCursor $this

    if {[catch {eval gedCmd $_cmd $options $expandedArgs} ret]} {
	SetNormalCursor $this
	return $ret
    }

    if {$_cmd == "clone"} {
	set clist [lindex $ret 1]
	set ret [lindex $ret 0]
    }

    # Checkpoint the created object
    checkpoint_olist $clist $LEDGER_CREATE

    refreshTree 1

    updateUndoState
    SetNormalCursor $this

    return $ret
}

::itcl::body Archer::gedWrapper {cmd eflag hflag sflag tflag args} {
    eval gedWrapper2 $cmd 0 -1 $eflag $hflag $sflag $tflag $args
}

::itcl::body Archer::gedWrapper2 {cmd oindex pindex eflag hflag sflag tflag args} {
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

    set obj [lindex $expandedArgs $oindex]
    if {$pindex >= 0} {
	set l [split $obj /]
	set len [llength $l]

	if {$pindex < $len} {
	    set obj [lindex $l $pindex]
	}
    }

    # Must be wanting help
    if {$obj == ""} {
	catch {eval gedCmd $cmd $options $expandedArgs} ret
	SetNormalCursor $this

	return $ret
    }

    if {$sflag} {
	checkpoint $obj $LEDGER_MODIFY
    }

    if {[catch {eval gedCmd $cmd $options $expandedArgs} ret]} {
	# This will remove the last ledger entry for obj
	if {$sflag} {
	    ledger_cleanup
	}

	SetNormalCursor $this
	error $ret
    }

    if {$sflag} {
	set l [$mLedger expand *_*_$obj]
	set l [lsort -dictionary $l]
	set le [lindex $l end]

	if {$le == ""} {
	    putString "No ledger entry found for $obj."
	} else {
	    # Assumed to have mods after the command invocation above
	    $mLedger attr set $le $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1

	    set mNeedSave 1
	    set mNeedGlobalUndo 1

	    if {$obj == $mSelectedObj} {
		# Checkpoint again in case the user starts interacting via the mouse
		checkpoint $obj $LEDGER_MODIFY
	    } else {
		updateUndoMode 0
	    }

	    updateSaveMode

	    # Possibly draw the updated object
	    set ditem ""
	    foreach item [gedCmd report 0] {
		regexp {/([^/]+$)} $item all item
		set l [split $item /]
		set i [lsearch -exact $l $obj]
		if {$i != -1} {
		    for {set j 1} {$j <= $i} {incr j} {
			if {$j == 1} {
			    append ditem [lindex $l $j]
			} else {
			    append ditem / [lindex $l $j]
			}
		    }
		    break
		}
	    }

	    if {$ditem != ""} {
		redrawObj $ditem
	    }
	}
    }

    gedCmd configure -primitiveLabels {}
    if {$tflag} {
	catch {refreshTree}
    }
    SetNormalCursor $this

    return $ret
}

::itcl::body Archer::globalWrapper {_cmd args} {
    # Simply return the current value
    if {$_cmd != "edcolor" && $args == {}} {
	return [gedCmd $_cmd]
    }

    if {$_cmd == "units" && [llength $args] == 1 && [lindex $args 0] == "-s"} {
	return [gedCmd units -s]
    }

    set old_units [gedCmd units -s]
    set lname [checkpoint _GLOBAL $LEDGER_MODIFY]

    if {[catch {eval gedCmd $_cmd $args} ret]} {
	return $ret
    }

    set new_units [gedCmd units -s]

    # Nothing else to do
    if {$_cmd == "units" && $old_units == $new_units} {
	ledger_cleanup
	return ""
    }

    if {$lname == ""} {
	return "No ledger entry found for _GLOBAL."
    } else {
	# Assumed to have mods after the command invocation above
	$mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
	$mLedger attr set $lname UNITS $old_units
    }

    set lname [checkpoint _GLOBAL $LEDGER_MODIFY]
    if {$lname == ""} {
	return "No ledger entry found for _GLOBAL."
    } else {
	$mLedger attr set $lname UNITS $new_units
    }

    set mNeedSave 1
    updateSaveMode

    gedCmd refresh
    return $ret
}

::itcl::body Archer::killWrapper {_cmd args} {
    # Returns a help message.
    if {[llength $args] == 0} {
	return [gedCmd $_cmd]
    }

    set optionsAndArgs [eval dbExpand $args]
    set options [lindex $optionsAndArgs 0]
    set expandedArgs [lindex $optionsAndArgs 1]

    # Returns a help message.
    if {[llength $expandedArgs] == 0} {
	return [gedCmd $_cmd]
    }

    SetWaitCursor $this

    # Get the list of killed and modified objects.
    if {[lsearch $options -a] != -1} {
	set alist [eval gedCmd $_cmd -a -n $expandedArgs]
    } else {
	set alist [eval gedCmd $_cmd -n $expandedArgs]
    }

    # The first sublist is for killed objects. The second is for modified objects.
    set klist [lindex $alist 0]
    set mlist [lindex $alist 1]

    # If an item is in both sublists, remove it from mlist.
    foreach item $klist {
	set l [lsearch -all $mlist $item]
	set l [lsort -decreasing $l]
	if {$l != -1} {
	    foreach i $l {
		# Delete the item (i.e. it no longer exists)
		set mlist [lreplace $mlist $i $i]
	    }
	}
    }

    set nindex [lsearch $options "-n"]
    if {$nindex == 0 || $nindex == 1} {
	SetNormalCursor $this
	return [list $klist $mlist]
    }

    if {[llength $klist] == 0} {
	SetNormalCursor $this
	return
    }

    # Remove duplicates from both klist and mlist
    set klist [lsort -unique $klist]
    set mlist [lsort -unique $mlist]

    # Need to checkpoint before they're gone
    checkpoint_olist $klist $LEDGER_DESTROY

    # Decrement the GID so that the modified
    # objects below have the same GID.
    incr mLedgerGID -1

    # Also need to checkpoint the objects that used to reference
    # the soon-to-be killed objects.
    set lnames [checkpoint_olist $mlist $LEDGER_MODIFY]

    if {[catch {eval gedCmd $_cmd $options $expandedArgs} ret]} {
	ledger_cleanup
	SetNormalCursor $this
	return $ret
    }

    foreach lname $lnames {
	$mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
    }

    refreshTree 1

    if {[lsearch $klist $mSelectedObj] != -1} {
	set mSelectedObj ""
	set mSelectedObjPath ""
	set mSelectedObjType ""
    } elseif {[lsearch $mlist $mSelectedObj] != -1} {
	checkpoint $mSelectedObj $LEDGER_MODIFY
    }

    updateUndoState
    SetNormalCursor $this

    return $ret
}

::itcl::body Archer::moveWrapper {_cmd args} {
    set alen [llength $args]

    # Returns a help message.
    if {$alen == 0} {
	return [gedCmd $_cmd]
    }

    if {$alen == 3} {
	# Must be using the -n option. If not, an error message
	# containing the usage string will be returned.
	return [eval gedCmd $_cmd $args]
    }

    # Get the list of potentially modified objects.
    if {$_cmd == "mvall"} {
	set mlist [eval gedCmd $_cmd -n $args]
    } else {
	set mlist {}
    }

    set mlen [llength $mlist]

    set old_name [lindex $args 0]
    set new_name [lindex $args 1]

    SetWaitCursor $this

    # Checkpoint the objects that used to reference
    # the soon-to-be renamed objects.
    if {$mlen} {
	set lnames [checkpoint_olist $mlist $LEDGER_MODIFY]
    } else {
	set lnames {}
    }

    if {[catch {eval gedCmd $_cmd $args} ret]} {
	ledger_cleanup
	SetNormalCursor $this
	return $ret
    }

    # Flag these as having mods
    foreach lname $lnames {
	$mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
    }

    # Decrement the GID so that the renamed
    # object below has the same GID as the
    # modified objects above.
    if {$mlen} {
	incr mLedgerGID -1
    }

    # Checkpoint the renamed object
    set lnew_name [checkpoint $new_name $LEDGER_RENAME]

    # Save the command for moving things back
    $mLedger attr set $lnew_name $LEDGER_ENTRY_MOVE_COMMAND "$_cmd $new_name $old_name"

    refreshTree 1

    if {$old_name == $mSelectedObj} {
	set mSelectedObj $new_name
	regsub {([^/]+)$} $mSelectedObjPath $new_name mSelectedObjPath
	initEdit 0
	checkpoint $mSelectedObj $LEDGER_MODIFY
    } elseif {[lsearch $mlist $mSelectedObj] != -1} {
	checkpoint $mSelectedObj $LEDGER_MODIFY
    }

    updateUndoState
    SetNormalCursor $this
}

::itcl::body Archer::initDefaultBindings {{_comp ""}} {
    if {$_comp == ""} {
	if {[info exists itk_component(ged)]} {
	    set _comp $itk_component(ged)
	} else {
	    return
	}
    }

    ArcherCore::initDefaultBindings $_comp

    foreach dname {ul ur ll lr} {
	set dm [$_comp component $dname]
	set win $dm

	if {$mViewOnly} {
	    bind $win <Control-ButtonPress-1> \
		"[::itcl::code $this launchDisplayMenuBegin $dname [$itk_component(canvas_menu) component view-menu] %X %Y]; break"
	    bind $win <3> \
		"[::itcl::code $this launchDisplayMenuBegin $dname [$itk_component(canvas_menu) component view-menu] %X %Y]; break"
	} else {
	    if {$ArcherCore::inheritFromToplevel} {
		bind $win <Control-ButtonPress-1> \
		    "[::itcl::code $this launchDisplayMenuBegin $dname $itk_component(displaymenu) %X %Y]; break"
		bind $win <3> \
		    "[::itcl::code $this launchDisplayMenuBegin $dname $itk_component(displaymenu) %X %Y]; break"
	    } else {
		bind $win <Control-ButtonPress-1> \
		    "[::itcl::code $this launchDisplayMenuBegin $dname [$itk_component(menubar) component display-menu] %X %Y]; break"
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

    if {!$mViewOnly} {
	if {$ArcherCore::inheritFromToplevel} {
	    $itk_component(filemenu) entryconfigure "Raytrace Control Panel..." -state normal

	    $itk_component(displaymenu) entryconfigure "Standard Views" -state normal
	    $itk_component(displaymenu) entryconfigure "Reset" -state normal
	    $itk_component(displaymenu) entryconfigure "Autoview" -state normal
	    $itk_component(displaymenu) entryconfigure "Center..." -state normal
	    $itk_component(displaymenu) entryconfigure "Clear" -state normal
	    $itk_component(displaymenu) entryconfigure "Refresh" -state normal

	    $itk_component(modesmenu) entryconfigure "Active Pane" -state normal
	    $itk_component(modesmenu) entryconfigure "Quad View" -state normal
	    $itk_component(modesmenu) entryconfigure "View Axes" -state normal
	    $itk_component(modesmenu) entryconfigure "Model Axes" -state normal
	    $itk_component(modesmenu) entryconfigure "Ground Plane" -state normal
	    $itk_component(modesmenu) entryconfigure "Primitive Labels" -state normal
	    $itk_component(modesmenu) entryconfigure "Viewing Parameters" -state normal
	    $itk_component(modesmenu) entryconfigure "Scale" -state normal
	    $itk_component(modesmenu) entryconfigure "Lighting" -state normal

	    $itk_component(raytracemenu) entryconfigure "rt" -state normal
	    $itk_component(raytracemenu) entryconfigure "rtcheck" -state normal
	    $itk_component(raytracemenu) entryconfigure "rtedge" -state normal
	    $itk_component(raytracemenu) entryconfigure "nirt" -state normal
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
	    $itk_component(menubar) menuconfigure .modes.scale -state normal
	    $itk_component(menubar) menuconfigure .modes.light -state normal

	    $itk_component(menubar) menuconfigure .raytrace.rt -state normal
	    $itk_component(menubar) menuconfigure .raytrace.rtcheck -state normal
	    $itk_component(menubar) menuconfigure .raytrace.rtedge -state normal
	    $itk_component(menubar) menuconfigure .raytrace.nirt -state normal
	}
    }
}

::itcl::body Archer::selectNode {tags {rflag 1}} {
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

    if {!$mViewOnly} {
	if {$mObjViewMode == $OBJ_ATTR_VIEW_MODE} {
	    initObjAttrView
	} else {
	    if {!$mRestoringTree} {
		selection_checkpoint $mSelectedObj
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


::itcl::body Archer::updateCheckpointMode {} {
    if {$mViewOnly} {
	return
    }

    if {!$mDbNoCopy && !$mDbReadOnly && $mNeedCheckpoint} {
	$itk_component(primaryToolbar) itemconfigure checkpoint \
	    -state normal
    } else {
	$itk_component(primaryToolbar) itemconfigure checkpoint \
	    -state disabled
    }
}

::itcl::body Archer::updateSaveMode {} {
    ArcherCore::updateSaveMode

    if {$mViewOnly} {
	return
    }

    catch {
	if {!$mDbNoCopy && !$mDbReadOnly && $mNeedSave} {
	    $itk_component(primaryToolbar) itemconfigure revert \
		-state normal

	    if {$ArcherCore::inheritFromToplevel} {
		$itk_component(filemenu) entryconfigure "Save" -state normal
		$itk_component(filemenu) entryconfigure "Revert" -state normal
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
		$itk_component(filemenu) entryconfigure "Save" -state disabled
		$itk_component(filemenu) entryconfigure "Revert" -state disabled
	    } else {
		$itk_component(menubar) menuconfigure .file.save \
		    -state disabled
		$itk_component(menubar) menuconfigure .file.revert \
		    -state disabled
	    }
	}
    }
}

::itcl::body Archer::updateUndoMode {{_oflag 1}} {
    if {$mViewOnly} {
	return
    }

    if {!$mDbNoCopy && !$mDbReadOnly && $mNeedGlobalUndo} {
	$itk_component(primaryToolbar) itemconfigure global_undo \
	    -state normal

	if {$_oflag} {
	    if {$mNeedObjUndo} {
		$itk_component(primaryToolbar) itemconfigure object_undo \
		    -state normal
	    } else {
		$itk_component(primaryToolbar) itemconfigure object_undo \
		    -state disabled
	    }
	}
    } else {
	$itk_component(primaryToolbar) itemconfigure global_undo \
	    -state disabled

	if {$_oflag} {
	    $itk_component(primaryToolbar) itemconfigure object_undo \
		-state disabled
	}
    }
}

::itcl::body Archer::updateUndoState {} {
    if {$mViewOnly} {
	return
    }

    set l [$mLedger ls -A $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1]
    set len [llength $l]
    if {$len == 0} {
	set mNeedSave 0
	set mNeedGlobalUndo 0
	set mNeedObjUndo 0
    } else {
	set mNeedSave 1
	set mNeedGlobalUndo 1

	# Get all ledger entries related to mSelectedObj
	set l [$mLedger expand *_*_$mSelectedObj]
	set len [llength $l]

	if {$len} {
	    set l [lsort -dictionary $l]
	    set le [lindex $l end]

	    if {$len > 1} {
		set mNeedObjUndo 1
	    } else {
		if {[$mLedger attr get $le $LEDGER_ENTRY_OUT_OF_SYNC_ATTR]} {
		    set mNeedObjUndo 1
		} else {
		    set mNeedObjUndo 0
		}
	    }
	} else {
	    set mNeedObjUndo 0
	}
    }

    updateUndoMode 1
    updateSaveMode
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
    set aboutImg [image create photo -file [file join $brlcadDataPath tclscripts archer images aboutArcher.png]]
    itk_component add aboutInfo {
	::ttk::label $itk_component(aboutDialogTabs).aboutInfo \
	    -image $aboutImg
    } {}

    # BRL-CAD License Info
    set fd [open [file join $brlcadDataPath COPYING] r]
    set mBrlcadLicenseInfo [read $fd]
    close $fd
    itk_component add brlcadLicenseInfo {
	::iwidgets::scrolledtext $itk_component(aboutDialogTabs).brlcadLicenseInfo \
	    -wrap word \
	    -hscrollmode dynamic \
	    -vscrollmode dynamic \
	    -textfont $mFontText \
	    -background $SystemButtonFace \
	    -textbackground $SystemButtonFace
    } {}
    $itk_component(brlcadLicenseInfo) insert 0.0 $mBrlcadLicenseInfo
    $itk_component(brlcadLicenseInfo) configure -state disabled

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

    $itk_component(aboutDialogTabs) add $itk_component(aboutInfo) -text "About" -stick ns
    $itk_component(aboutDialogTabs) add $itk_component(brlcadLicenseInfo) -text "License"
    $itk_component(aboutDialogTabs) add $itk_component(ackInfo) -text "Acknowledgements"

    # Version Info
    itk_component add versionInfo {
	::ttk::label $parent.versionInfo \
	    -text "Version: $mArcherVersion" \
	    -padding 0 \
	    -font $mFontText \
	    -anchor se
    } {}

    $itk_component(aboutDialog) configure -background $LABEL_BACKGROUND_COLOR

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
	::ttk::label $parent.redl \
	    -anchor e \
	    -text "Red:"
    } {}

    itk_component add backgroundRedE {
	::ttk::entry $parent.rede \
	    -width 3 \
	    -background $SystemWindow \
	    -textvariable [::itcl::scope mBackgroundRedPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateColorComp %P]
    } {}
    itk_component add backgroundGreenL {
	::ttk::label $parent.greenl \
	    -anchor e \
	    -text "Green:"
    } {}
    itk_component add backgroundGreenE {
	::ttk::entry $parent.greene \
	    -width 3 \
	    -background $SystemWindow \
	    -textvariable [::itcl::scope mBackgroundGreenPref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateColorComp %P]
    } {}
    itk_component add backgroundBlueL {
	::ttk::label $parent.bluel \
	    -anchor e \
	    -text "Blue:"
    } {}
    itk_component add backgroundBlueE {
	::ttk::entry $parent.bluee \
	    -width 3 \
	    -background $SystemWindow \
	    -textvariable [::itcl::scope mBackgroundBluePref] \
	    -validate key \
	    -validatecommand [::itcl::code $this validateColorComp %P]
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
    set oglParent $itk_component(preferenceTabs)
    itk_component add displayF {
	::ttk::frame $oglParent.displayF
    } {}

    set parent $itk_component(displayF)

    itk_component add zclipL {
	::ttk::label $parent.zclipL \
	    -text "Viewing Cube:"
    } {}

    set i 0
    set mZClipMode $i
    itk_component add smallZClipRB {
	::ttk::radiobutton $parent.smallZClipRB \
	    -text "Small (Default)" \
	    -value $i \
	    -variable [::itcl::scope mZClipModePref]
    } {}

    incr i
    itk_component add mediumZClipRB {
	::ttk::radiobutton $parent.mediumZClipRB \
	    -text "Medium" \
	    -value $i \
	    -variable [::itcl::scope mZClipModePref]
    } {}

    incr i
    itk_component add largeZClipRB {
	::ttk::radiobutton $parent.largeZClipRB \
	    -text "Large" \
	    -value $i \
	    -variable [::itcl::scope mZClipModePref]
    } {}

    incr i
    itk_component add infiniteZClipRB {
	::ttk::radiobutton $parent.infiniteZClipRB \
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

    $itk_component(preferenceTabs) add $itk_component(displayF) -text "Display"
}


::itcl::body Archer::buildGeneralPreferences {} {
    set parent $itk_component(preferenceTabs)
    itk_component add generalF {
	::ttk::frame $parent.generalF
    } {}

    itk_component add bindingL {
	::ttk::label $itk_component(generalF).bindingL \
	    -text "Display Bindings:"
    } {}

    set i 0
    set mBindingMode $i
    itk_component add defaultBindingRB {
	::ttk::radiobutton $itk_component(generalF).defaultBindingRB \
	    -text "Default" \
	    -value $i \
	    -variable [::itcl::scope mBindingModePref]
    } {}

    incr i
    itk_component add brlcadBindingRB {
	::ttk::radiobutton $itk_component(generalF).brlcadBindingRB \
	    -text "BRL-CAD" \
	    -value $i \
	    -variable [::itcl::scope mBindingModePref]
    } {}

    itk_component add measuringL {
	::ttk::label $itk_component(generalF).measuringL \
	    -text "Measuring Stick Mode:"
    } {}

    set i 0
    set mMeasuringStickMode $i
    itk_component add topMeasuringStickRB {
	::ttk::radiobutton $itk_component(generalF).topMeasuringStickRB \
	    -text "Default (Top)" \
	    -value $i \
	    -variable [::itcl::scope mMeasuringStickModePref]
    } {}

    incr i
    itk_component add embeddedMeasuringStickRB {
	::ttk::radiobutton $itk_component(generalF).embeddedMeasuringStickRB \
	    -text "Embedded" \
	    -value $i \
	    -variable [::itcl::scope mMeasuringStickModePref]
    } {}

    itk_component add backgroundColorL {
	::ttk::label $itk_component(generalF).backgroundColorL \
	    -text "Background Color:"
    } {}
    itk_component add backgroundColorF {
	::ttk::frame $itk_component(generalF).backgroundColorF
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
	::ttk::checkbutton $itk_component(generalF).bigECB \
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

    $itk_component(preferenceTabs) add $itk_component(generalF) -text "General"
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

    set i 0
    grid $itk_component(groundPlaneF) -column 0 -row $i -sticky nw

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1

    $itk_component(preferenceTabs) add $itk_component(groundPlaneF) -text "Ground Plane"
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

    set i 0
    grid $itk_component(modelAxesF) -column 0 -row $i -sticky nw

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1

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
	::ttk::notebook $parent.tabs
    } {}

    buildGeneralPreferences
    buildModelAxesPreferences
    buildViewAxesPreferences
    buildGroundPlanePreferences
    buildDisplayPreferences

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

::itcl::body Archer::buildToplevelMenubar {} {
    itk_component add menubar {
	::menu $itk_interior.menubar \
	    -tearoff 0
    } {
	keep -background
    }

    # File
    itk_component add filemenu {
	::menu $itk_component(menubar).filemenu \
	    -tearoff 0
    } {
	keep -background
    }

    # Populate File menu
    $itk_component(filemenu) add command \
	-label "New..." \
	-command [::itcl::code $this newDb]
    $itk_component(filemenu) add command \
	-label "Open..." \
	-command [::itcl::code $this openDb]
    $itk_component(filemenu) add command \
	-label "Save" \
	-command [::itcl::code $this askToSave] \
	-state disabled
    $itk_component(filemenu) add command \
	-label "Revert" \
	-command [::itcl::code $this askToRevert] \
	-state disabled
    $itk_component(filemenu) add separator
    $itk_component(filemenu) add command \
	-label "Raytrace Control Panel..." \
	-command [::itcl::code $this raytracePanel] \
	-state disabled
    $itk_component(filemenu) add command \
	-label "Preferences..." \
	-command [::itcl::code $this doPreferences]
    $itk_component(filemenu) add separator
    $itk_component(filemenu) add command \
	-label "Quit" \
	-command [::itcl::code $this Close]

    itk_component add displaymenu {
	::menu $itk_component(menubar).displaymenu \
	    -tearoff 0
    } {
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

    itk_component add backgroundmenu {
	::menu $itk_component(displaymenu).backgroundmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(backgroundmenu) add command \
	-label "Black" \
	-command [::itcl::code $this backgroundColor 0 0 0]
    $itk_component(backgroundmenu) add command \
	-label "Grey" \
	-command [::itcl::code $this backgroundColor 100 100 100]
    $itk_component(backgroundmenu) add command \
	-label "White" \
	-command [::itcl::code $this backgroundColor 255 255 255]
    $itk_component(backgroundmenu) add command \
	-label "Cyan" \
	-command [::itcl::code $this backgroundColor 0 200 200]
    $itk_component(backgroundmenu) add command \
	-label "Blue" \
	-command [::itcl::code $this backgroundColor 0 0 160]
    $itk_component(displaymenu) add cascade \
	-label "Background Color" \
	-menu $itk_component(backgroundmenu) \
	-state normal

    itk_component add stdviewsmenu {
	::menu $itk_component(displaymenu).stdviewsmenu \
	    -tearoff 0
    } {
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
	-command [::itcl::code $this zap] \
	-state disabled
    $itk_component(displaymenu) add command \
	-label "Refresh" \
	-command [::itcl::code $this refreshDisplay] \
	-state disabled

    buildModesMenu
    updateUtilityMenu

    itk_component add raytracemenu {
	::menu $itk_component(menubar).raytracemenu \
	    -tearoff 0
    } {
	keep -background
    }

    itk_component add rtmenu {
	::menu $itk_component(raytracemenu).rtmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(rtmenu) add command \
	-label "512x512" \
	-command [::itcl::code $this launchRtApp rt 512]
    $itk_component(rtmenu) add command \
	-label "1024x1024" \
	-command [::itcl::code $this launchRtApp rt 1024]
    $itk_component(rtmenu) add command \
	-label "Window Size" \
	-command [::itcl::code $this launchRtApp rt window]

    itk_component add rtcheckmenu {
	::menu $itk_component(raytracemenu).rtcheckmenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(rtcheckmenu) add command \
	-label "50x50" \
	-command [::itcl::code $this launchRtApp rtcheck 50]
    $itk_component(rtcheckmenu) add command \
	-label "100x100" \
	-command [::itcl::code $this launchRtApp rtcheck 100]
    $itk_component(rtcheckmenu) add command \
	-label "256x256" \
	-command [::itcl::code $this launchRtApp rtcheck 256]
    $itk_component(rtcheckmenu) add command \
	-label "512x512" \
	-command [::itcl::code $this launchRtApp rtcheck 512]

    itk_component add rtedgemenu {
	::menu $itk_component(raytracemenu).rtedgemenu \
	    -tearoff 0
    } {
	keep -background
    }
    $itk_component(rtedgemenu) add command \
	-label "512x512" \
	-command [::itcl::code $this launchRtApp rtedge 512]
    $itk_component(rtedgemenu) add command \
	-label "1024x1024" \
	-command [::itcl::code $this launchRtApp rtedge 1024]
    $itk_component(rtedgemenu) add command \
	-label "Window Size" \
	-command [::itcl::code $this launchRtApp rtedge window]

    $itk_component(raytracemenu) add cascade \
	-label "rt" \
	-menu $itk_component(rtmenu) \
	-state disabled
    $itk_component(raytracemenu) add cascade \
	-label "rtcheck" \
	-menu $itk_component(rtcheckmenu) \
	-state disabled
    $itk_component(raytracemenu) add cascade \
	-label "rtedge" \
	-menu $itk_component(rtedgemenu) \
	-state disabled
    $itk_component(raytracemenu) add separator
    $itk_component(raytracemenu) add command \
	-label "nirt" \
	-command [::itcl::code $this launchNirt] \
	-state disabled

    itk_component add helpmenu {
	::menu $itk_component(menubar).helpmenu \
	    -tearoff 0
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
	-label "Raytrace" \
	-menu $itk_component(raytracemenu) \

    $itk_component(menubar) add cascade \
	-label "Help" \
	-menu $itk_component(helpmenu)

    # set up bindings for status
    bind $itk_component(filemenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(displaymenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(backgroundmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(stdviewsmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(modesmenu) <<MenuSelect>> [::itcl::code $this modesMenuStatusCB %W]
    bind $itk_component(activepanemenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(helpmenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]

    bind $itk_component(raytracemenu) <<MenuSelect>> [::itcl::code $this menuStatusCB %W]
    bind $itk_component(rtmenu) <<MenuSelect>> [::itcl::code $this rtMenuStatusCB %W]
    bind $itk_component(rtcheckmenu) <<MenuSelect>> [::itcl::code $this rtCheckMenuStatusCB %W]
    bind $itk_component(rtedgemenu) <<MenuSelect>> [::itcl::code $this rtEdgeMenuStatusCB %W]
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

    set i 0
    grid $itk_component(viewAxesF) -column 0 -row $i -sticky nw

    grid rowconfigure $parent 0 -weight 1
    grid columnconfigure $parent 0 -weight 1

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


::itcl::body Archer::doArcherHelp {} {
    global tcl_platform

    if {$tcl_platform(platform) == "windows"} {
	exec hh [file join $brlcadDataPath html manuals archer Archer_Documentation.chm] &
    } else {
	tk_messageBox -title "" -message "Not yet implemented!"
    }
}


::itcl::body Archer::launchDisplayMenuBegin {_dm _m _x _y} {
    set mCurrentPaneName $_dm
    tk_popup $_m $_x $_y
    after idle [::itcl::code $this launchDisplayMenuEnd]
}

::itcl::body Archer::launchDisplayMenuEnd {} {
#    set mCurrentPaneName ""
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

    $itk_component(primaryToolbar) insert rotate button arb6 \
	-balloonstr "Create an arb6" \
	-helpstr "Create an arb6" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj arb6]

    $itk_component(primaryToolbar) insert rotate button arb8 \
	-balloonstr "Create an arb8" \
	-helpstr "Create an arb8" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj arb8]

    $itk_component(primaryToolbar) insert rotate button cone \
	-balloonstr "Create a tgc" \
	-helpstr "Create a tgc" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj tgc]

    $itk_component(primaryToolbar) insert rotate button sphere \
	-balloonstr "Create a sphere" \
	-helpstr "Create a sphere" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this createObj sph]

    $itk_component(primaryToolbar) insert rotate button torus \
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
    itk_component add arbMenu {
	::menu $itk_component(primitiveMenu).arbmenu \
	    -tearoff 0
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
		    -file [file join $mImgDir Themes $mTheme edit_rotate.png]]
    $itk_component(primaryToolbar) add radiobutton edit_translate \
	-balloonstr "Translate selected object" \
	-helpstr "Translate selected object" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $OBJECT_TRANSLATE_MODE \
	-command [::itcl::code $this beginObjTranslate] \
	-image [image create photo \
		    -file [file join $mImgDir Themes $mTheme edit_translate.png]]
    $itk_component(primaryToolbar) add radiobutton edit_scale \
	-balloonstr "Scale selected object" \
	-helpstr "Scale selected object" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $OBJECT_SCALE_MODE \
	-command [::itcl::code $this beginObjScale] \
	-image [image create photo \
		    -file [file join $mImgDir Themes $mTheme edit_scale.png]]
    $itk_component(primaryToolbar) add radiobutton edit_center \
	-balloonstr "Center selected object" \
	-helpstr "Center selected object" \
	-variable [::itcl::scope mDefaultBindingMode] \
	-value $OBJECT_CENTER_MODE \
	-command [::itcl::code $this beginObjCenter] \
	-image [image create photo \
		    -file [file join $mImgDir Themes $mTheme edit_select.png]]

    $itk_component(primaryToolbar) itemconfigure edit_rotate -state disabled
    $itk_component(primaryToolbar) itemconfigure edit_translate -state disabled
    $itk_component(primaryToolbar) itemconfigure edit_scale -state disabled
    $itk_component(primaryToolbar) itemconfigure edit_center -state disabled
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
		    -helpstr "Set display background to black"
		command grey -label "Grey" \
		    -helpstr "Set display background to grey"
		command white -label "White" \
		    -helpstr "Set display background to white"
		command cyan -label "Cyan" \
		    -helpstr "Set display background to cyan"
		command blue -label "Blue" \
		    -helpstr "Set display background to blue"
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
		command 35,25 -label "35,25" \
		    -helpstr "Set view to az=35, el=25"
		command 45,45 -label "45,45" \
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
	-command [::itcl::code $this backgroundColor 0 0 0]
    $itk_component(menubar) menuconfigure .display.background.grey \
	-command [::itcl::code $this backgroundColor 100 100 100]
    $itk_component(menubar) menuconfigure .display.background.white \
	-command [::itcl::code $this backgroundColor 255 255 255]
    $itk_component(menubar) menuconfigure .display.background.cyan \
	-command [::itcl::code $this backgroundColor 0 200 200]
    $itk_component(menubar) menuconfigure .display.background.blue \
	-command [::itcl::code $this backgroundColor 0 0 160]
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
	-command [::itcl::code $this doArcherHelp]
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

::itcl::body Archer::buildModesMenu {} {
    itk_component add modesmenu {
	menu $itk_component(menubar).modesmenu \
	    -tearoff 0
    } {
	keep -background
    }

    itk_component add activepanemenu {
	menu $itk_component(modesmenu).activepanemenu \
	    -tearoff 0
    } {
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

    $itk_component(modesmenu) add cascade \
	-label "Active Pane" \
	-menu $itk_component(activepanemenu) \
	-state disabled
    $itk_component(modesmenu) add checkbutton \
	-label "Quad View" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mMultiPane] \
	-command [::itcl::code $this doMultiPane] \
	-state disabled
    $itk_component(modesmenu) add separator
    $itk_component(modesmenu) add checkbutton \
	-label "View Axes" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowViewAxes] \
	-command [::itcl::code $this showViewAxes] \
	-state disabled
    $itk_component(modesmenu) add checkbutton \
	-label "Model Axes" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowModelAxes] \
	-command [::itcl::code $this showModelAxes] \
	-state disabled
    $itk_component(modesmenu) add checkbutton \
	-label "Ground Plane" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowGroundPlane] \
	-command [::itcl::code $this showGroundPlane] \
	-state disabled
    $itk_component(modesmenu) add checkbutton \
	-label "Primitive Labels" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowPrimitiveLabels] \
	-command [::itcl::code $this showPrimitiveLabels] \
	-state disabled
    $itk_component(modesmenu) add checkbutton \
	-label "Viewing Parameters" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowViewingParams] \
	-command [::itcl::code $this showViewParams] \
	-state disabled
    $itk_component(modesmenu) add checkbutton \
	-label "Scale" \
	-offvalue 0 \
	-onvalue 1 \
	-variable [::itcl::scope mShowScale] \
	-command [::itcl::code $this showScale] \
	-state disabled
    $itk_component(modesmenu) add checkbutton \
	-label "Lighting" \
	-offvalue 0 \
	-onvalue 2 \
	-variable [::itcl::scope mLighting] \
	-command [::itcl::code $this doLighting] \
	-state disabled
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
	$mBindingMode == 0} {
	$itk_component(primaryToolbar) itemconfigure edit_rotate -state normal
	$itk_component(primaryToolbar) itemconfigure edit_translate -state normal
	$itk_component(primaryToolbar) itemconfigure edit_scale -state normal
	$itk_component(primaryToolbar) itemconfigure edit_center -state normal
    }

    $itk_component(hpane) show bottomView
    $itk_component(hpane) fraction $mHPaneFraction1 $mHPaneFraction2
    $itk_component(vpane) show attrView

    set toggle3 $mVPaneToggle3
    set toggle5 $mVPaneToggle5
    set mVPaneToggle3 $toggle3
    set mVPaneToggle5 $toggle5

    # How screwed up is this?
    $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
    update
    after idle $itk_component(vpane) fraction $mVPaneFraction3 $mVPaneFraction4 $mVPaneFraction5
}


################################### Object Edit Section ###################################


::itcl::body Archer::initEdit {{_initEditMode 1}} {
    if {![info exists itk_component(ged)]} {
	return
    }

    set mSelectedObjType [gedCmd get_type $mSelectedObj]

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
    if {![info exists itk_component(ged)]} {
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
	return
    }

    if {$GeometryEditFrame::mEditClass != $GeometryEditFrame::EDIT_CLASS_ROT} {
	initEdit
    }

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
}

::itcl::body Archer::beginObjScale {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
	return
    }

    if {$GeometryEditFrame::mEditClass != $GeometryEditFrame::EDIT_CLASS_SCALE} {
	initEdit
    }

    foreach dname {ul ur ll lr} {
	set win [$itk_component(ged) component $dname]

	if {$GeometryEditFrame::mEditCommand != ""} {
	    bind $win <1> "$itk_component(ged) pane_$GeometryEditFrame::mEditCommand\_mode $dname $obj $GeometryEditFrame::mEditParam1 %x %y; break"
	} else {
	    bind $win <1> "$itk_component(ged) pane_oscale_mode $dname $obj %x %y; break"
	}

	bind $win <ButtonRelease-1> "[::itcl::code $this endObjScale $dname $obj]; break"
    }
}

::itcl::body Archer::beginObjTranslate {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
	return
    }

    if {$GeometryEditFrame::mEditClass != $GeometryEditFrame::EDIT_CLASS_TRANS} {
	initEdit
    }

    set ::GeometryEditFrame::mEditLastTransMode $OBJECT_TRANSLATE_MODE

    foreach dname {ul ur ll lr} {
	set win [$itk_component(ged) component $dname]

	if {$GeometryEditFrame::mEditCommand != ""} {
	    bind $win <1> "$itk_component(ged) pane_$GeometryEditFrame::mEditCommand\_mode $dname $obj $GeometryEditFrame::mEditParam1 %x %y; break"
	} else {
	    bind $win <1> "$itk_component(ged) pane_otranslate_mode $dname $obj %x %y; break"
	}

	bind $win <ButtonRelease-1> "[::itcl::code $this endObjTranslate $dname $obj]; break"
    }
}

::itcl::body Archer::beginObjCenter {} {
    if {![info exists itk_component(ged)]} {
	return
    }

    set obj $mSelectedObjPath

    if {$obj == ""} {
	set mDefaultBindingMode $ROTATE_MODE
	beginViewRotate
	return
    }

    if {$GeometryEditFrame::mEditClass != $GeometryEditFrame::EDIT_CLASS_TRANS} {
	initEdit
    }

    set ::GeometryEditFrame::mEditLastTransMode $OBJECT_CENTER_MODE

    foreach dname {ul ur ll lr} {
	set win [$itk_component(ged) component $dname]
	bind $win <1> "[::itcl::code $this handleObjCenter $obj %x %y]; break"
	bind $win <ButtonRelease-1> "[::itcl::code $this endObjCenter $dname $obj]; break"
    }
}

::itcl::body Archer::endObjCenter {dname obj} {
    if {![info exists itk_component(ged)]} {
	return
    }

    updateObjSave
    initEdit 0

    set center [$itk_component(ged) ocenter $obj]
    addHistory "ocenter $center"
}

::itcl::body Archer::endObjRotate {dname obj} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) pane_idle_mode $dname
    updateObjSave
    initEdit 0

    #XXX Need code to track overall transformation
    if {[info exists itk_component(ged)]} {
	#addHistory "orotate obj rx ry rz"
    }
}

::itcl::body Archer::endObjScale {dname obj} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) pane_idle_mode $dname
    updateObjSave
    initEdit 0

    #XXX Need code to track overall transformation
    if {[info exists itk_component(ged)]} {
	#addHistory "oscale obj sf"
    }
}

::itcl::body Archer::endObjTranslate {dname obj} {
    if {![info exists itk_component(ged)]} {
	return
    }

    $itk_component(ged) pane_idle_mode $dname
    updateObjSave
    initEdit 0

    #XXX Need code to track overall transformation
    #addHistory "otranslate obj dx dy dz"
}

::itcl::body Archer::handleObjCenter {obj x y} {
    set ocenter [gedCmd ocenter $obj]
    set ocenter [vscale $ocenter [gedCmd local2base]]
    set ovcenter [eval gedCmd m2v_point $ocenter]

    # This is the updated view center (i.e. we keep the original view Z)
    set vcenter [gedCmd screen2view $x $y]
    set vcenter [list [lindex $vcenter 0] [lindex $vcenter 1] [lindex $ovcenter 2]]

    set ocenter [vscale [eval gedCmd v2m_point $vcenter] [gedCmd base2local]]

    if {$GeometryEditFrame::mEditCommand != ""} {
	gedCmd $GeometryEditFrame::mEditCommand $obj $GeometryEditFrame::mEditParam1 $ocenter
    } else {
	eval gedCmd ocenter $obj $ocenter
    }

    redrawObj $obj 0
    initEdit 0
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

#     set parent $itk_component(objEditView)
#     itk_component add pipeView {
# 	PipeEditFrame $parent.pipeview \
# 	    -units "mm"
#     } {}
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
	-geometryChangedCallback [::itcl::code $this updateObjEditView] \
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
    if {![info exists itk_component(ged)]} {
	return
    }

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
    if {![info exists itk_component(ged)]} {
	return
    }

    if {$mSelectedObj == ""} {
	return
    }

    set mPrevObjViewMode $mObjViewMode

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
    if {![info exists itk_component(ged)]} {
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

    set mPrevObjViewMode $mObjViewMode

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

	if {[info exists itk_component(ged)]} {
	    pack $itk_component(objViewToolbar) -expand no -fill x -anchor n
	    pack $itk_component(objEditView) -expand yes -fill both -anchor n
	}
    } else {
	if {[pluginQuery $mWizardClass] == -1} {
	    # the wizard plugin has not been loaded
	    initObjWizard $mSelectedObj 0
	} else {
	    initObjWizard $mSelectedObj 1
	}
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
    #XXX Not ready yet
    return

    $itk_component(pipeView) configure \
	-geometryObject $mSelectedObj \
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
    updateObjSave
    redrawObj $mSelectedObjPath
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

    refreshTree
    selectNode [$itk_component(tree) find $obj]
}

::itcl::body Archer::pluginGetMinAllowableRid {} {
    if {![info exists itk_component(ged)]} {
	return 0
    }

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
    gedCmd configure -measuringStickColor $mMeasuringStickColor
    gedCmd configure -measuringStickMode $mMeasuringStickMode
    gedCmd configure -primitiveLabelColor $mPrimitiveLabelColor
    gedCmd configure -scaleColor $mScaleColor
    gedCmd configure -viewingParamsColor $mViewingParamsColor
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

	if {[info exists itk_component(ged)]} {
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
    }

    set X [lindex $mModelAxesPosition 0]
    set Y [lindex $mModelAxesPosition 0]
    set Z [lindex $mModelAxesPosition 0]
    if {$mModelAxesPositionXPref != $X ||
	$mModelAxesPositionYPref != $Y ||
	$mModelAxesPositionZPref != $Z} {
	set mModelAxesPosition \
	    "$mModelAxesPositionXPref $mModelAxesPositionYPref $mModelAxesPositionZPref"

	if {[info exists itk_component(ged)]} {
	    gedCmd configure -modelAxesPosition $mModelAxesPosition
	}
    }

    if {$mModelAxesLineWidthPref != $mModelAxesLineWidth} {
	set mModelAxesLineWidth $mModelAxesLineWidthPref

	if {[info exists itk_component(ged)]} {
	    gedCmd configure -modelAxesLineWidth $mModelAxesLineWidth
	}
    }

    if {$mModelAxesColorPref != $mModelAxesColor} {
	set mModelAxesColor $mModelAxesColorPref
    }

    if {$mModelAxesLabelColorPref != $mModelAxesLabelColor} {
	set mModelAxesLabelColor $mModelAxesLabelColorPref
    }

    if {$mModelAxesTickIntervalPref != $mModelAxesTickInterval} {
	set mModelAxesTickInterval $mModelAxesTickIntervalPref

	if {[info exists itk_component(ged)]} {
	    gedCmd configure -modelAxesTickInterval $mModelAxesTickInterval
	}
    }

    if {$mModelAxesTicksPerMajorPref != $mModelAxesTicksPerMajor} {
	set mModelAxesTicksPerMajor $mModelAxesTicksPerMajorPref

	if {[info exists itk_component(ged)]} {
	    gedCmd configure -modelAxesTicksPerMajor $mModelAxesTicksPerMajor
	}
    }

    if {$mModelAxesTickThresholdPref != $mModelAxesTickThreshold} {
	set mModelAxesTickThreshold $mModelAxesTickThresholdPref

	if {[info exists itk_component(ged)]} {
	    gedCmd configure -modelAxesTickThreshold $mModelAxesTickThreshold
	}
    }

    if {$mModelAxesTickLengthPref != $mModelAxesTickLength} {
	set mModelAxesTickLength $mModelAxesTickLengthPref

	if {[info exists itk_component(ged)]} {
	    gedCmd configure -modelAxesTickLength $mModelAxesTickLength
	}
    }

    if {$mModelAxesTickMajorLengthPref != $mModelAxesTickMajorLength} {
	set mModelAxesTickMajorLength $mModelAxesTickMajorLengthPref

	if {[info exists itk_component(ged)]} {
	    gedCmd configure -modelAxesTickMajorLength $mModelAxesTickMajorLength
	}
    }

    if {$mModelAxesTickColorPref != $mModelAxesTickColor} {
	set mModelAxesTickColor $mModelAxesTickColorPref
    }

    if {$mModelAxesTickMajorColorPref != $mModelAxesTickMajorColor} {
	set mModelAxesTickMajorColor $mModelAxesTickMajorColorPref
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
    $itk_component(ged) refresh_off

    applyGeneralPreferencesIfDiff
    applyViewAxesPreferencesIfDiff
    applyModelAxesPreferencesIfDiff
    applyGroundPlanePreferencesIfDiff
    applyDisplayPreferencesIfDiff

    ::update
    $itk_component(ged) refresh_on
    $itk_component(ged) refresh
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

	if {[info exists itk_component(ged)]} {
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
	}

	set positionNotSet 0
	set mViewAxesPosition $mViewAxesPositionPref

	if {[info exists itk_component(ged)]} {
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
    }

    if {$positionNotSet &&
	$mViewAxesPositionPref != $mViewAxesPosition} {
	set mViewAxesPosition $mViewAxesPositionPref

	if {[info exists itk_component(ged)]} {
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
    }

    if {$mViewAxesLineWidthPref != $mViewAxesLineWidth} {
	set mViewAxesLineWidth $mViewAxesLineWidthPref

	if {[info exists itk_component(ged)]} {
	    gedCmd configure -viewAxesLineWidth $mViewAxesLineWidth
	}
    }

    if {$mViewAxesColorPref != $mViewAxesColor} {
	set mViewAxesColor $mViewAxesColorPref
    }

    if {$mViewAxesLabelColorPref != $mViewAxesLabelColor} {
	set mViewAxesLabelColor $mViewAxesLabelColorPref
    }
}

::itcl::body Archer::doPreferences {} {
    # update preference variables
    set mZClipModePref $mZClipMode

    set mBackgroundRedPref [lindex $mBackground 0]
    set mBackgroundGreenPref [lindex $mBackground 1]
    set mBackgroundBluePref [lindex $mBackground 2]
    set mBindingModePref $mBindingMode
    set mEnableBigEPref $mEnableBigE
    set mMeasuringStickColorPref $mMeasuringStickColor
    set mMeasuringStickModePref $mMeasuringStickMode
    set mPrimitiveLabelColorPref $mPrimitiveLabelColor
    set mScaleColorPref $mScaleColor
    set mViewingParamsColorPref $mViewingParamsColor
    set mThemePref $mTheme

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
    ::update
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

    readPreferencesInit

    # Read in the preferences file.
    if {![catch {open [file join $home $mPrefFile] r} pfile]} {
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

    update
    initMode
    updateTheme
    updateToggleMode
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
    puts $_pfile "set mBackground \"$mBackground\""
    puts $_pfile "set mBindingMode $mBindingMode"
    puts $_pfile "set mEnableBigE $mEnableBigE"
    puts $_pfile "set mMeasuringStickColor \"$mMeasuringStickColor\""
    puts $_pfile "set mMeasuringStickMode $mMeasuringStickMode"
    puts $_pfile "set mPrimitiveLabelColor \"$mPrimitiveLabelColor\""
    puts $_pfile "set mScaleColor \"$mScaleColor\""
    puts $_pfile "set mViewingParamsColor \"$mViewingParamsColor\""
    puts $_pfile "set mTheme \"$mTheme\""

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
    puts $_pfile "set mZClipMode $mZClipMode"

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
	"bot" {
	    #XXX Not ready yet
	    return

	    set name [gedCmd make_name "bot."]
	    createBot $name
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
	"epa" {
	    set name [gedCmd make_name "epa."]
	    createEpa $name
	}
	"eto" {
	    set name [gedCmd make_name "eto."]
	    createEto $name
	}
	"extrude" {
	    #XXX Not ready yet
	    return

	    set name [gedCmd make_name "extrude."]
	    createExtrude $name
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
	"part" {
	    set name [gedCmd make_name "part."]
	    createPart $name
	}
	"pipe" {
	    #XXX Not ready yet
	    return

	    set name [gedCmd make_name "pipe."]
	    createPipe $name
	}
	"rhc" {
	    set name [gedCmd make_name "rhc."]
	    createRhc $name
	}
	"rpc" {
	    set name [gedCmd make_name "rpc."]
	    createRpc $name
	}
	"sketch" {
	    set name [gedCmd make_name "sketch."]
	    createSketch $name
	}
	"sph" {
	    set name [gedCmd make_name "sph."]
	    createSphere $name
	}
	"superell" {
	    set name [gedCmd make_name "ell."]
	    createSuperell $name
	}
	"tgc" {
	    set name [gedCmd make_name "tgc."]
	    createTgc $name
	}
	"tor" {
	    set name [gedCmd make_name "tor."]
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

    gedCmd configure -autoViewEnable 0
    dblClick [$itk_component(tree) selection get]
    gedCmd configure -autoViewEnable 1

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
    #XXX Not ready yet
    return

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

::itcl::body Archer::checkpoint {_obj _type} {
    if {$_obj == "" || $mLedger == ""} {
	return
    }

    # Check for the existence of _obj
    if {[catch {gedCmd attr show $_obj} adata]} {
	return
    }

    # Get all ledger entries related to _obj
    set l [$mLedger expand *_*_$_obj]
    set len [llength $l]

    # Find next oid (object ID - a counter for object entries) for _obj
    if {$len == 0} {
	set oid 0
    } else {
	set l [lsort -dictionary $l]
	set le [lindex $l end]
	regexp {([0-9]+)_([0-9]+)_(.+)} $le all gid oid gname

	set oosync [$mLedger attr get $le $LEDGER_ENTRY_OUT_OF_SYNC_ATTR]

	# No need to checkpoint again (i.e. no mods since last checkpoint)
	if {!$oosync} {
	    if {$_obj == $mSelectedObj && $len > 1} {
		set mNeedGlobalUndo 1
		set mNeedObjUndo 1
	    } else {
		set mNeedObjUndo 0

		# Check for other entries having mods
		set l [$mLedger ls -A $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1]
		set len [llength $l]

		if {$len == 0} {
		    set mNeedGlobalUndo 0
		} else {
		    set mNeedGlobalUndo 1
		}
	    }

	    if {$_obj == $mSelectedObj} {
		set oflag 1
		set mNeedCheckpoint 0
		updateCheckpointMode
	    } else {
		set oflag 0
	    }

	    updateUndoMode $oflag

	    return $le
	}

	incr oid
    }

    incr mLedgerGID

    # Create the ledger entry
    set lname $mLedgerGID\_$oid\_$_obj
    gedCmd cp $_obj $mLedger\:$lname

    # Set the attributes
    $mLedger attr set $lname $LEDGER_ENTRY_TYPE_ATTR $_type
    switch $_type \
	$LEDGER_CREATE - \
	$LEDGER_DESTROY - \
	$LEDGER_RENAME {
	    $mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
	} \
	$LEDGER_MODIFY - \
	default {
	    $mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 0
	}

    set l [$mLedger ls -A $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1]
    set len [llength $l]
    if {$len == 0} {
	set mNeedGlobalUndo 0
	set mNeedObjUndo 0

	set mNeedCheckpoint 0
	updateCheckpointMode

	set oflag 1
    } else {
	set mNeedGlobalUndo 1

	if {$_obj == $mSelectedObj} {
	    if {$oid == 0} {
		set mNeedObjUndo 0
	    } else {
		set mNeedObjUndo 1
	    }

	    set oflag 1

	    set mNeedCheckpoint 0
	    updateCheckpointMode
	} else {
	    set oflag 0
	}
    }

    updateUndoMode $oflag

    return $lname
}

##
# This method creates ledger entries for each object in _olist
# using the same global ID and an object ID of zero.
#
# Note - this method is not currently being used. Before using this
#        method the undo methods will need to accomodate multiple
#        entries having the same global ID.
#
::itcl::body Archer::checkpoint_olist {_olist _type} {
    set olen [llength $_olist]
    if {$olen == 0 || $mLedger == ""} {
	return
    }

    incr mLedgerGID
    set oid 0
    set ouflag 0
    set foundSelectedObj 0
    set lnames {}

    # Create the ledger entries
    foreach obj $_olist {
	if {[catch {gedCmd attr show $obj} adata]} {
	    continue
	}

	if {$obj == $mSelectedObj} {
	    set foundSelectedObj 1

	    # Get all ledger entries related to mSelectedObj
	    set l [$mLedger expand *_*_$mSelectedObj]
	    set len [llength $l]

	    if {$len} {
		set l [lsort -dictionary $l]
		set le [lindex $l end]

		if {![$mLedger attr get $le $LEDGER_ENTRY_OUT_OF_SYNC_ATTR]} {
		    $mLedger kill $le

		    if {$len > 1} {
			set ouflag 1
		    }
		}
	    }
	}

	# Create the ledger entry
	set lname $mLedgerGID\_$oid\_$obj
	lappend lnames $lname
	gedCmd cp $obj $mLedger\:$lname

	# Set the attributes
	$mLedger attr set $lname $LEDGER_ENTRY_TYPE_ATTR $_type
	switch $_type \
	    $LEDGER_CREATE - \
	    $LEDGER_DESTROY - \
	    $LEDGER_RENAME {
		$mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1
	    } \
	    $LEDGER_MODIFY - \
	    default {
		$mLedger attr set $lname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 0
	    }
    }

    set l [$mLedger ls -A $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1]
    set len [llength $l]
    if {$len == 0} {
	set mNeedGlobalUndo 0
	set mNeedObjUndo 0

	set mNeedCheckpoint 0
	updateCheckpointMode

	set oflag 1
    } else {
	set mNeedGlobalUndo 1

	if {$foundSelectedObj} {

	    if {$ouflag} {
		set mNeedObjUndo 1
	    } else {
		set mNeedObjUndo 0
	    }

	    set oflag 1

	    set mNeedCheckpoint 0
	    updateCheckpointMode
	} else {
	    set oflag 0
	}
    }

    updateUndoMode $oflag
    return $lnames
}

::itcl::body Archer::clearTargetLedger {} {
    set mLedgerGID 0
    set alist [$mLedger expand *]
    eval $mLedger kill $alist
}

::itcl::body Archer::createTargetLedger {} {
    # This belongs in the openDb and newDb
    # Delete previous ledger
    if {$mLedger != ""} {
	catch {rename $mLedger ""}
    }

    set mLedgerGID 0
    set mLedger "ledger"
    go_open $mLedger inmem 0
}

::itcl::body Archer::global_undo {} {
    if {$mLedger == ""} {
	return
    }

    # Removes unnecessary entries
    ledger_cleanup

    # Get last ledger entry
    set l [$mLedger expand *_*_*]
    set len [llength $l]

    if {$len == 0} {
	set mNeedCheckpoint 0
	set mNeedGlobalUndo 0
	set mNeedObjUndo 0
	set mNeedSave 0
	set mLedgerGID 0

	tk_messageBox -message "global_undo: should not get here. Must be a programming error."
	
	updateCheckpointMode
	updateSaveMode
	updateUndoMode

	return
    }

    set l [lsort -dictionary $l]
    set le [lindex $l end]
    regexp {([0-9]+)_([0-9]+)_(.+)} $le all gid oid gname
    set mLedgerGID $gid
    incr mLedgerGID -1

    set cflag 0
    set gnames {}

    # Undo each object associated with this transaction
    foreach lentry [$mLedger expand $gid\_$oid\_*] {
	regexp {([0-9]+)_([0-9]+)_(.+)} $lentry all gid oid gname

	set type [$mLedger attr get $lentry $LEDGER_ENTRY_TYPE_ATTR]
	switch $type \
	    $LEDGER_CREATE {
		gedCmd kill $gname

		if {$gname == $mSelectedObj} {
		    initDbAttrView $mTarget
		}

		set mSelectedObj ""
		set mSelectedObjPath ""
		set mSelectedObjType ""
		set cflag 1
	    } \
	    $LEDGER_RENAME {
		if {![catch {$mLedger attr get $lentry $LEDGER_ENTRY_MOVE_COMMAND} move_cmd]} {
		    eval gedCmd $move_cmd

		    set curr_name [lindex $move_cmd 1]
		    set gname [lindex $move_cmd 2]
		    if {$curr_name == $mSelectedObj} {
			set mSelectedObj $gname
			regsub {([^/]+)$} $mSelectedObjPath $gname mSelectedObjPath
		    }
		} else {
		    putString "No old name found for $lentry"
		    continue
		}
	    } \
	    $LEDGER_DESTROY - \
	    $LEDGER_MODIFY - \
	    default {
		# Adjust the corresponding object according to the ledger entry
		gedCmd cp -f $mLedger\:$lentry $gname
		gedCmd attr rm $gname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR
		gedCmd attr rm $gname $LEDGER_ENTRY_TYPE_ATTR
	    }

#	if {$gname == "_GLOBAL"} {
#	    global_undo_callback
#	}
	global_undo_callback $gname

	# Remove the ledger entry
	$mLedger kill $lentry

	lappend gnames $gname
    }

    if {!$cflag} {
	foreach gname $gnames {
	    if {$gname != "_GLOBAL"} {
		if {$gname == $mSelectedObj} {
		    set mNeedObjSave 0
		    redrawObj $mSelectedObjPath
		    initEdit 0

		    # Make sure the selected object has atleast one checkpoint
		    checkpoint $mSelectedObj $LEDGER_MODIFY
		} else {
		    # Possibly draw the updated object
		    regsub {[0-9]+_[0-9]+_} $lentry "" stripped_lentry
		    foreach item [gedCmd report 0] {
			regexp {/([^/]+$)} $item all last

			if {$last == $stripped_lentry} {
			    redrawObj $item
			}
		    }
		}
	    }
	}
    }

    refreshTree 1

    set l [$mLedger ls -A $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1]
    set len [llength $l]
    if {$len == 0} {
	set mNeedCheckpoint 0
	set mNeedGlobalUndo 0
	set mNeedObjUndo 0
	set mNeedSave 0

	updateCheckpointMode
	updateSaveMode
	updateUndoMode
    }
}

::itcl::body Archer::global_undo_callback {_gname} {
    gedCmd refresh
}

::itcl::body Archer::ledger_cleanup {} {
    if {$mLedger == ""} {
	return
    }

    foreach le [$mLedger ls -A $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 0] {
	regsub {/$|/R$} $le "" le
	$mLedger kill $le
    }
}

::itcl::body Archer::object_checkpoint {} {
    checkpoint $mSelectedObj $LEDGER_MODIFY
}

::itcl::body Archer::object_undo {} {
    if {$mSelectedObj == "" || $mLedger == ""} {
	return
    }

    # Removes unnecessary entries
    ledger_cleanup

    # Get all ledger entries related to mSelectedObj
    set l [$mLedger expand *_*_$mSelectedObj]
    set len [llength $l]

    if {$len == 0} {
	set mNeedCheckpoint 0
	set mNeedObjUndo 0
	set mNeedSave 0

	set l [$mLedger ls -A $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1]
	set len [llength $l]
	if {$len == 0} {
	    set mNeedGlobalUndo 0
	    set mLedgerGID 0
	}

	tk_messageBox -message "object_undo: should not get here. Must be a programming error."

	updateCheckpointMode
	updateSaveMode
	updateUndoMode

	return
    }

    # Find last ledger entry corresponding to mSelectedObj
    set l [lsort -dictionary $l]
    set le [lindex $l end]

    if {![$mLedger attr get $le $LEDGER_ENTRY_OUT_OF_SYNC_ATTR]} {
	# No mods yet
	return
    }

    regexp {([0-9]+)_([0-9]+)_(.+)} $le all gid oid gname

    # Also decrement mLedgerGID if the entry is the last one
    if {$gid == $mLedgerGID} {
	incr mLedgerGID -1
    }

    set cflag 0

    # Undo each object associated with this transaction
    foreach lentry [$mLedger expand $gid\_$oid\_*] {
	regexp {([0-9]+)_([0-9]+)_(.+)} $lentry all gid oid gname

	# Undo it (Note - the destroy transaction will never show up here)
	set type [$mLedger attr get $lentry $LEDGER_ENTRY_TYPE_ATTR]
	switch $type \
	    $LEDGER_CREATE {
		gedCmd kill $gname
		set mSelectedObj ""
		set mSelectedObjPath ""
		set mSelectedObjType ""
		set cflag 1
	    } \
	    $LEDGER_RENAME {
		if {![catch {$mLedger attr get $lentry $LEDGER_ENTRY_MOVE_COMMAND} move_cmd]} {
		    eval gedCmd $move_cmd

		    set curr_name [lindex $move_cmd 1]
		    set gname [lindex $move_cmd 2]
		    if {$curr_name == $mSelectedObj} {
			set mSelectedObj $gname
			regsub {([^/]+)$} $mSelectedObjPath $gname mSelectedObjPath
		    }
		} else {
		    putString "No old name found for $lentry"
		    continue
		}
	    } \
	    $LEDGER_DESTROY - \
	    $LEDGER_MODIFY - \
	    default {
		# Adjust the corresponding object according to the ledger entry
		gedCmd cp -f $mLedger\:$lentry $gname
		gedCmd attr rm $gname $LEDGER_ENTRY_OUT_OF_SYNC_ATTR
		gedCmd attr rm $gname $LEDGER_ENTRY_TYPE_ATTR
	    }


	# Remove the ledger entry
	$mLedger kill $lentry
    }

    set mNeedObjSave 0

    if {!$cflag} {
	redrawObj $mSelectedObjPath
	initEdit 0
    } else {
	initDbAttrView $mTarget
    }

    set mNeedCheckpoint 0
    updateUndoState

    refreshTree 1

    # Make sure the selected object has atleast one checkpoint
    checkpoint $mSelectedObj $LEDGER_MODIFY

    updateCheckpointMode
    updateSaveMode
    updateUndoMode
}

::itcl::body Archer::revert {} {
    set mNeedSave 0
    Load $mTarget

    set mLedgerGID 0

    set mNeedCheckpoint 0
    updateCheckpointMode

    set mNeedGlobalUndo 0
    set mNeedObjSave 0
    set mNeedObjUndo 0
    set mNeedSave 0

    updateSaveMode
    updateUndoMode
}

::itcl::body Archer::selection_checkpoint {_obj} {
    if {$_obj == "" || $mLedger == ""} {
	return
    }

    # Get all ledger entries related to _obj
    set l [$mLedger expand *_*_$_obj]
    set len [llength $l]

    checkpoint $_obj $LEDGER_MODIFY
}

::itcl::body Archer::updateObjSave {} {
    if {$mSelectedObj == "" || $mLedger == ""} {
	return
    }

    # Get all ledger entries related to mSelectedObj
    set l [$mLedger expand *_*_$mSelectedObj]
    set len [llength $l]

    if {$len == 0} {
	# Should never get here
	tk_messageBox -message "updateObjSave: no ledger entry found for $mSelectedObj"
	return
    } else {
	# Find last ledger entry corresponding to mSelectedObj
	set l [lsort -dictionary $l]
	set le [lindex $l end]
    }

    $mLedger attr set $le $LEDGER_ENTRY_OUT_OF_SYNC_ATTR 1

    set mNeedCheckpoint 1
    set mNeedGlobalUndo 1
    set mNeedObjSave 1
    set mNeedObjUndo 1
    set mNeedSave 1

    updateCheckpointMode
    updateSaveMode
    updateUndoMode
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
