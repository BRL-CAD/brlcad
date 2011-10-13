
proc sourcefile {file} [string map              \
  [list %HV3_DIR% [bu_brlcad_root lib/hv3]] \
{ 
  return [file join {%HV3_DIR%} $file] 
}]

package require Tk
tk scaling 1.33333
package require Tkhtml 3.0
package require tkpng
package provide ManBrowser 1.1

::itcl::class ::ManBrowser {
    inherit iwidgets::Dialog

    public {
        variable path
        variable parentName
        variable disabledPages
        variable enabledPages

        method setPageNames     {}
        method loadPage         {pageName}
        method select           {pageName}
    }

    # List of pages loaded into ToC listbox
    private common pages

    constructor {args} {}
}

# ------------------------------------------------------------
#                      OPTIONS
# ------------------------------------------------------------

##
# Path to HTML manual pages
#
::itcl::configbody ManBrowser::path {
    if {![info exists path] || ![file isdirectory $path]} {
        set path [file join [bu_brlcad_data "html"] mann en]
    }
}

##
# Used in window title string
#
::itcl::configbody ManBrowser::parentName {
    if {![info exists parentName] || ![string is print -strict $parentName]} {
        set parentName BRLCAD
    }
    configure -title "[string trim $parentName] Manual Page Browser"
}

##
# Page names in disabledByDefault and those added to this list are *always*
# disabled.
::itcl::configbody ManBrowser::disabledPages {
    set disabledByDefault [list Introduction]

    if {![info exists disabledPages] || ![string is list $disabledPages]} {
        set disabledPages $disabledByDefault
    } else {
        lappend disabledPages $disabledByDefault
    }
    set disabledPages [lsort $disabledPages]

    # Reset pages list
    if {[info exists pages($this)] && $pages($this) != {}} {
        setPageNames
    }
}

##
# All pages are enabled by default. If this list is defined, page names added
# to it are the only ones that may be enabled.
::itcl::configbody ManBrowser::enabledPages {
    if {![info exists enabledPages] || ![string is list $enabledPages]} {
        set enabledPages [list]
    }
    set enabledPages [lsort $enabledPages]

    # Reset pages list
    if {[info exists pages($this)] && $pages($this) != {}} {
        setPageNames
    }
}

# ------------------------------------------------------------
#                      OPERATIONS
# ------------------------------------------------------------

##
# Loads a list of enabled commands into ToC, after comparing pages found in
# 'path' with those listed in 'disabledPages' and 'enabledPages'.
::itcl::body ManBrowser::setPageNames {} {
   if {[file exists $path]} {
    set manFiles [glob -nocomplain -directory $path *.html ]

    set pages($this) [list]
    foreach manFile $manFiles {
        set rootName [file rootname [file tail $manFile]]

        # If the page exists in disabledPages, disable it 
        set isDisabled [expr [lsearch -sorted -exact \
                              $disabledPages $rootName] != -1]

        # If enabledPages is defined and the page exists, enable it
        if {$enabledPages != {}} {
            set isEnabled [expr [lsearch -sorted -exact \
                                 $enabledPages $rootName] != -1]
        } else {
            set isEnabled 1
        }

        # Obviously, if the page is both disabled/enabled, it will be disabled
        if {!$isDisabled && $isEnabled} {
            lappend pages($this) $rootName
        }
    }
    set pages($this) [lsort $pages($this)]
  }
}


##
# Loads pages selected graphically or through the command line into HTML browser
#
::itcl::body ManBrowser::loadPage {pageName} {
    # Get page
    if {[file exists [file join $path $pageName.html]]} {
       gui_current goto file:///[file join $path $pageName.html]      
    }
}

# Selects page in ToC & loads into HTML browser; used for command line calls
#
::itcl::body ManBrowser::select {pageName} {
    # Select the requested man page 
    set idx [lsearch -sorted -exact $pages($this) $pageName]

    if {$idx != -1} {
        set result True
        set toc $itk_component(toc_listbox)

        # Deselect previous selection
        $toc selection clear 0 [$toc index end]

        # Select pageName in table of contents
        $toc selection set $idx
        $toc activate $idx
        $toc see $idx

        loadPage $pageName
    } else {
        set result False
    }

    return $result
}

# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------
::itcl::body ManBrowser::constructor {args} {
    eval itk_initialize $args

    # Trigger configbody defaults if user didn't pass them
    set opts {{path} {parentName} {disabledPages} {enabledPages}}
    foreach o $opts {
        if {![info exists $o]} {eval itk_initialize {-$o {}}}
    }

    setPageNames

    $this hide 1
    $this hide 2
    $this hide 3
    $this configure \
        -modality none \
        -thickness 2 \
        -buttonboxpady 0
    $this buttonconfigure 0 \
        -defaultring yes \
        -defaultringpad 3 \
        -borderwidth 1 \
        -pady 0

    # ITCL can be nasty
    set win [$this component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$this childsite]

    # Table of Contents
    itk_component add toc {
        ::tk::frame $parent.toc
    } {}

    set toc $itk_component(toc)

    itk_component add toc_scrollbar {
        ::ttk::scrollbar $toc.toc_scrollbar
    } {}

    itk_component add toc_listbox {
        ::tk::listbox $toc.toc_listbox -bd 2 \
                                       -width 16 \
                                       -exportselection false \
                                       -yscroll "$toc.toc_scrollbar set" \
                                       -listvariable [scope pages($this)]
    } {}

    $toc.toc_scrollbar configure -command "$toc.toc_listbox yview"

    grid $toc.toc_listbox $toc.toc_scrollbar -sticky nsew -in $toc

    grid columnconfigure $toc 0 -weight 1
    grid rowconfigure $toc 0 -weight 1

    pack $toc -side left -expand no -fill y

    # Main HTML window
    itk_component add browser {
        ::tk::frame $parent.browser
    } {}
    set sfcsman $itk_component(browser)
    pack $sfcsman -expand yes -fill both

    # HTML widget
    set manhtmlviewer [::hv3::hv3 $sfcsman.htmlview]
    set manhtml [$manhtmlviewer html]

    grid $manhtmlviewer -sticky nsew -in $sfcsman

    grid columnconfigure $sfcsman 0 -weight 1
    grid rowconfigure $sfcsman 0 -weight 1

    pack $itk_component(browser) -side left -expand yes -fill both

    # Load Introduction.html if it's there, otherwise load first page
    if {[file exists [file join $path introduction.html]]} {
        loadPage Introduction
    } else {
        loadPage [lindex $pages($this) 0]
    }

    bind $toc.toc_listbox <<ListboxSelect>> {
        set mb [itcl_info objects -class ManBrowser]
        $mb loadPage [%W get [%W curselection]]
    }

    configure -height 600 -width 800
    return $this
}


source [sourcefile hv3_browser.tcl]

namespace eval ::hv3 {
  set log_source_option 0
  set reformat_scripts_option 0
}

# This class is used to create toplevel "sub-window" widgets. Sub-windows
# are toplevel windows that contain a single [::hv3::browser] object.
#
# Sub-windows are different from the main window in several ways:
#
#   * There is no menubar.
#   * There is no "new tab", "home" or "bug report button on the toolbar.
#   * It is not possible to open new tabs in a sub-window.
#
# These restrictions are because Hv3 is a tabbed browser. New views are
# supposed to live in tabs, not separate toplevel windows. If the user 
# really wants more than one window, more than one copy of the browser
# should be started. Sub-windows are provided purely for the benefit of
# those javascript applications that have UIs that require multiple 
# windows.
#
namespace eval ::hv3::subwindow {

  set counter 1

  proc new {me args} {
    upvar #0 $me O

    set O(browser)  [::hv3::browser $O(win).browser]
    set O(label)    [::hv3::label $O(win).label -anchor w -width 1]
    set O(location) [::hv3::locationentry $O(win).location]

    set O(stop_button) $O(win).stop
    set O(back_button) $O(win).back
    set O(next_button) $O(win).next

    ::hv3::toolbutton $O(stop_button) -text {Stop} -tooltip "Stop"
    ::hv3::toolbutton $O(next_button) -text {Forward} -tooltip "Go Forward"
    ::hv3::toolbutton $O(back_button) -text {Back} -tooltip "Go Back"
   
    grid $O(back_button) $O(next_button) $O(stop_button) 
    grid $O(location)    -column 3 -row 0 -sticky ew
    grid $O(browser)     -column 0 -row 1 -sticky nsew -columnspan 4
    grid $O(label)       -column 0 -row 2 -sticky nsew -columnspan 4

    grid columnconfigure $O(win) 3 -weight 1
    grid rowconfigure    $O(win) 1 -weight 1

    $O(back_button) configure -image hv3_previmg
    $O(next_button) configure -image hv3_nextimg
    $O(stop_button) configure -image hv3_reloadimg

    $O(label)    configure -textvar       [$O(browser) statusvar] 
    $O(browser)  configure -stopbutton    $O(stop_button)
    $O(browser)  configure -forwardbutton $O(next_button)
    $O(browser)  configure -backbutton    $O(back_button)
    $O(browser)  configure -locationentry $O(location)
    $O(location) configure -command       [list $me GotoLocation]

    $O(browser) configure -width 600 -height 400

    set O(titlevarname)    [$O(browser) titlevar]
    set O(locationvarname) [$O(browser) locationvar]

    # Set up traces on the browser title and location. Use these to
    # set the title of the toplevel window.
    trace add variable $O(titlevarname)    write [list $me SetTitle]
    trace add variable $O(locationvarname) write [list $me SetTitle]
  }

