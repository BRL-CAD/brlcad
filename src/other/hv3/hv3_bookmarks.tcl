namespace eval hv3 { set {version($Id$)} 1 }

namespace eval ::hv3::bookmarks {

  set INSTRUCTIONS {
BASICS:

This set of frames is Hv3's bookmarks-system-slash-homepage. Initially,
there are 5 bookmarks in the system. You can see them in the left-hand frame,
under the words "Start here" and above "Trash".

Changes made to the bookmarks system are saved in the state-file
selected by the -statefile command-line option. If no -statefile option
is supplied, then changes are discarded when the browser window is closed.

Initially, there are no folders in the database. Folders can be 
added to the bookmarks list by pressing the "New Folder" button. You can then
organize existing bookmarks into folders by dragging and dropping them.
Clicking the folder icon beside a folder expands or hides it's contents.
Clicking on the name of a folder loads it's contents into this window.

Similarly, new bookmarks can be added to the system and initialized manually by
clicking "New Bookmark". Or, to bookmark the current page when browsing an HTML
document, press Ctrl-B or select the "File->Bookmark Page" menu option.

To edit an existing bookmark, click on it's containing folder (or "Start
here" if the bookmark is not in any folder). Then click the corresponding
"Edit Bookmark..." button in the right-hand frame (this window).

SNAPSHOTS:

Hv3's bookmarks system has one unique feature - it allows you to save a
copy of a bookmarked document in the database along with the bookmark
itself. This copy is called a snapshot. The snapshot contains all 
document content and style information, but any scripts are discarded
before it is saved. Images are not saved. Snapshots are useful for two
reasons - they can be viewed when the original resource is not available 
(i.e. because you are not online) and they make it easier to search the
bookmarks database (see below).

You can tell if a bookmark has an accompanying snapshot by looking at
it's icon. This document (INSTRUCTIONS) has a document icon, indicating
that it has a snapshot and if the accompanying hyperlink is clicked the
default action is to load the snapshot into the right-hand frame (this 
window). The four Hv3 related links have a different icon, one that
indicates no snapshot is saved in the database for this link.

The third kind of icon a bookmark indicates that a snapshot is saved,
but clicking on the hyperlink still navigates to the original resource,
not the saved snapshot. You can toggle the status of any bookmark that
has a saved snapshot by clicking on the icon itself. Try it now with
the INSTRUCTIONS bookmark, so that you know what the third type of icon
looks like.

When bookmarking a webpage using Ctrl-B or the "File->New Bookmark" menu,
you can save a snapshot along with the bookmark by checking the "Save website
text in database" checkbox on the create new bookmark dialog.

Documents can also be imported from the local file system by clicking 
"Import Data...", then "Import Document Tree". This allows you to select a 
file-system directory to import. All *.html or *.htm documents within the
selected directory and any sub-directories are saved with snapshots to 
the database. This is useful for creating a searchable full-text database
of downloaded documentation distributed in html format.

SEARCHING:

You can search the bookmarks database using SQLite's FTS3 extension by 
entering a string in the search box at the top of this frame and 
pressing enter.
  }

  set fts3_warning ""

  proc noop {args} {}

  proc have_fts3 {} {
    if {[info exists ::hv3::have_fts3]} {return $::hv3::have_fts3}
    # Test if fts3 is present.
    #
    set ::hv3::have_fts3 [expr {![catch {::hv3::sqlitedb eval {
      SELECT * FROM bm_fulltext2 WHERE 0
    }} msg]}]
    if {$::hv3::have_fts3 == 0} {
      puts "WARNING: fts3 not loaded ($msg)"
      set ::hv3::bookmarks::save_snapshot_variable 0
    }
  }

  proc ensure_initialized {} {
    set db ::hv3::sqlitedb
    $db transaction {
	if {![$db exists {select * from sqlite_master
	    where name = 'bm_bookmark2'}]} {
	    puts stderr "initializing bookmark database.."
	    initialise_database
	}
    }
    have_fts3
  }

  proc initialise_database {} {
    set rc [catch {
      ::hv3::sqlitedb eval {
        CREATE VIRTUAL TABLE bm_fulltext2 USING fts3(
          caption, description, snapshot
        );
      }
    }]

    have_fts3

    set rc [catch {
      ::hv3::sqlitedb eval {

        -- has_snapshot interpretation:
        --
        --     0 -> There is no snapshot (snapshot column is NULL)
        --     1 -> There is a snapshot, but it is not used by default
        --     2 -> There is a snapshot, used by default
        --
        CREATE TABLE bm_bookmark2(
          bookmarkid INTEGER PRIMARY KEY,
          caption TEXT,
          uri TEXT,
          description TEXT,
          has_snapshot INTEGER NOT NULL DEFAULT 0,
          image BLOB,
          snapshot TEXT
        );

        CREATE TABLE bm_folder2(
          folderid INTEGER PRIMARY KEY,
          name TEXT
        );

        CREATE TABLE bm_tree2(
          linkid     INTEGER PRIMARY KEY,
          folderid   INTEGER,               -- Index into bm_folder2
          objecttype TEXT,                  -- Either "f" or "b"
          objectid   INTEGER,               -- Index into object table

          UNIQUE(objecttype, objectid)
        );

        CREATE INDEX bm_tree2_i1 ON bm_tree2(folderid, objecttype);
      }
    } msg]

    if {$rc == 0} {
      set folderid 0

      ::hv3::sqlitedb transaction {
        foreach {A B C} {
  "INSTRUCTIONS"                http://tkhtml.tcl.tk 1
  "Hv3/Tkhtml3 Home page"       http://tkhtml.tcl.tk 0
  "Hv3/Tkhtml3 Mailing List"    http://groups.google.com/group/tkhtml3 0
  "Hv3/Tkhtml3 CVSTrac"         http://tkhtml.tcl.tk/cvstrac/timeline 0
  "Hv3 @ freshmeat.net"         http://freshmeat.net/hv3 0
        } {
          if {$C} {
            set zSnapshot "
              <DIV style=font-family:courier>
              [string map {\n\n <P>} $::hv3::bookmarks::INSTRUCTIONS]
              </DIV>
            "
            set zSnapshotText $::hv3::bookmarks::INSTRUCTIONS
            set iSnapshot 2
            set zDesc {
              Instruction manual for Hv3.
            }
          } else {
            set zSnapshot ""
            set zDesc ""
            set zSnapshotText ""
            set iSnapshot 0
          }
          db_store_new_bookmark 0 $A $B $zDesc $iSnapshot $zSnapshot $zSnapshotText
        }
      }
    } else {
	puts stderr "Can't create bookmark tables! $msg"
    }

    if {$::hv3::have_fts3} {
      set rc [catch {::hv3::sqlitedb eval {SELECT * FROM bm_rebuild_fulltext2}}]
      if {$rc == 0} {
        set ::hv3::bookmarks::fts3_warning {
          <DIV style="font-size:small;color:darkred">
          <APPLET 
            style="float:right"
            class=bookmarks_button
            text="Rebuild Fts3 Database"
            command="::hv3::bookmarks::rebuild_fts_database"
          ></APPLET>
	    WARNING: Fts3 database requires rebuild. Fts3 is available now,
	    but the database was edited while it was not.  Until the database
            is rebuilt, search results will be unreliable.
          </DIV>
        }
      }
    } else {
      set ::hv3::bookmarks::fts3_warning {
        <DIV style="font-size:small;color:darkred">
              WARNING: Fts3 extension not available.
              Search functionality is severely compromised.
        </DIV>
      }
    }
  }

  proc init {hv3} {
    set frames [[$hv3 win] child_frames]
    set browser  [[winfo parent [$hv3 win]] browser]

    set tree_hv3 [[lindex $frames 0] hv3]
    set html_hv3 [[lindex $frames 1] hv3]

    #set controller [winfo parent [$html_hv3 win]].controller
    set controller [$html_hv3 win].controller
    set treewidget [$tree_hv3 html].treewidget

    controller $controller $browser $html_hv3 $treewidget 
    treewidget $treewidget $browser $controller

    #pack $controller -before [$html_hv3 win] -side top -fill x
    grid configure $controller -row 0 -column 0 -columnspan 2 -sticky ew
    place $treewidget -x 0.0 -y 0.0 -relwidth 1.0 -relheight 1.0

    $treewidget populate_tree
    focus ${controller}.filter

    foreach node [$html_hv3 html search applet] {
      $controller applet $node
    }
  }

