catch {namespace eval hv3 { set {version($Id$)} 1 }}

###########################################################################
# hv3_form.tcl --
#
#     This file contains code to implement Html forms for Tkhtml based
#     browsers. The only requirement is that no other code register for
#     node-handler callbacks for <input>, <button> <select> or <textarea> 
#     elements. The application must provide this module with callback
#     scripts to execute for GET and POST form submissions.
#

# Load Bryan Oakley combobox. 
#
# Todo: Puppy linux has this (combobox) packaged already. Should use 
# this fact to reduce installation footprint size on that platform.
#
source [file join [file dirname [info script]] combobox.tcl]

#----------------------------------------------------------------------
#     The following HTML elements create document nodes that are replaced with
#     form controls:
#
#         <!ELEMENT INPUT    - O EMPTY> 
#         <!ELEMENT BUTTON   - - (%flow;)* -(A|%formctrl;|FORM|FIELDSET)>
#         <!ELEMENT SELECT   - - (OPTGROUP|OPTION)+> 
#         <!ELEMENT TEXTAREA - - (#PCDATA)> 
#         <!ELEMENT ISINDEX  - O EMPTY> 
#
#     This module registers node handler scripts with client html widgets for
#     these five element types. The <isindex> element (from ancient times) is
#     handled specially, by transforming it to an equivalent HTML4 form.
#
#         <input>       -> button|radiobutton|checkbutton|combobox|entry|image
#         <button>      -> button|image
#         <select>      -> combobox
#         <textarea>    -> text
#
# <input>
# type = text|password|checkbox|radio|submit|reset|file|hidden|image|button
#
# <button>
# type = submit|button|reset
#
#     <select>   -> [::hv3::forms::select]
#     <textarea> -> [::hv3::forms::textarea]
#     <isindex>  -> Transformed to <INPUT type="text"> by script handler 
#
# TODO: Handle <BUTTON> markup.
#

#----------------------------------------------------------------------
# Code in this file is organized into the following types:
#
#     ::hv3::fileselect (widget)
#     ::hv3::control (widget)
#     ::hv3::clickcontrol
#     ::hv3::form
#     ::hv3::formmanager
#

#
#     ::hv3::forms::checkbox
#     ::hv3::forms::entrycontrol
#     ::hv3::forms::select
#     ::hv3::forms::textarea
#

#----------------------------------------------------------------------
# Standard controls interface. All control types implement this.
#
#         formsreport
#         name
#         value
#         success
#         filename
#         stylecmd
#         reset
#
#         get_text_widget
#         configurecmd
#
#     get_text_widget and configurecmd will be removed sooner or later.
#
# As well as the standard controls interface, each type implements an
# interface for interaction with the DOM. For HTMLInputElement objects:
#
#         dom_checked
#         dom_value
#         dom_select
#         dom_focus
#         dom_blur
#         dom_click
#

namespace eval ::hv3::forms {
  proc configurecmd {win font} {
    set descent [font metrics $font -descent]
    set ascent  [font metrics $font -ascent]
    expr {([winfo reqheight $win] + $descent - $ascent) / 2}
  }
}

proc ::hv3::boolean_attr {node attr def} {
  set val [$node attribute -default $def $attr]
  if {$val eq "" || ![string is boolean $val]} {
    set val true
  }
  return $val
}
proc ::hv3::put_boolean_attr {node attr val} {
  if {$val eq "" || ![string is boolean $val]} {
    set val true
  }
  $node attribute $attr $val
}

# The argument node must be either a <FORM> or an element that generates 
# a form control. The return value is a list of node handles. The first
# is the associated <FORM> node, followed by all controls also associated
# with the same <FORM> node.
#
# If there is no associated <FORM>, an empty string is returned.
#
proc ::hv3::get_form_nodes {node} {
  set html [$node html]
  set N [$html search INPUT,SELECT,TEXTAREA,BUTTON,FORM]

  set idx [lsearch -exact $N $node]
  if {$idx >= 0} {
    set iFirst $idx
    while { $iFirst>=0 && [[lindex $N $iFirst] tag] ne "form" } {
      incr iFirst -1
    }
  
    set iLast [expr $idx+1]
    while { $iLast<[llength $N] && [[lindex $N $iLast] tag] ne "form" } {
      incr iLast 1
    }
  
    if {$iFirst>=0} {
      return [lrange $N $iFirst [expr $iLast-1]]
    }
  }

  return ""
}

# Scan the document currently displayed by html widget $html, returning
# a list of nodes that can accept focus. The list is ordered according
# to the order in which they should be navigated by the user agent (the
# "tabindex" order).
#
# In Hv3, the following are considered focusable:
#
#   + <TEXTAREA>
#   + <INPUT type="text"> <INPUT type="password"> <INPUT type="file">
#
proc ::hv3::get_focusable_nodes {html} {
  set ret [list]
  foreach N [$html search TEXTAREA,INPUT] {
    if {[::hv3::boolean_attr $N disabled 0]} continue
    if {[string toupper [$N tag]] eq "INPUT"} {
      set type [string tolower [$N attr -default "" type]]
      set L [list radio button hidden checkbox image reset submit]
      if {[lsearch $L $type]>=0} continue
    }
    lappend ret $N
  }

  lsort -command [list ::hv3::compare_focusable $ret] $ret
}

proc ::hv3::compare_focusable {orig L R} {
  set tl [$L attr -default 0 tabindex]
  set tr [$R attr -default 0 tabindex]
  if {![string is integer $tl]} {set tl 0}
  if {![string is integer $tr]} {set tr 0}
  if {$tr<0} {set tr 0}
  if {$tl<0} {set tl 0}

  if {$tr == $tl} {
    # Compare based on order in $orig
    set il [lsearch $orig $L]
    set ir [lsearch $orig $R]
    return [expr {$il - $ir}]
  }

  # Nodes with tabindex=0 come after those with +ve tabindex values
  if {$tr == 0} {return -1}
  if {$tl == 0} {return 1}

  # Node with the smallest tabindex comes first.
  return [expr {$tl - $tr}]
}

# Called when <Tab> or <Shift-Tab> is pressed when the html widget or
# one of it's form controls has the focus. This makes sure the stacking
# order of the controls within the Html widget is correct for
# html traversal rules (i.e. the "tabindex" attribute).
#
proc ::hv3::forms::tab {html} {
  set L [::hv3::get_focusable_nodes $html]
  set prev ""
  foreach node $L {
    set win [$node replace]
    raise $win
  }
}

# Given a node that generates a control - $node - return the
# corresponding <FORM> node. Or an empty string, if there is
# no such node.
proc ::hv3::control_to_form {node} {
  lindex [::hv3::get_form_nodes $node] 0
}