  proc SetTitle {me args} {
    upvar #0 $me O
    set T [set [$O(browser) titlevar]]
    if {$T eq ""} {
      set T [set [$O(browser) locationvar]]
    }
    wm title $O(win) $T
  }

  proc destroy {me} {
    upvar #0 $me O
    trace remove variable $O(titlevarname)    write [list $me SetTitle]
    trace remove variable $O(locationvarname) write [list $me SetTitle]
  }

  proc goto {me uri} {
    upvar #0 $me O
    $O(browser) goto $uri
    puts $url
  }

  proc GotoLocation {me} {
    upvar #0 $me O
    set uri [$O(location) get]
    $O(browser) goto $uri
  }
}
::hv3::make_constructor ::hv3::subwindow toplevel

# ::hv3::config
#
#     An instance of this class manages the application "View" menu, 
#     which contains all the runtime configuration options (font size, 
#     image loading etc.).
#
snit::type ::hv3::config {

  # The SQLite database containing the configuration used
  # by this application instance. 
  #
  variable myDb ""
  variable myPollActive 0

  foreach {opt def type} [list \
    -enableimages     1                         Boolean \
    -enablejavascript 0                         Boolean \
    -forcefontmetrics 1                         Boolean \
    -hidegui          0                         Boolean \
    -zoom             1.0                       Double  \
    -fontscale        1.0                       Double  \
    -guifont          11                        Integer \
    -icons            default_icons             Icons   \
    -debuglevel       0                         Integer \
    -fonttable        [list 8 9 10 11 13 15 17] SevenIntegers \
  ] {
    option $opt -default $def -validatemethod $type -configuremethod SetOption
  }
  
  constructor {db args} {
    set myDb $db


    $myDb transaction {
      set rc [catch {
        $myDb eval {
          CREATE TABLE cfg_options1(name TEXT PRIMARY KEY, value);
        }
      }]
      if {$rc == 0} {
        foreach {n v} [array get options] {
          $myDb eval {INSERT INTO cfg_options1 VALUES($n, $v)}
        } 
        if {[llength [info commands ::tkhtml::heapdebug]] > 0} {
          $self configure -debuglevel 1
        }
      } else {
        $myDb eval {SELECT name, value FROM cfg_options1} {
          set options($name) $value
          if {$name eq "-guifont"} {
            after idle [list ::hv3::SetFont [list -size $value]]
          }
        }
      }
    }

    ::hv3::$options(-icons)

    $self configurelist $args
    after 2000 [list $self PollConfiguration]
  }

  method PollConfiguration {} {
    set myPollActive 1
    $myDb transaction {
      foreach n [array names options] {
        $myDb eval { SELECT value AS v FROM cfg_options1 WHERE name = $n } {
          if {$options($n) ne $v} {
            $self configure $n $v
          }
        }
      }
    }
    set myPollActive 0
    after 2000 [list $self PollConfiguration]
  }

  method populate_menu {path} {

   set cmd [list gui_current Find]
   set acc (Ctrl-F)
   $path add command -label {Find in page...} -command $cmd -accelerator $acc
   bind Hv3HotKeys <Control-f>  [list gui_current Find]
   bind Hv3HotKeys <Control-F>  [list gui_current Find]

    # Add the 'Gui Font (size)' menu
    ::hv3::menu ${path}.guifont
    $self PopulateRadioMenu ${path}.guifont -guifont [list \
        8      "8 pts" \
        9      "9 pts" \
        10    "10 pts" \
        11    "11 pts" \
        12    "12 pts" \
        14    "14 pts" \
        16    "16 pts" \
    ]
    $path add cascade -label {Gui Font} -menu ${path}.guifont

    # Add the 'Icons' menu
    ::hv3::menu ${path}.icons
    $self PopulateRadioMenu ${path}.icons -icons [list    \
        grey_icons     "Great looking classy grey icons"      \
        color_icons22  "22x22 Tango icons"                    \
        color_icons32  "32x32 Tango icons"                    \
    ]
    $path add cascade -label {Gui Icons} -menu ${path}.icons

    $self populate_hidegui_entry $path
    $path add separator

    # Add the 'Zoom' menu
    ::hv3::menu ${path}.zoom
    $self PopulateRadioMenu ${path}.zoom -zoom [list \
        0.25    25% \
        0.5     50% \
        0.75    75% \
        0.87    87% \
        1.0    100% \
        1.131  113% \
        1.25   125% \
        1.5    150% \
        2.0    200% \
    ]
    $path add cascade -label {Browser Zoom} -menu ${path}.zoom

    # Add the 'Font Scale' menu
    ::hv3::menu ${path}.fontscale
    $self PopulateRadioMenu ${path}.fontscale -fontscale [list \
        0.8     80% \
        0.9     90% \
        1.0    100% \
        1.2    120% \
        1.4    140% \
        2.0    200% \
    ]
    $path add cascade -label {Browser Font Scale} -menu ${path}.fontscale
      
    # Add the 'Font Size Table' menu
    set fonttable [::hv3::menu ${path}.fonttable]
    $self PopulateRadioMenu $fonttable -fonttable [list \
        {7 8 9 10 12 14 16}    "Normal"            \
        {8 9 10 11 13 15 17}   "Medium"            \
        {9 10 11 12 14 16 18}  "Large"             \
        {11 12 13 14 16 18 20} "Very Large"        \
        {13 14 15 16 18 20 22} "Extra Large"       \
        {15 16 17 18 20 22 24} "Recklessly Large"  \
    ]
    $path add cascade -label {Browser Font Size Table} -menu $fonttable

    foreach {option label} [list \
        -forcefontmetrics "Force CSS Font Metrics" \
        -enableimages     "Enable Images"          \
        --                --                       \
        -enablejavascript "Enable ECMAscript"      \
    ] {
      if {$option eq "--"} {
        $path add separator
      } else {
        set var [myvar options($option)]
        set cmd [list $self Reconfigure $option]
        $path add checkbutton -label $label -variable $var -command $cmd
      }
    }
    if {[info commands ::see::interp] eq ""} {
      $path entryconfigure end -state disabled
    }
  }

  method populate_hidegui_entry {path} {
    $path add checkbutton -label "Hide Gui" -variable [myvar options(-hidegui)]
    $path entryconfigure end -command [list $self Reconfigure -hidegui]
  }

  method PopulateRadioMenu {path option config} {
    foreach {val label} $config {
      $path add radiobutton                      \
        -variable [myvar options($option)]       \
        -value $val                              \
        -command [list $self Reconfigure $option]  \
        -label $label 
    }
  }

  method Reconfigure {option} {
    $self configure $option $options($option)
  }

  method Boolean {option value} {
    if {![string is boolean $value]} { error "Bad boolean value: $value" }
  }
  method Double {option value} {
    if {![string is double $value]} { error "Bad double value: $value" }
  }
  method Integer {option value} {
    if {![string is integer $value]} { error "Bad integer value: $value" }
  }
  method Icons {option value} {
    if {[info commands ::hv3::$value] eq ""} { error "Bad icon scheme: $value" }
  }
  method SevenIntegers {option value} {
    set len [llength $value]
    if {$len != 7} { error "Bad seven-integers value: $value" }
    foreach elem $value {
      if {![string is integer $elem]} { 
        error "Bad seven-integers value: $value"
      }
    }
  }

  method SetOption {option value} {
    set options($option) $value
    if {$myPollActive == 0} {
      $myDb eval {REPLACE INTO cfg_options1 VALUES($option, $value)}
    }

    switch -- $option {
      -hidegui {
        if {$value} {
          . configure -menu ""
          pack forget .status
          pack forget .toolbar
        } else {
          . configure -menu .m
          pack .status -after .middle -fill x -side bottom
          pack .toolbar -before .middle -fill x -side top
        }
      }
      -guifont {
        ::hv3::SetFont [list -size $options(-guifont)]
      }
      -icons {
        ::hv3::$options(-icons)
      }
      -debuglevel {
        switch -- $value {
          0 {
            set ::hv3::reformat_scripts_option 0
            set ::hv3::log_source_option 0
          }
          1 {
            set ::hv3::reformat_scripts_option 0
            set ::hv3::log_source_option 1
          }
          2 {
            set ::hv3::reformat_scripts_option 1
            set ::hv3::log_source_option 1
          }
        }
      }
      default {
        $self configurebrowser [.middle.notebook current]
      } 
    }
  }

  method StoreOptions {} {
  }
  method RetrieveOptions {} {
  }

  method configurebrowser {b} {
    if {$b eq ""} return
    foreach {option var} [list                       \
        -fonttable        options(-fonttable)        \
        -fontscale        options(-fontscale)        \
        -zoom             options(-zoom)             \
        -forcefontmetrics options(-forcefontmetrics) \
        -enableimages     options(-enableimages)     \
        -enablejavascript options(-enablejavascript) \
    ] {
      if {[$b cget $option] ne [set $var]} {
        $b configure $option [set $var]
        foreach f [$b get_frames] {
          if {[$f positionid] ne "0"} {
            $self configureframe $f
          }
        }
      }
    }
  }
  method configureframe {b} {
    foreach {option var} [list                       \
        -fonttable        options(-fonttable)        \
        -fontscale        options(-fontscale)        \
        -zoom             options(-zoom)             \
        -forcefontmetrics options(-forcefontmetrics) \
        -enableimages     options(-enableimages)     \
        -enablejavascript options(-enablejavascript) \
    ] {
      if {[$b cget $option] ne [set $var]} {
        $b configure $option [set $var]
      }
    }
  }

  destructor {
    after cancel [list $self PollConfiguration]
  }
}

