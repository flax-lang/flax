@echo off
robocopy libs build\sysroot\usr\local\lib\flaxlibs /e /nfl /ndl /njh /njs /nc /ns /np
echo Compiling...

REM fuck msbuild.
REM msbuild /nologo /verbosity:m /nr:false /p:Configuration=%1 /m flax.vcxproj && cls && build\sysroot\windows\%1\flax.exe -sysroot build\sysroot -run build\%2.flx

meson meson_build_dir && ninja -C meson_build_dir && copy meson_build_dir\flaxc.exe build\sysroot\windows\%1\flaxc.exe && cls &&build\sysroot\windows\%1\flaxc.exe -sysroot build\sysroot -run build\%2.flx