#--------------------------------------------------------------------------
# ::hv3::forms::checkbox 
#
#     Object for controls created elements of the following form:
#    
#         <INPUT type="checkbox">
#
::snit::widgetadaptor ::hv3::forms::checkbox {
  option -takefocus -default 0

  variable mySuccess 0        ;# -variable for checkbutton widget
  variable myNode             ;# Tkhtml <INPUT> node

  delegate option * to hull
  delegate method * to hull

  constructor {node bindtag args} {
    installhull [checkbutton $win]
    $hull configure -variable [myvar mySuccess]
    $hull configure -highlightthickness 0 -pady 0 -padx 0 -borderwidth 0
    set myNode $node
    bindtags $self [concat $bindtag [bindtags $self]]
    $self reset
  }

  # Generate html for the "HTML Forms" tab of the tree-browser.
  #
  method formsreport {} { 
    subst {}
  }

  # This method is called during form submission to determine the 
  # name of the control. It returns the value of the Html "name" 
  # attribute. Or, failing that, an empty string.
  #
  method name {} { return [$myNode attr -default "" name] }

  # This method is called during form submission to determine the 
  # value of the control. It returns the value of the Html "value" 
  # attribute. Or, failing that, an empty string.
  #
  method value {} { return [$myNode attr -default "" value] }

  # True if the control is considered successful for the purposes
  # of submitting this form.
  #
  method success {} { return [expr {$mySuccess && [$self name] ne ""}] }

  # Empty string. This method is only implemented by 
  # <INPUT type="file"> controls.
  #
  method filename {} { return "" }

  # Reset the state of the control.
  #
  method reset {} { 
    set mySuccess [expr [catch {$myNode attr checked}] ? 0 : 1]
  }

  # TODO: The sole purpose of this is to return a linebox offset...
  method configurecmd {values} { 
    ::hv3::forms::configurecmd $win [$hull cget -font]
  }

  method stylecmd {} {
    set N $myNode
    set bg "transparent"
    while {$bg eq "transparent" && $N ne ""} {
      set bg [$N property background-color]
      set N [$N parent]
    }
    if {$bg eq "transparent"} {set bg white}
    catch {
      $hull configure -bg $bg
      $hull configure -highlightbackground $bg
      $hull configure -activebackground $bg
      $hull configure -highlightcolor $bg
    }
  }

  #---------------------------------------------------------------------
  # START OF DOM FUNCTIONALITY
  #
  # Below this point are some methods used by the DOM class 
  # HTMLInputElement. None of this is used unless scripting is enabled.
  #

  # Get/set on the DOM "checked" attribute. This means the state 
  # of control (1==checked, 0==not checked) for this type of object.
  #
  method dom_checked {args} {
    if {[llength $args]>0} {
      set mySuccess [expr {[lindex $args 0] ? 1 : 0}]
    }
    return $mySuccess
  }

  # DOM Implementation does not call this. HTMLInputElement.value is
  # the "value" attribute of the HTML element for this type of object.
  #
  method dom_value {args} { error "N/A" }

  # HTMLInputElement.select() is a no-op for this kind of object. It
  # contains no text so there is nothing to select...
  #
  method dom_select  {} {}

  # Hv3 will not support keyboard access to checkboxes. Until
  # this changes these can be no-ops :)
  method dom_focus {} {}
  method dom_blur  {} {}

  # Generate a synthetic click. This same trick can be used for <INPUT>
  # elements with "type" set to "Button", "Radio", "Reset", or "Submit".
  #
  method dom_click {} {
    set x [expr [winfo width $win]/2]
    set y [expr [winfo height $win]/2]
    event generate $win <ButtonPress-1> -x $x -y $y
    event generate $win <ButtonRelease-1> -x $x -y $y
  }
}

#--------------------------------------------------------------------------
# ::hv3::forms::radio 
#
#     Object for controls created by elements of the following form:
#    
#         <INPUT type="radio">
#
::snit::widgetadaptor ::hv3::forms::radio {
  option -takefocus -default 0

  variable myNode             ;# Tkhtml <INPUT> node
  variable myVarname

  delegate option * to hull
  delegate method * to hull

  constructor {node bindtag} {
    installhull [radiobutton $win]
    set myNode $node
    set myVarname ::hv3::radiobutton_[$node attr -default "" name]

    $hull configure -variable [myvar mySuccess]
    $hull configure -highlightthickness 0 -pady 0 -padx 0 -borderwidth 0
    catch { $hull configure -tristatevalue EWLhwEUGHWZAZWWZE }

    bindtags $self [concat $bindtag [bindtags $self]]
    $self reset

    $hull configure -value $myNode
    $hull configure -variable $myVarname
    if {[::hv3::boolean_attr $myNode checked 0] || ![info exists $myVarname]} {
      set $myVarname $myNode
    }
  }

  # Generate html for the "HTML Forms" tab of the tree-browser.
  #
  method formsreport {} { 
    subst {}
  }

  # This method is called during form submission to determine the 
  # name of the control. It returns the value of the Html "name" 
  # attribute. Or, failing that, an empty string.
  #
  method name {} { return [$myNode attr -default "" name] }

  # This method is called during form submission to determine the 
  # value of the control. It returns the value of the Html "value" 
  # attribute. Or, failing that, an empty string.
  #
  method value {} { return [$myNode attr -default "" value] }

  # True if the control is considered successful for the purposes
  # of submitting this form.
  #
  method success {} { 
    if {[catch {$myNode attr name}]} {return 0}
    return [expr {[set $myVarname] eq $myNode}]
  }

  # Empty string. This method is only implemented by 
  # <INPUT type="file"> controls.
  #
  method filename {} { return "" }

  # Reset the state of the control.
  #
  method reset {} {
    #puts "TODO: ::hv3::forms::radio reset"
  }

  # TODO: The sole purpose of this is to return a linebox offset...
  method configurecmd {values} { 
    ::hv3::forms::configurecmd $win [$hull cget -font]
  }

  # Style the widget. All we do is set the background color.
  #
  method stylecmd {} {
    set N $myNode
    set bg "transparent"
    while {$bg eq "transparent" && $N ne ""} {
      set bg [$N property background-color]
      set N [$N parent]
    }
    if {$bg eq "transparent"} {set bg white}
    catch {
      $hull configure -bg $bg
      $hull configure -highlightbackground $bg
      $hull configure -activebackground $bg
      $hull configure -highlightcolor $bg
    }
  }

  #---------------------------------------------------------------------
  # START OF DOM FUNCTIONALITY
  #
  # Below this point are some methods used by the DOM class 
  # HTMLInputElement. None of this is used unless scripting is enabled.
  #

  # Get/set on the DOM "checked" attribute.
  #
  method dom_checked {args} {
    if {[llength $args] == 1} {
      if {[lindex $args 0]} {
        set $myVarname $myNode
      } else {
        set $myVarname ""
      }
    }
    return [expr {[set $myVarname] eq $myNode}]
  }

  method dom_select  {} { }
  method dom_focus {} { }
  method dom_blur  {} { }

  method dom_click {} {
    set x [expr [winfo width $me]/2]
    set y [expr [winfo height $me]/2]
    event generate $me <ButtonPress-1> -x $x -y $y
    event generate $me <ButtonRelease-1> -x $x -y $y
  }
}

