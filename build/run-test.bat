@echo off

SETLOCAL

IF /I"%1%"=="Release" (
	SET buildDir="build\meson-rel"
) ELSE (
	SET buildDir="build\meson-dbg"
)

call "build\build.bat" %1 %2 && cls &&build\sysroot\windows\%1\flaxc.exe -sysroot build\sysroot -run build\%2.flx

ENDLOCAL