
namespace eval hv3 { set {version($Id$)} 1 }

# This file contains code to control a single instance of the 
# external program "hv3_polipo" that may be optionally used by
# hv3 as a web proxy.
#
# hv3_polipo is a slightly modified version of "polipo" by 
# (TODO: Juliusz full name) (TODO: polipo website).
#

namespace eval ::hv3::polipo {
  variable g

  # The port to listen on.
  set g(port) 8123

  # The path to the polipo binary.
  set g(binary) ""

  # The file-handle to the polipo's stdout. 
  set g(filehandle) ""

  # The file-handle to the polipo keepalive connection
  set g(keepalive) ""

  # Text of the log.
  set g(log) ""

  proc print {string} {
    variable g
    append g(log) $string
    if {[winfo exists .polipo]} {
      .polipo.text insert end $string
    }
  }

  # Initialise this sub-system. This proc sets the g(binary) variable -
  # the path to "polipo".
  #
  proc init {} {
    variable g

    set dir [file dirname [file normalize [info script]]]
    set prog hv3_polipo
    if {$::tcl_platform(platform) eq "windows"} {
      set prog hv3_polipo.exe
    }

    set locations [list [file dirname $dir] [pwd]]
    catch {
      set locations [concat $locations [split $::env(PATH) :]]
    }
    if {![info exists ::HV3_STARKIT]} {
      set locations [linsert $locations 0 $dir]
    }
    foreach loc $locations {
      set g(binary) [file join $loc $prog]
      if {[file executable $g(binary)]} {
        print "Using binary \"$g(binary)\"\n"
        break
      }
      set g(binary) ""
    }
    if {$g(binary) eq "" && [info exists ::HV3_STARKIT]} {
      set install_prog [file join $dir $prog]
      if {[file exists $install_prog]} {
        set g(binary) [install $install_prog]
      }
    }
  }

  # This proc is called when hv3 starts up if the following conditions
  # are met:
  #
  #     1. Hv3 is deployed as a starkit.
  #     2. There is no hv3_polipo[.exe] binary to be found.
  #     3. The starkit contains such a binary.
  #
  # We present the user with a dialog offering them the option to
  # copy the hv3_polipo binary out of the starkit to some location
  # on the disk, where we can execute it from.
  #
  # If the user does install the binary, the full path to it is 
  # returned. Otherwise an empty string is returned.
  #
  proc install {prog} {

    wm state . withdrawn

    set bin [file tail $prog]
    set path [file join [file dirname [file dirname $prog]] $bin]
    set radiooptions "-highlightthickness 0 -var ::hv3::polipo::c -bg white"

    # Document to
    set Template {
      <h1 style="text-align:center">Install hv3_polipo?</h1>
      <p>
	  Hv3 optionally uses an auxillary program, "hv3_polipo" to connect 
          to the internet. Currently, you do not have this program installed,
          but hv3 can install it now if you wish.
      <p>
	  Installing hv3_polipo does not require any further downloads, 
          requires less than 200KB disk space and greatly improves the
          performance of the hv3 web browser. Furthermore, once hv3_polipo
	  is installed, this irritating dialog box will stop appearing.The
          correct answer is to go ahead with the install.

      <table style="border:1px solid black;margin:1em auto; padding: 0 1em">
        <tr><td>
          <div class="widget" 
            cmd="radiobutton ${h}.r1 -val 1 $radiooptions">
            <td style="white-space:nowrap">Install hv3_polipo to $path
        <tr><td>
          <div class="widget" 
            cmd="radiobutton ${h}.r0 -val 0 $radiooptions">
            <td>Run without hv3_polipo.
      </table>

      <div class="widget" 
        style="width:90% ; margin:auto"
        cmd="button ${h}.button -text Ok -command ::hv3::polipo::install_ok">
    }

    set t [toplevel .polipo_install]
    wm title $t "Install hv3_polipo?"
    set h [html ${t}.html -width 800 -height 400 -shrink 1]
    pack $h -fill both -expand 1

    $h parse -final [subst -nocommands $Template]

    foreach node [$h search .widget] {
      $node replace [eval [$node attr cmd]]
    }
    set ::hv3::polipo::c 1

    bind .polipo_install <Destroy> ::hv3::polipo::install_cancel
    vwait ::hv3::polipo::signal
    destroy $t

    if {$::hv3::polipo::c} {
      if {[catch {
        file copy -force $prog $path
        if {$::tcl_platform(platform) eq "unix"} {
          file attributes $path -permissions rwxr-xr-x
        }
      }]} {
        tk_dialog .polipo_install "Installation failed" \
          "Installation of hv3_polipo failed (try running as root)" error 0 OK
      } else {
        wm state . normal
        return $path
      }
    }

    wm state . normal
    return ""
  }

  proc install_ok {} {
    bind .polipo_install <Destroy> ""
    set ::hv3::polipo::signal 1
  }

  proc install_cancel {} {
    exit
  }

  # Popup the gui log window.    
  proc popup {} {
    variable g
    if {![winfo exists .polipo]} {
      toplevel .polipo

      ::hv3::scrolled ::hv3::text .polipo.text -width 400 -height 250
      ::hv3::button .polipo.button -text "Restart Polipo" 

      pack .polipo.button -side bottom -fill x
      pack .polipo.text -fill both -expand true
      .polipo.button configure -command [namespace code restart]

      if {[string length $g(log)] > 10240} {
        set g(log) [string range $g(log) end-5120 end]
      }
      .polipo.text insert end $g(log)
    }
    raise .polipo
  }