#--------------------------------------------------------------------------
# ::hv3::forms::entrycontrol 
#
#     Object for controls created elements of the following form:
#    
#         <INPUT type="text">
#         <INPUT type="password">
#
namespace eval ::hv3::forms::entrycontrol {

  proc new {me node bindtag args} {
    upvar #0 $me O

    set O(-takefocus) 0

    set O(myValue) ""

    set O(myNode) $node

    set O(myWidget) [entry $O(win).entry]

    $O(myWidget) configure -highlightthickness 0 -borderwidth 0 
    $O(myWidget) configure -selectborderwidth 0
    $O(myWidget) configure -textvar ${me}(myValue)
    $O(myWidget) configure -background white

    $O(myWidget) configure -validatecommand [list $me Validate %P]
    $O(myWidget) configure -validate key

    pack $O(myWidget) -expand true -fill both

    # If this is a password entry field, obscure it's contents
    set zType [string tolower [$node attr -default "" type]]
    if {$zType eq "password" } { $O(myWidget) configure -show * }

    # Set the default width of the widget to 20 characters. Unless there
    # is no size attribute and the CSS 'width' property is set to "auto",
    # this will be overidden.
    $O(myWidget) configure -width 20

    # Pressing enter in an entry widget submits the form.
    bind $O(myWidget) <KeyPress-Return> [list $me Submit]

    bind $O(myWidget) <Tab>       [list ::hv3::forms::tab [$O(myNode) html]]
    bind $O(myWidget) <Shift-Tab> [list ::hv3::forms::tab [$O(myNode) html]]

    set tags [bindtags $O(myWidget)]
    bindtags $O(myWidget) [concat $tags $O(win)]

    $me reset
    eval $me configure $args
  }

  proc destroy {me} {
    uplevel #0 [list unset $me]
    rename $me {}
  }

  # Generate html for the "HTML Forms" tab of the tree-browser.
  #
  proc formsreport {me} { return {<i color=red>TODO</i>} }

  # This method is called during form submission to determine the 
  # name of the control. It returns the value of the Html "name" 
  # attribute. Or, failing that, an empty string.
  #
  proc name {me} { 
    upvar #0 $me O
    return [$O(myNode) attr -default "" name] 
  }

  # This method is called during form submission to determine the 
  # value of the control. Return the current contents of the widget.
  #
  proc value {me} { 
    upvar #0 $me O
    return $O(myValue) 
  }

  # True if the control is considered successful for the 
  # purposes of submitting this form.
  #
  proc success {me} { 
    upvar #0 $me O
    return [expr {[$me name] ne ""}] 
  }

  # Empty string. This method is only implemented by 
  # <INPUT type="file"> controls.
  #
  proc filename {me} { 
    upvar #0 $me O
    return "" 
  }

  # Reset the state of the control.
  #
  proc reset {me} { 
    upvar #0 $me O
    set O(myValue) [$O(myNode) attr -default "" value]
  }

  # TODO: The sole purpose of this is to return a linebox offset...
  proc configurecmd {me values} { 
    upvar #0 $me O
    ::hv3::forms::configurecmd $O(myWidget) [$O(myWidget) cget -font]
  }

  proc stylecmd {me} {
    upvar #0 $me O
    catch { $O(myWidget) configure -font [$O(myNode) property font] }
  }

  proc Submit {me} {
    upvar #0 $me O
    set form [::hv3::control_to_form $O(myNode)]
    if {$form ne ""} {
      [$form replace] submit $me
    }
  }

  # This method is called each time a character is inserted or
  # removed from the [entry] widget. To enforce the semantics of
  # the HTML "maxlength" attribute.
  #
  proc Validate {me newvalue} {
    upvar #0 $me O
    set iLimit [$O(myNode) attr -default -1 maxlength]
    if {$iLimit >= 0 && [string length $newvalue] > $iLimit} {
      return 0
    }
    return 1
  }

  #---------------------------------------------------------------------
  # START OF DOM FUNCTIONALITY
  #
  # Below this point are some methods used by the DOM class 
  # HTMLInputElement. None of this is used unless scripting is enabled.
  #

  # Get/set on the DOM "checked" attribute. This is always 0 for
  # an entry widget.
  #
  proc dom_checked {me args} { return 0 }

  # HTMLInputElement.value is the current contents of the widget 
  # for this type of object.
  #
  proc dom_value {me args} {
    upvar #0 $me O
    if {[llength $args]>0} {
      set O(myValue) [lindex $args 0]
    }
    return $O(myValue)
  }

  # Select the text in this widget.
  #
  proc dom_select  {me} {
    upvar #0 $me O
    $O(myWidget) selection range 0 end
  }

  # Methods [dom_focus] and [dom_blur] are used to implement the
  # focus() and blur() methods on DOM classes HTMLInputElement,
  # HTMLTextAreaElement and HTMLSelectElement.
  #
  # At present, calling blur() when a widget has the focus causes the
  # focus to be transferred to the html widget. This should be fixed 
  # so that the focus is passed to the next control in tab-index order
  # But tab-index is not supported yet. :(
  # 
  proc dom_focus {me} {
    upvar #0 $me O
    focus $O(myWidget)
  }
  proc dom_blur {me} {
    upvar #0 $me O
    set now [focus]
    if {$O(myWidget) eq [focus]} {
      focus [winfo parent $win]
    }
  }

  # This is a no-op for this type of <INPUT> element.
  #
  proc dom_click {me} {}
}
::hv3::make_constructor ::hv3::forms::entrycontrol

