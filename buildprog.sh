#!/bin/bash

rm -r build/sysroot/usr/local/lib/flaxlibs/*
cp -R libs/* build/sysroot/usr/local/lib/flaxlibs/

build/sysroot/usr/local/bin/flaxc -Wno-unused-variable -no-lowercase-builtin -sysroot build/sysroot -jit build/test.flx
