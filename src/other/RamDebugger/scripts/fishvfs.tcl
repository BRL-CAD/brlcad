 # fishvfs.tcl --
 #
 # A "FIles transferred over SHell" virtual filesystem
 # This is not an official "FISH" protocol client as described at:
 #      http://mini.net/tcl/12792
 # but it utilizes the same concept of turning any computer that offers
 # access via ssh, rsh or similar shell into a file server.
 #
 # This code requires that the template vfs (http://mini.net/tcl/11938) procedures
 # have already been sourced into tclsh.
 #
 # Usage: Mount ?-volume? \
 #      <remote directory> \            # an existing directory on the remote filesystem
 #      ?-transport <protocol>? \       # can be ssh, rsh or plink
 #      ?-user <username>? \            # remote computer login name
 #      ?-password <password>? \        # remote computer login password
 #      ?-host <remote hostname>? \     # remote computer domain name
 #      ?-port <port number>? \         # override default port
 #      <virtual mount dir or URL>
 #
 # examples:
 #
 # Mount / -transport ssh -user root -host tcl.tk /mnt/vfs/tcl
 #
 # Mount -volume /home/foo rsh://foo@localcomp
 #
 # Mount -volume / -password foopass plink://foo@bar.org:2323/remotemount
 #
 # The vfs can be mounted as a local directory, or as a URL in conjunction with
 # the "-volume" option.
 #
 # The URL can be of the form:
 #
 # transport://[user[:password]@]host[:port][/filename]
 #
 # Option switches can be used in conjunction with a URL to specify connection
 # information; the option switch values will override the URL values.
 #
 # After a channel opened for writing is closed, if a file named ~/.fish_close
 # exists on the remote computer it will be executed as a shell script in the
 # background (with the name of the file written as a command line argument),
 # allowing post-write processing.  For example, .fish_close could be a script
 # that commits changes to a CVS repository.
 #
 # client configuration:
 #
 # The shell client must be in the PATH and configured for non-interactive
 # (no password prompt) use.
 #
 # The value of the -transport option is assumed to be the name of a handler
 # procedure which is called to handle the specifics of the particular client.
 # Handlers for the supported transports (ssh, rsh, plink) already exist.
 # New clients can be added simply by providing a suitable handler procedure.
 #
 # server configuration:
 #
 # The remote computer is assumed to have a bourne-type shell and the standard
 # GNU fileutils, but otherwise no configuration is needed.

 # templatevfs.tcl --
 #
 #         The template vfs simply mirrors a real directory to a virtual location.
 #
 # Usage: Mount ?-volume? <real directory> <virtual directory>
 #
 # Written by Stephen Huntley (stephen.huntley@alum.mit.edu)

package require vfs 1

