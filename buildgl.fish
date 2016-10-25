#!/usr/local/bin/fish



function callCompiler

	rm -r build/sysroot/usr/local/lib/flaxlibs/*
	cp -R libs/* build/sysroot/usr/local/lib/flaxlibs/

	set opt "-O3"
	set dump ""
	set compile "-jit"

	if contains "noopt" $argv
		set opt "-O0"
	end

	if contains "dump" $argv
		set dump "-print-lir"
		set compile "-jit"
	else if contains "dumpf" $argv
		set dump "-print-fir"
		set compile "-jit"
	else if contains "compile" $argv
		set dump ""
		set compile "-o build/gltest"
	end


	# copy the libraries
	eval mkdir -p "build/sysroot/usr/local/lib/flaxlibs/"
	eval cp "-R" "libs/*" "build/sysroot/usr/local/lib/flaxlibs/"


	eval time "build/sysroot/usr/local/bin/flaxc -framework OpenGL -framework GLUT -lsdl2 -sysroot build/sysroot" $opt $compile $dump "build/gltest.flx"

	if [ $compile != "-jit" -a $dump = "" ]
		build/test
	end
end


callCompiler $argv
