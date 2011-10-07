
namespace eval ::hv3::profile {

  variable isEnabled 0

  proc enabled {} {
    variable isEnabled
    return $isEnabled
  }

  proc init {argv} {
    variable isEnabled

    if {[lsearch -exact $argv -profile]>=0} {
      set isEnabled 1

      # The following block creates a wrapper around the [proc] command
      # so that all new [proc]s instrumented.
      #
      uplevel #0 {
        rename proc real_proc
        real_proc proc {name arglist body} {
          uplevel [list real_proc $name $arglist $body]
          uplevel [list ::tkhtml::instrument command $name]
        }
      }
    }
  }

  proc instrument {cmd} {
    variable isEnabled
    if {$isEnabled == 0} return
    uplevel [list ::tkhtml::instrument command $cmd]
  }

  proc zero {} {
    set msg "Profile statistics will be zeroed after pressing OK"
    set i [tk_dialog .alert $msg $msg "" 0 OK]
    ::tkhtml::instrument zero
  }

  proc cache {} {
    set db ::hv3::sqlitedb
    $db transaction {
      catch {$db eval {DROP TABLE vectors}}
      $db eval {CREATE TEMP TABLE vectors(caller, proc, calls, clicks)}
      foreach {c p n clicks} [::tkhtml::instrument vectors] {
        $db eval { INSERT INTO vectors VALUES($c, $p, $n, $clicks) }
      }
      $db eval {CREATE INDEX v_index1 ON vectors(caller)}
      $db eval {CREATE INDEX v_index2 ON vectors(proc)}
    }
  }

  proc Report_line {chan zIndent nCall nClick zProc {zPercent ""}} {
    set iClicksPerCall [expr {int(double($nClick) / double($nCall))}]
    
    puts $chan [format {%s%7s %10s %s %-20s %s} \
      $zIndent  \
      $nCall    \
      $nClick   \
      $zPercent \
      "($iClicksPerCall us/call)" \
      $zProc    \
    ]
    
  }

  proc report {chan} {
    set db ::hv3::sqlitedb

    set total_clicks [$db eval {
      SELECT sum(clicks) FROM vectors WHERE caller = '<toplevel>'
    }]
    puts $chan "Total clicks: $total_clicks"
    puts $chan ""

    puts $chan [format "%7s %10s %10s  %s" Calls Self Children Proc]
    $db eval {
      SELECT proc, 
        sum(calls) AS calls, 
        sum(clicks) - COALESCE(
          (SELECT sum(clicks) FROM vectors WHERE caller = v1.proc), 0) AS self,
        (SELECT sum(clicks) FROM vectors WHERE caller = v1.proc) AS children
      FROM vectors AS v1
      GROUP BY proc
      ORDER BY self DESC
    } {
      puts $chan [format "%7s %10s %10s  %s" $calls $self $children $proc]
    }

    puts $chan ""
    puts $chan ""

    set idx 1
    $db eval {
      SELECT proc, 
        sum(calls) AS calls,
        sum(clicks) AS clicks
      FROM vectors
      GROUP BY proc
      ORDER BY sum(clicks) DESC
    } {

      puts $chan ""

      $db eval {
        SELECT 
          caller, calls, clicks
          FROM vectors
          WHERE proc = $proc
          ORDER BY clicks ASC
      } p {
        set percent [format {% 6s} [format %.1f [expr {
          100.0*double($p(clicks))/double($clicks)
        }]]]
        Report_line $chan "    " $p(calls) $p(clicks) $p(caller) ${percent}%
      }

      Report_line $chan "" $calls $clicks "$proc (${idx})"

      $db eval {
        SELECT 
          proc   AS cproc,
          calls  AS ccalls,
          clicks AS cclicks
          FROM vectors
          WHERE caller = $proc
          ORDER BY clicks DESC
      } {
        set percent [format {% 6s} [format %.1f [expr {
          100.0*double($cclicks)/double($clicks)
        }]]]
        Report_line $chan "    " $ccalls $cclicks $cproc ${percent}%
      }

      incr idx
    }
  }

  proc report_to_file {} {
    cache
    set f [tk_getSaveFile -initialfile profile]
    if {$f eq ""} return
    set chan [open $f w]
    report $chan
    close $chan
  }
}

proc children {cmd} {
  cache_vectors
  set db ::hv3::sqlitedb
  set ret ""
  $db eval {
    SELECT proc, calls, clicks FROM vectors WHERE caller = $cmd
    ORDER BY clicks desc
  } {
    append ret [
      format "%8d %12d %s\n" $calls $clicks $proc
    ]
  }
  set ret
}


