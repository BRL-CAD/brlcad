#
#                     A T T R _ E D I T . T C L
#
#       Widget for editing attributes
#
#       Author - John Anderson
#

package require Iwidgets 3.0

class Attr_editor {
    inherit itk::Toplevel

    # remembers the attribute names at the start of editing
    private variable old_attrs {}

    # remembers the values at the start of editing
    private variable old_vals {}

    # the current list of attribute names in the editor
    private variable cur_attrs {}

    # the corresponding values in the editor
    private variable cur_vals {}

    # the index into cur_vals and cur_attrs for the selected attribute
    private variable cur_index -1

    # the name of the object at start  
    private variable old_obj_name ""

    # text variable for the "object" entry widget
    private variable obj_name ""

    # convenient name for the attribute name list box
    private variable listb ""

    # convenient name for the attribute value text widget
    private variable textb ""

    # convenient name for the object entry widget
    private variable objb ""

    # convenient name for attribute name entry widget
    private variable attrn

    # convenient name for the pane containing the attribute name listbox
    private variable name_pane

    # convenient name for the pane containing the attribute value text box
    private variable value_pane

    # text variable for the attribute name entry widget
    private variable cur_attr_name ""

    constructor { args } {
	# only arg is an optional object name
	set num_args [llength $args]
	if { $num_args > 1 } {
	    tk_messageBox -icon error -type ok -title "Error"\
		    -message "Incorrect number of arguments"
	    destroy $itk_component(hull)
	    return
	}

	if { $num_args == 1 } {
	    set obj_name $args

	    # save the object name
	    set old_obj_name $obj_name

	    # get the attributes and set the text variables
	    set cur_attr_name ""
	    do_reset_all
	}

	# build the widgets
	itk_initialize
	$this configure -title "Attribute Editor"


	# frame containing the database object name
	itk_component add frame_obj {
	    frame $itk_interior.fr_obj -relief groove -bd 3
	}
	label $itk_interior.fr_obj.obj_l -text "Object:" -width 7 -anchor w
	entry $itk_interior.fr_obj.obj_e -textvariable [scope obj_name]
	set objb $itk_interior.fr_obj.obj_e
	pack $itk_interior.fr_obj.obj_l \
		    -expand no -side left -anchor w
	pack $itk_interior.fr_obj.obj_e \
		    -expand yes -fill x -side left -anchor w
	grid $itk_interior.fr_obj -row 0 -column 0 -sticky new -pad 3


	# frame containing new attribute button and entry widget
	itk_component add frame_new_attr {
	    frame $itk_interior.fr_new_attr -relief groove -bd 3
	}
	button $itk_interior.fr_new_attr.new -text "New Attribute"\
		-command [code $this new_attr]
	label $itk_interior.fr_new_attr.attr_l -text "Attribute Name:"
	entry $itk_interior.fr_new_attr.attr_e\
		-textvariable [scope cur_attr_name]
	set attrn $itk_interior.fr_new_attr.attr_e
	grid $itk_interior.fr_new_attr.new -row 0 -column 0 -padx 3 -pady 3
	grid $itk_interior.fr_new_attr.attr_l -row 0 -column 1 -padx 3\
		-pady 3 -sticky e
	grid $itk_interior.fr_new_attr.attr_e -row 0 -column 2 -padx 3\
		-pady 3 -sticky ew
	grid $itk_interior.fr_new_attr -row 1 -column 0 -sticky ew -pad 3
	grid columnconfigure $itk_interior.fr_new_attr 2 -weight 1 -pad 3


	if { 1 } {
	    # use paned window
	    # paned widget to contain attribute names listbox and values text
	    itk_component add pane_attrs {
		iwidgets::panedwindow $itk_interior.pane_attrs -orient vertical \
			-height 100 -thickness 6 -sashborderwidth 6
	    }

	    set name_pane [$itk_interior.pane_attrs add names]
	    listbox $name_pane.attrs -height 10 -listvar [scope cur_attrs]\
		    -yscrollcommand [code $name_pane.asb set]\
		    -exportselection false
	    set listb $name_pane.attrs
	    scrollbar $name_pane.asb -command [code $name_pane.attrs yview]

	    set value_pane [$itk_interior.pane_attrs add values]
	    text $value_pane.txt -width 40 -height 10\
		    -yscrollcommand [code $value_pane.sbt set]
	    set textb $value_pane.txt
	    scrollbar $value_pane.sbt -command [code $value_pane.txt yview]
	    if { 1 } {
		# use placer
		place $name_pane.asb -x 0 -y 0\
			-width 20 -relheight 1.0
		place $name_pane.attrs -x 20 -y 0\
			-relwidth 1.0 -relheight 1.0
		place $value_pane.sbt -x 0 -y 0\
			-width 20 -relheight 1.0
		place $value_pane.txt -x 20 -y 0\
			-relwidth 1.0 -relheight 1.0
		place $name_pane -x 0 -y 0\
			-relwidth 1.0 -relheight 1.0
		place $value_pane -x 0 -y 0\
			-relwidth 1.0 -relheight 1.0
		$itk_interior.pane_attrs fraction 20 80
	    } else {
		# packer does not seem to work :-(
		pack $name_pane.asb -side left -expand yes -fill y -anchor w
		pack $name_pane.attrs -side left -expand yes -fill both
		pack $value_pane.txt -side left -expand yes -fill both
		pack $value_pane.sbt -side left -expand yes -fill y -anchor e
		pack $name_pane -expand yes -fill both
		pack $value_pane -expand yes -fill both
		$itk_interior.pane_attrs fraction 20 80
	    }
	
	    grid $itk_interior.pane_attrs -row 2 -column 0\
		    -padx 3 -pady 3 -sticky nsew
	    grid rowconfigure $itk_interior.pane_attrs 0 -weight 1
	    grid columnconfigure $itk_interior.pane_attrs 0 -weight 1
	} else {
	    # no paned window
	    itk_component add attrs {
		frame $itk_interior.fr_attrs -relief groove -bd 3
	    }
	    listbox $itk_interior.fr_attrs.attrs -height 10 -listvar [scope cur_attrs]\
		    -yscrollcommand [code $itk_interior.fr_attrs.asb set]\
		    -exportselection false
	    set listb $itk_interior.fr_attrs.attrs
	    scrollbar $itk_interior.fr_attrs.asb -command [code $itk_interior.fr_attrs.attrs yview]

	    text $itk_interior.fr_attrs.txt -width 40 -height 10\
		    -yscrollcommand [code $itk_interior.fr_attrs.sbt set]
	    set textb $itk_interior.fr_attrs.txt
	    scrollbar $itk_interior.fr_attrs.sbt -command [code $itk_interior.fr_attrs.txt yview]
	    grid $itk_interior.fr_attrs.asb -row 0 -column 0 -pad 3 -sticky nse
	    grid $listb -row 0 -column 1 -pad 3 -sticky nsew
	    grid $textb -row 0 -column 2 -pad 3 -sticky nsew
	    grid $itk_interior.fr_attrs.sbt -row 0 -column 3 -pad 3 -sticky nsw
	    grid columnconfigure $itk_interior.fr_attrs 1 -weight 1
	    grid columnconfigure $itk_interior.fr_attrs 2 -weight 1
	    grid rowconfigure $itk_interior.fr_attrs 0 -weight 1
	    grid $itk_interior.fr_attrs -row 2 -column 0 -sticky nsew -pad 3
	}


	# frame containing reset and delete buttons
	itk_component add controls1 {
	    frame $itk_interior.frc1 -relief groove -bd 3
	}
	button $itk_interior.frc1.reset_all -text "reset all"\
		-command [code $this do_reset_all]
	button $itk_interior.frc1.reset_sel -text "reset selected"\
		-command [code $this do_reset_selected]
	button $itk_interior.frc1.delete_selected -text "delete selected"\
		-command [code $this do_delete_sel]
	grid $itk_interior.frc1.reset_all -row 0 -column 0 -padx 3 -pady 3
	grid $itk_interior.frc1.reset_sel -row 0 -column 1 -padx 3 -pady 3
	grid $itk_interior.frc1.delete_selected -row 0 -column 2\
		-padx 3 -pady 3
	grid columnconfigure $itk_interior.frc1 { 0 1 2 } -weight 1 -pad 3
	grid $itk_interior.frc1 -row 3 -column 0 -sticky sew -padx 3 -pady 3


	# from containing ok, apply, and dismiss buttons
	itk_component add controls {
	    frame $itk_interior.frc -relief groove -bd 3
	}
	button $itk_interior.frc.ok -text "ok" -command [code $this do_ok]
	button $itk_interior.frc.apply -text "apply" -command [code $this do_apply]
	button $itk_interior.frc.dismiss -text "dismiss" -command [code $this do_dismiss]
	grid $itk_interior.frc.ok -row 0 -column 0 -padx 3 -pady 3
	grid $itk_interior.frc.apply -row 0 -column 1 -padx 3 -pady 3
	grid $itk_interior.frc.dismiss -row 0 -column 2 -padx 3 -pady 3
	grid $itk_interior.frc -row 4 -column 0 -sticky sew -padx 3 -pady 3
	grid columnconfigure $itk_interior.frc { 0 1 2 } -weight 1 -pad 3

	# configure row and column containing the attributes to expand
	grid columnconfigure $itk_interior 0 -weight 1
	grid rowconfigure $itk_interior 2 -weight 1

	# this keeps the attribute name entry widget current when a selection is made
	# in the attribute name list box
	bind $listb <<ListboxSelect>> [code $this update_attr_text]

	# this keeps the cur_value entry for the selected attribute current with the
	# text in the text widget
	bind $textb <KeyRelease> [code $this update_cur_attr]

	# this allows the attributes of a new object to be edited by typing name
	# into the object entry widget and hitting enter
	bind $objb <Key-Return> [code $this do_new_obj]

	# this allows an attribute name to be changed by editing the contents of the
	# attribute name entry widget and hitting enter
	bind $attrn <Key-Return> [code $this update_cur_attr_name]

	# help on context data
	hoc_register_data $itk_interior.fr_obj.obj_e "Object" {
	    {summary "This window displays the currently selected database object. To select an object\n\
		    type its name in this window and hit \"ENTER\". All the attributes of this object\n\
		    will be placed in a local copy, and the names of all the attributes will be displayed\n\
		    in the list below. If you select an object in this manner without using the \"ok\"\n\
		    button first, any currently pending attribute edit operations performed in this\n\
		    editor will be forgotten."}
	}
	hoc_register_data $itk_interior.fr_new_attr.new "New Attribute" {
	    {summary "Use this button to create a new attribute. After pressing this button,\n\
		    Type the name of the new attribute into the \"Attribute Name\" window and\n\
		    hit \"ENTER\". The new attribute name will be added to the local copy of\n\
		    attributes for the currently select database object. You may then enter a\n\
		    value for the new attribute by typing into the attribute value window.\n\
		    The new attribute and value will not be saved to the database until you\n\
		    press \"ok\" or \"apply\" buttons."}
	}
	hoc_register_data $attrn "Attribute Name" {
	    {summary "This window displays the attribute currently selected for editing.\n\
		    If nothing has been selected, you may type in an attribute name and hit\n\
		    enter to select it. If an attribute name is already selected, you may edit\n\
		    the attribute name in this window and hit enter to save the edited attribute\n\
		    in the local copy.  Remember, attribute edits are not saved to the database\n\
		    until \"ok\" or \"apply\" is selected."}
	}
	hoc_register_data $listb "Attribute Names List" {
	    {summary "This window contains a list of the names of all the attributes in the local\n\
		    copy of the attributes of the currently selected database object. You may select\n\
		    one of these attributes for editing using the left mouse button. Remember, attribute\n\
		    edits are not saved to the database until \"ok\" or \"apply\" is selected."}
	}
	hoc_register_data $textb "Attribute Value" {
	    {summary "This window displays the value of the local copy of the currently selected\n\
		    attribute. You may edit this value simply by editing the contents of this window.\n\
		    Remember, attribute edits are not saved to the database until \"ok\" or \"apply\"\n\
		    is selected."}
	}
	hoc_register_data $itk_interior.frc.ok "ok" {
	    {summary "Press this button to save all the pending attribute edits to the currently\n\
		    selected database object and dismiss the attribute editor window. Remember,\n\
		    attribute edits are not saved to the database until \"ok\" or \"apply\"\n\
		    is selected."} 
	}
	hoc_register_data $itk_interior.frc.apply "apply" {
	    {summary "Press this button to save all the pending attribute edits to the currently\n\
		    selected database object without dismissing the attribute editor window. Remember,\n\
		    attribute edits are not saved to the database until \"ok\" or \"apply\"\n\
		    is selected."} 
	}
	hoc_register_data $itk_interior.frc1.reset_sel "reset selected" {
	    {summary "Press this button to reset the value of the local copy of the currently selected\n\
		    attribute to that stored in the database. Remember, attribute edits are not saved to\n\
		    the database until \"ok\" or \"apply\" is selected."}
	}
	hoc_register_data $itk_interior.frc1.reset_all "reset all" {
	    {summary "Press this button to reset the local copy of the attributes of the currently selected\n\
		    object to those stored in the database. Remember, attribute edits are not saved to\n\
		    the database until \"ok\" or \"apply\" is selected."}
	}
	hoc_register_data $itk_interior.frc.dismiss "dismiss" {
	    {summary "Press this button to dismiss the attribute editor window without saving any\n\
		    currently pending attribute edits to the database."}
	}
	hoc_register_data $itk_interior.frc1.delete_selected "delete selected" {
	    {summary "Press this button to delete the currently selected attribute from the local\n\
		    copy of the currently selected object attributes. Remember, attribute edits are\n\
		    not saved to the database until \"ok\" or \"apply\" is selected."}
	}
    }

