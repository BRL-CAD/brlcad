
proc childcount { args } {
	if { [llength $args] != 1 } { return "Usage: childcount collection" }

	set collection [lindex $args 0]

	if { [lindex [get $collection] 0 ] != "comb" } { return "This is not a collection!" }

	set collData [lt $collection]

	return "[llength $collData]"	
}

proc remdup { args } {
	if { [llength $args] != 1 } { return "Usage: remdup collection" }

	set collection [lindex $args 0]

	if { [lindex [get $collection] 0 ] != "comb" } { return "This is not a collection!" }

	set collData [lt $collection]

	set beforeCnt [childcount $collection]

	foreach row $collData {
		
		set rowBool [lindex $row 0]
		set rowName [lindex $row 1]

		if { $rowBool == "u" } {

			rm $collection $rowName
			g $collection $rowName
				
		}
	}

	set collData [lt $collection]
	set afterCnt [childcount $collection]

	return "$beforeCnt \-\> $afterCnt "

}

proc get_Rectangle_coords {} {

	set dim [rset r dim]     
	set dimX [lindex $dim 0].0     
	set dimY [lindex $dim 1].0    
 
	if { $dimX == 0.0 || $dimY == 0.0 } {
	    return "no_result"
	}

	set size [dm size]     
	set sizeX [lindex $size 0].0     
	set adj [expr ($sizeX / 2.0) - 1.0]   

	set pos [rset r pos]     
	set posX [lindex $pos 0].0     
	set posY [lindex $pos 1].0  
   
	if { $dimX > 0 } { 	
		set minX [expr $posX - $adj] 	
		set maxX [expr ($posX + ($dimX - 1.0)) - $adj]     
	} else { 	
		set minX [expr ($posX + ($dimX + 1.0)) - $adj]     
		set maxX [expr $posX - $adj] 	
	}     

	if { $dimY > 0 } { 	
		set minY [expr $posY - $adj]	
		set maxY [expr ($posY + ($dimY - 1.0)) - $adj]     
	} else { 	
		set minY [expr ($posY + ($dimY + 1.0)) - $adj]     
		set maxY [expr $posY - $adj]	
	}      
	
	#puts "Rectangle: $minX $maxX $minY $maxY"
	return [list $minX $maxX $minY $maxY]
}


proc getObjInRectangle {} {

	set rect [get_Rectangle_coords]

	if { $rect == "no_result" } {
	    return $rect 
	}

        set size [dm size]
        set sizeX [lindex $size 0].0
	set adj [expr {($sizeX / 2.0) - 1.0}]

	set minX [lindex $rect 0]
	set maxX [lindex $rect 1]
	set minY [lindex $rect 2]
	set maxY [lindex $rect 3]

	# 'x 3' returns results in base units (mm)
	set xList [x 3]

	set indicies [lsearch -all $xList "/*"]
	set objs ""     
	set index -1
	
	#puts stdout "Starting run..."
	#puts stdout "indicies\: $indicies"
	#puts stdout "Length of xList\: [llength $xList]"

	set indicies_length [llength $indicies]
	
	for {set a 0} { $a < $indicies_length } { incr a } {
	
		#set boundries
		
		set thisID [lindex $indicies $a]
		if { [expr {$a + 1} ] == [llength $indicies]} {
			#last element
			set lastID [expr {[llength $xList] - 1} ]
		} else {
			set lastID [lindex $indicies [expr {$a + 1} ]]
		}
		
		#get solid name
		set line [lindex $xList $thisID]
		set tempIndex [string last "/" $line ] ; incr tempIndex
		set objName [string range $line $tempIndex end]
		#puts stdout "$objName"
		
		incr thisID 14

		#puts stdout "Searching range\: $thisID to $lastID"
		
		for {set i $thisID } {$i <= $lastID } { incr i } {
			set line [lindex $xList $i]
					
			if {$line == "line"} {
				#Extract the strings

				#tk_messageBox -message "line0: '$line'"
				
				incr i 2 ; set line [lindex $xList $i]
				set modelX [string range $line 1 end-1]
				
				incr i 1 ; set line [lindex $xList $i]
				set modelY [string range $line 0 end-1]
				
				incr i 1 ; set line [lindex $xList $i]
				set modelZ [string range $line 0 end-1]

				#Convert
				set viewCoord [model2view $modelX $modelY $modelZ]

                                #Evaluate
                                set ViewX [expr [lindex $viewCoord 0] * $adj]
                                if { [expr $ViewX < $minX] } {
                                    continue
                                }
                                if { [expr $ViewX > $maxX] } {
                                    continue
                                }
                                set ViewY [expr [lindex $viewCoord 1] * $adj]
                                if { [expr $ViewY < $minY] } {
                                    continue
                                }
                                if { [expr $ViewY > $maxY] } {
                                    continue
                                }

                                lappend objs $objName
                                break
			}
		}
	}

	#puts stdout "Ending run..."

	#puts stdout "Results\: $objs "
	return $objs
}