snit::type ::hv3::file_menu {

  variable MENU

  constructor {} {
    set MENU [list \
      "Open File..."  [list gui_openfile $::hv3::MGEDHelp(notebook)]           o  \
      "Open Tab"      [list $::hv3::MGEDHelp(notebook) add]                    t  \
      "Open Location" [list gui_openlocation $::hv3::MGEDHelp(location_entry)] l  \
      "-----"         ""                                                "" \
      "Close Tab"     [list $::hv3::MGEDHelp(notebook) close]                  "" \
      "Exit"          exit                                              q  \
    ]
  }

  method populate_menu {path} {
    $path delete 0 end

    foreach {label command key} $MENU {
      if {[string match ---* $label]} {
        $path add separator
        continue
      }
      $path add command -label $label -command $command 
      if {$key ne ""} {
        set acc "(Ctrl-[string toupper $key])"
        $path entryconfigure end -accelerator $acc
      }
    }

    if {[llength [$::hv3::MGEDHelp(notebook) tabs]] < 2} {
      $path entryconfigure "Close Tab" -state disabled
    }
  }

  method setup_hotkeys {} {
    foreach {label command key} $MENU {
      if {$key ne ""} {
        set uc [string toupper $key]
        bind Hv3HotKeys <Control-$key> $command
        bind Hv3HotKeys <Control-$uc> $command
      }
    }
  }
}

