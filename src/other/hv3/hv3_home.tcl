namespace eval hv3 { set {version($Id$)} 1 }

# Register the home: scheme handler with ::hv3::protocol $protocol.
#
proc ::hv3::home_scheme_init {hv3 protocol} {
  set dir $::hv3::maindir
  $protocol schemehandler home [list ::hv3::home_request $protocol $hv3 $dir]
}

# When a URI with the scheme "home:" is requested, this proc is invoked.
#
proc ::hv3::home_request {http hv3 dir downloadHandle} {

  set obj [::tkhtml::uri [$downloadHandle cget -uri]]
  set authority [$obj authority]
  set path      [$obj path]
  $obj destroy

  set data {}

  switch -exact -- $authority {

    blank { }

    about {
      set tkhtml_version [::tkhtml::version]
      set hv3_version ""
      foreach version [lsort [array names ::hv3::version]] {
        set t [string trim [string range $version 4 end-1]]
        append hv3_version "$t\n"
      }
    
      set data [subst {
        <html> <head> </head> <body>
        <h1>Tkhtml Source Code Versions</h1>
        <pre>$tkhtml_version</pre>
        <h1>Hv3 Source Code Versions</h1>
        <pre>$hv3_version</pre>
        </body> </html>
      }]
    }

    bug {
      set uri [::tkhtml::decode [string range $path 1 end]]
      after idle [list \
          ::hv3::bugreport::init [$downloadHandle cget -hv3] $uri
      ]
    }

    dom {
      if {$path eq "/style.css"} {
        $downloadHandle append {
          .overview {
            margin: 0 20px;
          }
          .spacer {
            width: 3ex;
            background-color: white;
          }

          .refs A[href] { display:block }
          .refs         { padding-left: 1.5cm }
          .superclass   { font-style: italic; padding: 0 1.5cm; }
          PRE           { margin: 0 1.5cm }

          .property,.method {
            font-family: monospace;
            white-space: nowrap;
          }
          TD { padding: 0 1ex; vertical-align: top;}
          .stripe0 {
            background-color: #EEEEEE;
          }
        }
      } else {
        set obj [string range $path 1 end]
        set data [::hv3::DOM::docs::${obj}]
      }
    }

    downloads {
      ::hv3::the_download_manager request $downloadHandle
      return
    }

    bookmarks_left { }
    bookmarks {
      if {$path eq "" || $path eq "/"} {
        $downloadHandle append {
          <FRAMESET cols="33%,*">
            <FRAME src="home://bookmarks_left">
            <FRAME src="home://bookmarks/folder/0">
        }
        ::hv3::bookmarks::ensure_initialized
        after idle [list ::hv3::bookmarks::init [$downloadHandle cget -hv3]]
      } else {
        set data [::hv3::bookmarks::requestpage $path]
      }
    }
  }

  $downloadHandle finish $data
}

proc ::hv3::hv3_version {} {
  set src [split [::tkhtml::version] "\n"]
  foreach version [lsort [array names ::hv3::version]] {
    set t [string trim [string range $version 5 end-1]]
    lappend src $t
  }
  foreach s $src {
    lappend datelist [lrange $s 2 3]
  }
  return [lindex [lsort -decreasing $datelist] 0]
}

