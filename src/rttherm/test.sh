#!/bin/sh
# -X100 just for coredumps
# -!00010000 for tabdata debug
# -X0f7 for shading and lighting

rm -f ./moss.ssamp*

./rttherm -P1 -o moss.ssamp -M -s64 -X100 -!00010000 -X0f7 $* \
  ../.db.li/moss.g all.g \
	<< EOF
viewsize 1.572026215e+02;
eye_pt 6.379990387e+01 3.271768951e+01 3.366661453e+01;
viewrot -5.735764503e-01 8.191520572e-01 0.000000000e+00
0.000000000e+00 -3.461886346e-01 -2.424038798e-01 9.063078165e-01
0.000000000e+00 7.424039245e-01 5.198368430e-01 4.226182699e-01
0.000000000e+00 0.000000000e+00 0.000000000e+00 0.000000000e+00
1.000000000e+00 ;
start 0;
anim all.g/platform.r temp 270;
end;
EOF


echo ./ssampview moss.ssamp

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
