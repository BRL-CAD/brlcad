static char glx_init_str[] = "\
proc create_glx { dpy parent child ref w h rgb db } {\
  toplevel $parent -screen $dpy;\
  glxwin $child -ref $ref -width $w -height $h -rgb $rgb -db $db;\
  bindem_glx $parent $child;\
  pack $child -expand 1 -fill both;\
\
  wm aspect $parent 1 1 1 1;\
};\
\
proc bindem_glx { parent child } {\
global hot_key;\
\
bind $parent <1> \"winset $child; zoom 0.5\";\
bind $parent <2> \"winset $child; dm mouse %b 1 %x %y\";\
bind $parent <3> \"winset $child; zoom 2.0\";\
\
bind $parent <Shift-ButtonRelease-1> \"winset $child; dm vtb t 0 %x %y\";\
bind $parent <Shift-ButtonPress-1> \"winset $child; dm vtb t 1 %x %y\";\
bind $parent <Shift-ButtonRelease-2> \"winset $child; dm vtb r 0 %x %y\";\
bind $parent <Shift-ButtonPress-2> \"winset $child; dm vtb r 1 %x %y\";\
bind $parent <Shift-ButtonRelease-3> \"winset $child; dm vtb z 0 %x %y\";\
bind $parent <Shift-ButtonPress-3> \"winset $child; dm vtb z 1 %x %y\";\
bind $parent <Button1-KeyRelease-Shift_L> \"winset $child; dm vtb t 0 %x %y\";\
bind $parent <Button1-KeyRelease-Shift_R> \"winset $child; dm vtb t 0 %x %y\";\
bind $parent <Button2-KeyRelease-Shift_L> \"winset $child; dm vtb r 0 %x %y\";\
bind $parent <Button2-KeyRelease-Shift_R> \"winset $child; dm vtb r 0 %x %y\";\
bind $parent <Button3-KeyRelease-Shift_L> \"winset $child; dm vtb z 0 %x %y\";\
bind $parent <Button3-KeyRelease-Shift_R> \"winset $child; dm vtb z 0 %x %y\";\
\
bind $parent a \"winset $child; adc\";\
bind $parent e \"winset $child; set e_axis !\";\
bind $parent v \"winset $child; set v_axis !\";\
bind $parent w \"winset $child; set w_axis !\";\
bind $parent i \"winset $child; aip f\";\
bind $parent I \"winset $child; aip b\";\
bind $parent u \"winset $child; aip b\";\
bind $parent p \"winset $child; M 1 0 0\";\
bind $parent 0 \"winset $child; knob zero\";\
bind $parent x \"winset $child; iknob x 0.05\";\
bind $parent y \"winset $child; iknob y 0.05\";\
bind $parent z \"winset $child; iknob z 0.05\";\
bind $parent <Control-x> \"winset $child; iknob x -0.05\";\
bind $parent <Control-y> \"winset $child; iknob y -0.05\";\
bind $parent <Control-z> \"winset $child; iknob z -0.05\";\
bind $parent X \"winset $child; iknob X 0.05\";\
bind $parent Y \"winset $child; iknob Y 0.05\";\
bind $parent Z \"winset $child; iknob Z 0.05\";\
bind $parent <Control-X> \"winset $child; iknob X -0.05\";\
bind $parent <Control-Y> \"winset $child; iknob Y -0.05\";\
bind $parent <Control-Z> \"winset $child; iknob Z -0.05\";\
bind $parent 3 \"winset $child; press 35,25\";\
bind $parent 4 \"winset $child; press 45,45\";\
bind $parent f \"winset $child; press front\";\
bind $parent t \"winset $child; press top\";\
bind $parent b \"winset $child; press bottom\";\
bind $parent l \"winset $child; press left\";\
bind $parent r \"winset $child; press right\";\
bind $parent R \"winset $child; press rear\";\
bind $parent s \"winset $child; press sill\";\
bind $parent o \"winset $child; press oill\";\
bind $parent q \"winset $child; press reject\";\
bind $parent <Left> \"winset $child; sv -10 0\";\
bind $parent <Right> \"winset $child; sv 10 0\";\
bind $parent <Down> \"winset $child; sv 0 -10\";\
bind $parent <Up> \"winset $child; sv 0 10\";\
bind $parent <F1> \"winset $child; dm set depthcue !\";\
bind $parent <F2> \"winset $child; dm set zclip !\";\
bind $parent <F3> \"winset $child; dm set perspective !\";\
bind $parent <F4> \"winset $child; dm set zbuffer !\";\
bind $parent <F5> \"winset $child; dm set lighting !\";\
bind $parent <F6> \"winset $child; dm set set_perspective !\";\
bind $parent <F7> \"winset $child; set faceplate !\";\
bind $parent <F8> \"winset $child; set show_menu !\";\
\
set hot_key 65478;\
bind $parent <F9> \"winset $child; set send_key !\";\
\
bind $parent <F10> \"winset $child; dm set virtual_trackball !\";\
bind $parent <F12> \"winset $child; knob zero\";\
bind $parent <Control-n> \"winset $child; set view 1\";\
bind $parent <Control-p> \"winset $child; set view 0\";\
bind $parent <Control-v> \"winset $child; set view !\";\
};\
";
