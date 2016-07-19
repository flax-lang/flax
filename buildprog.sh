#!/bin/bash

rm -r build/sysroot/usr/local/lib/flaxlibs/*
cp -R libs/* build/sysroot/usr/local/lib/flaxlibs/

if [ -n "$1" -a "$1" == "compile" ]; then
	time build/sysroot/usr/local/bin/flaxc -Wno-unused-variable -sysroot build/sysroot -o build/test build/test.flx
	echo -e "\n\n-----------------\n\n"
	build/test
elif [ -n "$1" -a "$1" == "dump" ]; then
	time build/sysroot/usr/local/bin/flaxc -Wno-unused-variable -sysroot build/sysroot -c -print-lir build/test.flx
else
	time build/sysroot/usr/local/bin/flaxc -Wno-unused-variable -sysroot build/sysroot -jit build/test.flx
fi
