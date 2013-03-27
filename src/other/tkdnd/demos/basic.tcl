# Load the tkdnd package
## Make sure that we can find the tkdnd package even if the user has not yet
## installed the package.
if {[catch {package require tkdnd}]} {
  set DIR [file dirname [file dirname [file normalize [info script]]]]
  source $DIR/library/tkdnd.tcl
  foreach dll [glob -type f $DIR/*tkdnd*[info sharedlibextension]] {
    tkdnd::initialise $DIR/library ../[file tail $dll] tkdnd
  }
}

# Create a widget that will be a drag source.
set text_data "hello from Tk! (\u20ac)"
set file_data [list /tmp/hello\u20ac [info script]]
grid [label .text_drag_source \
  -text "Text Drag Source ($text_data)"] -sticky snew -columnspan 2
grid [label .file_drag_source \
  -text "File Drag Source ($file_data)"] -sticky snew -columnspan 2
tkdnd::drag_source register .text_drag_source
tkdnd::drag_source register .file_drag_source
bind .text_drag_source <<DragInitCmd>> \
  [list drag_source DND_Text $text_data %e %W %s %X %Y %A]
bind .text_drag_source <<DragEndCmd>>  \
  [list drag_source DND_Text $text_data %e %W %s %X %Y %A]
bind .file_drag_source <<DragInitCmd>> \
  [list drag_source DND_Files $file_data %e %W %s %X %Y %A]
bind .file_drag_source <<DragEndCmd>> \
  [list drag_source DND_Files $file_data %e %W %s %X %Y %A]
proc drag_source {type data event path state x y action} {
  switch $event {
    <<DragInitCmd>> {return [list copy $type $data]}
    <<DragEndCmd>>  {puts "Drag action: $action (type: $type)"}
  }
};# drag_source

# Add some custom clipboard formats...
frame .colours
foreach colour {red green blue navy} {
  pack [label .colours.$colour -text $colour -fg white -bg $colour] \
    -side left -padx 2
  tkdnd::drag_source register .colours.$colour
  bind .colours.$colour <<DragInitCmd>> \
   "list copy DND_Color $colour"
}
grid .colours -sticky snew -columnspan 2

# Create a widget that can be a drop target.
grid [label .drop_target -text {Drop Target:} -bg yellow] \
     [label .drop_target_value -text {                  }] -sticky snew

# Register .drop_target as a drop target of every type!
tkdnd::drop_target register .drop_target *

# During drag and drop, the drop target receives information through 3
# virtual events: <<DropEnter>> <<DropPosition>> <<DropLeave>>
# The fields that can be used in the event callbacks, are given in "cmd",
# while their "label" is given in "itemList"...
set cmd {handle_event %e %W %X %Y %ST %TT %a %A %CST %CTT %t %T %b %D}
set itemList {Event Widget X Y Source_Types Target_Types Source_Actions Action
              Common_Source_Types Common_Target_Types Types Drop_Type
              Pressed_Keys Data}
# Add the various events...
bind .drop_target <<DropEnter>>      $cmd
bind .drop_target <<DropPosition>>   $cmd
bind .drop_target <<DropLeave>>      $cmd

# Add the generic <<Drop>> event. This will be called when more specilised
# drop event is not found for the drop.
bind .drop_target <<Drop>>           $cmd

# Add a specialised <<Drop>> event, when will be called if a file is dropped.
bind .drop_target <<Drop:DND_Files>> $cmd

# Add a special drop command for DND_Color...
bind .drop_target <<Drop:DND_Color>> $cmd 

# Create some widgets for showing event info.
foreach item $itemList {
  grid [label .[string tolower $item] -text [string map {_ \ } $item]:\
          -anchor w] [label .[string tolower $item]_val -width 30 -anchor w \
          -background white -foreground navy] -sticky snew -padx 1 -pady 1
}
grid columnconfigure . 1 -weight 1
grid rowconfigure . 1 -weight 1

proc handle_event $itemList {
  global itemList
  foreach item $itemList {
    .[string tolower $item]_val configure -text [set $item]
  }
  switch -glob $Event {
    <<DropEnter>>      {$Widget configure -bg green}
    <<DropLeave>>      {$Widget configure -bg yellow}
    <<Drop:DND_Color>> {
      $Widget configure -bg yellow
      .drop_target_value configure -text $Data
      ## Convert data into a Tk color: the colour data is a list of 4 elements
      ## (red green blue opacity), expressed as Hex numbers...
      set color "#"
      for {set i 0} {$i < 3} {incr i} {
        ## Just remove the 0x prefix...
        append color [string range [lindex $Data $i] 2 end]
      }
      .drop_target_value configure -background $color -foreground white
    }
    <<Drop>> -
    <<Drop:*>>         {
      $Widget configure -bg yellow
      .drop_target_value configure -text $Data \
           -background [. cget -background] -foreground black
    }
  }
  return copy
};# handle_event