  proc dropobject {target drag insertAfter inFolder} {
    foreach {drag_type drag_id}     $drag {}
    foreach {target_type target_id} $target {}

    set drag_type [string range $drag_type 0 0]
    set target_type [string range $target_type 0 0]

    set targetfolder ""
    set new_items [list]

    set N [::hv3::sqlitedb one {SELECT max(linkid) FROM bm_tree2}]
    incr N
    
    if {($target_type eq "f") && (
         $inFolder || $drag_type eq "b" || $target_id <= 0
        )
    } {
      set targetfolder $target_id
      lappend new_items $N $target_id $drag_type $drag_id
    } else {
      set targetfolder [::hv3::sqlitedb one {
        SELECT folderid 
        FROM bm_tree2 
        WHERE objecttype = $target_type AND objectid = $target_id
      }]
    }

    ::hv3::sqlitedb eval {
      SELECT objecttype, objectid 
      FROM bm_tree2
      WHERE folderid = $targetfolder
      ORDER BY linkid ASC
    } {
      set isTarget [expr {
          $objecttype eq $target_type && 
          $objectid == $target_id
      }]
      if {!$insertAfter && $isTarget} {
        incr N
        lappend new_items $N $targetfolder $drag_type $drag_id
      }

      if {$objecttype ne $drag_type || $objectid != $drag_id} {
        incr N
        lappend new_items $N $targetfolder $objecttype $objectid
      }

      if {$insertAfter && $isTarget} {
        incr N
        lappend new_items $N $targetfolder $drag_type $drag_id
      }
    }

    foreach {a b c d} $new_items {
      ::hv3::sqlitedb eval {
        REPLACE INTO bm_tree2(linkid, folderid, objecttype, objectid)
        VALUES($a, $b, $c, $d)
      }
    }
  }

  # The argument is a tkhtml3 widget. This proc generates an HTML 
  # document equivalent to the one currently loaded into the widget.
  # All style information is inlined in a <STYLE> block in the 
  # document head.
  #
  proc create_snapshot {hv3} {
    set zTitle ""
    set zStyle "\n"
    set zBody ""
    set zBase ""

    set html [$hv3 html]
    set zBase [$hv3 resolve_uri ""]

    foreach rule [$html _styleconfig] {
      foreach {selector properties origin} $rule {}
      if {$origin eq "agent"} continue
      append zStyle "$selector { $properties }\n"
    }

    set titlenode [$html search title]
    if {$titlenode ne ""} {
      set child [lindex [$titlenode children] 0]
      if {$child ne ""} { set zTitle [$child text] }
    }

    set bodynode [$html search body]
    set zBody [serialize_node_content $bodynode]

    return [subst {
      <HTML>
        <HEAD>
          <TITLE>$zTitle</TITLE>
          <STYLE>$zStyle</STYLE>
          <BASE href="$zBase"></BASE>
        </HEAD>
        <BODY>$zBody</BODY>
      </HTML>
    }]
  }

  proc serialize_node {node} {
    set tag [$node tag]
    if {$tag eq ""} {
      return [string map {< &lt; > &gt;} [$node text -pre]]
    } else {
      set attr ""
      foreach {k v} [$node attribute] {
        set val [string map [list "\x22" "\x5C\x22"] $v]
        append attr " $k=\"$val\""
      }
      set content [serialize_node_content $node]
      return "<${tag}${attr}>$content</${tag}>"
    }
  }

  proc serialize_node_content {node} {
    set zRes ""
    foreach child [$node children] {
      append zRes [serialize_node $child]
    }
    return $zRes
  }


