#
# tkdnd_windows.tcl --
# 
#    This file implements some utility procedures that are used by the TkDND
#    package.
#
# This software is copyrighted by:
# George Petasis, National Centre for Scientific Research "Demokritos",
# Aghia Paraskevi, Athens, Greece.
# e-mail: petasis@iit.demokritos.gr
#
# The following terms apply to all files associated
# with the software unless explicitly disclaimed in individual files.
#
# The authors hereby grant permission to use, copy, modify, distribute,
# and license this software and its documentation for any purpose, provided
# that existing copyright notices are retained in all copies and that this
# notice is included verbatim in any distributions. No written agreement,
# license, or royalty fee is required for any of the authorized uses.
# Modifications to this software may be copyrighted by their authors
# and need not follow the licensing terms described here, provided that
# the new terms are clearly indicated on the first page of each file where
# they apply.
# 
# IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
# FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
# ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
# DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
# 
# THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
# IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
# NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
# MODIFICATIONS.
#

namespace eval olednd {
  variable _types {}
  variable _typelist {}
  variable _codelist {}
  variable _actionlist {}
  variable _pressedkeys {}
  variable _action {}
  variable _common_drag_source_types {}
  variable _common_drop_target_types {}
  variable _unhandled_types {}

  variable _last_mouse_root_x 0
  variable _last_mouse_root_y 0
};# namespace olednd