#--------------------------------------------------------------------------
# ::hv3::forms::textarea 
#
#     Object for controls created elements of the following form:
#    
#         <TEXTAREA>
#
::snit::widget ::hv3::forms::textarea {
  option -takefocus -default 0

  option -submitcmd -default ""

  variable myWidget ""
  variable myNode ""

  constructor {node bindtag args} {
    set myWidget [::hv3::scrolled text ${win}.widget -width 500]

    $myWidget configure -borderwidth 0
    $myWidget configure -pady 0
    $myWidget configure -selectborderwidth 0
    $myWidget configure -highlightthickness 0
    $myWidget configure -background white

    set myNode $node
    bindtags $myWidget [concat $bindtag [bindtags $myWidget] $win]
    $self reset
    $self configurelist $args

    bind $myWidget <Tab>       [list ::hv3::forms::tab [$myNode html]]
    bind $myWidget <Shift-Tab> [list ::hv3::forms::tab [$myNode html]]

    pack $myWidget -expand true -fill both
  }

  # Generate html for the "HTML Forms" tab of the tree-browser.
  #
  method formsreport {} { return {<i color=red>TODO</i>} }

  # This method is called during form submission to determine the 
  # name of the control. It returns the value of the Html "name" 
  # attribute. Or, failing that, an empty string.
  #
  method name {} { return [$myNode attr -default "" name] }

  # This method is called during form submission to determine the 
  # value of the control. Return the current contents of the widget.
  #
  method value {} { 
    string range [$myWidget get 0.0 end] 0 end-1
  }

  # True if the control is considered successful for the 
  # purposes of submitting this form.
  #
  method success {} { return [expr {[$self name] ne ""}] }

  # Empty string. This method is only implemented by 
  # <INPUT type="file"> controls.
  #
  method filename {} { return "" }

  # Reset the state of the control.
  #
  method reset {} { 
    set state [$myWidget cget -state]
    $myWidget configure -state normal
    set contents ""
    catch { $myWidget delete 0.0 end }
    foreach child [$myNode children] {
      append contents [$child text -pre]
    }
    $myWidget insert 0.0 $contents
    $myWidget configure -state $state
  }

  # TODO: The sole purpose of this is to return a linebox offset...
  method configurecmd {values} { 
    ::hv3::forms::configurecmd $myWidget [$myWidget cget -font]
  }

  method stylecmd {} {
    catch { $myWidget configure -font [$myNode property font] }
  }

  #---------------------------------------------------------------------
  # START OF DOM FUNCTIONALITY
  #
  # Below this point are some methods used by the DOM class 
  # HTMLTextAreaElement. All the important stuff uses the text widget
  # directly (see hv3_dom_html.tcl).
  #
  method get_text_widget {} {
    return $myWidget
  }

  method dom_blur {} {
    set now [focus]
    if {[$myWidget widget] eq [focus]} {
      focus [winfo parent $win]
    }
  }
  method dom_focus {} {
    focus [$myWidget widget]
  }
}


#--------------------------------------------------------------------------
# ::hv3::forms::select 
#
#     Object for controls created by elements of the following form:
#    
#         <SELECT>
#
snit::widgetadaptor ::hv3::forms::select {

  variable myHv3 ""
  variable myNode ""
  variable myCurrentSelected -1

  variable myValues [list]
  variable myLabels [list]

  delegate option * to hull
  delegate method * to hull

  constructor {node hv3 args} {
    installhull [::combobox::combobox $win]
    set myNode $node
    set myHv3 $hv3
    bindtags $self [concat $myHv3 [bindtags $self]]

    $hull configure -highlightthickness 0
    $hull configure -background white
    $hull configure -borderwidth 0
    $hull configure -highlightthickness 0
    $hull configure -editable false
    $hull configure -command [list $self ComboboxChanged]
    $hull configure -takefocus 0

    $self treechanged
    $self reset
  }

  method formsreport {} {
    return <I>TODO</I>
  }

  method name {} {
    return [$myNode attr -default "" name]
  }

  method value {} {
    lindex $myValues [$hull curselection]
  }

  method success {} {
    # If it has a name and is not disabled, it is successful.
    if {[catch {$myNode attr name}]}              { return 0 }
    if {[::hv3::boolean_attr $myNode disabled 0]} { return 0 }
    return 1
  }

  method filename {} { 
    return "" 
  }

  method stylecmd {} {
    $hull configure -font [$myNode property font]
  }

  method reset {} {
    set idx 0
    set ii 0
    foreach child [$myNode children]  {
      if {[$child tag] == "option"} {
        if {![catch {$child attr selected}]} {
          set idx $ii
        }
        incr ii
      }
    }
    set myCurrentSelected $idx
    $win select $idx
  }

  # TODO: The sole purpose of this is to return a linebox offset...
  method configurecmd {values} { 
    $self treechanged
    ::hv3::forms::configurecmd $win [$hull cget -font]
  }

  method ComboboxChanged {widget newValue} {
    set idx [$hull curselection]
    if {$myCurrentSelected<0 || $idx eq "" || $idx == $myCurrentSelected} return
    set myCurrentSelected $idx
    focus [winfo parent $win]

    # Fire the "onchange" dom event.
    [$myHv3 dom] event change $myNode
  }

  # This is called by the DOM module whenever the tree-structure 
  # surrounding this element has been modified. Update the
  # state of the widget to reflect the new tree structure.
  method treechanged {} {

    # Figure out a list of options for the drop-down list. This block 
    # sets up two list variables, $labels and $values. The $labels
    # list stores the options from which the user may select, and the 
    # $values list stores the corresponding form control values.
    set maxlen 5
    set labels [list]
    set values [list]
    foreach child [$myNode children] {
      if {[$child tag] == "option"} {

        # If the element has text content, this is used as the default
	# for both the label and value of the entry (used if the Html
	# attributes "value" and/or "label" are not defined.
	set contents ""
        catch {
          set t [lindex [$child children] 0]
          set contents [$t text]
        }

        # Append entries to both $values and $labels
        set     label  [$child attr -default $contents label]
        set     value  [$child attr -default $contents value]
        lappend labels $label
        lappend values $value

        set len [string length $label]
        if {$len > $maxlen} {set maxlen $len}
      }
    }

    # If the following if{...} statement is true, then the tree has
    # not changed in any way that this object cares about. In this 
    # case, we can return early.
    #
    if {$labels eq $myLabels && $values eq $myValues} {
      return
    }

    $hull configure -state normal

    set myLabels $labels
    set myValues $values

    # Set up the combobox widget. 
    $hull list delete 0 end
    eval [concat [list $hull list insert 0] $labels]

    # Set the width and height of the combobox. Prevent manual entry.
    if {[set height [llength $myValues]] > 10} { set height 10 }
    $hull configure -width  $maxlen
    $hull configure -height $height

    if {$myCurrentSelected>0 && $myCurrentSelected>=[llength $myValues]} {
      set myCurrentSelected [expr [llength $myValues]-1]
    }
    $hull select $myCurrentSelected
    set disabled [::hv3::boolean_attr $myNode disabled 0]
    if {$disabled} {
      $hull configure -state disabled
    } else {
      $hull configure -state normal
    }
  }

  #---------------------------------------------------------------------
  # START OF DOM FUNCTIONALITY
  #
  # Below this point are some methods used by the DOM class 
  # HTMLSelectElement. None of this is used unless scripting
  # is enabled. This interface is unique to this object - no other
  # control type has to interface with HTMLSelectElement.
  #

  method dom_selectionIndex {} { 
    set idx [$hull curselection]
    if {[$hull cget -state] eq "disabled" || $idx eq ""} {
      set idx -1
    }
    set idx
  }
  method dom_setSelectionIndex {value} { 
    if {[$hull cget -state] ne "disabled"} {
      $hull select $value 
    }
  }

  # Selection widget cannot take the focus in Hv3, so these two are 
  # no-ops.  Maybe some keyboard enthusiast will change this one day.
  #
  method dom_blur  {} {}
  method dom_focus {} {}
}