  ::snit::widget treewidget {

    # The browser (::hv3::browser) containing this frameset
    #
    variable myBrowser

    # The controller that controls the left-hand frame of the frameset.
    #
    variable myController

    # Id of the current canvas text item that the mouse is hovering over.
    # A negative value means the mouse is not currently over any item.
    #
    variable myCurrentHover -1

    variable myTextId -array ""

    variable myIconId -array ""

    variable myTreeStart 0

    variable myOpenFolder -array ""

    variable myDragX ""
    variable myDragY ""
    variable myDragObject ""
    variable myPressedX ""
    variable myPressedY ""
    variable myPressedItem ""
    variable myPressedItemColor ""

    method click_new_folder {} {
      ${win}.newfolder configure -state normal
      ::hv3::bookmarks::new_folder $self
    }
    method click_new_bookmark {} {
      ${win}.newbookmark configure -state normal
      ::hv3::bookmarks::new_bookmark ""
    }

    method click_importexport {} {
      ${win}.importexport configure -state normal

      set initial /home/dan/.mozilla/firefox/jghthwxj.default/bookmarks.html
      set initialdir [file dirname $initial]
      set f [tk_getOpenFile -initialfile $initial -initialdir $initialdir \
          -filetypes [list \
              {{Html Files} {.html}} \
              {{All Files} *}
          ]
      ]
      if {$f eq ""} return

      set fd [open $f]
      set zBookmarks [read $fd]
      close $fd

      set H [html .boomarksparser]
      $H parse -final $zBookmarks

      unset -nocomplain zTitle
      foreach N [$H search dl,h3] {

        if {[$N tag] eq "dl"} {
          if {![info exists zTitle]} {
            set zTitle "Imported Bookmarks"
          }

          # Create the bm_folder2 record.
          ::hv3::sqlitedb eval {
            INSERT INTO bm_folder2(name) VALUES($zTitle);
          }
          set iFolderId [::hv3::sqlitedb last_insert_rowid]
          set imported_folders($N) $iFolderId

          set iParentId 0
          for {set P [$N parent]} {$P ne ""} {set P [$P parent]} {
            if {[$P tag] eq "dl"} {
              set iParentId $imported_folders($P)
              break
            }
          }

          # Create the bm_tree2 record.
          ::hv3::sqlitedb eval {
            INSERT INTO bm_tree2(folderid, objecttype, objectid) 
            VALUES($iParentId, 'f', $iFolderId)
          }

        } else {
          set zTitle [[lindex [$N children] 0] text]
        }
      }

      foreach N [$H search {dl a[href]}] {
        set zCaption [[lindex [$N children] 0] text]
        set zUri [$N attribute href]

        for {set P [$N parent]} {$P ne ""} {set P [$P parent]} {
          if {[$P tag] eq "dl"} {
            set iParentId $imported_folders($P)
            break
          }
        }

        ::hv3::bookmarks::db_store_new_bookmark \
            $iParentId $zCaption $zUri "" 0 "" ""
      }

      destroy $H

      $self populate_tree
    }

    constructor {browser controller} {
      set myBrowser $browser
      set myController $controller

      # On certain systems (linux using xft), it is very expensive
      # to grab a font for the first time. So create a label here to
      # make sure there is always at least one reference to the font
      # Hv3DefaultBold. There are always lot's of references to the
      # other font used by this widget - Hv3DefaultFont.
      #
      label ${win}.fontrefholder -font Hv3DefaultBold

      # Set up the button widgets used by this gui.
      #
      menubutton ${win}.importexport \
          -text "Import Data..."     \
          -font Hv3DefaultFont       \
          -borderwidth 1             \
          -relief raised             \
          -pady 1                    

      set menu [::hv3::menu ${win}.importexport.menu]
      ${win}.importexport configure -menu $menu
      $menu add command \
          -label "Auto-import Mozilla bookmarks"            \
          -command ::hv3::bookmarks::import_bookmarks
      $menu add command \
          -label "Import Mozilla bookmarks.html file"       \
          -command ::hv3::bookmarks::import_bookmarks_manual
      $menu add command \
          -label "Import Document Tree" \
          -command [list ::hv3::bookmarks::import_tree]
      $menu add command \
          -label "Index Hv3 DOM Reference" \
          -command [list ::hv3::bookmarks::import_dom]
      ::hv3::button ${win}.newfolder                        \
          -text "New Folder"                                \
          -command [list $self click_new_folder] 
      ::hv3::button ${win}.newbookmark                      \
          -text "New Bookmark"                              \
          -command [list $self click_new_bookmark] 
      ::hv3::button ${win}.expand                           \
          -text "Expand All"                                \
          -command [list $self expand_all]
      ::hv3::button ${win}.collapse                         \
          -text "Collapse All"                              \
          -command [list $self collapse_all]


      # Create the canvas on which the tree is drawn
      #
      ::hv3::scrolled canvas ${win}.canvas -background white -propagate 1
      ${win}.canvas configure -yscrollincrement 1

      # Set up the geometry manager
      #
      grid ${win}.importexport              -columnspan 2 -sticky ew     ;# 0
      grid ${win}.newfolder                 -columnspan 2 -sticky ew     ;# 1
      grid ${win}.newbookmark               -columnspan 2 -sticky ew     ;# 2
      grid ${win}.canvas                    -columnspan 2 -sticky nsew   ;# 3
      grid ${win}.expand ${win}.collapse    -sticky ew                   ;# 4
      grid rowconfigure    ${win} 3 -weight 1
      grid columnconfigure ${win} 0 -weight 1
      grid columnconfigure ${win} 1 -weight 1

      bind ${win}.canvas <Motion>          [list $self motion_event %x %y]
      bind ${win}.canvas <Leave>           [list $self leave_event]
      bind ${win}.canvas <ButtonPress-1>   [list $self press_event %x %y]
      bind ${win}.canvas <ButtonRelease-1> [list $self release_event %x %y]
    }

    method open_folders {objecttype objectid} {
      set parent [::hv3::sqlitedb one {
        SELECT folderid 
        FROM bm_tree2 
        WHERE objecttype = $objecttype AND objectid = $objectid
      }]
      if {$parent eq "" || $parent == 0} return
      set myOpenFolder($parent) 1
      if {$parent == -1} return

      $self open_folders f $parent
    }

    method seek_tree {} {
      #set C ${win}.canvas.widget
      set C ${win}.canvas
      foreach {page pageid} [$myController current] {}
      if {$page ne "folder" && $page ne "snapshot"} return

      if {$page eq "snapshot"} {set page bookmark}
      set tag "${page}$pageid"

      set bbox [$C bbox $tag]
      if {$bbox eq ""} return

      foreach {dummy1 top dummy2 bottom} $bbox {}
      if {$top < [$C canvasy 0]} {
        set pixels [expr {int($top - [$C canvasy 0] - 20)}]
        $C yview scroll $pixels units
      } elseif {$bottom > [$C canvasy [winfo height $C]]} {
        set pixels [expr {int($bottom + 20 - [$C canvasy [winfo height $C]])}]
        $C yview scroll $pixels units
      }
    }

    method populate_tree {{isAutoOpen 0}} {
      #set C ${win}.canvas.widget
      set C ${win}.canvas

      set y 20
      set x 10
      set yincr [expr [font metrics Hv3DefaultFont -linespace] + 5]

      set myPressedItem ""
      $C delete all
      array unset myTextId
      array unset myIconId

      set myTreeStart $y
      incr y $yincr
      ::hv3::sqlitedb transaction { 

        if {$isAutoOpen} {
          foreach {page pageid} [$myController current] {}
          if {$page eq "folder" || $page eq "snapshot"} {
            if {$page eq "folder"} {
              set page f
            } else {
              set page b
            }
            $self open_folders $page $pageid
          }
        }
  
        # Color for special links.
        #
        set c darkblue

        # Create the special "bookmarks" folder.
        #
        set f Hv3DefaultFont
        if {[$myController current] eq "folder 0"} {
          set f Hv3DefaultBold
        }
        set tid [
            $C create text $x $y -text "Start here" -anchor sw -font $f -fill $c
        ]
        set myTextId($tid) {folder 0 "Click to view root folder"}
        incr y $yincr
        incr y $yincr

        # Create the tree of user bookmarks.
        #
        set y [$self drawSubTree $x $y 0]
        incr y $yincr
  
        # Create the special "trash" folder.
        #
        set f Hv3DefaultFont
        set x2 [expr {$x + [image width itrash] + 5}]
        if {[$myController current] eq "folder -1"} {
          set f Hv3DefaultBold
        }
        set tid [
            $C create text $x2 $y -text "Trash" -anchor sw -font $f -fill $c
        ]
        set myTextId($tid)  "folder -1"
        set myTextId($tid) "folder -1 {Click to view trashcan contents}"
        if {[info exists myOpenFolder(-1)]} {
          set tid [$C create image $x $y -image iopentrash -anchor sw]
        } else {
          set tid [$C create image $x $y -image itrash -anchor sw]
        }
        set myIconId($tid) "folder -1"
        incr y $yincr
        if {[info exists myOpenFolder(-1)]} {
          set y [$self drawSubTree [expr $x + $yincr] $y -1]
        }

        # Create the special "history" folder.
        #
        set f Hv3DefaultFont
        if {[$myController current] eq "recent -1"} {
          set f Hv3DefaultBold
        }
        set tid [
            $C create text $x2 $y -text "History" -anchor sw -font $f -fill $c
        ]
        set myTextId($tid) {recent -1 "Click to view all recently viewed URIs"}
  
        $C configure -scrollregion [concat 0 0 [lrange [$C bbox all] 2 3]]

        if {$isAutoOpen} {
          $self seek_tree
        }
      }
    }

    method set_drag_tag {item} {
      set C ${win}.canvas.widget
      set tag1 [lindex [$C itemcget $item -tags] 0]
      if {[string range $tag1 0 7] eq "bookmark" ||
          [string range $tag1 0 5] eq "folder"
      } {
        $C itemconfigure $tag1 -tags draggable
        $C raise draggable

        set info ""
        if {[info exists myTextId($item)]} {
          set info [lrange $myTextId($item) 0 1]
        } elseif {[info exists myIconId($item)]} {
          set info $myIconId($item)
        }

        set myDragObject $info
      }
      return
    }

    method motion_event {x y} {
      set C ${win}.canvas.widget
      set x [$C canvasx $x]
      set y [$C canvasy $y]
      set hover [$C find overlapping $x $y $x $y]
      if {$myCurrentHover>=0 && $hover != $myCurrentHover} {
        $C delete underline
      }

      if {
          $myDragObject eq "" &&
          $myPressedItem ne "" && 
          (abs($myPressedX-$x) > 6 || abs($myPressedY-$y)>6)
      } {
        $self set_drag_tag $myPressedItem
        if {$myPressedItemColor ne ""} {
          $C itemconfigure $myPressedItem -fill $myPressedItemColor
          set myPressedItemColor ""
          $C delete underline
        }
      }

      if {$myDragObject ne ""} {
        $C move draggable [expr {$x-$myDragX}] [expr {$y-$myDragY}]
        set myDragX $x
        set myDragY $y
        return
      }

      if {[$C type $hover] eq "text"} {
        set bbox [$C bbox $hover]
        set y [expr [lindex $bbox 3] - 2]
        $C create line [lindex $bbox 0] $y [lindex $bbox 2] $y -tags underline
        $C configure -cursor hand2
        $myBrowser set_frame_status [lindex $myTextId($hover) 2]
      } else {
        set hover -1
        $C configure -cursor ""
        $myBrowser set_frame_status ""
      }

      set myCurrentHover $hover
    }

    method leave_event {} {
      set C ${win}.canvas
      $C delete underline
      set myCurrentHover -1
      $myBrowser set_frame_status ""
      $C configure -cursor ""
    }

    method press_event {x y} {
      set C ${win}.canvas
      set x [$C canvasx $x]
      set y [$C canvasy $y]
      set myPressedX $x
      set myPressedY $y
      set myDragX $x
      set myDragY $y

      set myPressedItem [$C find overlapping $x $y $x $y]
      if {[$C type $myPressedItem] eq "text"} {
        set myPressedItemColor [$C itemcget $myPressedItem -fill]
        $C itemconfigure $myPressedItem -fill red
      } else {
        set myPressedItemColor ""
      }
    }

    method drop {target drag insertAfter} {
      ::hv3::bookmarks::dropobject $target $drag $insertAfter \
          [info exists myOpenFolder([lindex $target 1])]
      ::hv3::bookmarks::refresh_gui
    }

    method release_event {x y} { 
      if {$myPressedItem eq ""} return

      set C ${win}.canvas
      if {[$C type $myPressedItem] eq "text"} {
        $C itemconfigure $myPressedItem -fill $myPressedItemColor
      }
      
      set x [$C canvasx $x]
      set y [$C canvasy $y]

      if {$myDragObject ne ""} {
        set item [lindex [
          $C find overlapping               \
              [expr {$x-300}] [expr {$y-3}] \
              [expr {$x+300}] [expr {$y+3}]
        ] 0]

        # We were dragging either a bookmark or a folder. Let's see what
        # we dropped it over:
        set dropid ""
        if {[info exists myTextId($item)]} {
          if {[lindex $myTextId($item) 0] ne "recent"} {
            set dropid [lrange $myTextId($item) 0 1]
          }
        } elseif {[info exists myIconId($item)]} {
          set dropid [lrange $myIconId($item) 0 1]
        }

        if {$dropid eq $myDragObject} {
          set dropid ""
        }
        if {$dropid eq "" && $y <= $myTreeStart} {
          set dropid [list folder 0]
        }

        ::hv3::sqlitedb transaction {
          if {$dropid ne "" && $myDragObject ne ""} {
            $self drop $dropid $myDragObject [expr $myDragY > $myPressedY]
          }
        
          $self populate_tree
        }
      } else {
        set item [lindex [$C find overlapping $x $y $x $y] 0]
        if {$item eq $myPressedItem} {
          $self click_event $item
        }
      }
   
      # Clear the DragObject and PressedItem variables. This has to
      # be in a [catch] block as the call to [click_event] above may
      # have destroyed the whole widget.
      catch {
        set myDragObject ""
        set myPressedItem ""
      }
    }

    method click_event {click} {
      set C ${win}.canvas

      if {$click eq ""} return
      set clicktype [$C type $click]
 
      if {$clicktype eq "text"} {
        foreach {type id msg} $myTextId($click) {}
        switch -exact -- $type {
          folder {
            if {![info exists myOpenFolder($id)]} {
              set myOpenFolder($id) 1
            }
            #$myController time populate_folder $id
            $myController goto "home://bookmarks/folder/$id"
          }
          bookmark {
            # Find the URI for this bookmark and send the browser there :)
            ::hv3::sqlitedb eval {
              SELECT uri, has_snapshot FROM bm_bookmark2 WHERE bookmarkid = $id
            } {
              if {$has_snapshot == 2} {
                $myController goto "home://bookmarks/snapshot/$id"
              } elseif {$uri ne ""} {
                $myBrowser goto $uri
              }
            }
          }
          recent {
            $myController goto "home://bookmarks/recent/$id"
          }
        }
      } elseif {$clicktype eq "image"} {
        foreach {type id} $myIconId($click) {}
        switch -exact -- $type {
          folder {
            if {[info exists myOpenFolder($id)]} {
              unset myOpenFolder($id)
            } else {
              set myOpenFolder($id) 1
            }
            $self populate_tree
          }
          bookmark {
            ::hv3::sqlitedb eval {
              UPDATE bm_bookmark2 SET has_snapshot = 
                CASE WHEN has_snapshot = 0 THEN 0 
                     WHEN has_snapshot = 1 THEN 2
                     ELSE 1 END
              WHERE bookmarkid = $id
            }
            $self populate_tree
          }
        }
      }
    }

    method expand_all {} {
      ::hv3::sqlitedb transaction {
        ::hv3::sqlitedb eval { SELECT folderid FROM bm_folder2 } {
          set myOpenFolder($folderid) 1
        }
        $self populate_tree
      }
    }
    method collapse_all {} {
      array unset myOpenFolder
      $self populate_tree
    }

    method drawSubTree {x y folderid {tags ""}} {
      set C ${win}.canvas

      set yincr [expr [font metrics Hv3DefaultFont -linespace] + 5]
      set x2 [expr {$x + [image width idir] + 5}]
      foreach {page pageid} [$myController current] {}

      ::hv3::sqlitedb eval {
        SELECT bookmarkid, caption, uri, has_snapshot
        FROM bm_tree2, bm_bookmark2
        WHERE 
          bm_tree2.folderid = $folderid AND
          bm_tree2.objecttype = 'b' AND
          bm_tree2.objectid = bm_bookmark2.bookmarkid
        ORDER BY bm_tree2.linkid
      } {
        set font Hv3DefaultFont
        if {$page eq "snapshot" && $pageid eq $bookmarkid} {
          set font Hv3DefaultBold
        }
        set t [concat "bookmark$bookmarkid" $tags]
        if {$caption eq ""} {set caption $uri}

        switch -exact -- $has_snapshot {
          0 { set image iworld }
          1 { set image iworldfile }
          2 { set image ifile }
        }
        set tid [$C create image $x $y -image $image -anchor sw -tags $t]
        set myIconId($tid) [list bookmark $bookmarkid]

        set tid [
          $C create text $x2 $y -text $caption -anchor sw -font $font -tags $t
        ]
        set myTextId($tid) [list bookmark $bookmarkid "hyper-link: $uri"]
        incr y $yincr
      }

      ::hv3::sqlitedb eval {
        SELECT 
          bm_folder2.folderid AS thisfolderid, 
          name
        FROM bm_tree2, bm_folder2
        WHERE 
          bm_tree2.folderid = $folderid AND
          bm_tree2.objecttype = 'f' AND
          bm_tree2.objectid = bm_folder2.folderid
        ORDER BY bm_tree2.linkid 
      } {
        set font Hv3DefaultFont
        if {$page eq "folder" && $pageid eq $thisfolderid} {
          set font Hv3DefaultBold
        }
        set t [concat "folder$thisfolderid" $tags]

        set image idir
        if {[info exists myOpenFolder($thisfolderid)]} {
          set image iopendir
        }

        set tid [$C create image $x $y -image $image -anchor sw -tags $t]
        set myIconId($tid) [list folder $thisfolderid]
        set tid [
          $C create text $x2 $y -text $name -anchor sw -font $font -tags $t
        ]
        set myTextId($tid) \
            [list folder $thisfolderid "Click to view folder \"$name\""]
        incr y $yincr
        if {[info exists myOpenFolder($thisfolderid)]} {
          set y [$self drawSubTree [expr $x + $yincr] $y $thisfolderid $t]
        }
      }

      return $y
    }
  }