proc gr {GroupName Boolean} {grouper $GroupName $Boolean}


proc grouper {GroupName Boolean} {
	global mged_gui mged_default mged_players

	for {set i 0} {1} {incr i} {
		set id [subst $mged_default(id)]_$i

		if {[lsearch -exact $mged_players $id] != -1} {
			break;
		}
	}
  	
	if {$Boolean != "+" && $Boolean != "-" } {return "Please provide a Boolean of either + or -"}
	
	set mged_gui($id,mouse_behavior) p     
	set_mouse_behavior $id     
	rset vars fb 0     
	rset r draw 1     
	bind $mged_gui($id,active_dm) <ButtonRelease-2> " winset $mged_gui($id,active_dm); dm idle; do_grouper $GroupName $Boolean" 
	bind $mged_gui($id,active_dm) <Control-ButtonRelease-2> " winset $mged_gui($id,active_dm); dm idle; done_grouper" 
}

proc do_grouper {GroupName Boolean } {

	set objs [lsort -unique [getObjInRectangle]]
	set tot_obj_in_rect [llength $objs]

	if { $objs == "no_result" } {
	    return
	}

	if {[search -name $GroupName] == ""} {
	    set grp_obj_list_before ""
	    set grp_obj_list_len_before 0
	} else {
	    set grp_obj_list_before [search $GroupName]
	    set grp_obj_list_len_before [expr [llength $grp_obj_list_before] - 1]
	}
	
	if {$Boolean == "+" } {
		foreach obj $objs {g $GroupName $obj}
	} else {
		foreach obj $objs {catch {rm $GroupName $obj} tmp_msg}
	}
	
	if {[search -name $GroupName] != "" && $Boolean == "+"} {
	    remdup $GroupName
	}

	if {[search -name $GroupName] == ""} {
	    set grp_obj_list_after ""
	    set grp_obj_list_len_after 0
	} else {
	    set grp_obj_list_after [search $GroupName]
	    set grp_obj_list_len_after [expr [llength $grp_obj_list_after] - 1]
	}

	if { $grp_obj_list_len_after > $grp_obj_list_len_before } {
		set objcnt_chng [expr $grp_obj_list_len_after - $grp_obj_list_len_before]
		puts stdout "$objcnt_chng of $tot_obj_in_rect objects added to '$GroupName'"
	} elseif { $grp_obj_list_len_after < $grp_obj_list_len_before } {
		set objcnt_chng [expr $grp_obj_list_len_before - $grp_obj_list_len_after]
		puts stdout "$objcnt_chng of $tot_obj_in_rect objects removed from '$GroupName'"
	} else {
		puts stdout "No change to '$GroupName'"
	}

	if { $tot_obj_in_rect > 0 } {
	    puts stdout "Items in Rectangle\:\n$objs"
	} else {
	    puts stdout "Rectangle is empty"
	}
}

proc dg {} { done_grouper }

proc done_grouper {} {
     global mged_gui mged_default mged_players

     for {set i 0} {1} {incr i} {
		set id [subst $mged_default(id)]_$i
 		if {[lsearch -exact $mged_players $id] != -1} {
			break;
		}
     }

     set mged_gui($id,mouse_behavior) d

     set_mouse_behavior $id
     rset r draw 0
     bind $mged_gui($id,active_dm) <ButtonRelease-2> " winset $mged_gui($id,active_dm); dm idle"
}


