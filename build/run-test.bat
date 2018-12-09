@echo off

robocopy libs build\sysroot\usr\local\lib\flaxlibs /e /purge /nfl /ndl /njh /njs /nc /ns /np >nul 2>&1

SETLOCAL

IF /I "%1"=="release" (
	SET buildDir="build\meson-rel"
)

IF /I "%1"=="debug" (
	SET buildDir="build\meson-dbg"
)

IF /I "%1"=="debugopt" (
	SET buildDir="build\meson-reldbg"
)


ninja -C %buildDir% && cls && %buildDir%\flaxc.exe -Ox -sysroot build\sysroot -run build\%2.flx

IF /I "%1"=="release" (
	copy %buildDir%\flaxc.exe build\sysroot\usr\local\bin\ >NUL
)

ENDLOCAL