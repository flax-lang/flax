@echo off
robocopy libs build\sysroot\usr\local\lib\flaxlibs /e /nfl /ndl /njh /njs /nc /ns /np
echo Compiling...

REM fuck msbuild.
REM msbuild /nologo /verbosity:m /nr:false /p:Configuration=%1 /m flax.vcxproj && cls && build\sysroot\windows\%1\flax.exe -sysroot build\sysroot -run build\%2.flx

REM meson meson-build-dir
ninja -C build\meson && copy /B build\meson\flaxc.exe build\sysroot\windows\%1\ && copy /B build\meson\flaxc.pdb build\sysroot\windows\%1\ && cls &&build\sysroot\windows\%1\flaxc.exe -sysroot build\sysroot -run build\%2.flx