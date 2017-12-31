#!/usr/local/bin/fish
eval make -R -j4 build; and build/sysroot/usr/local/bin/flaxc -sysroot build/sysroot -run build/$argv[1].flx