# ::hv3::fileselect
#
snit::widget ::hv3::forms::fileselect {
  option -takefocus -default 0

  component myButton
  component myEntry

  delegate option -text to myButton
  delegate option -highlightthickness to hull

  variable myNode ""

  constructor {node bindtag} {
    set myNode $node
    set myEntry [entry ${win}.entry -width 30]
    set myButton [button ${win}.button -command [list $self Browse]]
    $myButton configure -text "Browse..."

    $myEntry configure -highlightthickness 0
    $myEntry configure -borderwidth 0
    $myEntry configure -bg white

    $myButton configure -highlightthickness 0
    $myButton configure -pady 0

    # The [entry] widget may take the focus. The [button] does not.
    #
    $myButton configure -takefocus 0
    $myEntry configure  -takefocus 1
    bind $myEntry <Tab>       [list ::hv3::forms::tab [$node html]]
    bind $myEntry <Shift-Tab> [list ::hv3::forms::tab [$node html]]

    bindtags $myEntry  [concat $bindtag [bindtags $myEntry] $self]
    bindtags $myButton [concat $bindtag [bindtags $myButton] $self]

    pack $myButton -side right
    pack $myEntry -fill both -expand true
  }

  method success {} {
    set fname [${win}.entry get]
    if {$fname ne ""} {
      return 1
    }
    return 0
  }
  method value {} {
    set fname [${win}.entry get]
    if {$fname ne ""} {
      set fd [open $fname]
      fconfigure $fd -encoding binary -translation binary
      set data [read $fd]
      close $fd
      return $data
    }
    return ""
  }
  method filename {} {
    set fname [${win}.entry get]
    return [file tail $fname]
  }
  method name {} {
    return [$myNode attr -default "" name]
  }

  method formsreport {} {
    return <I>TODO</I>
  }
  
  method reset {} {
    $myEntry delete 0 end
  }

  method stylecmd {} {
    set font [$myNode property font]
    $myEntry configure -font $font
    $myButton configure -font $font
  }

  method configurecmd {values} { 
    ::hv3::forms::configurecmd $win [$myEntry cget -font]
  }

  #-----------------------------------------------------------------------
  # End of standard controls interface. Start of internal methods.
  #
  method Browse {} {
    set file [tk_getOpenFile]
    if {$file ne ""} {
      $myEntry delete 0 end
      $myEntry insert 0 $file
    }
  }

  #-----------------------------------------------------------------------
  # DOM interface for HTMLInputElement
  #
  method dom_checked {args} {return 0}

  method dom_value {args} {
    if {[llength $args]>0} {
      $myEntry delete 0 end
      $myEntry insert 0 [lindex $args 0]
    }
    return [$myEntry get]
  }

  method dom_select  {} {
    $myEntry selection range 0 end
  }

  method dom_focus  {} {
    focus $myEntry
  }

  method dom_blur  {} {
    set now [focus]
    if {$myEntry eq [focus]} {
      focus [winfo parent $win]
    }
  }

  method dom_click  {} { }
}

#--------------------------------------------------------------------------
# ::hv3::clickcontrol
#
#     An object of this class is used for the following types of form
#     control elements:
#
#         <input type=hidden>
#         <input type=image>
#         <input type=button>
#         <input type=submit>
#         <input type=reset>
#
#
namespace eval ::hv3::clickcontrol {

  proc new {me node} {
    upvar #0 $me O
    set O(myClicked) 0
    set O(myClickX) 0
    set O(myClickY) 0 
    set O(-clickcmd) ""
    set O(myNode) $node
  }
  proc destroy {me} {
    rename $me ""
    uplevel #0 [list unset $me]
  }

  # This method is used by graphical-submit buttons only. Controls
  # created by markup like:
  #
  #     <INPUT type="image">
  #
  proc graphicalSubmit {me} {
    upvar #0 $me O
    set t    [string tolower [$myNode attr -default "" type]]
    set name [$myNode attr -default "" name]
    if {$t ne "image" || $name eq ""} {return [list]}

    list "${name}.x" $myClickX "${name}.y" $myClickY
  }
 
  proc value {me} { 
    upvar #0 $me O
    return [$O(myNode) attr -default "" value] 
  }
  proc name {me}  {
    upvar #0 $me O
    return [$O(myNode) attr -default "" name]
  }

  proc success {me} { 
    upvar #0 $me O

    # Controls that are disabled cannot be succesful:
    if {[$O(myNode) attr -default 0 disabled]} {return 0}

    if {[catch {$O(myNode) attr name ; $O(myNode) attr value}]} {
      return 0
    }
    switch -- [string tolower [$O(myNode) attr type]] {
      hidden { return 1 }
      submit { return $O(myClicked) }
      image  { return 0 }
      button { return 0 }
      reset  { return 0 }
      default { 
        return 0 
      }
    }
  }

  # click --
  #
  #     This method is called externally when this widget is clicked
  #     on. If it is not "", evaluate the script configured as -clickcmd
  #
  proc click {me {isSynthetic 1}} {
    upvar #0 $me O

    # Controls that are disabled cannot be activated:
    #
    if {[$O(myNode) attr -default 0 disabled]} return

    set cmd $O(-clickcmd)
    set formnode [::hv3::control_to_form $O(myNode)]
    if {$cmd ne "" && $formnode ne ""} {

      set bbox [[$O(myNode) html] bbox $O(myNode)]
      foreach {x1 y1 x2 y2} $bbox {}
      if {$isSynthetic} {
        set O(myClickX) [expr {($x2-$x1)/2}]
        set O(myClickY) [expr {($y2-$y1)/2}]
      } else {
        foreach {px py} [winfo pointerxy [$O(myNode) html]] {}
        set wx [winfo rootx [$O(myNode) html]]
        set wy [winfo rooty [$O(myNode) html]]
        set O(myClickX) [expr $px - ($x1 + $wx)]
        set O(myClickY) [expr $py - ($y1 + $wy)]
      }

      set O(myClicked) 1
      eval [[$formnode replace] $cmd $me]

      catch {
        # Catch these in case this object has been destroyed by the 
        # form method invoked above.
        set O(myClicked) 0
        set O(myClickX) 0
        set O(myClickY) 0
      }
    }
  }

