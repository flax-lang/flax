@echo off
robocopy libs build\sysroot\usr\local\lib\flaxlibs /e /nfl /ndl /njh /njs /nc /ns /np
msbuild /nologo /verbosity:m flax.vcxproj && build\sysroot\windows\Debug\flax.exe -sysroot build\sysroot -run build\%1.flx