    # a proc to insure this is a singleton
    proc start_editor { args } {
	set num_args [llength $args]
	if { $num_args < 1 } return

	set id [lindex $args 0]
	if { [winfo exists .${id}_attr_edit] } {
	    wm deiconify .${id}_attr_edit
	    raise .${id}_attr_edit
	    return
	}

	if { $num_args == 1 } {
	    Attr_editor .${id}_attr_edit
	} else {
	    set new_args [lrange $args 1 end]
	    eval Attr_editor .${id}_attr_edit $new_args
	}
    }

    method do_delete_sel {} {
	if { $cur_index < 0 } return

	set cur_attrs [lreplace $cur_attrs $cur_index $cur_index]
	set cur_vals [lreplace $cur_vals $cur_index $cur_index]

	set len [llength $cur_attrs]
	while { $cur_index >= $len } {
	    incr cur_index -1
	}
	$listb selection set $cur_index
	update_attr_text
    }

    method create_new_attribute {} {
	grab release $itk_interior.fr_new_attr.attr_e
	if { [attr_name_is_valid $cur_attr_name] == 0 } {
	   tk_messageBox -icon error -type ok -title "Error: illegal attribute name"\
              -message "Attribute names must not have imbedded white space nor non-printable characters"
	    return
	}

	set cur_index [llength $cur_attrs]
	lappend cur_attrs $cur_attr_name
	lappend cur_vals {}

	$listb selection clear 0 end
	$listb selection set $cur_index
	update_attr_text

	# restore normal binding for attribute name entry widget
	bind $itk_interior.fr_new_attr.attr_e <Key-Return> [code $this update_cur_attr_name]

	focus $textb
    }

