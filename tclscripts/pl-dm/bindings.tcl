proc sampler_bind_dm { name } {
bind $name <1> "zoom 0.5"
bind $name <2> "dm m %b 1 %x %y"
bind $name <3> "zoom 2.0"

bind $name <Control-ButtonRelease-1> "dm am t 0 %x %y"
bind $name <Control-ButtonPress-1> "dm am t 1 %x %y"
bind $name <Control-ButtonRelease-2> "dm am r 0 %x %y"
bind $name <Control-ButtonPress-2> "dm am r 1 %x %y"
bind $name <Control-ButtonRelease-3> "dm am z 0 %x %y"
bind $name <Control-ButtonPress-3> "dm am z 1 %x %y"
bind $name <Button1-KeyRelease-Control_L> "dm am t 0 %x %y"
bind $name <Button1-KeyRelease-Control_R> "dm am t 0 %x %y"
bind $name <Button2-KeyRelease-Control_L> "dm am r 0 %x %y"
bind $name <Button2-KeyRelease-Control_R> "dm am r 0 %x %y"
bind $name <Button3-KeyRelease-Control_L> "dm am z 0 %x %y"
bind $name <Button3-KeyRelease-Control_R> "dm am z 0 %x %y"

bind $name f "ae 0 0"
bind $name l "ae 90 0"
bind $name R "ae 180 0"
bind $name r "ae 270 0"
bind $name "ae -90 90"
bind $name "ae -90 -90"
bind $name "ae 35 25"
bind $name "ae 45 45"

bind $name <F1> "dm set depthcue !"
bind $name <F2> "dm set zclip !"
bind $name <F4> "dm set zbuffer !"
bind $name <F5> "dm set lighting !"

#bind $name <Left> "ae -i -1 0 0"
#bind $name <Right> "ae -i 1 0 0"
#bind $name <Down> "ae -i 0 -1 0"
#bind $name <Up> "ae -i 0 1 0"
}