  proc requestpage {zPath} {
    set components [lrange [split $zPath /] 1 end]

    set page   [lindex $components 0]
    set pageid [lindex $components 1]

    ::hv3::sqlitedb transaction {
      switch -exact -- $page {
        images   { return [idir data -format ppm] }
        recent   { request_recent $pageid }
        folder   { request_folder $pageid }
        search   { request_search $pageid }
        snapshot { request_snapshot $pageid }
        default {
          error [join "
            {Bad page: \"$page\"}
            {- should be recent, folder, bookmark or search}
          "]
        }
      }
    }
  }

  proc start_page {title} {
    set Template {
      <STYLE>
        :visited { color: darkblue; }
        .lastvisited, .uri, .description { 
          display: block ; padding-left: 10ex 
        }
        .time,.info {font-size: small; text-align: center; font-style: italic}
        .info { margin: 0 10px }

        a[bookmarks_page] { color: purple }

        .uri a[href] { font-size: small ; color: green }

        .snippet {
            margin: 0 10ex;
            font-size: small;
        }

        .subfolder {
            float:left;
            padding: 0;
            padding-left:50px;
            min-width:20ex;
            white-space:nowrap;
        }
        .subfolder a {
            display: list-item;
            list-style-image: url(home://bookmarks/images/idir);
        }

        .viewbookmark { float:right }
        .viewfolder   { width:90% ; margin: 5px auto }
        .delete,.rename { float:right }

        .edit { width: 90% }

        a[href]:hover  { text-decoration: none; }
        a[href]:active { color: red; }

        h1 { font-size: 1.4em; }
      </STYLE>
    }
    return "$Template<H1>$title</H1>"
  }

  proc pp_bookmark_record {N id caption uri description has_snapshot snippet} {
    if {$caption eq ""} {set caption $uri}
    if {$has_snapshot > 0} {
      set A_attr "
        href=\"snapshot of $uri\" bookmarks_page=\"snapshot $id\"
      "
    } else {
      set A_attr "href=\"$uri\""
    }
    set applet ""
    if {$id != 0} {
      set applet "
        <APPLET 
          style=\"float:right\"
          class=bookmarks_button
          text=\"Edit Bookmark...\"
          command=\"::hv3::bookmarks::edit_bookmark $id\"
        ></APPLET>
      "
    }
    subst -nocommands {
      <DIV style="margin:1em">
        <DIV>
          <SPAN style="white-space:nowrap">
            ${N}. <A $A_attr>$caption</A>
          </SPAN>
          $applet
        </DIV>
        <DIV class="snippet">
          $snippet
        </DIV>
        <DIV class="description">$description</DIV>
        <SPAN class="uri"><A href="$uri">$uri</A></SPAN>
      </DIV>
    }
  }

  proc request_recent {nLimit} {
    set zRes "<APPLET bookmarks_page=\"recent $nLimit\"></APPLET>"

    append zRes [start_page "Recently Visited URIs"]

    set sql { 
      SELECT uri, title, lastvisited 
      FROM visiteddb 
      WHERE title IS NOT NULL
      ORDER BY oid DESC 
      LIMIT 200
    }
    set N 0
    ::hv3::sqlitedb eval { 
      SELECT uri, title, lastvisited 
      FROM visiteddb 
      WHERE title IS NOT NULL
      ORDER BY oid DESC 
      LIMIT 200
    } {
      incr N
      set description "Last visited: [clock format $lastvisited]"
      append zRes [pp_bookmark_record $N 0 $title $uri $description 0 ""]
    }
    return $zRes
  }

  proc html_to_text {zHtml} {
    .tmphv3 reset
    .tmphv3 parse -final $zHtml
    return [.tmphv3 html text text]
  }

  proc rebuild_fts_database {} {
    html .tmphv3
    ::hv3::sqlitedb transaction {
      ::hv3::sqlitedb eval {
        DELETE FROM bm_fulltext2;
        INSERT INTO bm_fulltext2(docid, caption, snapshot) 
          SELECT bookmarkid, caption, html_to_text(snapshot)
          FROM bm_bookmark2;
        DROP TABLE IF EXISTS bm_rebuild_fulltext2;
      }
      set ::hv3::bookmarks::fts3_warning {}
    }
    destroy .tmphv3
    refresh_gui
  }

  proc request_search {search} {
    set zRes "<APPLET bookmarks_page=\"search {$search}\"></APPLET>"
    append zRes [start_page "Search for $search"]

    append zRes $::hv3::bookmarks::fts3_warning

    if {$::hv3::have_fts3} {
      set sql {
        SELECT 
          bookmarkid, 
          bm_bookmark2.caption AS caption,
          bm_bookmark2.uri AS uri,
          bm_bookmark2.description AS description,
          bm_bookmark2.has_snapshot AS has_snapshot,
          snippet(bm_fulltext2) AS snippet
        FROM bm_fulltext2, bm_bookmark2
        WHERE 
          bm_fulltext2 MATCH $search AND 
          bm_fulltext2.docid = bookmarkid
        ORDER BY result_transform(offsets(bm_fulltext2)) DESC
        LIMIT 500
      }
    } else {
      set like "%${search}%"
      set sql {
        SELECT 
          bookmarkid, 
          bm_bookmark2.caption AS caption,
          bm_bookmark2.uri AS uri,
          bm_bookmark2.description AS description,
          bm_bookmark2.has_snapshot AS has_snapshot,
          '' AS snippet
        FROM bm_bookmark2
        WHERE 
          caption     LIKE $like OR
          description LIKE $like OR
          uri         LIKE $like
        ORDER BY ((caption LIKE $like) * 100 + (description LIKE $like)) DESC
        LIMIT 500
      }
    }

    set N 0
    ::hv3::sqlitedb eval $sql {
      incr N
      append zRes [pp_bookmark_record \
          $N $bookmarkid $caption $uri $description $has_snapshot $snippet
      ]
    }
    return $zRes
  }

  proc request_snapshot {bookmarkid} {
    set zRes "<APPLET bookmarks_page=\"snapshot $bookmarkid\"></APPLET>"
    append zRes [::hv3::sqlitedb one {
      SELECT snapshot FROM bm_bookmark2 WHERE bookmarkid = $bookmarkid
    }]
    return $zRes
  }

  proc request_folder {folderid} {

    set zRes "<APPLET bookmarks_page=\"folder $folderid\"></APPLET>"

    if {$folderid > 0} {
      set name [::hv3::sqlitedb one {
        SELECT name FROM bm_folder2 WHERE folderid = $folderid
      }]
      append zRes [start_page "$name
        <APPLET 
          style=\"float:right\"
          class=bookmarks_button
          text=\"Rename Folder...\"
          command=\"::hv3::bookmarks::rename_folder $folderid\"
        ></APPLET>
      "]
    } else {
      if {$folderid == 0} {
        append zRes [start_page "Bookmarks"]
      }
      if {$folderid == -1} {
        append zRes [start_page "Trash
          <APPLET 
            style=\"float:right\"
            class=bookmarks_button
            text=\"Permanently delete trashcan contents\"
            command=\"::hv3::bookmarks::delete_folder_contents $folderid\"
          ></APPLET>
        "]
      }
    }
 
    append zRes $::hv3::bookmarks::fts3_warning

    set N 0
    ::hv3::sqlitedb eval {
      SELECT bookmarkid, caption, uri, description, image, has_snapshot
      FROM bm_tree2, bm_bookmark2
      WHERE 
        bm_tree2.folderid = $folderid AND
        bm_tree2.objecttype = 'b' AND
        bm_tree2.objectid = bm_bookmark2.bookmarkid
      ORDER BY(bm_tree2.linkid)
    } {
      incr N
      set snippet ""
      append zRes [pp_bookmark_record \
          $N $bookmarkid $caption $uri $description $has_snapshot $snippet
      ]
    }
    append zRes <HR>
    ::hv3::sqlitedb eval {
      SELECT linkid, folderid AS thisfolderid, '.. (parent folder)' AS name
      FROM bm_tree2 
      WHERE objecttype = 'f' AND objectid = $folderid

      UNION ALL

      SELECT linkid, bm_folder2.folderid AS thisfolderid, name
      FROM bm_tree2, bm_folder2
      WHERE 
        bm_tree2.folderid = $folderid AND
        bm_tree2.objecttype = 'f' AND
        bm_tree2.objectid = bm_folder2.folderid
      ORDER BY 1
    } {
      append zRes [subst -nocommands {
          <DIV class=subfolder>
            <A href="." bookmarks_page="folder $thisfolderid">$name</A>
          </DIV>
      }]
      set foldername($thisfolderid) $name
    }
    return $zRes
  }


  ::snit::widget controller {
    variable myHv3
    variable myBrowser
    variable myTree

    variable myPage folder
    variable myPageId 0

    delegate option -width to hull
    delegate option -height to hull

    method goto {args} {eval $myHv3 goto $args}
    method refresh {} {
      $myHv3 seek_to_yview [lindex [$myHv3 yview] 0]
      $myHv3 goto "" -nosave -cachecontrol "no-cache"
    }

    method applet {node} {
      set bookmarks_page [$node attr -default "" bookmarks_page]
      if {$bookmarks_page ne ""} {
        eval $self set_page_id $bookmarks_page
      }

      set class [$node attr -default "" class]
      if {$class eq "bookmarks_button"} {
        set w [$myHv3 html].document.button_[string map {: _} $node]
        ::hv3::button $w -text [$node attr text] -command [$node attr command]
        $node replace $w -deletecmd [list destroy $w]
      }
    }

    constructor {browser hv3 tree} {
      set myHv3 $hv3
      set myTree $tree
      set myBrowser $browser

      $myHv3 html handler node applet [mymethod applet]

      set searchcmd [subst -nocommands {
          $myHv3 goto "home://bookmarks/search/[${win}.filter get]"
      }]

      ::hv3::label ${win}.filter_label -text "Search: " -background white
      ::hv3::entry  ${win}.filter
      ::hv3::button ${win}.go -text Go -command $searchcmd
  
      pack ${win}.filter_label -side left -padx 5 -pady 10
      pack ${win}.filter -side left
      pack ${win}.go -side left

      bind ${win}.filter <KeyPress-Return> $searchcmd

      $hv3 configure -targetcmd [list $self TargetCmd]

      $hull configure -background white
    }

    method TargetCmd {node} {
      set bookmarks_page [$node attr -default "" bookmarks_page]
 
      if {$myPage eq "snapshot" && [$node tag] eq "a"} {
        set obj [::tkhtml::uri [$myHv3 resolve_uri [$node attr href]]]
        set uri [$obj get_no_fragment]

        set bookmarkid [::hv3::sqlitedb one {
          SELECT bookmarkid FROM bm_bookmark2 
          WHERE uri = $uri AND has_snapshot
          ORDER BY bookmarkid DESC
        }]
        set fragment [$obj fragment]
        $obj destroy
        if {$bookmarkid ne ""} {
          set internal_uri "home://bookmarks/snapshot/$bookmarkid"
          if {$fragment ne ""} {
            append internal_uri #$fragment
          }
          $myHv3 goto $internal_uri
          return ::hv3::bookmarks::noop
        }
      }
      
      if {$bookmarks_page ne ""} {
        $myHv3 goto "home://bookmarks/[join $bookmarks_page /]"
        return ::hv3::bookmarks::noop
      }
      return [$myBrowser hv3]
    }

    method time {args} {
      set t [time {eval $self $args}]
      $myHv3 parse "
        <p class=time>
          Page generated in [lrange $t 0 1]
        </p>
      "
    }

    method start_page {title} {
      $myHv3 reset 0
      $myHv3 parse {
        <STYLE>
          :visited { color: darkblue; }
          .lastvisited, .uri, .description { 
            display: block ; padding-left: 10ex 
          }
          .time,.info {font-size: small; text-align: center; font-style: italic}
          .info { margin: 0 10px }

          a[bookmarks_page] { color: purple }

          .uri a[href] { font-size: small ; color: green }

          .snippet {
              margin: 0 10ex;
              font-size: small;
          }

          .viewbookmark { float:right }
          .viewfolder   { width:90% ; margin: 5px auto }
          .delete,.rename { float:right }

          .edit { width: 90% }

          a[href]:hover  { text-decoration: none; }
          a[href]:active { color: red; }

          h1 { font-size: 1.4em; }
        </STYLE>
      }
      $myHv3 parse "<H1>$title</H1>"
    }

    proc configurecmd {win args} {
      set descent [font metrics [$win cget -font] -descent]
      set ascent  [font metrics [$win cget -font] -ascent]
      expr {([winfo reqheight $win] + $descent - $ascent) / 2}
    }

    method set_page_id {page pageid} { 
      set myPage $page
      set myPageId $pageid
      $myTree populate_tree 1
    }
    method sub_page_id {pageid} {
      set myPageId $pageid
    }
    method current {} { list $myPage $myPageId }
  }

  proc db_delete_folder_contents {folderid} {
    ::hv3::sqlitedb eval {
      SELECT objectid AS thisfolderid 
      FROM bm_tree2 
      WHERE folderid = $folderid AND objecttype = 'f'
    } {
      db_delete_folder_contents $thisfolderid
    }

    if {$::hv3::have_fts3} {
      ::hv3::sqlitedb eval {
        DELETE FROM bm_fulltext2 WHERE rowid IN (
          SELECT objectid FROM bm_tree2 
          WHERE folderid = $folderid AND objecttype = 'b'
        );
      }
    } else {
      catch { ::hv3::sqlitedb eval {CREATE TABLE bm_rebuild_fulltext2(a)} }
    }

    ::hv3::sqlitedb eval {
      DELETE FROM bm_folder2 WHERE folderid IN (
        SELECT objectid FROM bm_tree2 
        WHERE folderid = $folderid AND objecttype = 'f'
      );
      DELETE FROM bm_bookmark2 WHERE bookmarkid IN (
        SELECT objectid FROM bm_tree2 
        WHERE folderid = $folderid AND objecttype = 'b'
      );
      DELETE FROM bm_tree2 WHERE folderid = $folderid
    }
  }
  proc delete_folder_contents {folderid} {
    ::hv3::sqlitedb transaction {
      db_delete_folder_contents $folderid
    }
    refresh_gui
  }

  #
  # Each match is worth a number of points based on whether it was 
  # found in the caption (0), description (1) or snapshot (2) column.
  # Matches with large scores are displayed before those with smaller
  # scores.
  #
  #    Caption:     1,000,000 points per match
  #    Description: 1,000 points per match
  #    Snapshot:    1 point per match
  #
  proc result_transform {offsets} {
    set iScore 0
    set INCR(0) 1000000
    set INCR(1) 1000
    set INCR(2) 1
    foreach {a b c d} $offsets {
      incr iScore $INCR($a)
    }
    return $iScore
  }

  proc store_new_folder {treewidget} {
    set name [.new.entry get]

    destroy .new

    ::hv3::sqlitedb transaction {
      ::hv3::sqlitedb eval { INSERT INTO bm_folder2(name) VALUES($name) }
      set object [list folder [::hv3::sqlitedb last_insert_rowid]]
      ::hv3::bookmarks::dropobject [list folder 0] $object 0 1
    }
    refresh_gui
  }

  proc store_rename_folder {folderid} {
    set name [.new.entry get]
    destroy .new
    ::hv3::sqlitedb eval {
      UPDATE bm_folder2 SET name = $name WHERE folderid = $folderid
    }
    refresh_gui
  }

  proc refresh_gui {} {
    foreach obj [::hv3::bookmarks::treewidget info instances] {
      $obj populate_tree
    }
    foreach obj [::hv3::bookmarks::controller info instances] {
      $obj refresh
    }
  }

  proc db_store_new_folder {iFolder zName} {
    ::hv3::sqlitedb eval {
      INSERT INTO bm_folder2(name) VALUES($zName);
    }
    set folderid [::hv3::sqlitedb last_insert_rowid]
    ::hv3::sqlitedb eval {
      INSERT INTO bm_tree2(folderid, objecttype, objectid)
      VALUES($iFolder, 'f', $folderid)
    }
    return $folderid
  }

  proc db_store_new_bookmark {
      iFolder zCaption zUri zDescription iSnapshot zSnapshot zSnapshotText
  } {
    # To store a new bookmark, we need to store a row in each of the
    # following tables:
    #
    #     + bm_bookmark2               (primary bookmark record)
    #     + bm_tree2                   (link the bookmark into the tree)
    #     + bm_fulltext2               (if available - for searching)
    #

    ::hv3::sqlitedb eval {
      INSERT INTO bm_bookmark2
      (caption, uri, description, has_snapshot, snapshot) 
      VALUES($zCaption, $zUri, $zDescription, $iSnapshot, $zSnapshot)
    }

    set bookmarkid [::hv3::sqlitedb last_insert_rowid]
    ::hv3::sqlitedb eval {
      INSERT INTO bm_tree2(folderid, objecttype, objectid)
      VALUES($iFolder, 'b', $bookmarkid)
    }

    if {$::hv3::have_fts3} {
      ::hv3::sqlitedb eval {
        INSERT INTO bm_fulltext2(rowid, caption, description, snapshot)
        VALUES($bookmarkid, $zCaption, $zDescription, $zSnapshotText)
      }
    } else {
      catch { ::hv3::sqlitedb eval {CREATE TABLE bm_rebuild_fulltext2(a)} }
    }

    return $bookmarkid
  }

  set save_snapshot_variable 1
  proc store_new_bookmark {hv3} {
    set caption [.new.caption get]
    set uri [.new.uri get]
    set desc [.new.desc get 0.0 end]
    destroy .new

    ::hv3::sqlitedb transaction {
      set has_snapshot 0
      set snapshot ""
      set snapshot_text ""
      if {$::hv3::bookmarks::save_snapshot_variable && $hv3 ne ""} {
        set has_snapshot 1
        set snapshot [create_snapshot $hv3]
        set snapshot_text [$hv3 html text text]
      }
      set bookmarkid [
        db_store_new_bookmark 0 $caption $uri $desc $has_snapshot $snapshot $snapshot_text
      ]
      set object [list bookmark $bookmarkid]
      ::hv3::bookmarks::dropobject [list folder 0] $object 0 1
    }

    refresh_gui
  }

  proc store_edit_bookmark {bookmarkid} {
    set caption     [.new.caption get]
    set uri         [.new.uri get]
    set description [.new.desc get 0.0 end]

    ::hv3::sqlitedb transaction {
      ::hv3::sqlitedb eval {
        UPDATE bm_bookmark2 
        SET caption = $caption, uri = $uri, description = $description
        WHERE bookmarkid = $bookmarkid
      }
      if {$::hv3::have_fts3} {
        ::hv3::sqlitedb eval {
          UPDATE bm_fulltext2 
          SET caption = $caption, description = $description
          WHERE rowid = $bookmarkid
        } 
      } else {
        catch { ::hv3::sqlitedb eval {CREATE TABLE bm_rebuild_fulltext2(a)} }
      }
    }
    destroy .new

    refresh_gui
  }

  proc new_folder {treewidget} {
    toplevel .new

    ::hv3::label .new.label -text "Create New Bookmarks Folder: " -anchor s
    ::hv3::entry .new.entry -width 60
    ::hv3::button .new.save                            \
        -text "Save"                                   \
        -command [list ::hv3::bookmarks::store_new_folder $treewidget]
    ::hv3::button .new.cancel -text "Cancel" -command {destroy .new}

    grid .new.label -columnspan 2 -sticky ew
    grid .new.entry -columnspan 2 -sticky ew -padx 5
    grid .new.cancel .new.save -pady 5 -padx 5 -sticky e

    grid columnconfigure .new 0 -weight 1

    bind .new.entry <KeyPress-Return> {.new.save invoke}
    .new.entry insert 0 "New Folder"
    .new.entry selection range 0 end
    focus .new.entry

    launch_dialog .new
    tkwait window .new
  }

  proc rename_folder {folderid} {
    toplevel .new

    set name [::hv3::sqlitedb one {
      SELECT name FROM bm_folder2 WHERE folderid = $folderid
    }]

    ::hv3::label .new.label -text "Rename Folder \"$name\" To: " -anchor s
    ::hv3::entry .new.entry -width 60
    set cmd [list ::hv3::bookmarks::store_rename_folder $folderid]
    ::hv3::button .new.save -text "Save" -command $cmd
    ::hv3::button .new.cancel -text "Cancel" -command {destroy .new}

    grid .new.label -columnspan 2 -sticky ew
    grid .new.entry -columnspan 2 -sticky ew -padx 5
    grid .new.cancel .new.save -pady 5 -padx 5 -sticky e

    grid columnconfigure .new 0 -weight 1

    bind .new.entry <KeyPress-Return> {.new.save invoke}
    .new.entry insert 0 $name
    .new.entry selection range 0 end
    focus .new.entry

    launch_dialog .new
    tkwait window .new
  }

  proc create_bookmark_dialog {isCreate} {
    toplevel .new

    if {$isCreate} {
      ::hv3::label .new.label -text "Create New Bookmark: " -anchor s
    } else {
      ::hv3::label .new.label -text "Edit Bookmark: " -anchor s
    }
    ::hv3::entry .new.caption -width 40
    ::hv3::entry .new.uri -width 40
    ::hv3::text .new.desc -height 10 -background white -borderwidth 1 -width 40

    ::hv3::label .new.l_caption -text "Caption: "
    ::hv3::label .new.l_uri -text "Uri: "
    ::hv3::label .new.l_desc -text "Description: "

    ::hv3::button .new.save -text "Save"
    ::hv3::button .new.cancel -text "Cancel" -command {destroy .new}

    grid .new.label -columnspan 4

    grid .new.l_caption .new.caption -pady 5 -padx 5
    grid .new.l_uri     .new.uri -pady 5 -padx 5
    grid .new.l_desc    .new.desc -pady 5 -padx 5

    grid configure .new.caption -columnspan 3 -sticky ew
    grid configure .new.uri -columnspan 3 -sticky ew 
    grid configure .new.desc -columnspan 3 -sticky ewns

    grid configure .new.l_caption -sticky e
    grid configure .new.l_uri -sticky e
    grid configure .new.l_desc -sticky e

    if {$isCreate<2} {
      checkbutton .new.snapshot \
          -text "Save website text in database"                   \
          -variable ::hv3::bookmarks::save_snapshot_variable
      grid .new.snapshot -column 1 -padx 5 -pady 5 -sticky w
    }

    grid .new.save .new.cancel -pady 5 -padx 5 -sticky e
    grid configure .new.save -column 3
    grid configure .new.cancel -column 2

    grid columnconfigure .new 1 -weight 1
    grid rowconfigure .new 3 -weight 1

    bind .new.caption <KeyPress-Return> {.new.save invoke}
    bind .new.uri     <KeyPress-Return> {.new.save invoke}
  }

  proc new_bookmark {hv3} {
    ::hv3::bookmarks::ensure_initialized
    set dialog_version [expr {($hv3 eq "") ? 2 : 1}]

    create_bookmark_dialog $dialog_version
    .new.save configure \
        -command [list ::hv3::bookmarks::store_new_bookmark $hv3]

    focus .new.caption
    if {$hv3 ne ""} {
      .new.caption insert 0 [$hv3 title]
      .new.uri insert 0     [$hv3 uri get]
      .new.caption selection range 0 end
    }

    launch_dialog .new
    tkwait window .new
  }

  proc edit_bookmark {bookmarkid} {
    create_bookmark_dialog 0
    set cmd [list ::hv3::bookmarks::store_edit_bookmark $bookmarkid]
    .new.save configure -command $cmd

    focus .new.caption
    ::hv3::sqlitedb eval {
      SELECT caption, uri, description 
      FROM bm_bookmark2 
      WHERE bookmarkid = $bookmarkid
    } {}
    .new.caption insert 0 $caption
    .new.uri insert 0     $uri
    .new.desc insert 0.0     $description
    .new.caption selection range 0 end

    launch_dialog .new
    tkwait window .new
  }

  proc launch_dialog {dialog} {
    wm withdraw $dialog
    update idletasks

    set w [winfo reqwidth $dialog]
    set h [winfo reqheight $dialog]
    scan [wm geometry [winfo parent $dialog]] "%dx%d+%d+%d" pw ph px py
    set geom "+[expr $px + $pw/2 - $w/2]+[expr $py + $ph/2 - $h/2]"
    wm geometry $dialog $geom

    wm deiconify $dialog

    grab $dialog
    wm transient $dialog .
    # wm protocol .new WM_DELETE_WINDOW { grab release .new ; destroy .new }
    raise $dialog
  }

  # Notes on the mozilla bookmarks.html format:
  #
  #     * Each folder is a <DL> element.
  #     * Each bookmark is an <A> element.
  #     * Folder names are stored in <H3> elements.
  #
  proc import_bookmarks_file {zFile} {

    set fd [open $zFile]
    set zBookmarks [read $fd]
    close $fd

    set H [html .boomarksparser]
    $H parse -final $zBookmarks

    unset -nocomplain zTitle
    foreach N [$H search dl,h3] {

      if {[$N tag] eq "dl"} {
        if {![info exists zTitle]} {
          set zTitle "Imported Bookmarks"
        }

        # Create the bm_folder2 record.
        ::hv3::sqlitedb eval {
          INSERT INTO bm_folder2(name) VALUES($zTitle);
        }
        set iFolderId [::hv3::sqlitedb last_insert_rowid]
        set imported_folders($N) $iFolderId

        set iParentId 0
        for {set P [$N parent]} {$P ne ""} {set P [$P parent]} {
          if {[$P tag] eq "dl"} {
            set iParentId $imported_folders($P)
            break
          }
        }

        # Create the bm_tree2 record.
        ::hv3::sqlitedb eval {
          INSERT INTO bm_tree2(folderid, objecttype, objectid) 
          VALUES($iParentId, 'f', $iFolderId)
        }

      } else {
        set zTitle [[lindex [$N children] 0] text]
      }
    }

    foreach N [$H search {dl a[href]}] {
      set zCaption [[lindex [$N children] 0] text]
      set zUri [$N attribute href]

      for {set P [$N parent]} {$P ne ""} {set P [$P parent]} {
        if {[$P tag] eq "dl"} {
          set iParentId $imported_folders($P)
          break
        }
      }

      ::hv3::bookmarks::db_store_new_bookmark \
          $iParentId $zCaption $zUri "" 0 "" ""
    }

    destroy $H
  }

  proc import_bookmarks_dir {zDir} {
    if {[file exists [file join $zDir bookmarks.html]]} {
      import_bookmarks_file [file join $zDir bookmarks.html]
    }
    foreach dir [glob -nocomplain -directory ${zDir} -type d *] {
      import_bookmarks_dir $dir
    }
  }

  proc import_bookmarks {} {
    import_bookmarks_dir [file join $::env(HOME) .mozilla]

    foreach obj [::hv3::bookmarks::treewidget info instances] {
      $obj populate_tree
    }
  }

  proc import_bookmarks_manual {} {
    set zFile [tk_getOpenFile]
    if {$zFile ne ""} {
      import_bookmarks_file $zFile
      foreach obj [::hv3::bookmarks::treewidget info instances] {
        $obj populate_tree
      }
    }
  }

  proc import_tree {{zDirname ""}} {
    if {$zDirname eq ""} {
      set zDirname [tk_chooseDirectory]
    }
    if {$zDirname eq ""} return

    setup_tmphv3

    toplevel .import
    ::hv3::label .import.label -width 60 -anchor w
    ::hv3::button .import.cancel \
        -command [list destroy .import] -text "Cancel Import"
    pack .import.label -fill both -expand 1 -side top
    pack .import.cancel -fill x -expand 1

    launch_dialog .import
    set ::hv3::bookmarks::files_to_import [count_html_files $zDirname]
    set ::hv3::bookmarks::files_imported 0

    set rc [catch {::hv3::sqlitedb transaction {
      set iFolder [db_store_new_folder 0 "Imported from $zDirname"]
      import_dir $iFolder $zDirname
    }} msg]

    destroy .tmphv3
    tmpprotocol destroy
    destroy .import

    if {$rc == 0} {
      foreach obj [::hv3::bookmarks::treewidget info instances] {
        $obj populate_tree
      }
    } else {
      error $msg
    }
  }

  set files_to_import 0
  set files_imported 0

  proc count_html_files {dir} {
    if {![winfo exists .import.label]} {
      error "Operation cancelled by user"
    }
    .import.label configure -text "Searching for html files in $dir"
    update
    set iRes [llength [
      glob -nocomplain -directory $dir *.html *.htm *.HTML *.HTM
    ]]
    foreach d [glob -nocomplain -type d -directory $dir *] {
      incr iRes [count_html_files $d]
    }
    set iRes
  }

  proc import_local_uri {iFolder zUri} {
    set zCaption $zUri
    set zDesc ""
    set zSnapshot ""

    set hv3 .tmphv3
    $hv3 reset 0
    $hv3 goto $zUri
    set zSnapshot [create_snapshot $hv3]
   
    set titlenode [$hv3 html search title]
    if {$titlenode ne "" && [llength [$titlenode children]]>0} {
      set zCaption [[lindex [$titlenode children] 0] text]
    }

    set text [$hv3 html text text]
    db_store_new_bookmark $iFolder $zCaption $zUri $zDesc 2 $zSnapshot $text
  }

  proc import_dir {iFolder dir} {
    set obj [::tkhtml::uri file://]
    set hv3 .tmphv3

    set flist [glob -nocomplain -directory $dir *.html *.htm *.HTML *.HTM]
    foreach f $flist {
      if {![winfo exists .import.label]} {
        error "Operation cancelled by user"
      }
      incr ::hv3::bookmarks::files_imported
      set    T "Importing file ${::hv3::bookmarks::files_imported}/"
      append T "${::hv3::bookmarks::files_to_import} ($f)"

      .import.label configure -text $T
      update

      set zUri [$obj resolve $f]
      import_local_uri $iFolder $zUri
    }
    $obj destroy

    foreach f [glob -nocomplain -directory $dir *] {
      if {[file isdirectory $f]} {
        set iSubFolder [db_store_new_folder $iFolder [file tail $f]]
        import_dir $iSubFolder $f
      }
    }
  }

  proc import_dom {} {
    setup_tmphv3
    ::hv3::sqlitedb transaction {
      set iFolder      [db_store_new_folder 0 "Hv3 DOM Reference"]
      set iTreeFolder  [db_store_new_folder $iFolder "DOM Tree Objects"]
      set iEventFolder [db_store_new_folder $iFolder "DOM Event Objects"]
      set iContFolder  [db_store_new_folder $iFolder "DOM Container Objects"]
      set iStyleFolder [db_store_new_folder $iFolder "DOM Style Objects"]
      set iXMLFolder   [db_store_new_folder $iFolder "XMLHttpRequest Objects"]
      set iNSFolder    [db_store_new_folder $iFolder "Compatibility Objects"]
      set iElemFolder  [
        db_store_new_folder $iTreeFolder "HTMLElement Sub-classes"
      ]
      
      #set fname [file normalize [file join $::hv3::scriptdir dom_events.html]]
      #import_local_uri $iFolder file://$fname

      if {[llength [info commands ::hv3::DOM::docs::HTMLElement]] == 0} {
        ::hv3::dom_init 1
      }

      foreach cmd [lsort [info commands ::hv3::DOM::docs::*]] {
        set localcmd [string range $cmd [expr {[string last : $cmd]+1}] end]
        set folder $iFolder

        if {[string match HTML?*Element $localcmd]} {
          set folder $iElemFolder
        } elseif { [string match *XML* $localcmd] } {
          set folder $iXMLFolder
        } elseif { [string match *Event* $localcmd] } {
          set folder $iEventFolder
        } elseif { [lsearch \
          {History Location Screen Window Navigator FramesList} $localcmd]>=0 
        } {
          set folder $iNSFolder
        } elseif { [lsearch \
          {HTMLDocument HTMLElement NodePrototype Text} $localcmd]>=0 
        } {
          set folder $iTreeFolder
        } elseif { [lsearch \
          {NodeListC NodeListS HTMLCollectionC HTMLCollectionS} $localcmd]>=0 
        } {
          set folder $iContFolder
        } elseif { [lsearch {CSSStyleDeclaration} $localcmd]>=0 } {
          set folder $iStyleFolder
        }

        import_local_uri $folder home://dom/$localcmd
      }
    }
    destroy .tmphv3
    tmpprotocol destroy
    refresh_gui
  }

  proc setup_tmphv3 {} {
    set hv3      [::hv3::hv3 .tmphv3]
    set protocol [::hv3::protocol ::tmpprotocol]
    ::hv3::home_scheme_init $hv3 $protocol
    $hv3 html handler node object ""
    $hv3 html handler node frameset ""
    $hv3 html handler node embed ""
    $hv3 configure -requestcmd [list $protocol requestcmd]
  }

}

