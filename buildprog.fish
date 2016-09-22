#!/usr/local/bin/fish



function callCompiler

	rm -r build/sysroot/usr/local/lib/flaxlibs/*
	cp -R libs/* build/sysroot/usr/local/lib/flaxlibs/

	set opt "-O3"
	set dump ""
	set compile "-jit"

	if contains "noopt" $argv
		set opt "-Ox"
	end

	if contains "dump" $argv
		set dump "-print-lir -c"
		set compile ""
	else if contains "dumpf" $argv
		set dump "-print-fir -c"
		set compile ""
	else if contains "compile"
		set dump ""
		set compile "-o build/test"
	end


	eval time "build/sysroot/usr/local/bin/flaxc -Wno-unused-variable -sysroot build/sysroot" $opt $compile $dump "build/test.flx"

	if [ $compile != "-jit" -a $dump = "" ]
		echo -e "\n\n-----------------\n\n"
		build/test
	end
end


callCompiler $argv
