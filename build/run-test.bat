@echo off

robocopy libs build\sysroot\usr\local\lib\flaxlibs /e /nfl /ndl /njh /njs /nc /ns /np

SETLOCAL

IF /I"%1%"=="Release" (
	SET buildDir="build\meson-rel"
) ELSE (
	SET buildDir="build\meson-dbg"
)

ninja -C %buildDir% && copy /B %buildDir%\flaxc.exe build\sysroot\windows\%1\ && cls &&build\sysroot\windows\%1\flaxc.exe -sysroot build\sysroot -run build\%2.flx

ENDLOCAL