@echo off
msbuild /nologo /verbosity:m /nologo flax.vcxproj
build\sysroot\windows\Debug\flax.exe -sysroot build\sysroot -run build\%1.flx