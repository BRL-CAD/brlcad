static char ogl_init_str[] = "\
proc init_ogl { win } {\
  bindem_ogl $win;\
  wm aspect $win 1 1 1 1;\
};\
\
proc bindem_ogl { name } {\
global hot_key;\
\
bind $name <1> \"winset $name; zoom 0.5\";\
bind $name <2> \"winset $name; dm mouse %b 1 %x %y\";\
bind $name <3> \"winset $name; zoom 2.0\";\
bind $name <Shift-ButtonRelease-1> \"winset $name; dm vtb t 0 %x %y\";\
bind $name <Shift-ButtonPress-1> \"winset $name; dm vtb t 1 %x %y\";\
bind $name <Shift-ButtonRelease-2> \"winset $name; dm vtb r 0 %x %y\";\
bind $name <Shift-ButtonPress-2> \"winset $name; dm vtb r 1 %x %y\";\
bind $name <Shift-ButtonRelease-3> \"winset $name; dm vtb z 0 %x %y\";\
bind $name <Shift-ButtonPress-3> \"winset $name; dm vtb z 1 %x %y\";\
bind $name <Button1-KeyRelease-Shift_L> \"winset $name; dm vtb t 0 %x %y\";\
bind $name <Button1-KeyRelease-Shift_R> \"winset $name; dm vtb t 0 %x %y\";\
bind $name <Button2-KeyRelease-Shift_L> \"winset $name; dm vtb r 0 %x %y\";\
bind $name <Button2-KeyRelease-Shift_R> \"winset $name; dm vtb r 0 %x %y\";\
bind $name <Button3-KeyRelease-Shift_L> \"winset $name; dm vtb z 0 %x %y\";\
bind $name <Button3-KeyRelease-Shift_R> \"winset $name; dm vtb z 0 %x %y\";\
\
bind $name a \"winset $name; adc\";\
bind $name e \"winset $name; set e_axis !\";\
bind $name v \"winset $name; set v_axis !\";\
bind $name w \"winset $name; set w_axis !\";\
bind $name i \"winset $name; aip f\";\
bind $name I \"winset $name; aip b\";\
bind $name u \"winset $name; aip b\";\
bind $name p \"winset $name; M 1 0 0\";\
bind $name 0 \"winset $name; knob zero\";\
bind $name x \"winset $name; iknob x 0.05\";\
bind $name y \"winset $name; iknob y 0.05\";\
bind $name z \"winset $name; iknob z 0.05\";\
bind $name <Control-x> \"winset $name; iknob x -0.05\";\
bind $name <Control-y> \"winset $name; iknob y -0.05\";\
bind $name <Control-z> \"winset $name; iknob z -0.05\";\
bind $name X \"winset $name; iknob X 0.05\";\
bind $name Y \"winset $name; iknob Y 0.05\";\
bind $name Z \"winset $name; iknob Z 0.05\";\
bind $name <Control-X> \"winset $name; iknob X -0.05\";\
bind $name <Control-Y> \"winset $name; iknob Y -0.05\";\
bind $name <Control-Z> \"winset $name; iknob Z -0.05\";\
bind $name 3 \"winset $name; press 35,25\";\
bind $name 4 \"winset $name; press 45,45\";\
bind $name f \"winset $name; press front\";\
bind $name t \"winset $name; press top\";\
bind $name b \"winset $name; press bottom\";\
bind $name l \"winset $name; press left\";\
bind $name r \"winset $name; press right\";\
bind $name R \"winset $name; press rear\";\
bind $name s \"winset $name; press sill\";\
bind $name o \"winset $name; press oill\";\
bind $name q \"winset $name; press reject\";\
bind $name <Left> \"winset $name; sv 10 0\";\
bind $name <Right> \"winset $name; sv -10 0\";\
bind $name <Down> \"winset $name; sv 0 10\";\
bind $name <Up> \"winset $name; sv 0 -10\";\
bind $name <F1> \"winset $name; dm set depthcue !\";\
bind $name <F2> \"winset $name; dm set zclip !\";\
bind $name <F3> \"winset $name; dm set perspective !\";\
bind $name <F4> \"winset $name; dm set zbuffer !\";\
bind $name <F5> \"winset $name; dm set lighting !\";\
bind $name <F6> \"winset $name; dm set set_perspective !\";\
bind $name <F7> \"winset $name; set faceplate !\";\
bind $name <F8> \"winset $name; set show_menu !\";\
\
set hot_key 65478;\
bind $name <F9> \"winset $name; set send_key !\";\
\
bind $name <F10> \"winset $name; dm set virtual_trackball !\";\
bind $name <F12> \"winset $name; knob zero\";\
bind $name <Control-n> \"winset $name; set view 1\";\
bind $name <Control-p> \"winset $name; set view 0\";\
bind $name <Control-v> \"winset $name; set view !\";\
};\
";