# begin namespace eval ::vfs::template
 namespace eval ::vfs::template {}

 proc ::vfs::template::Mount {args} {
	set volume {}
	if {[lindex $args 0] == "-volume"} {set volume "-volume"}
	set pathto [eval MountProcedure $args]
	set path [lindex $pathto 0]
	set to [lindex $pathto 1]
	eval ::vfs::filesystem mount $volume \$to \[list [namespace current]::handler \$path\]
	::vfs::RegisterMount $to [list [namespace current]::Unmount]
	if {[trace info execution close] == {}} {
		trace add execution close leave [namespace current]::CloseTrace
	}
	return $to
 }

 proc ::vfs::template::Unmount {to} {
	set to [file normalize $to]
	set path [lindex [::vfs::filesystem info $to] end]
	UnmountProcedure $path $to
	::vfs::filesystem unmount $to
 }

 proc ::vfs::template::CloseTrace {commandString code result op} {
	set errorCode $::errorCode
	set errorInfo $::errorInfo
	set channelID [lindex $commandString 1]
	if [regexp {::Close ([^ ]+?) } $errorInfo trash errorChannelID] {
		if [string equal $channelID $errorChannelID] {
		        if {[lindex $errorCode 0] == "POSIX"} {
		                set pError [lindex $errorCode 1]
		                ::vfs::filesystem posixerror $::vfs::posix($pError) ; return -code error $::vfs::posix($pError)
		        }
		        set message [lindex [split $errorInfo \n] 0]
		        error $message $errorInfo $errorCode
		}
	}
 }

 proc ::vfs::template::handler {path cmd root relative actualpath args} {
	switch -- $cmd {
		access {
		        set mode [lindex $args 0]
		        set error [catch {Access $path $root $relative $actualpath $mode}]
		        if $error {::vfs::filesystem posixerror $::vfs::posix(EACCES) ; return -code error $::vfs::posix(EACCES)}
		}
		createdirectory {
		        CreateDirectory $path $root $relative $actualpath
		}
		deletefile {
		        DeleteFile $path $root $relative $actualpath
		}
		fileattributes {
		        set index [lindex $args 0]
		        set value [lindex $args 1]
		        if [info exists [namespace current]::attributes([file join $root $relative])] {
		                array set attributes [set [namespace current]::attributes([file join $root $relative])]
		        } else {
		                array set attributes [FileAttributes $path $root $relative $actualpath]
		                set [namespace current]::attributes([file join $root $relative]) [array get attributes]
		        }
		        if {$index == {}} {
		                return [lsort [array names attributes]]
		        }
		        set attribute [lindex [lsort [array names attributes]] $index]
		        if {$value == {}} {
		                return $attributes($attribute)
		        }
		        FileAttributesSet $path $root $relative $actualpath $attribute $value
		        array unset [namespace current]::attributes [file join $root $relative]
		}
		matchindirectory {
		        set pattern [lindex $args 0]
		        set types [lindex $args 1]
		        return [MatchInDirectory $path $root $relative $actualpath $pattern $types]
		}
		open {
		        set mode [lindex $args 0]
		        if {$mode == {}} {set mode r}
		        set permissions [lindex $args 1]
		        set channelID [Open $path $root $relative $actualpath $mode $permissions]
		        set eofChar {{} {}}
		        if [string equal $::tcl_platform(platform) "windows"] {set eofChar "\x1a {}"}
		        fconfigure $channelID -encoding [encoding system] -eofchar $eofChar -translation auto
		        switch -glob -- $mode {
		                "" -
		                "r*" -
		                "w*" {
		                        seek $channelID 0
		                }
		                "a*" {
		                         seek $channelID 0 end
		                }
		                default {
		                        ::vfs::filesystem posixerror $::vfs::posix(EINVAL)
		                        return -code error $::vfs::posix(EINVAL)
		                }
		        }
		        set result $channelID
		        lappend result [list [namespace current]::Close $channelID $path $root $relative $actualpath $mode]
		        return $result
		}
		removedirectory {
		        set recursive [lindex $args 0]
		        if !$recursive {
		                if {[MatchInDirectory $path $root $relative $actualpath * 0] != {}} {
		                        ::vfs::filesystem posixerror $::vfs::posix(ENOTEMPTY)
		                        return -code error $::vfs::posix(ENOTEMPTY)
		                }
		        }
		        RemoveDirectory $path $root $relative $actualpath
		}
		stat {
		        return [Stat $path $root $relative $actualpath]
		}
		utime {
		        set atime [lindex $args 0]
		        set mtime [lindex $args 1]
		        Utime $path $root $relative $actualpath $atime $mtime
		}
	}
 }

 namespace eval ::vfs::template {

 proc MountProcedure {args} {
	foreach templateProc "Mount Unmount CloseTrace handler Access CreateDirectory DeleteFile FileAttributes FileAttributesSet MatchInDirectory Open RemoveDirectory Stat Utime" {
		set infoArgs [info args ::vfs::template::$templateProc]
		set infoBody [info body ::vfs::template::$templateProc]
		proc $templateProc $infoArgs $infoBody
	}
	if {[lindex $args 0] == "-volume"} {
		set args [lrange $args 1 end]
		set to [lindex $args end]
	} else {
		set to [file normalize [lindex $args end]]
	}
	set path [file normalize [lindex $args 0]]
	if [info exists ::vfs::_unmountCmd($to)] {::vfs::unmount $to}

	file mkdir $path
	array unset ::vfs::_unmountCmd $to
	lappend pathto $path
	lappend pathto $to
	return $pathto
 }

 proc UnmountProcedure {path to} {
	return
 }

 proc Access {path root relative actualpath mode} {
	set fileName [file join $path $relative]
	set modeString [::vfs::accessMode $mode]
	if {$modeString == "F"} {
		if [file exists $fileName] {return}
		error "no such file or directory"
	}
	set modeString [split $modeString {}]
	set fileString {}
	if {[string equal $modeString "R"] && [file readable $fileName]} {return}
	if {[string equal $modeString "W"] && [file writable $fileName]} {return}
	if {[string equal $modeString "X"] && [file executable $fileName]} {return}
	file stat $fileName stat
	foreach { mask pairs } {
		00400 { 00400 r }
		00200 { 00200 w }
		04100 { 04100 s 04000 S 00100 x }
		00040 { 00040 r }
		00020 { 00020 w }
		02010 { 02010 s 02000 S 00010 x }
		00004 { 00004 r }
		00002 { 00002 w }
		01001 { 01001 t 01000 T 00001 x }
	    } {
		set value [expr $stat(mode) & $mask]
		set bit -
		foreach { x b } $pairs {
		    if { $value == $x } {
		        set bit $b
		    }
		}
		append bitString $bit
      }
	set readable [regexp -all "r" $bitString]
	set writable [regexp -all "w" $bitString]
	set executable [regexp -all "x" $bitString]
	foreach {mode count} "R $readable W $writable X $executable" {
		if {([string first $mode $modeString] > -1) && !$count} {error "$mode access not allowed"}
	}
	if [string equal $modeString "X W"] {
		if {($writable == 3) && ($executable == 3)} {
		        return
		} elseif [file writable $fileName] {
		        if {[regexp -all "wx" $bitString] == $writable} {
		                return
		        } elseif [file executable $fileName] {
		                return
		        }
		}
	}
	if [string equal $modeString "R W"] {
		if {($writable == 3) && ($readable == 3)} {
		        return
		} elseif [file writable $fileName] {
		        if {[regexp -all "rw" $bitString] == $writable} {
		                return
		        } elseif [file readable $fileName] {
		                return
		        }
		}
	}
	if [string equal $modeString "R X"] {
		if {($readable == 3) && ($executable == 3)} {
		        return
		} elseif [file executable $fileName] {
		        if {[regexp -all {r[w-]x} $bitString] == $executable} {
		                return
		        } elseif [file readable $fileName] {
		                return
		        }
		}
	}

	foreach mS $modeString {
		set errorMessage "not [string map {R readable W writable X executable} $mS]"
		if {[lsearch $fileString $mS] == -1} {error $errorMessage}
	}
 }

 proc CreateDirectory {path root relative actualpath} {
	file mkdir [file join $path $relative]
 }

 proc DeleteFile {path root relative actualpath} {
	set fileName [file join $path $relative]
	file delete $fileName
 }

 proc FileAttributes {path root relative actualpath} {
	set fileName [file join $path $relative]
	file attributes $fileName
 }

 proc FileAttributesSet {path root relative actualpath attribute value} {
	set fileName [file join $path $relative]
	file attributes $fileName $attribute $value
 }

 proc MatchInDirectory {path root relative actualpath pattern types} {
	if [::vfs::matchDirectories $types] {lappend typeString d}
	if [::vfs::matchFiles $types] {lappend typeString f}

	set globList [glob -directory [file join $path $relative] -nocomplain -types $typeString $pattern]
	if [catch {::vfs::filesystem info $path}] {
	    append globList " [glob -directory [file join $path $relative] -nocomplain -types "$typeString hidden" $pattern]"
	}
	set newGlobList {}
	foreach gL $globList {
		set gL [eval file join \$root [lrange [file split $gL] [llength [file split $path]] end]]
		lappend newGlobList $gL
	}
	set newGlobList [lsort -unique $newGlobList]
	return $newGlobList
 }

 proc Open {path root relative actualpath mode permissions} {
	set fileName [file join $path $relative]
	set newFile 0
	if ![file exists $fileName] {set newFile 1}
	set channelID [open $fileName $mode]
	if $newFile {catch {file attributes $fileName -permissions $permissions}}
	return $channelID
 }

 proc Close {channelID path root relative actualpath mode} {
 # Do not close the channel in the close callback!
 # It will crash Tcl!  Honest!
 # The core will close the channel after you've taken what info you need from it.

	if [string equal $mode "r"] {return}
 #        close $channelID
	return
 }

 proc RemoveDirectory {path root relative actualpath} {
	file delete -force [file join $path $relative]
 }

 proc Stat {path root relative actualpath} {
	file stat [file join $path $relative] fs
	return [list dev $fs(dev) ino $fs(ino) mode $fs(mode) nlink $fs(nlink) uid $fs(uid) gid $fs(gid) size $fs(size) atime $fs(atime) mtime $fs(mtime) ctime $fs(ctime) type $fs(type)]
 }

 proc Utime {path root relative actualpath atime mtime} {
	set fileName [file join $path $relative]
	file atime $fileName $atime
	file mtime $fileName $mtime
 }

 }
 # end namespace eval ::vfs::template


 package require vfs 1
 namespace eval ::vfs::template::fish {}

 proc ::vfs::template::fish::Mount {args} {
	eval [info body ::vfs::template::Mount]
 }

 namespace eval ::vfs::template::fish {

 proc MountProcedure {args} {
	foreach templateProc "Mount Unmount CloseTrace handler Access CreateDirectory DeleteFile FileAttributes FileAttributesSet MatchInDirectory Open RemoveDirectory Stat Utime" {
		set infoArgs [info args ::vfs::template::$templateProc]
		set infoBody [info body ::vfs::template::$templateProc]
		proc $templateProc $infoArgs $infoBody
	}
	if {[lindex $args 0] == "-volume"} {
		set args [lrange $args 1 end]
		set to [lindex $args end]
	} else {
		set to [file normalize [lindex $args end]]
	}
	set path [lindex $args 0]
	if [info exists ::vfs::_unmountCmd($to)] {::vfs::unmount $to}
	array unset ::vfs::_unmountCmd $to

	array set params [FileTransport $to]
	if {[llength $args] > 2} {
		set args [lrange $args 1 end-1]
		set argsIndex [llength $args]
		for {set i 0} {$i < $argsIndex} {incr i} {
		        set arg [lindex $args $i]
		        if {[string index $arg 0] == "-"} {
		                set arg [string range $arg 1 end]
		                set params($arg) [lindex $args [incr i]]
		        }
		}
	}
	set [namespace current]::transport($to) [array get params]

	file mkdir $path
	lappend pathto $path
	lappend pathto $to
	return $pathto
 }

 proc UnmountProcedure {path to} {
	unset [namespace current]::transport($to)
	array unset ::vfs::_unmountCmd $to
	return
 }

 proc Close {channelID path root relative actualpath mode} {
 # Do not close the channel in the close callback!
 # It will crash Tcl!  Honest!
 # The core will close the channel after you've taken what info you need from it.

	if [string equal $mode "r"] {return}

	# Ha ha ha! Try and stop me!
	close $channelID
	return
 }

 proc close {channelID} {
	upvar 1 root root
	upvar 1 path path
	upvar 1 relative relative
	set fileName [file join $path $relative]

	fconfigure $channelID -translation binary
	seek $channelID 0 end
	set channelSize [tell $channelID]

	set command "cat>'$fileName'\;cat>/dev/null"
	FileCommand $root $command stdin $channelID

	set command "ls -l '$fileName' | ( read a b c d x e\; echo \$x )"
	set fileSize [FileCommand $root $command]
	if {$channelSize != $fileSize} {error "couldn't save \"$fileName\": Input/output error" "Input/output error" {POSIX EIO {Input/output error}}}
	set command "nohup ~/.fish_close '$fileName' &"
	catch {FileCommand $root $command}
	return
 }

 proc file {args} {
	switch -- [lindex $args 0] {
		join -
		normalize -
		split -
		volume {
		        return [eval ::file $args]
		}
	}

	upvar 1 to fileTo
	upvar 1 root fileRoot
	if [info exists fileTo] {set root $fileTo}
	if [info exists fileRoot] {set root $fileRoot}

	set fileName [lindex $args 1]
	set tail [::file tail $fileName]
	if [string equal $tail {}] {set tail $fileName}

	switch -- [lindex $args 0] {
		atime {
		        set atime [lindex $args 2]
		        set command "find '$fileName' -maxdepth 1 -name '$tail' -printf %A@\\\\n"
		        if ![string equal $atime {}] {
		                set atime [clock format $atime -format %Y%m%d%H%M.%S]
		                set command "touch -a -c -t $atime '$fileName'"
		        }
		}
		attributes {
		        set attribute [lindex $args 2]
		        set value [lindex $args 3]
		        if {([string equal $attribute {}]) || ([string equal $value {}])} {
		                set command "find $fileName -maxdepth 1 -name '$tail' -printf '%u %g %m\\n'"
		        } elseif ![string first $attribute "-group"] {
		                set command "chgrp $value $fileName"
		        } elseif ![string first $attribute "-owner"] {
		                set command "chown $value $fileName"
		        } elseif ![string first $attribute "-permissions"] {
		                set command "chmod $value $fileName"
		        }
		}
		delete {
		        set command "rm -f '$fileName'"
		        if [string equal $fileName "-force"] {
		                set dirName [lindex $args 2]
		                set command "rm -rf '$dirName'"
		        }
		}
		executable -
		exists -
		readable -
		writable {
		        set type [string map {executable x exists e readable r writable w} [lindex $args 0]]
		        set command "if \[ -$type '$fileName' \]\; then echo 1\; else echo 0\; fi"
		}
		mkdir {
		        set  command "mkdir -p '$fileName'"
		}
		mtime {
		        set mtime [lindex $args 2]
		        set command "find '$fileName' -maxdepth 1 -name '$tail' -printf %T@\\\\n"
		        if ![string equal $mtime {}] {
		                set mtime [clock format $mtime -format %Y%m%d%H%M.%S]
		                set command "touch -c -m -t $mtime '$fileName'"
		        }
		}
		stat {
		        set arrayName [lindex $args 2]
		        set command "find '$fileName' -maxdepth 1 -name '$tail' -printf '%A@ %C@ %G %i %m %T@ %n %s %U\\n' \; if \[ -d '$fileName' \]\; then echo 1\; else echo 0\; fi"

		        if [info exists ::vfs::template::fish::stat($fileName)] {
		                set returnValue $::vfs::template::fish::stat($fileName)
		                unset ::vfs::template::fish::stat($fileName)
		        }
		}
	}
	if ![info exists returnValue] {set returnValue [FileCommand $root $command]}
	set returnValue [string trim $returnValue]
	switch -- [lindex $args 0] {
		atime -
		mtime {
		        if [string equal [lindex $args 2] {}] {
		                return $returnValue
		        }
		}
		attributes {
		        if [string equal $attribute {}] {
		                return "-group [lindex $returnValue 1] -owner [lindex $returnValue 0] -permissions [lindex $returnValue 2]"
		        }
		        if [string equal $value {}] {
		                if ![string first $attribute "-group"] {
		                        return [lindex $returnValue 1]
		                } elseif ![string first $attribute "-owner"] {
		                        return [lindex $returnValue 0]
		                } elseif ![string first $attribute "-permissions"] {
		                        return [lindex $returnValue 2]
		                }
		        }
		}
		executable -
		exists -
		readable -
		writable {
		        return $returnValue
		}
		stat {
		        eval upvar 1 $arrayName\(mtime) mtime $arrayName\(gid) gid $arrayName\(nlink) nlink $arrayName\(atime) atime $arrayName\(mode) mode $arrayName\(type) type $arrayName\(ctime) ctime $arrayName\(uid) uid $arrayName\(ino) ino $arrayName\(size) size $arrayName\(dev) dev
		        set atime [lindex $returnValue 0]
		        set ctime [lindex $returnValue 1]
		        set gid [lindex $returnValue 2]
		        set ino [lindex $returnValue 3]
		        set mode [lindex $returnValue 4]
		        set mtime [lindex $returnValue 5]
		        set nlink [lindex $returnValue 6]
		        set size [lindex $returnValue 7]
		        set uid [lindex $returnValue 8]
		        set dir [lindex $returnValue 9]
		        if $dir {set type directory} else {set type file}
		        set dev 0
		}
	}
	return
 }

 proc glob {args} {
	upvar 1 path path
	upvar 1 root root
	upvar 1 relative relative

	set pattern [lindex $args end]
	set args [string map {-nocomplain {}} $args]
	array set argsArray [lrange $args 0 end-1]
	set hidden 0
	if {[lindex $argsArray(-types) end] == "hidden"} {
		set hidden 1
		set argsArray(-types) [lrange $argsArray(-types) 0 end-1]
	}
	if $hidden {eval return \$[namespace current]::hidden(\$argsArray(-directory))}
	array unset [namespace current]::hidden $argsArray(-directory)

	set command "find '$argsArray(-directory)' -maxdepth 1 -mindepth 1 -type d -printf '%A@ %C@ %G %i %m %T@ %n %s %U \{%f\}\\n' \; echo / \; find '$argsArray(-directory)' -maxdepth 1 -type f -printf '%A@ %C@ %G %i %m %T@ %n %s %U \{%f\}\\n'"

	set returnValue [FileCommand $root $command]
	set returnValue [split $returnValue /]

	set dirs [lindex $returnValue 0]
	set dirs [string trim $dirs]
	set dirs [split $dirs \n]
	#ramsan
	set newDirs ""
	foreach dir $dirs {
		set dir [linsert $dir end-1 1]
		lappend newDirs $dir
	}
	set dirs $newDirs
	unset newDirs

	set files [lindex $returnValue 1]
	set files [string trim $files]
	set files [split $files \n]
	set newFiles ""
	foreach file $files {
		set file [linsert $file end-1 0]
		lappend newFiles $file
	}
	set files $newFiles
	unset newFiles

	set dir [lsearch $argsArray(-types) "d"]
	set file [lsearch $argsArray(-types) "f"]
	incr dir ; incr file

	if $dir {set values $dirs}
	if $file {set values $files}
	if {$dir && $file} {set values [concat $dirs $files]}

	# ramsan change
	set ::vfs::template::fish::hidden($argsArray(-directory)) ""
	set fileNames ""
	foreach fileName $values {
		set stat [lrange $fileName 0 end-1]
		set fileName [lindex $fileName end]
		set ::vfs::template::fish::stat([file join $path $relative $fileName]) $stat
		if [string equal $fileName ".fish_close"] {continue}
		if ![string match $pattern $fileName] {continue}
		if {[string index $fileName 0] == "."} {lappend ::vfs::template::fish::hidden($argsArray(-directory)) [file join $path $relative $fileName] ; continue}
		lappend fileNames [file join $path $relative $fileName]
	}

	return $fileNames
 }

 proc open {fileName mode} {
	upvar 1 root root
	set command "ls -l '$fileName' | ( read a b c d x e\; echo \$x )"
	if {([catch {set fileSize [FileCommand $root $command]}]) && ($mode == "r")} {error "couldn't open \"$fileName\": no such file or directory" "no such file or directory" {POSIX ENOENT {no such file or directory}}}

	set channelID [::vfs::memchan]

	set command "touch -a '$fileName'"
	catch { FileCommand $root $command }
	if [string equal $mode w] {return $channelID}

	fconfigure $channelID -translation binary
	set command "cat '$fileName'"
	FileCommand $root $command stdout $channelID
	seek $channelID 0 end
	set channelSize [tell $channelID]
	if {[info exists $fileSize] && ($channelSize != $fileSize)} {error "Input/output error" "Input/output error" {POSIX EIO {Input/output error}}}
	return $channelID
 }

 proc FileCommand {root command args} {
	array set params $::vfs::template::fish::transport($root)
	array set params $args
	set params(command) $command
	if ![info exists params(transport)] {set params(transport) local}
	set commandLine [eval ::vfs::template::fish::transport::\$params(transport) [array get params]]

	if [string equal $commandLine {}] {return}

	if [info exists params(stdin)] {
		set execID [eval ::open \"|$commandLine\" w]
		fconfigure $execID -translation binary
		seek $params(stdin) 0
		puts -nonewline $execID [read $params(stdin)]
		::close $execID
		return
	}

	if [info exists params(stdout)] {
		set execID [eval ::open \"|$commandLine\" r]
		fconfigure $execID -translation binary
		seek $params(stdout) 0
		puts -nonewline $params(stdout) [read $execID]
		::close $execID
		return
	}
	eval exec $commandLine
 }

 proc FileTransport {filename} {
	if {[string first : $filename] < 0} {return [list transport {} user {} password {} host {} port {} filename [file normalize $filename]]}
	#if {[string first [string range $filename 0 [string first : $filename]] [file volume]] > -1} {return [list transport {} user {} password {} host {} port {} filename [file normalize $filename]]}

	set filename $filename/f
	set transport {} ; set user {} ; set password {} ; set host {} ; set port {}
	set transport {} ; set user {} ; set password {} ; set host {} ; set port {}
	regexp {(^[^:]+)://} $filename trash transport
	regsub {(^[^:]+://)} $filename "" userpasshost
	set userpass [lindex [split $userpasshost @] 0]
	set user $userpass
	regexp {(^[^:]+):(.+)$} $userpass trash user password

	if {[string first @ $userpasshost] == -1} {set user {} ; set password {}}

	regsub {([^/]+)(:[^/]+)(@[^/]+)} $filename \\1\\3 filename

	if [regexp {(^[^:]+)://([^/:]+)(:[^/:]*)*(.+$)} $filename trash transport host port filename] {
		regexp {([0-9]+)} $port trash port
		if {[string first [lindex [file split $filename] 1] [file volume]] > -1} {set filename [string range $filename 1 end]}
	} else {
		set host [lindex [split $filename /] 0]
		set filename [string range $filename [string length $host] end]
		set port [lindex [split $host :] 1]
		set host [lindex [split $host :] 0]
	}
	regexp {^.+@(.+)} $host trash host
	set filename [string range $filename 0 end-2]
	return [list transport $transport user $user password $password host $host port $port filename $filename ]
 }

 }
 # end namespace eval ::vfs::template::fish

 namespace eval ::vfs::template::fish::transport {

 proc local {command args} {
	return $command
 }

 proc plink {args} {
	array set params $args
	set port {}
	set user ""
	set password ""
	if ![string equal $params(port) {}] {set port "-P $params(port)"}
	if ![string equal $params(user) {}] {set user "-l $params(user)"}
	if ![string equal $params(password) {}] {set password "-pw $params(password)"}
	#ramsan: added /bin/bash -c \\
	return "plink -ssh $port $user -batch $password $params(host) \$command"
	#return "plink -ssh $port $user -batch $password $params(host) /bin/bash -c \\\"\$command\\\""
 }

 proc rsh {args} {
	array set params $args
	set user {}
	if ![string equal $params(user) {}] {set user "-l $params(user)"}

	return "rsh $user $params(host) \"$params(command)\""
 }

 proc ssh {args} {
	array set params $args
	set port {}
	if ![string equal $params(port) {}] {set port "-p $params(port)"}
	set user {}
	if ![string equal $params(user) {}] {set user "-l $params(user)"}

	return "ssh $port $user $params(host) \"$params(command)\""
 }

 }
 # end namespace eval ::vfs::template::fish::transport