  proc configurecmd {me values} {}
  proc stylecmd {me} {}

  proc formsreport {me} {
    upvar #0 $me O
    set n [::hv3::control_to_form $O(myNode)]
    set report "<p>"
    if {$n eq ""} {
      append report {<i>No associated form node.</i>}
    } else {
      append report [subst -nocommands {
        <i>Controled by form node <a href="$n">$n</a></i>
      }]
    }
    append report "</p>"
    return $report
  }

  proc reset {me } { # no-op }

  #---------------------------------------------------------------------
  # START OF DOM FUNCTIONALITY
  #
  # Below this point are some methods used by the DOM class 
  # HTMLInputElement. None of this is used unless scripting is enabled.
  #

  # Get/set on the DOM "checked" attribute. This means the state 
  # of control (1==checked, 0==not checked) for this type of object.
  #
  proc dom_checked {me args} {
    return 0
  }

  # DOM Implementation does not call this. HTMLInputElement.value is
  # the "value" attribute of the HTML element for this type of object.
  #
  proc dom_value {me args} { error "N/A" }

  # HTMLInputElement.select() is a no-op for this kind of object. It
  # contains no text so there is nothing to select...
  #
  proc dom_select {me} {}

  # Hv3 will not support keyboard access to checkboxes. Until
  # this changes these can be no-ops :)
  proc dom_focus {me} {}
  proc dom_blur {me} {}

  # Generate a synthetic click. This same trick can be used for <INPUT>
  # elements with "type" set to "Button", "Radio", "Reset", or "Submit".
  #
  proc dom_click {me} {
    upvar #0 $me O
    set x [expr [winfo width $win]/2]
    set y [expr [winfo height $win]/2]
    event generate $win <ButtonPress-1> -x $x -y $y
    event generate $win <ButtonRelease-1> -x $x -y $y
  }
}

::hv3::make_constructor ::hv3::clickcontrol

#-----------------------------------------------------------------------
# ::hv3::format_query
#
#     This command is intended as a replacement for [::http::formatQuery].
#     It does the same thing, except it allows the following characters
#     to slip through unescaped:
#
#         - _ . ! ~ * ' ( )
#
#     as well as the alphanumeric characters (::http::formatQuery only
#     allows the alphanumeric characters through).
#
#     QUOTE FROM RFC2396:
#
#     2.3. Unreserved Characters
#     
#        Data characters that are allowed in a URI but do not have a reserved
#        purpose are called unreserved.  These include upper and lower case
#        letters, decimal digits, and a limited set of punctuation marks and
#        symbols.
#     
#           unreserved  = alphanum | mark
#     
#           mark        = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
#     
#        Unreserved characters can be escaped without changing the semantics
#        of the URI, but this should not be done unless the URI is being used
#        in a context that does not allow the unescaped character to appear.
#
#     END QUOTE
#
#     So in a way both versions are correct. But some websites (yahoo.com)
#     do not work unless we allow the extra characters through unescaped.
#
proc ::hv3::format_query {enc args} {
  set result ""
  set sep ""
  foreach i $args {
    append result $sep [::hv3::escape_string [encoding convertto $enc $i]]
    if {$sep eq "="} {
      set sep &
    } else {
      set sep =
    }
  }
  return $result
}
set ::hv3::escape_map ""
proc ::hv3::escape_string {string} {
  if {$::hv3::escape_map eq ""} {
    for {set i 0} {$i < 256} {incr i} {
      set c [format %c $i]
      if {$c ne "-" && ![string match {[a-zA-Z0-9_.!~*'()]} $c]} {
        set map($c) %[format %.2X $i]
      }
    }
    set {map( )} +
    set ::hv3::escape_map [array get map]
  }

  set converted [string map $::hv3::escape_map $string]
  return $converted
}
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# ::hv3::form
#
#     A single instance of this type is created for each HTML form in the 
#     document. 
#
#     This object is set as the "replacement" object for the corresponding
#     Tkhtml3 <form> node, even though it is not a Tk window, and therefore 
#     has no effect on display.
#
#   Options:
#
#       -getcmd
#       -postcmd
#
#   Methods
#
#       add_control NODE IS-SUBMIT 
#           Called to register a node that generates a control with this
#           form object.
#
#       submit ?SUBMIT-CONTROL?
#           Submit the form. Optionally, specify the control which did the
#           submitting.
#
#       reset
#           Reset the form.
#
#       controls
#           Return a list of nodes that create controls associated with
#           this <FORM> object (i.e. everything added via [add_control]).
#
#       formsreport 
#           For the tree-browser. Return a nicely formatted HTML report
#           summarizing the form state.
#    
snit::type ::hv3::form {

  # <FORM> element that corresponds to this object.
  variable myFormNode 

  variable myHv3

  # When the onsubmit() event is fired, this boolean variable is set.
  # If the event handler calls submit() on this form object, it is
  # submitted immediately, without running the event handler.
  #
  variable myInSubmitEvent 0

  option -getcmd  -default ""
  option -postcmd -default ""

  constructor {node hv3} {
    set myFormNode $node
    set myHv3 $hv3
    $node replace $self -deletecmd [list $self destroy]
  }

  destructor { }

  # Return a list of control nodes associated with this form.
  #
  method controls {} {
    return [lrange [::hv3::get_form_nodes $myFormNode] 1 end]
  }

  method reset {resetcontrol} {
    foreach c [lrange [::hv3::get_form_nodes $myFormNode] 1 end] {
      [$c replace] reset
    }
  }

  method ControlNodes {} {
    set ret [list]
    foreach c [lrange [::hv3::get_form_nodes $myFormNode] 1 end] {
      lappend ret $c
    }
    set ret
  }

  method SubmitNodes {} {
    set ret [list]
    foreach c [lrange [::hv3::get_form_nodes $myFormNode] 1 end] {
      set tag [string toupper [$c tag]]
      set type [string toupper [$c attr -default "" type]]
      if {$tag eq "INPUT" && $type eq "SUBMIT"} {
        lappend ret [$c replace]
      }
    }
    set ret
  }

  method submit {submitcontrol} {

    # Before doing anything, execute the onsubmit event
    # handlers, if any. If the submit handler script returns
    # false, do not submit the form. Otherwise, proceed.
    #
    if {!$myInSubmitEvent} {
      set myInSubmitEvent 1
      set rc [[$myHv3 dom] event onsubmit $myFormNode]
      if {$rc eq "prevent"} return
      if {$rc eq "error"} return
      set myInSubmitEvent 0
    }

    set SubmitControls [$self SubmitNodes]
    set Controls       [$self ControlNodes]

    set data [list]
    if {
        [lsearch $SubmitControls $submitcontrol] < 0 &&
        [llength $SubmitControls] > 0
    } {
      foreach s $SubmitControls {
        if {[$s name] ne ""} {
          lappend data [$s name]
          lappend data 1
          break
        }
      }
    }

    # If $submitcontrol is a graphical submit control, this line adds
    # the ${name}.x and ${name}.y elements to the form submission data.
    #
    catch { eval lappend data [$submitcontrol graphicalSubmit] }

    foreach controlnode $Controls {
      set control [$controlnode replace]
      if {$control eq ""} continue
      set success [$control success]
      set name    [$control name]
      if {$success} {
        set value [$control value]
        # puts "    Control \"$name\" is successful: \"$value\""
        lappend data $name $value
      } else {
        # puts "    Control \"$name\" is unsuccessful"
      }
    }

    # Now encode the data, depending on the enctype attribute of the
    set enctype [$myFormNode attr -default "" enctype]
    if {[string match -nocase *multipart* $enctype]} {
      # Generate a pseudo-random boundary string. The key here is that
      # if this exact string actually appears in any form control values,
      # the form submission will fail. So generate something nice and
      # long to minimize the odds of this happening.
      set bound "-----Submitted_by_Hv3_[clock seconds].[pid].[expr rand()]"

      set querytype "multipart/form-data ; boundary=$bound"
      set querydata ""
      set CR "\r\n"
      foreach controlnode $Controls {
        set control [$controlnode replace]
        if {$control eq ""} continue
        if {[$control success]} {

          set name  [$control name]
          set value [$control value]

          set filename ""
          catch {set filename [$control filename]}

          append querydata "--${bound}$CR"
          append querydata "Content-Disposition: form-data; name=\"${name}\""
          if { $filename ne "" } {
            append querydata "; filename=\"$filename\""
          }
          append querydata "$CR$CR"
          append querydata "${value}$CR"
        }
      }
      append querydata "--${bound}--$CR"
    } else {
      set querytype "application/x-www-form-urlencoded"
      set enc [$myHv3 encoding]
      set querydata [eval [linsert $data 0 ::hv3::format_query $enc]]
    }

    set action [$myFormNode attr -default "" action]
    set method [string toupper [$myFormNode attr -default get method]]
    switch -- $method {
      GET     { set script $options(-getcmd) }
      POST    { set script $options(-postcmd) }
      ISINDEX { 
        set script $options(-getcmd) 
        set control [[lindex $Controls 0] replace]
        set querydata [::hv3::format_query [$myHv3 encoding] [$control value]]
      }
      default { set script "" }
    }

    if {$script ne ""} {
      set exec [concat $script [list $myFormNode $action $querytype $querydata]]
      eval $exec
    }
  }

  method formsreport {} {
    set action [$myFormNode attr -default "" action]
    set method [$myFormNode attr -default "" method]

    set Template {
      <table>
        <tr><th>Action: <td>$action
        <tr><th>Method: <td>$method
      </table>

      <table>
        <tr><th>Name<th>Successful?<th>Value<th>Is Submit?
    }

    set report [subst $Template]

    foreach controlnode [lrange [::hv3::get_form_nodes $myFormNode] 1 end] {
      set control [$controlnode replace]
      if {$control eq ""} continue
      set success [$control success]
      set name    [$control name]
      set isSubmit [expr {([lsearch [$self SubmitNodes] $controlnode]>=0)}]

      if {$success} {
        set value [::hv3::string::htmlize [$control value]]
      } else {
        set value "<i>N/A</i>"
      }
      append report "<tr><td>"
      append report "<a href=\"$controlnode\">"
      if {$name ne ""} {
        append report "[::hv3::string::htmlize $name]"
      } else {
        append report "<i>$controlnode</i>"
      }
      append report "<td>$success<td>$value<td>$isSubmit"
    }
    append report "</table>"

    return $report
  }
}

#-----------------------------------------------------------------------
# ::hv3::formmanager
#
#     Each hv3 mega-widget has a single instance of the following type
#     It contains the logic and state required to manager any HTML forms
#     contained in the current document.
#    
snit::type ::hv3::formmanager {

  option -getcmd  -default ""
  option -postcmd -default ""

  # Map from node-handle to ::hv3::clickcontrol object for all clickable
  # form controls currently managed by this form-manager.
  variable myClickControls -array [list]
  variable myClicked ""

  variable myHv3
  variable myHtml

  constructor {hv3 args} {
    $self configurelist $args
    set myHv3  $hv3
    set myHtml [$myHv3 html]

    # Register handlers for elements that create controls. (todo: <button>).
    #
    $myHtml handler node input     [list $self control_handler]
    $myHtml handler node textarea  [list $self control_handler]
    $myHtml handler node select    [list $self control_handler]
    $myHtml handler node button    [list $self control_handler]
    $myHtml handler script isindex [list ::hv3::isindex_handler $hv3]

    $myHtml handler node form [list $self FormHandler]

    # Subscribe to mouse-clicks (for the benefit of ::hv3::clickcontrol
    # instances).
    $myHv3 Subscribe onclick [list $self clickhandler]
  }

  # FormHandler
  #
  #     A Tkhtml parse-handler for <form> and </form> tags.
  method FormHandler {node} {
    # This ::hv3::form object will be automatically deleted when
    # the <FORM> node is removed from the document.
    set form [::hv3::form %AUTO% $node $myHv3]

    $form configure -getcmd $options(-getcmd)
    $form configure -postcmd $options(-postcmd)
  }

  # This method is called by the [control_handler] method to add [bind] 
  # scripts to the forms control widget passed as an argument. The
  # [bind] scripts are used to generate the "keyup", "keydown" and 
  # "keypress" HTML 4.01 scripting events.
  #
  method SetupKeyBindings {widget node} {
    bind $widget <KeyPress>   +[list $self WidgetKeyPress $widget $node]
    bind $widget <KeyRelease> +[list $self WidgetKeyRelease $widget $node]
    bind $widget <FocusIn>    +[list $self WidgetFocus $widget $node]
    bind $widget <FocusOut>   +[list $self WidgetBlur $widget $node]
  }

  # Handler scripts for the <KeyPress> and <KeyRelease> events.
  #
  variable myKeyPressNode ""
  method WidgetKeyPress {widget node} {
    [$myHv3 dom] event keydown $node
    set myKeyPressNode $node
  }
  method WidgetKeyRelease {widget node} {
    [$myHv3 dom] event keyup $node
    if {$node eq $myKeyPressNode} {
      [$myHv3 dom] event keypress $node
    }
    set myKeyPressNode ""
  }
  method WidgetFocus {widget node} {
    [$myHv3 dom] event focus $node
  }
  method WidgetBlur {widget node} {
    [$myHv3 dom] event blur $node
  }

  method control_handler {node} {

    #set zWinPath ${myHtml}.document.control_[string map {: _} $node]
    set zWinPath ${myHtml}.document.control_[string map {: _} $node]
    set isSubmit 0

    set tag [string tolower [$node tag]]
    set type ""
    if {$tag eq "input"} {
      set type [string tolower [$node attr -default {} type]]
    }

    switch -- ${tag}.${type} {

      select. {
        set control [::hv3::forms::select $zWinPath $node $myHv3]
      }

      textarea. {
        set control [::hv3::forms::textarea $zWinPath $node $myHv3]
      }

      input.image {
        set control [::hv3::clickcontrol %AUTO% $node]
        set myClickControls($node) $control
        $control configure -clickcmd submit
        set isSubmit 1
      }
      input.submit {
        set control [::hv3::clickcontrol %AUTO% $node]
        set myClickControls($node) $control
        $control configure -clickcmd submit
        set isSubmit 1
      }
      input.reset {
        set control [::hv3::clickcontrol %AUTO% $node]
        $control configure -clickcmd reset
        set myClickControls($node) $control
      }
      button. {
        set buttontype [string tolower [$node attr -default {} type]]
        if {$buttontype eq "submit" || $buttontype eq "reset"} {
          set control [::hv3::clickcontrol %AUTO% $node]
          set myClickControls($node) $control
          $control configure -clickcmd $buttontype
          set isSubmit [expr {$buttontype eq "reset"}]
        } else {
          return
        }
      }
      input.button {
        set control [::hv3::clickcontrol %AUTO% $node]
        set myClickControls($node) $control
      }
      input.hidden {
        set control [::hv3::clickcontrol %AUTO% $node]
        set myClickControls($node) $control
      }
      input.checkbox {
        set hv3 [winfo parent [winfo parent $myHtml]]
        set control [::hv3::forms::checkbox $zWinPath $node $hv3]
      }
      input.radio {
        set hv3 [winfo parent [winfo parent $myHtml]]
        set control [::hv3::forms::radio $zWinPath $node $hv3]
      }
      input.file {
        set hv3 [winfo parent [winfo parent $myHtml]]
        set control [::hv3::forms::fileselect $zWinPath $node $hv3]
      }

      default {
        # This includes <INPUT type="password">, <INPUT type="text"> and
        # any unrecognized value for the type attribute.
        #
        set hv3 [winfo parent [winfo parent $myHtml]]
        set control [::hv3::::forms::entrycontrol $zWinPath $node $hv3]
      }
    }

    $self SetupKeyBindings $control $node

    if {[info exists myClickControls($node)]} {
      set deletecmd [list $control destroy]
    } else {
      set deletecmd [list destroy $control]
    }
    $node replace $control                         \
        -configurecmd [list $control configurecmd] \
        -stylecmd     [list $control stylecmd]     \
        -deletecmd    $deletecmd

  }

  destructor {
    $self reset
  }

  method reset {} {
    array unset myClickControls
  }

  method dumpforms {} {
    foreach node [$myHv3 html search form] {
      set form [$node replace]
      puts [$form dump]
    }
  }

  method clickhandler {node} {
    if {[info exists myClickControls($node)]} {
      $myClickControls($node) click 0
    }
  }
}

#-----------------------------------------------------------------------
# ::hv3::formsreport
#
#     This proc is called by the tree-browser code to obtain the HTML
#     text for the "HTML Forms" tab. If the argument $node is a <FORM>
#     node, or a node that generates a form control, a report is
#     returned explaining that nodes role in the HTML form.
#
#     Otherwise, a message is returned to say that the forms module
#     doesn't care two figs for node $node.
# 
proc ::hv3::formsreport {node} {

  # Never return a report for a text node.
  if {[$node tag] eq ""} return

  # If the [replace] object for the node exists and is of
  # one of the following classes, then we have a forms object!
  # The following classes all support the [formsreport] method
  # to return the report body.
  #
  set FORMS_CLASSES [list    \
      ::hv3::clickcontrol    \
      ::hv3::form            \
  ]

  set CONTROL_CLASSES [list      \
      ::hv3::forms::checkbox     \
      ::hv3::forms::entrycontrol \
      ::hv3::forms::select       \
      ::hv3::forms::textarea     \
      ::hv3::forms::fileselect   \
      ::hv3::forms::radio        \
  ]

  set R [$node replace]
  set rc [catch { set T [$R info type] } msg]

  if {$rc == 0} {
    if {[lsearch $CONTROL_CLASSES $T] >= 0} {
      set formnode [::hv3::control_to_form $node]
      if {$formnode eq ""} {
        set formnode "none"
      } else {
        set formnode "<A href=\"$formnode\">$formnode</A>"
      }
  
      return [subst {
        <TABLE>
          <TR><TH>Tcl (snit) class <TD>$T
          <TR><TH>Form node        <TD>$formnode
        </TABLE>
      }]
    }
  
    if {[lsearch $FORMS_CLASSES $T] >= 0} {
      return [$R formsreport]
    }
  }

  return {<i>No forms engine handling for this node</i>}
}

#-----------------------------------------------------------------------
# ::hv3::isindex_handler
#
#     This proc is registered as a Tkhtml script-handler for <isindex> 
#     elements. An <isindex> element is essentially an entire form
#     in and of itself.
#
#     Example from HTML 4.01:
#         The following ISINDEX declaration: 
#
#              <ISINDEX prompt="Enter your search phrase: "> 
#
#         could be rewritten with INPUT as follows: 
#
#              <FORM action="..." method="post">
#                  <P> Enter your search phrase:<INPUT type="text"> </P>
#              </FORM>
#
proc ::hv3::isindex_handler {hv3 attr script} {
  set a(prompt) ""
  array set a $attr

  set loc [::tkhtml::uri [$hv3 location]]
  set LOCATION "[$loc scheme]://[$loc authority][$loc path]"
  set PROMPT   $a(prompt)
  $loc destroy

  $hv3 write text [subst {
    <hr>
    <form action="$LOCATION" method="ISINDEX">
      <p>
        $PROMPT
        <input type="text">
      </p>
    </form>
    <hr>
  }]
}

