@echo off
echo ---------- %1 ----------
cd %1
nmake /f makefile.vc /nologo %2