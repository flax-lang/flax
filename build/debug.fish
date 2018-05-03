#!/usr/local/bin/fish
lldb build/sysroot/usr/local/bin/flaxc -- -sysroot build/sysroot/ -run $argv[1]
