@echo off
robocopy libs build\sysroot\usr\local\lib\flaxlibs /e /nfl /ndl /njh /njs /nc /ns /np
echo Compiling...
msbuild /nologo /verbosity:m /nr:false /p:Configuration=Debug /m flax.vcxproj && cls && build\sysroot\windows\Debug\flax.exe -sysroot build\sysroot -run build\%1.flx