    method new_attr {} {
	set cur_index -1
	set cur_attr_name ""
	$textb delete 1.0 end

	bind $itk_interior.fr_new_attr.attr_e <Key-Return> [code $this create_new_attribute]
	focus $itk_interior.fr_new_attr.attr_e
	grab set $itk_interior.fr_new_attr.attr_e
    }

    # attribute names may not contain embedded white space nor non-priontable chars
    method attr_name_is_valid { name } {
	set len [string length $name]
	if { $len < 1 } {
	    return 0
	}
	for { set index 0 } { $index < $len } { incr index } {
	    set char [string index $name $index]
	    if [string is space $char] {
		return 0
	    }
	    if { [string is print $char] == 0 } {
		return 0
	    }
	}

	return 1
    }

    # method called to copy attribute name from attribute name entry widget to the
    # currently indexed slot in the list of attribute names, or if nothing is selected
    # in the list, look for this attribute name and select it, if found
    method update_cur_attr_name {} {
	if { $cur_index < 0 } {
	    set cur_index [lsearch -exact $cur_attrs $cur_attr_name]
	    if { $cur_index > -1 } {
		$listb selection set $cur_index
		update_attr_text
	    }
	    return
	}

	if { [attr_name_is_valid $cur_attr_name] == 0 } {
	   tk_messageBox -icon error -type ok -title "Error: illegal attribute name"\
              -message "Attribute names must not have imbedded white space nor non-printable characters"
	    return
	}

	set tmp_index [lsearch -exact $cur_attrs $cur_attr_name]
	if { $tmp_index > -1 } {
	    set cur_index $tmp_index
	    $listb selection clear 0 end
	    $listb selection set $cur_index
	    update_attr_text
	} else {
	    # update the list
	    set cur_attrs [lreplace $cur_attrs $cur_index $cur_index $cur_attr_name]
	}
    }

