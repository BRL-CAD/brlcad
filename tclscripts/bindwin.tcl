proc _init_dm { func win } {
    $func $win
    catch { init_dm $win }
}

proc bind_dm { name } {
puts "bind_dm: no default bindings"
}

