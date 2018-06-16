@echo off

robocopy libs build\sysroot\usr\local\lib\flaxlibs /e /nfl /ndl /njh /njs /nc /ns /np
build\meson-rel\flaxc.exe -sysroot build\sysroot -run programs\fic\main.flx