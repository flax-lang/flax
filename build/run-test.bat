@echo off
robocopy libs build\sysroot\usr\local\lib\flaxlibs /e /nfl /ndl /njh /njs /nc /ns /np
echo Compiling...
msbuild /nologo /verbosity:m /nr:false /p:Configuration=%1 /m flax.vcxproj && cls && build\sysroot\windows\%1\flax.exe -sysroot build\sysroot -run build\%2.flx