# ----------------------------------------------------------------------------
#  Command olednd::_HandleDragEnter
# ----------------------------------------------------------------------------
proc olednd::_HandleDragEnter { drop_target typelist actionlist pressedkeys
                                rootX rootY codelist } {
  variable _typelist;                 set _typelist    $typelist
  variable _codelist;                 set _codelist    $codelist
  variable _actionlist;               set _actionlist  $actionlist
  variable _pressedkeys;              set _pressedkeys $pressedkeys
  variable _action;                   set _action      {}
  variable _common_drag_source_types; set _common_drag_source_types {}
  variable _common_drop_target_types; set _common_drop_target_types {}

  variable _last_mouse_root_x;        set _last_mouse_root_x $rootX
  variable _last_mouse_root_y;        set _last_mouse_root_y $rootY

  # puts "olednd::_HandleDragEnter: drop_target=$drop_target,\
  #       typelist=$typelist, actionlist=$actionlist,\
  #       pressedkeys=$pressedkeys, rootX=$rootX, rootY=$rootY"
  focus $drop_target

  ## Does the new drop target support any of our new types? 
  variable _types; set _types [bind $drop_target <<DropTargetTypes>>]
  if {[llength $_types]} {
    ## Examine the drop target types, to find at least one match with the drag
    ## source types...
    set supported_types [_supported_types $_typelist]
    foreach type $_types {
      foreach matched [lsearch -glob -all -inline $supported_types $type] {
        ## Drop target supports this type.
        lappend common_drag_source_types $matched
        lappend common_drop_target_types $type
      }
    }
  }

  set _action refuse_drop
  if {[info exists common_drag_source_types]} {
    set _action copy
    set _common_drag_source_types $common_drag_source_types
    set _common_drop_target_types $common_drop_target_types
    ## Drop target supports at least one type. Send a <<DropEnter>>.
    set cmd [bind $drop_target <<DropEnter>>]
    if {[string length $cmd]} {
      set cmd [string map [list %W $drop_target %X $rootX %Y $rootY \
        %CST \{$_common_drag_source_types\} \
        %CTT \{$_common_drop_target_types\} \
        %ST  \{$_typelist\}    %TT \{$_types\} \
        %A   $_action          %a  \{$_actionlist\} \
        %b   \{$_pressedkeys\} %m  \{$_pressedkeys\} \
        %D   \{\}              %e  <<DropEnter>> \
        %L   \{$_typelist\}    %%  % \
        %t   \{$_typelist\}    %T  \{[lindex $_common_drag_source_types 0]\} \
        %c   \{$_codelist\}    %C  \{[lindex $_codelist 0]\} \
        ] $cmd]
      set _action [uplevel \#0 $cmd]
    }
  }
  if {$::tkdnd::_auto_update} {update}
  # Return values: copy, move, link, ask, private, refuse_drop, default
  return $_action
};# olednd::_HandleDragEnter

# ----------------------------------------------------------------------------
#  Command olednd::_HandleDragOver
# ----------------------------------------------------------------------------
proc olednd::_HandleDragOver { drop_target pressedkeys rootX rootY } {
  variable _types
  variable _typelist
  variable _codelist
  variable _actionlist
  variable _pressedkeys
  variable _action
  variable _common_drag_source_types
  variable _common_drop_target_types

  variable _last_mouse_root_x;        set _last_mouse_root_x $rootX
  variable _last_mouse_root_y;        set _last_mouse_root_y $rootY

  # puts "olednd::_HandleDragOver: drop_target=$drop_target,\
  #             pressedkeys=$pressedkeys, rootX=$rootX, rootY=$rootY"

  if {![llength $_common_drag_source_types]} {return refuse_drop}
  set _pressedkeys $pressedkeys
  set cmd [bind $drop_target <<DropPosition>>]
  if {[string length $cmd]} {
    set cmd [string map [list %W $drop_target %X $rootX %Y $rootY \
      %CST \{$_common_drag_source_types\} \
      %CTT \{$_common_drop_target_types\} \
      %ST  \{$_typelist\}    %TT \{$_types\} \
      %A   $_action          %a  \{$_actionlist\} \
      %b   \{$_pressedkeys\} %m  \{$_pressedkeys\} \
      %D   \{\}              %e  <<DropPosition>> \
      %L   \{$_typelist\}    %%  % \
      %t   \{$_typelist\}    %T  \{[lindex $_common_drag_source_types 0]\} \
      %c   \{$_codelist\}    %C  \{[lindex $_codelist 0]\} \
      ] $cmd]
    set _action [uplevel \#0 $cmd]
  }
  if {$::tkdnd::_auto_update} {update}
  # Return values: copy, move, link, ask, private, refuse_drop, default
  return $_action
};# olednd::_HandleDragOver

# ----------------------------------------------------------------------------
#  Command olednd::_HandleDragLeave
# ----------------------------------------------------------------------------
proc olednd::_HandleDragLeave { drop_target } {
  variable _types
  variable _typelist
  variable _codelist
  variable _actionlist
  variable _pressedkeys
  variable _action
  variable _common_drag_source_types
  variable _common_drop_target_types
  variable _last_mouse_root_x
  variable _last_mouse_root_y

  if {![llength $_common_drag_source_types]} {return}
  foreach var {_types _typelist _actionlist _pressedkeys _action
               _common_drag_source_types _common_drop_target_types} {
    set $var {}
  }

  set cmd [bind $drop_target <<DropLeave>>]
  if {[string length $cmd]} {
    set cmd [string map [list %W $drop_target \
      %X $_last_mouse_root_x %Y $_last_mouse_root_y \
      %CST \{$_common_drag_source_types\} \
      %CTT \{$_common_drop_target_types\} \
      %ST  \{$_typelist\}    %TT \{$_types\} \
      %A   \{$_action\}      %a  \{$_actionlist\} \
      %b   \{$_pressedkeys\} %m  \{$_pressedkeys\} \
      %D   \{\}              %e  <<DropLeave>> \
      %L   \{$_typelist\}    %%  % \
      %t   \{$_typelist\}    %T  \{[lindex $_common_drag_source_types 0]\} \
      %u   \{$_codelist\}    %C  \{[lindex $_codelist 0]\} \
      ] $cmd]
    set _action [uplevel \#0 $cmd]
  }
  if {$::tkdnd::_auto_update} {update}
};# olednd::_HandleDragLeave

# ----------------------------------------------------------------------------
#  Command olednd::_HandleXdndDrop
# ----------------------------------------------------------------------------
proc olednd::_HandleDrop { drop_target pressedkeys rootX rootY _type data } {
  variable _types
  variable _typelist
  variable _codelist
  variable _actionlist
  variable _pressedkeys
  variable _action
  variable _common_drag_source_types
  variable _common_drop_target_types
  set data [_normalise_data $_type $data]
  # puts "olednd::_HandleDrop: drop_target=$drop_target,\
  #             pressedkeys=$pressedkeys, rootX=$rootX, rootY=$rootY,\
  #             data=\"$data\""

  if {![llength $_common_drag_source_types]} {return refuse_drop}
  set _pressedkeys $pressedkeys
  ## Try to select the most specific <<Drop>> event.
  foreach type [concat $_common_drag_source_types $_common_drop_target_types] {
    set type [_platform_independent_type $type]
    set cmd [bind $drop_target <<Drop:$type>>]
    if {[string length $cmd]} {
      set cmd [string map [list %W $drop_target %X $rootX %Y $rootY \
        %CST \{$_common_drag_source_types\} \
        %CTT \{$_common_drop_target_types\} \
        %ST  \{$_typelist\}    %TT \{$_types\} \
        %A   $_action          %a \{$_actionlist\} \
        %b   \{$_pressedkeys\} %m \{$_pressedkeys\} \
        %D   [list $data]      %e <<Drop:$type>> \
        %L   \{$_typelist\}    %% % \
        %t   \{$_typelist\}    %T \{[lindex $_common_drag_source_types 0]\} \
        %c   \{$_codelist\}    %C  \{[lindex $_codelist 0]\} \
        ] $cmd]
      return [uplevel \#0 $cmd]
    }
  }
  set cmd [bind $drop_target <<Drop>>]
  if {[string length $cmd]} {
    set cmd [string map [list %W $drop_target %X $rootX %Y $rootY \
      %CST \{$_common_drag_source_types\} \
      %CTT \{$_common_drop_target_types\} \
      %ST  \{$_typelist\}    %TT \{$_types\} \
      %A   $_action          %a \{$_actionlist\} \
      %b   \{$_pressedkeys\} %m \{$_pressedkeys\} \
      %D   [list $data]      %e <<Drop>> \
      %L   \{$_typelist\}    %% % \
      %t   \{$_typelist\}    %T \{[lindex $_common_drag_source_types 0]\} \
      %c   \{$_codelist\}    %C  \{[lindex $_codelist 0]\} \
      ] $cmd]
    set _action [uplevel \#0 $cmd]
  }
  if {$::tkdnd::_auto_update} {update}
  # Return values: copy, move, link, ask, private, refuse_drop
  return $_action
};# olednd::_HandleXdndDrop

# ----------------------------------------------------------------------------
#  Command olednd::_GetDropTypes
# ----------------------------------------------------------------------------
proc olednd::_GetDropTypes { drop_target } {
  variable _common_drag_source_types
  return $_common_drag_source_types
};# olednd::_GetDropTypes

# ----------------------------------------------------------------------------
#  Command olednd::_GetDroppedData
# ----------------------------------------------------------------------------
proc olednd::_GetDroppedData {  } {
  variable _drop_target
  return [selection get -displayof $_drop_target \
                        -selection XdndSelection -type STRING]
};# olednd::_GetDroppedData

# ----------------------------------------------------------------------------
#  Command olednd::_GetDragSource
# ----------------------------------------------------------------------------
proc olednd::_GetDragSource {  } {
  variable _drag_source
  return $_drag_source
};# olednd::_GetDragSource

# ----------------------------------------------------------------------------
#  Command olednd::_GetDropTarget
# ----------------------------------------------------------------------------
proc olednd::_GetDropTarget {  } {
  variable _drop_target
  return [winfo id $_drop_target]
};# olednd::_GetDropTarget

# ----------------------------------------------------------------------------
#  Command olednd::_supported_types
# ----------------------------------------------------------------------------
proc olednd::_supported_types { types } {
  set new_types {}
  foreach type $types {
    if {[_supported_type $type]} {lappend new_types $type}
  }
  return $new_types
}; # olednd::_supported_types

# ----------------------------------------------------------------------------
#  Command olednd::_platform_specific_types
# ----------------------------------------------------------------------------
proc olednd::_platform_specific_types { types } {
  set new_types {}
  foreach type $types {
    set new_types [concat $new_types [_platform_specific_type $type]]
  }
  return $new_types
}; # olednd::_platform_specific_types

# ----------------------------------------------------------------------------
#  Command olednd::_platform_independent_types
# ----------------------------------------------------------------------------
proc olednd::_platform_independent_types { types } {
  set new_types {}
  foreach type $types {
    set new_types [concat $new_types [_platform_independent_type $type]]
  }
  return $new_types
}; # olednd::_platform_independent_types

# ----------------------------------------------------------------------------
#  Command olednd::_normalise_data
# ----------------------------------------------------------------------------
proc olednd::_normalise_data { type data } {
  switch $type {
    CF_HDROP   {return $data}
    DND_Text   {return [list CF_UNICODETEXT CF_TEXT]}
    DND_Files  {return [list CF_HDROP]}
    default    {return $data}
  }
}; # olednd::_normalise_data

# ----------------------------------------------------------------------------
#  Command olednd::_platform_specific_type
# ----------------------------------------------------------------------------
proc olednd::_platform_specific_type { type } {
  switch $type {
    DND_Text   {return [list CF_UNICODETEXT CF_TEXT]}
    DND_Files  {return [list CF_HDROP]}
    default    {
      # variable _unhandled_types
      # if {[lsearch -exact $_unhandled_types $type] == -1} {
      #   lappend _unhandled_types $type
      # }
      return [list $type]}
  }
}; # olednd::_platform_specific_type

# ----------------------------------------------------------------------------
#  Command olednd::_platform_independent_type
# ----------------------------------------------------------------------------
proc olednd::_platform_independent_type { type } {
  switch $type {
    CF_UNICODETEXT - CF_TEXT {return DND_Text}
    CF_HDROP                 {return DND_Files}
    default    {return [list $type]}
  }
}; # olednd::_platform_independent_type

# ----------------------------------------------------------------------------
#  Command olednd::_supported_type
# ----------------------------------------------------------------------------
proc olednd::_supported_type { type } {
  # return 1;
  switch $type {
    CF_UNICODETEXT - CF_TEXT -
    FileGroupDescriptor - FileGroupDescriptorW -
    CF_HDROP {return 1}
  }
  # Is the type in our known, but unhandled types?
  variable _unhandled_types
  if {[lsearch -exact $_unhandled_types $type] != -1} {return 1}
  return 0
}; # olednd::_supported_type