    # method called when enter is hit in object entry widget to start editing a different object
    method do_new_obj {} {
	set cur_index -1
	do_reset_all
    }

    # method called when any key is pressed in the attribute value text widget
    # updates the value saved for the currently selected attribute
    method update_cur_attr {} {
	if { $cur_index < 0 } return

	set cur_vals [lreplace $cur_vals $cur_index $cur_index [$textb get 1.0 {end - 1 chars}]]
    }

    # method called when a selection is made in the attribute name listbox
    # updates the attribute name entry widget and the value test widget
    method update_attr_text {} {
	$textb delete 1.0 end
	set cur_index [$listb curselection]
	if { $cur_index < 0 } return
	set cur_attr_name [lindex $cur_attrs $cur_index]
	$textb insert end [lindex $cur_vals $cur_index]
	$textb see 1.0
    }

    # save the edits to the database, called by "apply" button and indiretcly by "ok" button
    # returns 0 if all is well
    # returns 1 otherwise
    method do_apply {} {

	if { $obj_name == "" } { return 1 }

	# warn user if we may be copying attributes to a different object
	if { $obj_name != $old_obj_name } {
	    set answer [tk_messageBox -icon warning -type yesno -title "Warning: copying attributes" -message "Warning: clicking on \"yes\" below will have the effect of copying all the\n attributes in this table to the \"$obj_name\" object.  Note that they probably came from the \"$old_obj_name\" object.\nIs this copy what you really intended??"]
	    if { $answer == "no" } {
		return 1
	    }
	}

	# remove ALL attributes from this object
	# first get an up-to-date list of attributes for the object
	if [catch {attr get $obj_name} ret] {
	   tk_messageBox -icon error -type ok -title "Error getting current attributes" -message $ret
	    return 1
	}

	# now extract the attribute names from this list
	set attr_names {}
	foreach {attr val} $ret {
	    lappend attr_names $attr
	}

	# and remove them
	if [catch {eval attr rm $obj_name $attr_names} ret ] {
	   tk_messageBox -icon error -type ok -title "Error removing old attributes" -message $ret
	    return 1
	}

	# now create a list of the new attribute value pairs
	set attr_val_list {}
	foreach attr $cur_attrs val $cur_vals {
	    set attr_val_list [concat [list $attr] [list $val] $attr_val_list]
	}

	# and set them
	if [catch {eval attr set $obj_name $attr_val_list} ret] {
	   tk_messageBox -icon error -type ok -title "Error saving attributes" -message $ret
	    return 1
	}

	set old_obj_name $obj_name

	return 0
    }