  proc polipo_stdout {} {
    variable g
    if {[eof $g(filehandle)]} {
      set s "ERROR: Polipo failed."
      stop
      popup
    } else {
      set s [gets $g(filehandle)]
      if {$g(keepalive) eq "" && [scan $s "polipo port is %d" g(port)] == 1} {
        set g(keepalive) ""
        set g(keepalive) [socket localhost $g(port)]
      }
    }
    if {$s ne ""} { print "$s\n" }
  }

  # Stop any running polipo instance. If polipo is not running, this
  # proc is a no-op.
  proc stop {} {
    variable g
    catch {close $g(filehandle)}
    catch {close $g(keepalive)}
    set g(filehandle) ""
    set g(keepalive) ""
  }

  # (Re)start the polipo process. This proc blocks until polipo has
  # been successfully (re)started.
  proc restart {} {
    variable g

    # Make sure any previous polipo instance is finished.
    stop

    if {$g(binary) eq ""} {
      print "ERROR: No hv3_polipo binary found.\n"
      return
    }

    # Kick off polipo.
    set cmd "|{$g(binary)} dontCacheRedirects=true dontCacheCookies=true"
    append cmd " allowedPorts=1-65535"
    if {[info exists ::env(http_proxy)]} {
      append cmd " parentProxy=$::env(http_proxy)"
    }
    if {$::tcl_platform(platform) eq "unix"} {
      append cmd " |& cat"
      # set cmd "|{$g(binary)} diskCacheRoot=/home/dan/cache |& cat"
    }
    set fd [open $cmd r]
    fconfigure $fd -blocking 0
    fconfigure $fd -buffering none
    fileevent $fd readable [namespace code polipo_stdout]

    # Wait until the keepalive connection is established.
    set g(filehandle) $fd
    if {$g(keepalive) eq ""} {
      vwait ::hv3::polipo::g(keepalive)
    }

    # Log a fun and friendly message.
    if {$g(keepalive) ne ""} {
      print "INFO:  Polipo (re)started successfully.\n"
      catch {
        ::http::config -proxyhost 127.0.0.1
        ::http::config -proxyport $g(port)
      }
    } 
  }
}

namespace eval ::hv3::polipoclient {

  set N 0

  proc new {me args} {
    upvar #0 $me O

    set O(socket) [socket localhost [::http::config -proxyport]]

    fconfigure $O(socket)   \
        -blocking 0         \
        -encoding binary    \
        -translation binary \
        -buffering full     \
        -buffersize 8192

    fileevent $O(socket) readable [list $me ReadEvent]

    # State is always one of:
    # 
    #     "ready" 
    #     "waiting"
    #     "progress"
    #     "finished"
    #
    set O(state) ready

    set O(requesthandle) ""
    set O(protocol) ""

    set O(response) ""
    set O(headers) ""
    set O(data) ""

    incr ::hv3::polipoclient::N
    #puts "$::hv3::polipoclient::N requests outstanding"
  }

  proc destroy {me} {
    upvar #0 $me O
    catch {close $O(socket)}
    unset $me
    rename $me {}
    incr ::hv3::polipoclient::N -1
    #puts "$::hv3::polipoclient::N requests outstanding"
  }

  proc GET {me protocol requesthandle} {
    upvar #0 $me O
    if {$O(state) ne "ready"} { error "polipoclient state error" }

    set R $requesthandle
    set O(requesthandle) $R
    set O(protocol) $protocol

    # Make the GET request:
    #
    puts $O(socket)   "GET [$R cget -uri] HTTP/1.0"
    puts $O(socket)   "Accept: */*"
    puts $O(socket)   "User-Agent: [::http::config -useragent]"
    foreach {k v} [$R cget -requestheader] {
      puts $O(socket) "$k: $v"
    }
    puts $O(socket)   ""

    set O(state) "waiting"
    flush $O(socket)
  }

  proc ReadEvent {me args} {
    upvar #0 $me O

    append data [read $O(socket)]

    if {$O(state) eq "waiting"} {
      set iStart [expr { [string first "\x0D\x0A" $data]         + 2}]
      set iEnd   [expr { [string first "\x0D\x0A\x0D\x0A" $data] - 1}]
      if {$iStart>=0} {
        set O(response) [string range $data 0 [expr {$iStart - 3}]]
      }
      if {$iEnd>=0} {
        set O(state) progress
        set header ""
        foreach line [split [string range $data $iStart $iEnd] "\x0D\x0A"] {
          if {$line eq ""} continue
          set i [string first : $line]
          if {$i>=0} {
            lappend header [string trim [string range $line 0 [expr {$i-1}]]]
            lappend header [string trim [string range $line [expr {$i+1}] end]]
          }
        }
        $O(requesthandle) configure -header $header
        $O(protocol) AddToProgressList $O(requesthandle)

        set data [string range $data $iEnd+5 end]
      }
    }

    $O(requesthandle) append $data
    $O(protocol) BytesReceived $O(requesthandle) [string length $data]

    #puts "read [string length [read $O(socket)]] bytes"
    if {[eof $O(socket)]} {
      close $O(socket)
      $O(requesthandle) finish $data
    }
  }
}
::hv3::make_constructor ::hv3::polipoclient

::hv3::polipo::init
::hv3::polipo::restart
#::hv3::polipo::popup