proc ::hv3::gui_bookmark {} {
  ::hv3::bookmarks::new_bookmark [gui_current hv3]
}


# get a list of the mann pages
proc mann_pages {lang_type} {
   set disabledByDefault [list Introduction]
   set mann_list []
   set manFiles [glob -nocomplain -directory [file join [bu_brlcad_data html] mann/$lang_type] *.html ]
   foreach manFile $manFiles {
      set rootName [file rootname [file tail $manFile]]
      set isDisabled [expr [lsearch -sorted -exact \
                                    $disabledByDefault $rootName] != -1]
      if {!$isDisabled} {
      	lappend mann_list $rootName
      }
   }
   set mann_list [lsort $mann_list]
   return $mann_list
}
   


#--------------------------------------------------------------------------
# The following functions are all called during startup to construct the
# static components of the web browser gui:
#
#     gui_build
#     gui_menu
#       create_fontsize_menu
#       create_fontscale_menu
#

# gui_build --
#
#     This procedure is called once at the start of the script to build
#     the GUI used by the application. It creates all the widgets for
#     the main window. 
#
#     The argument is the name of an array variable in the parent context
#     into which widget names are written, according to the following 
#     table:
#
#         Array Key            Widget
#     ------------------------------------------------------------
#         stop_button          The "stop" button
#         back_button          The "back" button
#         forward_button       The "forward" button
#         location_entry       The location bar
#         notebook             The ::hv3::tabset instance
#         status_label         The label used for a status bar
#         history_menu         The pulldown menu used for history
#
proc gui_build {widget_array} {
  upvar $widget_array G
  global HTML

  # Create the top bit of the GUI - the URI entry and buttons.
  frame .toolbar
  frame .toolbar.b
  ::hv3::locationentry .toolbar.entry
  ::hv3::toolbutton .toolbar.b.back    -text {Back} -tooltip    "Go Back"
  ::hv3::toolbutton .toolbar.b.stop    -text {Stop} -tooltip    "Stop"
  ::hv3::toolbutton .toolbar.b.forward -text {Forward} -tooltip "Go Forward"

  ::ttk::panedwindow .middle -orient horizontal
  ::hv3::toolbutton .toolbar.b.new -text {New Tab} -command [list .middle.notebook add]
  ::hv3::toolbutton .toolbar.b.home -text Home -command [list \
      gui_current goto $::hv3::homeuri
  ]
  .toolbar.b.new configure -tooltip "Open New Tab"
  .toolbar.b.home configure -tooltip "Go to Bookmarks Manager"

  # Create the middle bit - the browser window
  #
  ::hv3::tabset .middle.notebook              \
      -newcmd    gui_new                 \
      -switchcmd gui_switch

  # And the bottom bit - the status bar
  ::hv3::label .status -anchor w -width 1
  bind .status <1>     [list gui_current ProtocolGui toggle]

  bind .status <3>     [list gui_status_toggle $widget_array]
  bind .status <Enter> [list gui_status_enter  $widget_array]
  bind .status <Leave> [list gui_status_leave  $widget_array]

  # Set the widget-array variables
  set G(new_button)     .toolbar.b.new
  set G(stop_button)    .toolbar.b.stop
  set G(back_button)    .toolbar.b.back
  set G(forward_button) .toolbar.b.forward
  set G(home_button)    .toolbar.b.home
  set G(location_entry) .toolbar.entry
  set G(notebook)       .middle.notebook
  set G(status_label)   .status

  # The G(status_mode) variable takes one of the following values:
  #
  #     "browser"      - Normal browser status bar.
  #     "browser-tree" - Similar to "browser", but displays the document tree
  #                      hierachy for the node the cursor is currently 
  #                      hovering over. This used to be the default.
  #     "memory"       - Show information to do with Hv3's memory usage.
  #
  # The "browser" mode uses less CPU than "browser-tree" and "memory". 
  # The user cycles through the modes by right-clicking on the status bar.
  #
  set G(status_mode)    "browser"

  # Pack the elements of the "top bit" into the .entry frame
  pack .toolbar.b.new -side left
  pack .toolbar.b.back -side left
  pack .toolbar.b.forward -side left
  pack .toolbar.b.stop -side left
  pack .toolbar.b.home -side left
  pack [frame .toolbar.b.spacer -width 2 -height 1] -side left

  pack .toolbar.b -side left
  pack .toolbar.entry -fill x -expand true

  # Pack the top, bottom and middle, in that order. The middle must be 
  # packed last, as it is the bit we want to shrink if the size of the 
  # main window is reduced.
  pack .toolbar -fill x -side top 
  pack .status -fill x -side bottom
  
 
 ::ttk::frame .middle.treeframe -borderwidth 1
  set mann_list [mann_pages en]
  ::ttk::treeview .middle.treeframe.tree
  .middle.treeframe.tree heading #0 -text "Manual Pages"
  .middle.treeframe.tree column #0 -width 130
  foreach manFile $mann_list {
     .middle.treeframe.tree insert {} end -text "$manFile"
  }
  bind .middle.treeframe.tree <<TreeviewSelect>> {gui_current goto file:///[file join [bu_brlcad_data html] mann/en/[.middle.treeframe.tree item [.middle.treeframe.tree focus] -text].html]}
  bind .middle.treeframe.tree <Control-f> {gui_current Find}
  bind .middle.treeframe.tree <Control-F> {gui_current Find}

  ::ttk::scrollbar .middle.treeframe.treevscroll -orient vertical
  .middle.treeframe.tree configure -yscrollcommand ".middle.treeframe.treevscroll set"
  .middle.treeframe.treevscroll configure -command ".middle.treeframe.tree yview"

  grid .middle.treeframe.tree .middle.treeframe.treevscroll -sticky nsew
  grid columnconfigure .middle.treeframe 0 -weight 1
  grid rowconfigure .middle.treeframe 0 -weight 1

  .middle add .middle.treeframe 
  .middle add .middle.notebook
  pack .middle -fill both -expand true
}

proc goto_gui_location {browser entry args} {
  set location [$entry get]
  $browser goto $location
}

proc gui_openlocation {location_entry} {
  $location_entry selection range 0 end
  $location_entry OpenDropdown *
  focus ${location_entry}.entry
}

proc gui_populate_menu {eMenu menu_widget} {
  switch -- [string tolower $eMenu] {
    file {
      set cmd [list $::hv3::MGEDHelp(file_menu) populate_menu $menu_widget]
      $menu_widget configure -postcommand $cmd
    }

    options {
      $::hv3::MGEDHelp(config) populate_menu $menu_widget
    }

    default {
      error "gui_populate_menu: No such menu: $eMenu"
    }
  }
}

proc gui_menu {widget_array} {
  upvar $widget_array G

  # Attach a menu widget - .m - to the toplevel application window.
  . config -menu [::hv3::menu .m]

  set G(config)     [::hv3::config %AUTO% ::hv3::sqlitedb]
  set G(file_menu)  [::hv3::file_menu %AUTO%]

  # Add the "File" and "Options" menus.
  foreach m [list File Options] {
    set menu_widget .m.[string tolower $m]
    gui_populate_menu $m [::hv3::menu $menu_widget]
    .m add cascade -label $m -menu $menu_widget -underline 0
  }

  $G(file_menu) setup_hotkeys

  catch {
    .toolbar.b.back configure -image hv3_previmg
    .toolbar.b.forward configure -image hv3_nextimg
    .toolbar.b.stop configure -image hv3_stopimg
    .toolbar.b.new configure -image hv3_newimg
    .toolbar.b.home configure -image hv3_homeimg
  }
}
#--------------------------------------------------------------------------

proc gui_current {args} {
  eval [linsert $args 0 [.middle.notebook current]]
}

proc gui_firefox_remote {} {
  set url [.toolbar.entry get]
  exec firefox -remote "openurl($url,new-tab)"
}

proc gui_switch {new} {
  upvar #0 ::hv3::MGEDHelp G

  # Loop through *all* tabs and detach them from the history
  # related controls. This is so that when the state of a background
  # tab is updated, the history menu is not updated (only the data
  # structures in the corresponding ::hv3::history object).
  #
  foreach browser [.middle.notebook tabs] {
    $browser configure -backbutton    ""
    $browser configure -stopbutton    ""
    $browser configure -forwardbutton ""
    $browser configure -locationentry ""
  }

  # Configure the new current tab to control the history controls.
  #
  set new [.middle.notebook current]
  $new configure -backbutton    $G(back_button)
  $new configure -stopbutton    $G(stop_button)
  $new configure -forwardbutton $G(forward_button)
  $new configure -locationentry $G(location_entry)
  $new populatehistorymenu

  # Attach some other GUI elements to the new current tab.
  #
  set gotocmd [list goto_gui_location $new $G(location_entry)]
  $G(location_entry) configure -command $gotocmd
  gui_status_leave ::hv3::MGEDHelp

  # Configure the new current tab with the contents of the drop-down
  # config menu (i.e. font-size, are images enabled etc.).
  #
  $G(config) configurebrowser $new

  # Set the top-level window title to the title of the new current tab.
  #
  wm title . [.middle.notebook get_title $new]

  # Focus on the root HTML widget of the new tab.
  #
  focus [[$new hv3] html]
}

proc gui_new {path args} {
  set new [::hv3::browser $path]
  $::hv3::MGEDHelp(config) configurebrowser $new

  set var [$new titlevar]
  trace add variable $var write [list gui_settitle $new $var]

  set var [$new locationvar]
  trace add variable $var write [list gui_settitle $new $var]

  if {[llength $args] == 0} {
    $new goto $::hv3::newuri
  } else {
    $new goto [lindex $args 0]
  }
  
  # This black magic is required to initialise the history system.
  # A <<Location>> event will be generated from within the [$new goto]
  # command above, but the history system won't see it, because 
  # events are not generated until the window is mapped. So generate
  # an extra <<Location>> when the window is mapped.
  #
  set w [[$new hv3] win]
  bind $w <Map>  [list event generate $w <<Location>>]
  bind $w <Map> +[list bind <Map> $w ""]

  # [[$new hv3] html] configure -logcmd print

  return $new
}

proc gui_settitle {browser var args} {
  if {[.middle.notebook current] eq $browser} {
    wm title . [set $var]
  }
  .middle.notebook set_title $browser [set $var]
}

# This procedure is invoked when the user selects the File->Open menu
# option. It launches the standard Tcl file-selector GUI. If the user
# selects a file, then the corresponding URI is passed to [.hv3 goto]
#
proc gui_openfile {notebook} {
  set browser [$notebook current]
  set f [tk_getOpenFile -filetypes [list \
      {{Html Files} {.html}} \
      {{Html Files} {.htm}}  \
      {{All Files} *}
  ]]
  if {$f != ""} {
    if {$::tcl_platform(platform) eq "windows"} {
      set f [string map {: {}} $f]
    }
    $browser goto file://$f 
  }
}

proc gui_log_window {notebook} {
  set browser [$notebook current]
  ::hv3::log_window [[$browser hv3] html]
}

proc gui_report_bug {} {
  upvar ::hv3::MGEDHelp G
  set uri [[[$G(notebook) current] hv3] uri get]
  .middle.notebook add "home://bug/[::hv3::format_query [encoding system] $uri]"

  set cookie "tkhtml_captcha=[expr [clock seconds]+86399]; Path=/; Version=1"
  ::hv3::the_cookie_manager SetCookie http://tkhtml.tcl.tk/ $cookie
}

proc gui_escape {} {
  upvar ::hv3::MGEDHelp G
  gui_current escape
  $G(location_entry) escape
  focus [[gui_current hv3] html]
}
bind Hv3HotKeys <KeyPress-Escape> gui_escape

proc gui_status_enter {widget_array} {
  upvar $widget_array G
  after cancel [list gui_set_memstatus $widget_array]
  gui_status_help $widget_array
  $G(status_label) configure -textvar ::hv3::MGEDHelp(status_help)
}
proc gui_status_help {widget_array} {
  upvar $widget_array G
  set G(status_help)    "Current status-bar mode: "
  switch -- $G(status_mode) {
    browser      { append G(status_help) "Normal" }
    browser-tree { append G(status_help) "Tree-Browser" }
    memory       { append G(status_help) "Memory-Usage" }
  }
  append G(status_help) "        "
  append G(status_help) "(To toggle mode, right-click)"
  append G(status_help) "        "
  append G(status_help) "(To view outstanding resource requests, left-click)"
}
proc gui_status_leave {widget_array} {
  upvar $widget_array G

  switch -exact -- $G(status_mode) {
    browser {
      $G(status_label) configure -textvar [gui_current statusvar]
    }
    browser-tree {
      $G(status_label) configure -textvar [gui_current statusvar]
    }
    memory {
      $G(status_label) configure -textvar ""
      gui_set_memstatus $widget_array
    }
  }
}
proc gui_status_toggle {widget_array} {
  upvar $widget_array G
  set modes [list browser browser-tree memory]
  set iNewMode [expr {([lsearch $modes $G(status_mode)]+1)%[llength $modes]}]
  set G(status_mode) [lindex $modes $iNewMode]
  gui_status_help $widget_array
}

proc gui_set_memstatus {widget_array} {
  upvar $widget_array G
  if {$G(status_mode) eq "memory"} {
    set status "Script:   "
    append status "[::count_vars] vars, [::count_commands] commands,"
    append status "[::count_namespaces] namespaces"

    catch {
      array set v [::see::alloc]
      set nHeap [expr {int($v(GC_get_heap_size) / 1000)}]
      set nFree [expr {int($v(GC_get_free_bytes) / 1000)}]
      set nDom $v(SeeTclObject)
      append status "          "
      append status "GC Heap: ${nHeap}K (${nFree}K free) "
      append status "($v(SeeTclObject) DOM objects)"
    }
    catch {
      foreach line [split [memory info] "\n"] {
        if {[string match {current packets allocated*} $line]} {
          set nAllocs [lindex $line end]
        }
        if {[string match {current bytes allocated*} $line]} {
          set nBytes [lindex $line end]
        }
      }
      set nBytes "[expr {int($nBytes / 1000)}]K"
      append status "          Tcl Heap: ${nBytes} in $nAllocs allocs"
    }

    $G(status_label) configure -text $status
    after 2000 [list gui_set_memstatus $widget_array]
  }
}

# Launch a new sub-window.
#
proc gui_subwindow {{uri ""}} {
  set name ".subwindow_[incr ::hv3::subwindow::counter]"
  ::hv3::subwindow $name
  if {$uri eq ""} {
    set uri [[gui_current hv3] uri get]
  }
  $name goto $uri
}

# Override the [exit] command to check if the widget code leaked memory
# or not before exiting.
#
rename exit tcl_exit
proc exit {args} {
  destroy .middle.notebook
  catch {destroy .prop.hv3}
  catch {::tkhtml::htmlalloc}
  eval [concat tcl_exit $args]
}

#--------------------------------------------------------------------------
# main URI
#
#     The main() program for the application. This proc handles
#     parsing of command line arguments.
#
proc main {args} {

  set docs [list]

  for {set ii 0} {$ii < [llength $args]} {incr ii} {
    set val [lindex $args $ii]
    switch -glob -- $val {
      -s* {                  # -statefile <file-name>
        if {$ii == [llength $args] - 1} ::hv3::usage
        incr ii
        set ::hv3::statefile [lindex $args $ii]
      }
      -profile { 
	# Ignore this here. If the -profile option is present it will 
        # have been handled already.
      }
      -enablejavascript { 
        set enablejavascript 1
      }
      default {
        set uri [::tkhtml::uri file:///[file join [bu_brlcad_data html] mann en Introduction.html] ]
        lappend docs [$uri resolve $val.html]
        $uri destroy
      }
    }
  }

  ::hv3::dbinit

  if {[llength $docs] == 0} {set docs [list file:///[file join [bu_brlcad_data html] mann/en/Introduction.html]] }
  set ::hv3::newuri [lindex $docs 0]
  set ::hv3::homeuri file:///[file join [bu_brlcad_data html] mann/en/Introduction.html ]

  # Build the GUI
  gui_build     ::hv3::MGEDHelp
  gui_menu      ::hv3::MGEDHelp

  array set ::hv3::G [array get ::hv3::MGEDHelp]

  # After the event loop has run to create the GUI, run [main2]
  # to load the startup document. It's better if the GUI is created first,
  # because otherwise if an error occurs Tcl deems it to be fatal.
  after idle [list main2 $docs]
}
proc main2 {docs} {
  foreach doc $docs {
    set tab [$::hv3::MGEDHelp(notebook) add $doc]
  }
  focus $tab
}
proc ::hv3::usage {} {
  puts stderr "Usage:"
  puts stderr "    $::argv0 ?-statefile <file-name>? ?<uri>?"
  puts stderr ""
  tcl_exit
}

set ::hv3::statefile ":memory:"

# Remote scaling interface:
proc hv3_zoom      {newval} { $::hv3::MGEDHelp(config) set_zoom $newval }
proc hv3_fontscale {newval} { $::hv3::MGEDHelp(config) set_fontscale $newval }
proc hv3_forcewidth {forcewidth width} { 
  [[gui_current hv3] html] configure -forcewidth $forcewidth -width $width
}

proc hv3_guifont {newval} { $::hv3::MGEDHelp(config) set_guifont $newval }

proc hv3_html {args} { 
  set html [[gui_current hv3] html]
  eval [concat $html $args]
}

# Set variable $::hv3::maindir to the directory containing the 
# application files. Then run the [main] command with the command line
# arguments passed to the application.
set ::hv3::maindir [file dirname [info script]] 
eval [concat main $argv]

proc print {args} { puts [join $args] }

#--------------------------------------------------------------------------

