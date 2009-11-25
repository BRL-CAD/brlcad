The StereoI files are from the NVidia SDK 9.5 and are needed to use the
NVidia Consumer 3D Stereo driver:

"c:/Program Files/NVIDIA Corporation/SDK 9.5/LIBS/lib/Release/StereoI.lib"
"c:/Program Files/NVIDIA Corporation/SDK 9.5/LIBS/inc/StereoI/StereoI.h"

Currently, the Microsoft compiler must be used because StereoI.h is
for C++ only, and StereoI.lib has references to msvcrt's operator new
and operator delete and mingw does not provide entry points for those
operators (??2@YAPAXI@Z and ??3@YAXPAX@Z) in its import library for
msvcrt.