    # method called by "ok" button
    method do_ok {} {
	if [do_apply] return
	do_dismiss
    }

    # method called by "dismiss" button
    method do_dismiss {} {
	delete object $this
    }

    # method called by "reset selected" button
    # fetches value of currently selected attribute from database and saves as current value
    method do_reset_selected {} {
	if { $cur_index < 0 } return

	set attr_name [lindex $cur_attrs $cur_index]
	if { [catch {attr get $obj_name $attr_name} attr_val] } {
	    tk_messageBox -icon error -type ok -title "Error: unrecognized attribute" -message $attr_val
	    return
	}
	set cur_vals [lreplace $cur_vals $cur_index $cur_index $attr_val]
	update_attr_text
    }

    # method called by "reset all" button and by constructor
    # get all the attributes for the current object and prepare to edit them
    method do_reset_all {} {
	if { $cur_index > -1 } {
	    set cur_attr_name [lindex $cur_attrs $cur_index]
	}

	# clear some variables
	set old_attrs {}
	set old_vals {}
	set cur_attrs {}
	set cur_vals {}

	# get the attributes
	if { [string is space $obj_name] } {
	    set ret {}
	    set cur_attr_name ""
	} else {
	    if { [catch "attr get $obj_name" ret] != 0 } {
		tk_messageBox -icon error -type ok -title "Error" -message $ret
		set obj_name ""
		set old_obj_name ""
		return
	    }
	}

	set old_obj_name $obj_name

	# save the attributes
	foreach {attr val} $ret {
	    lappend old_attrs $attr
	    lappend old_vals $val
	    lappend cur_attrs $attr
	    lappend cur_vals $val
	}

	$textb delete 1.0 end
	if { $cur_index > -1 } {
	    set cur_index [lsearch -exact $cur_attrs $cur_attr_name]
	    if { $cur_index > -1 } {
		$listb selection set $cur_index
	    } else {
		$listb selection clear 0 end
		set cur_attr_name ""
	    }
	    update_attr_text
	} else {
	    set cur_attr_name ""
	}
    }
}




